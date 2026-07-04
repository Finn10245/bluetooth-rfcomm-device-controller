// src/rfcomm_epoll.c
#include "rfcomm_epoll.h"
#include "app_hsm.h"
#include "crc16.h"
#include "log.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define RFCOMM_CHANNEL 3

static int epfd = -1;
static int server_sock = -1;
static int client_sock = -1;

static int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* 簡單封包解析 buffer */
#define RX_BUF_SIZE 512
static uint8_t rx_buf[RX_BUF_SIZE];
static size_t  rx_len = 0;

/* 封包格式：
 * [0] 0x55
 * [1] 0xAA
 * [2] LEN (from CMD)
 * [3] CMD
 * [4..] PAYLOAD
 * [N-2] CRC_LO
 * [N-1] CRC_HI
 */

static void send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t buf[64];
    if (len > 32) len = 32;

    buf[0] = 0x55;
    buf[1] = 0xAA;
    buf[2] = 1 + len; // CMD + payload

    buf[3] = cmd;
    memcpy(&buf[4], payload, len);

    size_t crc_start = 2; // from LEN
    size_t crc_len   = 1 + len; // LEN not included, or included? 這裡我們算 CMD+payload
    uint16_t crc = crc16_modbus(&buf[3], crc_len); // CMD + payload

    size_t total = 4 + len + 2;
    buf[4 + len] = (uint8_t)(crc & 0xFF);
    buf[5 + len] = (uint8_t)(crc >> 8);

    if (client_sock >= 0) {
        write(client_sock, buf, total);
        log_info("[RFCOMM] send frame cmd=0x%02X len=%u", cmd, len);
    }
}

static void process_rx_buffer(void)
{
    size_t i = 0;

    while (rx_len >= 6) { // 最小長度：STX(2) + LEN(1) + CMD(1) + CRC(2)
        // 找 STX
        if (!(rx_buf[0] == 0x55 && rx_buf[1] == 0xAA)) {
            // shift 一個 byte
            memmove(rx_buf, rx_buf + 1, rx_len - 1);
            rx_len--;
            continue;
        }

        uint8_t len_field = rx_buf[2]; // CMD + payload 長度
        size_t frame_len = 2 + 1 + len_field + 2; // STX(2) + LEN(1) + data + CRC(2)

        if (frame_len > RX_BUF_SIZE) {
            log_error("[RFCOMM] frame too long, drop buffer");
            rx_len = 0;
            return;
        }

        if (rx_len < frame_len) {
            // 還沒收完
            return;
        }

        // 解析
        uint8_t cmd = rx_buf[3];
        uint8_t payload_len = len_field - 1;
        if (payload_len > 32) payload_len = 32;

        uint8_t *payload = &rx_buf[4];
        uint8_t crc_lo = rx_buf[frame_len - 2];
        uint8_t crc_hi = rx_buf[frame_len - 1];
        uint16_t crc_rx = (uint16_t)crc_lo | ((uint16_t)crc_hi << 8);

        uint16_t crc_calc = crc16_modbus(&rx_buf[3], len_field); // CMD + payload

        if (crc_rx != crc_calc) {
            log_error("[RFCOMM] CRC error: cmd=0x%02X len=%u", cmd, payload_len);
        } else {
            AppEvent e = {0};
            e.type = EVT_RX_FRAME;
            e.frame.cmd = cmd;
            e.frame.len = payload_len;
            if (payload_len > 0) {
                memcpy(e.frame.payload, payload, payload_len);
            }
            app_hsm_dispatch(&e);

            // 若是 HELLO，就回 ACK
            if (cmd == 0x01) { // CMD_HELLO
                uint8_t dummy = 0;
                send_frame(0x81, &dummy, 1); // HELLO_ACK
            }
        }

        // 移除已處理的 frame
        memmove(rx_buf, rx_buf + frame_len, rx_len - frame_len);
        rx_len -= frame_len;
    }
}

int rfcomm_init(void)
{
    epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        return -1;
    }

    server_sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (server_sock < 0) {
        perror("socket");
        return -1;
    }

    set_nonblock(server_sock);

    struct sockaddr_rc loc = {0};
    loc.rc_family = AF_BLUETOOTH;
    loc.rc_bdaddr = *BDADDR_ANY;
    loc.rc_channel = RFCOMM_CHANNEL;

    if (bind(server_sock, (struct sockaddr *)&loc, sizeof(loc)) < 0) {
        perror("bind");
        return -1;
    }

    listen(server_sock, 1);

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = server_sock };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_sock, &ev) < 0) {
        perror("epoll_ctl ADD server_sock");
        return -1;
    }

    log_info("[RFCOMM] Listening on channel %d (epoll)...", RFCOMM_CHANNEL);
    return 0;
}

int rfcomm_poll(void)
{
    struct epoll_event events[4];
    int n = epoll_wait(epfd, events, 4, 50); // 50ms timeout

    for (int i = 0; i < n; i++) {
        int fd = events[i].data.fd;

        if (fd == server_sock) {
            struct sockaddr_rc rem = {0};
            socklen_t opt = sizeof(rem);

            int cs = accept(server_sock, (struct sockaddr *)&rem, &opt);
            if (cs >= 0) {
                set_nonblock(cs);

                struct epoll_event ev = { .events = EPOLLIN, .data.fd = cs };
                epoll_ctl(epfd, EPOLL_CTL_ADD, cs, &ev);

                client_sock = cs;
                rx_len = 0; // reset buffer

                char addr[18];
                ba2str(&rem.rc_bdaddr, addr);
                log_info("[RFCOMM] Client connected: %s", addr);

                AppEvent e = { .type = EVT_LINK_UP };
                app_hsm_dispatch(&e);
            }
        }
        else if (fd == client_sock) {
            uint8_t buf[256];
            int r = read(client_sock, buf, sizeof(buf));
            
            printf("[RAW] ");
	    for (int k = 0; k < r; k++) {
		    printf("%02X ", buf[k]);
	    }
	    printf("\n");


            if (r <= 0) {
                log_info("[RFCOMM] Client disconnected");
                epoll_ctl(epfd, EPOLL_CTL_DEL, client_sock, NULL);
                close(client_sock);
                client_sock = -1;

                AppEvent e = { .type = EVT_LINK_DOWN };
                app_hsm_dispatch(&e);
                continue;
            }

            if (rx_len + r > RX_BUF_SIZE) {
                log_error("[RFCOMM] RX buffer overflow, drop");
                rx_len = 0;
            } else {
                memcpy(rx_buf + rx_len, buf, r);
                rx_len += r;
                process_rx_buffer();
            }
        }
    }

    return 0;
}

void rfcomm_close(void)
{
    if (client_sock >= 0) {
        close(client_sock);
        client_sock = -1;
    }
    if (server_sock >= 0) {
        close(server_sock);
        server_sock = -1;
    }
    if (epfd >= 0) {
        close(epfd);
        epfd = -1;
    }
}

