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
 * 初始值: 直接沿用 LVGL_Demo 旧 10-bit map {78,183,403,667,950} (已知良好),
 * 待 KeyADC_Test 实测 500 次采样后用 min/max 中点重标定。 */
#define KEY_RIGHT_MAX   78     /* < 78   -> KEY_RIGHT */
#define KEY_UP_MAX      183    /* < 183  -> KEY_UP */
#define KEY_SELECT_MAX  403    /* < 403  -> KEY_SELECT */
#define KEY_LEFT_MAX    667    /* < 667  -> KEY_LEFT */
#define KEY_DOWN_MAX    950    /* < 950  -> KEY_DOWN, 其余 NONE */

/* 死区 (raw 单位): 读到值距任一阈值 < 此值时保持上一状态, 防止按键在档位边界抖动 */
#define KEY_ADC_DEADBAND 12

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
