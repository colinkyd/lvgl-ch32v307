/* 五向摇杆 ADC 驱动 (ST7735 TFT shield 板载摇杆, PIN_JOY_ADC = A3)
 * 不依赖 LVGL: 纯 bsp 层, key_adc_init / key_adc_read / key_scan。
 * 软件滤波 = 滑动平均 (KEY_ADC_AVG_N) + 死区保持 + 连续 N 次一致去抖。 */
#include "key_adc.h"
#include <Arduino.h>

/* 阈值表: 与 key_adc_config.h 的 T_* 顺序一致 (升序), 索引 0..4 对应
 * RIGHT/UP/SELECT/LEFT/DOWN 五档的上边界 */
static const uint16_t key_adc_map[5] = {
  KEY_RIGHT_MAX, KEY_UP_MAX, KEY_SELECT_MAX, KEY_LEFT_MAX, KEY_DOWN_MAX
};

/* 阈值表索引(0=RIGHT,1=UP,2=SELECT,3=LEFT,4=DOWN) -> key_t 枚举值映射。
 * key_t 枚举顺序 (UP=1,DOWN=2,LEFT=3,RIGHT=4,SELECT=5) 与阈值表索引不一致,
 * 故不能直接用 (key_t)(i+1), 必须显式映射 (对照 PC_HUD: 它枚举顺序恰好=表顺序)。 */
static const key_t key_adc_index_to_key[5] = {
  KEY_RIGHT,   /* i=0 */
  KEY_UP,      /* i=1 */
  KEY_SELECT,  /* i=2 */
  KEY_LEFT,    /* i=3 */
  KEY_DOWN     /* i=4 */
};

/* 滑动平均环形缓冲 */
static uint16_t adc_avg_buf[KEY_ADC_AVG_N];
static uint8_t  adc_avg_idx = 0;
static bool     adc_avg_filled = false;

/* 去抖状态 */
static key_t last_class = KEY_NONE;   /* 未确认状态 (含死区保持) */
static key_t stable_key = KEY_NONE;   /* 已确认按键 */
static uint8_t debounce_cnt = 0;

void key_adc_init(void) {
  analogReadResolution(10);           /* 0..1023, 与 board.cpp / 旧标定一致; WCH core 硬件 12bit 读取后自动映射回 10bit */
  for (int i = 0; i < KEY_ADC_AVG_N; i++) adc_avg_buf[i] = key_adc_read();
  adc_avg_idx = 0;
  adc_avg_filled = true;
  last_class = KEY_NONE;
  stable_key = KEY_NONE;
  debounce_cnt = 0;
}

uint16_t key_adc_read(void) {
  return (uint16_t)analogRead(PIN_JOY_ADC);
}

/* raw -> key_t 分类 (含死区保持: 返回值可能等于 hold)。
 * 死区规则 (v2 修正): 新档与 hold 不同, 且 v 距任一档位边界 (含 950 的 NONE 边界)
 * < DEADBAND 时保持 hold。
 * v1 只保护"本档上边界 + hold 即本档"的情况, 挡不住跨档抖动:
 * 按 UP 时值在 183 附近抖到 190 (进 SELECT 档), v1 拿 403 去比 -> 误触 SELECT;
 * 按 RIGHT 抖动越过 78 -> 误触 UP/DOWN。v2 对全部 5 个边界统一设防。 */
static key_t classify(uint16_t v, key_t hold) {
  key_t cur = KEY_NONE;
  for (int i = 0; i < 5; i++) {
    if (v < key_adc_map[i]) { cur = key_adc_index_to_key[i]; break; }
  }
  if (cur == hold) return cur;
  for (int i = 0; i < 5; i++) {
    int16_t d = (int16_t)v - (int16_t)key_adc_map[i];
    if (d < 0) d = -d;
    if (d < KEY_ADC_DEADBAND) return hold;   /* 贴任一边界 -> 保持前态 */
  }
  return cur;
}

key_t key_scan(void) {
  /* 滑动平均: 更新一个样本后取窗口均值 */
  uint16_t raw = key_adc_read();
  uint32_t sum = 0;
  adc_avg_buf[adc_avg_idx] = raw;
  adc_avg_idx = (uint8_t)((adc_avg_idx + 1) & (KEY_ADC_AVG_N - 1));
  for (int i = 0; i < KEY_ADC_AVG_N; i++) sum += adc_avg_buf[i];
  uint16_t avg = (uint16_t)(sum / KEY_ADC_AVG_N);

  key_t cls = classify(avg, last_class);
  last_class = cls;

  /* 去抖: 连续 KEY_ADC_DEBOUNCE 次一致才切换 */
  if (cls == stable_key) {
    debounce_cnt = 0;
  } else if (++debounce_cnt >= KEY_ADC_DEBOUNCE) {
    stable_key = cls;
    debounce_cnt = 0;
  }
  return stable_key;
}

/* ================= 边沿事件模型 (对照 PC_HUD_Cyber key_scan.cpp) =================
 * 单层 30ms 边沿去抖 + 按住计时, 不用滑动平均/死区 (PC_HUD 实测单层即可)。
 * 每 loop: key_adc_poll() -> key_adc_event() 取一个边沿 -> 手动分发焦点。 */
static key_t  e_last_key   = KEY_NONE;   /* 已确认按键状态 (含 NONE=释放) */
static uint32_t e_last_change = 0;       /* 上次确认切换时刻 (30ms 去抖) */
static uint32_t e_press_ms    = 0;       /* 当前键按下时刻 (按住计时) */
static key_t    e_pending_key = KEY_NONE;/* 待取走的边沿 (按下) */
static bool     e_started     = false;   /* 边沿模型是否已起算 (首次 poll 起) */

void key_adc_poll(void) {
  /* 5 连采中值: LVGL 主循环 ~9Hz (CPU 99%), 单采样易恰好采到 ADC 尖峰;
   * 中值抗单点/双点尖峰 (5 采 3 中), 开销 <1ms, 不影响 poll 周期。 */
  uint16_t s[5];
  for (int i = 0; i < 5; i++) s[i] = key_adc_read();
  for (int i = 0; i < 4; i++)
    for (int j = i + 1; j < 5; j++)
      if (s[j] < s[i]) { uint16_t t = s[i]; s[i] = s[j]; s[j] = t; }
  uint16_t v = s[2];
  key_t cur = classify(v, e_last_key);   /* 死区分类 (全边界设防) */

  if (!e_started) {                       /* 首次: 锁定初始态, 不发边沿 */
    e_last_key = cur;
    e_last_change = millis();
    e_press_ms = millis();
    e_pending_key = KEY_NONE;
    e_started = true;
    return;
  }

  if (cur != e_last_key) {
    if (millis() - e_last_change >= 30) { /* 30ms 边沿去抖 */
      e_last_change = millis();
      e_last_key = cur;
      e_press_ms = millis();              /* 按住计时起点 */
      e_pending_key = cur;                /* 边沿: 按下 (NONE=释放也记, event 里忽略 NONE) */
    }
  }
}

key_t key_adc_event(void) {
  key_t k = e_pending_key;
  e_pending_key = KEY_NONE;
  if (k == KEY_NONE) return KEY_NONE;     /* NONE 边沿(释放)对分发无意义, 返回 NONE */
  return k;
}

uint32_t key_adc_hold_ms(void) {
  if (e_last_key == KEY_NONE) return 0;
  return millis() - e_press_ms;
}

/* 当前稳定按键状态 (不消费 pending, 供日志用) */
key_t key_adc_current(void) { return e_last_key; }
