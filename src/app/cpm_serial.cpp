/* ============================================================
 * cpm_serial.cpp — CPM 协议串口接收 (CH32V307, 无 FreeRTOS)
 *
 * 物理层: Serial = USART1 (PA9/PA10, WCH-Link 虚拟 COM), 9600 8N1.
 *
 * 接收路径 (关键): WCH core 的 Serial::read() 走 uart_getc() 纯轮询
 *   (查 RXNE 标志, 读单字节 DATAR, 无 RX 中断 / 无 FIFO, 见 uart.c).
 *   LVGL 全屏 SPI flush 阻塞主循环 ~18ms, 期间 9600 波特率 (1 字节 ~1.04ms)
 *   的帧字节会 overrun 掉单字节 RX 寄存器 -> 整帧丢. 实测 30 帧丢 7.
 *   修复: 使能 USART1 RXNE 中断, ISR 把字节收进软件环形缓冲,
 *   cpm_serial_poll() 从 ring 取字节跑状态机. loop 阻塞期间字节不丢.
 *
 * 状态机 (与 RP2040 v_task_usb_uart 的解析逻辑同构, 源换为 RX ring):
 *   S_WAIT_A : 收 0x5A 进入 S_WAIT_B; 其他字节丢弃 (错位重同步)
 *   S_WAIT_B : 收 0xA5 进入 S_WAIT_CMD; 收 0x5A 留在 S_WAIT_B; 其他回 S_WAIT_A
 *   S_WAIT_CMD: 收 CMD 进入 S_WAIT_VAL
 *   S_WAIT_VAL: 收 VAL -> 完整帧, 分发 + 应答, 回 S_WAIT_A
 *
 * 连续帧: 完整帧后立即回 S_WAIT_A, 下一帧 0x5A 即刻进入, 无间隙要求.
 * 非阻塞: poll 只取 ring 现有字节, 不等待; ISR 只做入队 + 清标志 (极简).
 * ============================================================ */
#include "cpm_serial.h"

#include <Arduino.h>
#include "ch32v30x_usart.h"   /* USART1, USART_IT_RXNE, USART_GetITStatus, USART_ReceiveData, USART_ClearITPendingBit */
#include "ch32v30x_misc.h"    /* NVIC_Init, NVIC_PriorityGroupConfig, NVIC_PriorityGroup_2 */
#include "../bsp/board.h"     /* CPM_DBG (Serial 由 Arduino.h 提供) */

static CPM_Data  s_data;
static uint8_t   s_state = 0;        /* S_WAIT_A */
static uint8_t   s_cmd = 0;
static uint32_t  s_rx_frames = 0;
static uint32_t  s_resyncs = 0;

enum { S_WAIT_A = 0, S_WAIT_B, S_WAIT_CMD, S_WAIT_VAL };

/* ---------------- RX 软件环形缓冲 (ISR 写, poll 读, 单生产者/单消费者) ---------------- */
#define CPM_RX_RING  256
static volatile uint8_t  s_rxb[CPM_RX_RING];
static volatile uint16_t s_rx_head = 0;   /* ISR 写位置 */
static uint16_t          s_rx_tail = 0;   /* poll 读位置 */

static void cpm_rx_push(uint8_t b) {
  uint16_t next = (uint16_t)((s_rx_head + 1) & (CPM_RX_RING - 1));
  if (next != s_rx_tail) s_rxb[s_rx_head] = b, s_rx_head = next;
  /* next == s_rx_tail: ring 满, 丢字节 (9600 下极难发生, poll 每 loop 都清) */
}

static int cpm_rx_pop(void) {
  if (s_rx_tail == s_rx_head) return -1;
  uint8_t b = s_rxb[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1) & (CPM_RX_RING - 1));
  return b;
}

/* ---------------- USART1 RX 中断 ---------------- */
extern "C" {
void USART1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART1_IRQHandler(void) {
  if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
    cpm_rx_push((uint8_t)USART_ReceiveData(USART1));
    USART_ClearITPendingBit(USART1, USART_IT_RXNE);
  }
  /* ORE (overrun) 也清掉, 避免它持续挂起. 读 DATAR 已顺带清 ORE,
   * 但保险起见: ORE 由读 STATR->DATAR 清除, ReceiveData 已读 DATAR. */
}
}

void cpm_serial_init(void) {
  /* 协议口: 9600 8N1 (PC 端 CBR_9600). board_init 已开 115200, 这里重设. */
  Serial.begin(CPM_BAUD, SERIAL_8N1);
  s_state = S_WAIT_A;
  s_cmd = 0;
  s_rx_frames = 0;
  s_resyncs = 0;
  s_rx_head = s_rx_tail = 0;
  memset((void*)&s_data, 0, sizeof(s_data));

  /* 使能 RX 中断 + NVIC (优先级组 2: 1 位抢占, 这里抢占=1 最高).
   * core 未设过优先级组, 这里设 Group_2 不影响其他中断 (EXTI 用 NVIC_SetPriority 直接值). */
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
  USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
  NVIC_InitTypeDef n;
  n.NVIC_IRQChannel = USART1_IRQn;
  n.NVIC_IRQChannelPreemptionPriority = 1;
  n.NVIC_IRQChannelSubPriority = 0;
  n.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&n);

