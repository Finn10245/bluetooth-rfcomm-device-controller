#ifndef RFCOMM_EPOLL_H
#define RFCOMM_EPOLL_H

#include <stdint.h>

/* RFCOMM 初始化（建立 epoll + RFCOMM server socket） */
int rfcomm_init(void);

/* RFCOMM 輪詢（epoll_wait + 收封包 + 解析 + 丟給 HSM） */
int rfcomm_poll(void);

/* RFCOMM 關閉（關閉 epoll、socket） */
void rfcomm_close(void);

/* 封包傳送 API（給 HSM 或其他模組用） */
void rfcomm_send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len);

#endif

