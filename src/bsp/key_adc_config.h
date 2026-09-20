#ifndef BSP_KEY_ADC_CONFIG_H
#define BSP_KEY_ADC_CONFIG_H

/* ===== 五向摇杆 ADC 阈值 (10-bit, 0..1023) =====
 * 判定规则 (升序): v < T_RIGHT -> KEY_RIGHT
 *                  v < T_UP    -> KEY_UP
 *                  v < T_SELECT-> KEY_SELECT
 *                  v < T_LEFT  -> KEY_LEFT
 *                  v < T_DOWN  -> KEY_DOWN
 *                  其余        -> KEY_NONE
 *
 * 标定来源 (2026-09-20 实测, Vref=3.3V, raw = V/3.3*1023):
 *   RIGHT 0.0V -> raw ~0     | UP 0.6V -> raw ~187 | SELECT 1.0V -> raw ~310
 *   LEFT  1.6V -> raw ~498   | DOWN 2.7V -> raw ~838 | NONE 3.3V -> raw ~1023
 * 阈值 = 相邻两档 raw 中点 (各键离边界 >60, 远离 DEADBAND=25):
 *   RIGHT/UP (0+187)/2=93 | UP/SELECT (187+310)/2=248 | SELECT/LEFT (310+498)/2=404
 *   LEFT/DOWN (498+838)/2=668 | DOWN/NONE (838+1023)/2=930
 * (旧值 {78,183,403,667,950} 沿用别的板, UP 上界 183 < 本板 UP 实测 187, 导致 UP 落进 SELECT 区) */
#define KEY_RIGHT_MAX   93     /* < 93   -> KEY_RIGHT */
#define KEY_UP_MAX      248    /* < 248  -> KEY_UP */
#define KEY_SELECT_MAX  404    /* < 404  -> KEY_SELECT */
#define KEY_LEFT_MAX    668    /* < 668  -> KEY_LEFT */
#define KEY_DOWN_MAX    930    /* < 930  -> KEY_DOWN, 其余 NONE */

/* 死区 (raw 单位): 值距任一档位边界 < 此值时保持上一状态, 防档位边界抖动误触。
 * 误触实测: UP 档高位值抖过 183 -> 误触 SELECT; DOWN 档高位值抖过 950 -> 误触 NONE
 * (释放后焦点不动, 表现同误触)。原值 12 太窄, 扩到 25 覆盖边界抖动带。 */
#define KEY_ADC_DEADBAND 25

/* ===== 软件滤波 =====
 * 注意: 滑动平均每次 read_cb 只塞 1 个新采样, 窗口 N 的收敛延迟 ≈ N × read_period。
 * read_cb 周期 = indev_drv.read_period (lv_port_indev.cpp 设 15ms)。
 * 窗口过大 -> 短按被滤波吞掉 (read_cb 来不及报 PRESSED 边沿, LVGL 焦点不动)。
 * 实测 N=16 + 30ms 周期需 ~540ms 才确认一次按键, 短按全丢。
 * N=4 + 15ms 周期 -> 确认延迟 ~75ms, 正常一按(80-100ms)即可通过, 仍保留去抖抗噪。
 * (N=8 实测延迟~150ms 仍吞短按, 再砍到 4) */
#define KEY_ADC_AVG_N   4

/* 去抖: 连续 N 次分类结果一致才接受新按键状态 (2 次 = ~30ms, 配合 15ms 周期) */
#define KEY_ADC_DEBOUNCE 2

#endif /* BSP_KEY_ADC_CONFIG_H */
