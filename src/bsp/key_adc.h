#ifndef BSP_KEY_ADC_H
#define BSP_KEY_ADC_H

#include <stdint.h>
#include "board.h"          /* PIN_JOY_ADC */
#include "key_adc_config.h" /* 阈值表 (实测标定) */

#ifdef __cplusplus
extern "C" {
#endif

/* 五向摇杆 ADC 按键 (ST7735 TFT shield 板载 ADPS7528 摇杆, 接 PIN_JOY_ADC=A3)
 * 10-bit analogRead, 0..1023 (与 board.cpp / 旧标定一致)。
 * 电压顺序 (低->高): RIGHT < UP < SELECT < LEFT < DOWN < NONE(悬空, ~1023) */
/* 注意: WCH core 经 Arduino.h -> stdio.h -> sys/types.h 链入 POSIX 的
 *   typedef __key_t key_t;   (= int, IPC key, 嵌入式里无人用)
 * 我的 typedef enum 会与其冲突, 故枚举真名用 key_adc_t, 再用宏把 key_t 映射过去,
 * 对外接口保持 key_t。key_t 是宏不是类型名, C/C++ 均安全。 */
typedef enum {
  KEY_NONE = 0,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_SELECT
} key_adc_t;
#ifndef key_t
#define key_t key_adc_t
#endif

/* 初始化: 设置 ADC 10bit 分辨率 (须在 board_init 的 Serial 之后调用) */
void key_adc_init(void);

/* 单次原始采样 (无滤波): 返回 10-bit raw 值 */
uint16_t key_adc_read(void);

/* 带软件滤波的读取: 滑动平均 (KEY_ADC_AVG_N) 后做阈值分类 [电平模型, 旧] */
key_t key_scan(void);

/* ================= 边沿事件模型 (对照 PC_HUD_Cyber, 替代 LVGL indev) =================
 * PC_HUD 实测可靠: 单层 30ms 边沿去抖 + 每 loop 高频轮询 + 手动分发,
 * 不用 LVGL keypad indev 的"电平→边沿"转换 (LVGL 8.3 多层滤波下短按被吞)。
 * 调用约定: 每个 loop 调一次 key_adc_poll(); 随后 key_adc_event() 取走一个边沿。 */

/* 每 loop 调用: 采样 + 30ms 边沿去抖, 更新 pending_key / hold_ms */
void key_adc_poll(void);

/* 取走一个边沿事件 (按下); 无事件返回 KEY_NONE (NONE 边沿即释放, 也返回 KEY_NONE) */
key_t key_adc_event(void);

/* 当前键已持续按住 ms (无键返回 0), 供长按判定 */
uint32_t key_adc_hold_ms(void);

/* 当前稳定按键状态 (不消费 pending, 供日志/诊断用) */
key_t key_adc_current(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_KEY_ADC_H */
