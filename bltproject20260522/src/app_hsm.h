// src/app_hsm.h
#ifndef APP_HSM_H
#define APP_HSM_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    EVT_NONE = 0,
    EVT_LINK_UP,
    EVT_LINK_DOWN,
    EVT_RX_FRAME,   // 新增：收到一個完整且 CRC 正確的封包
} AppEventType;

typedef struct {
    uint8_t cmd;
    uint8_t payload[32];
    uint8_t len;    // payload 長度
} AppFrame;

typedef struct {
    AppEventType type;
    AppFrame frame; // 只有 EVT_RX_FRAME 會用到
} AppEvent;

void app_hsm_init(void);
void app_hsm_dispatch(const AppEvent *e);
void app_hsm_close(void);

#endif