#if CPM_DEBUG
  Serial.println();
  Serial.println("[cpm] CPM_DEBUG=1: protocol+debug on same port (PC app must be OFF)");
#endif
}

/* 应答: 5A A5 CMD FF (4 字节, 与 PC encode 后的线上序一致).
 * 用 uart 直接写 (Serial.write 走 uart_debug_write, 同样 USART1). */
static void cpm_reply(uint8_t cmd) {
#if CPM_DEBUG
  Serial.printf("[cpm] tx ACK CMD=0x%02X\r\n", cmd);
#endif
  Serial.write(CPM_FRAME_A);
  Serial.write(CPM_FRAME_B);
  Serial.write(cmd);
  Serial.write(0xFF);
}

/* 完整帧分发: 存数据 + 应答 */
static void cpm_dispatch(uint8_t cmd, uint8_t val) {
  switch (cmd) {
    case CPM_CMD_CPU_TEMP:      s_data.cpu_temp      = val; cpm_reply(cmd); break;
    case CPM_CMD_CPU_LOAD:      s_data.cpu_load      = val; cpm_reply(cmd); break;
    case CPM_CMD_RAM_LOAD:      s_data.ram_load      = val; cpm_reply(cmd); break;
    case CPM_CMD_RAM_USED:      s_data.ram_used      = val; cpm_reply(cmd); break;
    case CPM_CMD_RAM_TOTAL:     s_data.ram_total     = val; cpm_reply(cmd); break;
    case CPM_CMD_GPU_TEMP:      s_data.gpu_temp      = val; cpm_reply(cmd); break;
    case CPM_CMD_GPU_LOAD:      s_data.gpu_load      = val; cpm_reply(cmd); break;
    case CPM_CMD_GPU_MEM_LOAD:  s_data.gpu_mem_load  = val; cpm_reply(cmd); break;
    case CPM_CMD_GPU_MEM_USED:  s_data.gpu_mem_used  = val; cpm_reply(cmd); break;
    case CPM_CMD_GPU_MEM_TOTAL: s_data.gpu_mem_total = val; cpm_reply(cmd); break;
    case CPM_CMD_CPU_FREQ:      s_data.cpu_freq      = val; cpm_reply(cmd); break;
    case CPM_CMD_GPU_FREQ:      s_data.gpu_freq      = val; cpm_reply(cmd); break;
    /* 别名 CMD (与 0x04/0x05/0x09/0x0A 同字段, 兼容不同 PC 版本) */
    case CPM_CMD_RAM_USED_B:    s_data.ram_used      = val; cpm_reply(cmd); break;
    case CPM_CMD_RAM_TOTAL_B:   s_data.ram_total     = val; cpm_reply(cmd); break;
    case CPM_CMD_VRAM_USED_B:   s_data.gpu_mem_used  = val; cpm_reply(cmd); break;
    case CPM_CMD_VRAM_TOTAL_B:  s_data.gpu_mem_total = val; cpm_reply(cmd); break;
    case CPM_CMD_NET_DOWN:      s_data.net_down      = val; cpm_reply(cmd); break;
    case CPM_CMD_NET_UP:        s_data.net_up        = val; cpm_reply(cmd); break;
    case CPM_CMD_HANDSHAKE:
      /* 5A A5 FF 01 -> 5A A5 FF 10; 其他 VAL 不应答 (容错) */
      if (val == 0x01) {
        Serial.write(CPM_FRAME_A);
        Serial.write(CPM_FRAME_B);
        Serial.write(CPM_CMD_HANDSHAKE);
        Serial.write(0x10);
      }
      break;
    default:
      /* 未知命令: 不应答 (PC 端会按超时处理), 不污染统计 */
      break;
  }
  s_rx_frames++;

#if CPM_DEBUG
  Serial.printf("[cpm] rx CMD=0x%02X VALUE=%u\r\n", cmd, val);
#endif
}

void cpm_serial_poll(void) {
  int c;
  while ((c = cpm_rx_pop()) >= 0) {
    switch (s_state) {
      case S_WAIT_A:
        if (c == CPM_FRAME_A) s_state = S_WAIT_B;
        break;  /* 其他字节: 丢弃 (重同步) */

      case S_WAIT_B:
        if (c == CPM_FRAME_B)      s_state = S_WAIT_CMD;
        else if (c == CPM_FRAME_A) { /* 连续 5A: 仍等 A5 */ }
        else { s_state = S_WAIT_A; s_resyncs++; }
        break;

      case S_WAIT_CMD:
        if (c == CPM_FRAME_A) { /* 5A: 可能错位的新帧头, 回等 A5 */
          s_state = S_WAIT_B; s_resyncs++;
        } else {
          s_cmd = (uint8_t)c;
          s_state = S_WAIT_VAL;
        }
        break;

      case S_WAIT_VAL:
        cpm_dispatch(s_cmd, (uint8_t)c);
        s_state = S_WAIT_A;
        break;

      default:
        s_state = S_WAIT_A;
        break;
    }
  }
}

const CPM_Data *cpm_serial_data(void) { return &s_data; }
uint32_t cpm_serial_rx_frames(void) { return s_rx_frames; }
uint32_t cpm_serial_resyncs(void) { return s_resyncs; }
