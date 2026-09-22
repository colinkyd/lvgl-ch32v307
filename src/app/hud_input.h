#ifndef APP_HUD_INPUT_H
#define APP_HUD_INPUT_H

/* ============================================================
 * hud_input — 五向键输入层 (复用 bsp/key_adc 边沿事件模型)
 *
 * key_adc 是成熟的非阻塞边沿模型 (30ms 去抖, key_adc_poll + key_adc_event)。
 * 本层只做: 每 loop 取一个边沿 -> 映射到 HUD 动作 (切页 / 亮度)。
 * 不阻塞, 不重复采样 (key_adc 内部已 5 连采中值 + 死区去抖)。
 *
 * 按键映射:
 *   UP     -> 上一页面 (循环)
 *   DOWN   -> 下一页面 (循环)
 *   LEFT   -> 减少亮度 (预留, 仅计数, 页脚显示)
 *   RIGHT  -> 增加亮度 (预留, 仅计数, 页脚显示)
 *   CENTER -> 确认/返回主页 (PAGE_MAIN)
 * ============================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化: 起 key_adc 边沿模型 (key_adc_init 已设 ADC 分辨率) */
void hud_input_init(void);

/* 每 loop 调用: key_adc_poll + 取一个边沿 + 分发。非阻塞。 */
void hud_input_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_HUD_INPUT_H */
