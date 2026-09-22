#ifndef APP_CPM_SERIAL_H
#define APP_CPM_SERIAL_H

#include <stdint.h>

/* ============================================================
 * CPM 串口协议 (与 PC 上位机 computer_performance_monitor 配套)
 *
 * 物理层: Serial (USART1 / WCH-Link 虚拟 COM), 9600 8N1
 * 帧格式: [0x5A][0xA5][CMD][VAL]  4 字节定长, 无长度无 CRC
 *
 * 命令:
 *   0x01 CPU温度      0x02 CPU利用率   0x03 RAM利用率
 *   0x04 RAM已用GB    0x05 RAM总量GB
 *   0x06 GPU温度      0x07 GPU利用率   0x08 GPU显存利用率
 *   0x09 GPU显存已用  0x0A GPU显存总量
 *   0x0B CPU频率(100MHz/VAL, 36=3600MHz)   0x0C GPU频率(100MHz/VAL, 24=2400MHz)
 *   0x0D RAM已用(别名0x04)  0x0E RAM总量(别名0x05)
 *   0x0F 显存已用(别名0x09) 0x10 显存总量(别名0x0A)
 *   0x11 网络下载(10MB/s/VAL, 12=120MB/s)  0x12 网络上传(10MB/s/VAL, 2=20MB/s)
 *   0xFF 握手: 收 5A A5 FF 01 -> 回 5A A5 FF 10
 *
 * 应答: 收到 5A A5 CMD VAL (0x01~0x12) -> 立即回 5A A5 CMD FF
 *
 * 协议口不能混入 debug 输出: 本模块的 CPM_DEBUG 与全工程
 * CPM_DBG (board.h) 联动, 0 = 正式 (协议口纯净), 1 = 调试 (同口打日志).
 * ============================================================ */

/* 调试模式: 0=正式 (协议口只走 5A A5 帧), 1=串口打印收到的 CMD/VALUE */
#ifndef CPM_DEBUG
#define CPM_DEBUG 0
#endif

/* CPM_DEBUG=1 时全工程 debug 打印 (board.h CPM_DBG) 一并打开,
 * 走同一个协议口 (要求 PC 上位机未运行). 兼容两种 include 顺序. */
#if CPM_DEBUG
#undef CPM_DBG
#define CPM_DBG 1
#endif

/* CPM 性能数据 (8-bit: 温度 °C / 利用率 % / 容量 GB / 频率 100MHz / 网速 10MB/s) */
typedef struct
{
    uint8_t cpu_temp;        /* 0x01 */
    uint8_t cpu_load;        /* 0x02 */
    uint8_t ram_load;        /* 0x03 */
    uint8_t ram_used;        /* 0x04 GB (0x0D 别名) */
    uint8_t ram_total;       /* 0x05 GB (0x0E 别名) */
    uint8_t gpu_temp;        /* 0x06 */
    uint8_t gpu_load;        /* 0x07 */
    uint8_t gpu_mem_load;    /* 0x08 */
    uint8_t gpu_mem_used;    /* 0x09 GB (0x0F 别名) */
    uint8_t gpu_mem_total;   /* 0x0A GB (0x10 别名) */
    uint8_t cpu_freq;        /* 0x0B 100MHz/VAL (36 = 3600MHz) */
    uint8_t gpu_freq;        /* 0x0C 100MHz/VAL (24 = 2400MHz) */
    uint8_t net_down;        /* 0x11 1MB/s/VAL (12 = 12MB/s) */
    uint8_t net_up;          /* 0x12 1MB/s/VAL (2 = 2MB/s) */
} CPM_Data;

/* 帧头魔数 (线上序) */
#define CPM_FRAME_A   0x5A
#define CPM_FRAME_B   0xA5

/* CMD 常量 */
#define CPM_CMD_CPU_TEMP      0x01
#define CPM_CMD_CPU_LOAD      0x02
#define CPM_CMD_RAM_LOAD      0x03
#define CPM_CMD_RAM_USED      0x04
#define CPM_CMD_RAM_TOTAL     0x05
#define CPM_CMD_GPU_TEMP      0x06
#define CPM_CMD_GPU_LOAD      0x07
#define CPM_CMD_GPU_MEM_LOAD  0x08
#define CPM_CMD_GPU_MEM_USED  0x09
#define CPM_CMD_GPU_MEM_TOTAL 0x0A
#define CPM_CMD_CPU_FREQ      0x0B   /* 100MHz/VAL */
#define CPM_CMD_GPU_FREQ      0x0C   /* 100MHz/VAL */
#define CPM_CMD_RAM_USED_B    0x0D   /* 别名 -> ram_used (同 0x04) */
#define CPM_CMD_RAM_TOTAL_B   0x0E   /* 别名 -> ram_total (同 0x05) */
#define CPM_CMD_VRAM_USED_B   0x0F   /* 别名 -> gpu_mem_used (同 0x09) */
#define CPM_CMD_VRAM_TOTAL_B  0x10   /* 别名 -> gpu_mem_total (同 0x0A) */
#define CPM_CMD_NET_DOWN      0x11   /* 10MB/s/VAL */
#define CPM_CMD_NET_UP        0x12   /* 10MB/s/VAL */
#define CPM_CMD_HANDSHAKE     0xFF

#define CPM_BAUD      9600UL

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化: Serial.begin(9600, 8N1) + 使能 USART1 RX 中断 + 配 NVIC + 清状态.
 * 需在 board_init() 之后调用 (ui_init 的 115200 banner 打完再切). */
void cpm_serial_init(void);

/* loop 中非阻塞轮询: 从 RX 环形缓冲批量取字节 -> 状态机解析 -> 存 CPM_Data -> 立即应答.
 * 支持字节错位自动重同步 (任何非 0x5A 字节丢弃, 0x5A 后不接 0xA5 则重置).
 * 接收走 USART1 RX 中断 + 软件环形缓冲 (WCH core 默认纯轮询, 无 RX 中断,
 * LVGL SPI flush 阻塞 ~18ms 会 overrun 掉 9600 字节 -> 整帧丢). */
void cpm_serial_poll(void);

/* 当前累积数据 (调用者只读; 同一 loop 上下文内无竞争) */
const CPM_Data *cpm_serial_data(void);

/* 累计收到的有效帧数 / 错位重同步次数 (调试用) */
uint32_t cpm_serial_rx_frames(void);
uint32_t cpm_serial_resyncs(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CPM_SERIAL_H */
