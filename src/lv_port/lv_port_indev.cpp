#include "lv_port_indev.h"
#include "../bsp/board.h"

extern "C" {
#include <lvgl.h>
}

static void joy_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  (void)drv;
  uint8_t idx = board_joystick_read();
  if (idx < 5) {
    data->state = LV_INDEV_STATE_PRESSED;
    switch (idx) {
      case 0: data->key = LV_KEY_RIGHT; break;
      case 1: data->key = LV_KEY_UP;    break;
      case 2: data->key = LV_KEY_ENTER; break;
      case 3: data->key = LV_KEY_LEFT;  break;
      case 4: data->key = LV_KEY_DOWN;  break;
    }
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    data->key = 0;
  }
}

void lv_port_indev_init(void) {
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_KEYPAD;
  indev_drv.read_cb = joy_read_cb;
  lv_indev_t *joy = lv_indev_drv_register(&indev_drv);
  lv_indev_set_group(joy, lv_group_create());
}
