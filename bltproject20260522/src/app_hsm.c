// src/app_hsm.c
#include "app_hsm.h"
#include "gpio.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

typedef int (*StateHandler)(const AppEvent *e);

static int st_top(const AppEvent *e);
static int st_disconnected(const AppEvent *e);
static int st_handshake(const AppEvent *e);
static int st_cmd_idle(const AppEvent *e);

static StateHandler current;
static StateHandler parent;

static int handshake_done = 0;

static void hsm_transition(StateHandler target, StateHandler parent_state)
{
    current = target;
    parent  = parent_state;
}

void app_hsm_init(void)
{
    if (gpio_init() < 0) {
        log_error("GPIO init failed");
    }
    gpio_set(0);

    handshake_done = 0;
    current = st_disconnected;
    parent  = st_top;

    log_info("[HSM] INIT → DISCONNECTED");
}

void app_hsm_dispatch(const AppEvent *e)
{
    int handled = 0;

    if (current) {
        handled = current(e);
    }
    if (!handled && parent) {
        parent(e);
    }
}

void app_hsm_close(void)
{
    gpio_set(0);
    gpio_close();
    log_info("[HSM] closed");
}

/* 頂層狀態：共用處理 */
static int st_top(const AppEvent *e)
{
    switch (e->type) {
    case EVT_LINK_DOWN:
        log_info("[HSM][TOP] LINK_DOWN → GPIO LOW & reset handshake");
        gpio_set(0);
        handshake_done = 0;
        hsm_transition(st_disconnected, st_top);
        return 1;
    default:
        return 0;
    }
}

/* 未連線狀態 */
static int st_disconnected(const AppEvent *e)
{
    switch (e->type) {
    case EVT_LINK_UP:
        log_info("[HSM] DISCONNECTED → HANDSHAKE");
        handshake_done = 0;
        hsm_transition(st_handshake, st_top);
        return 1;
    default:
        return 0;
    }
}

/* Handshake 狀態：等待 HELLO 封包 */
#define CMD_HELLO     0x01
#define CMD_HELLO_ACK 0x81
#define CMD_SET_GPIO  0x10

static int st_handshake(const AppEvent *e)
{
    if (e->type == EVT_RX_FRAME) {
        const AppFrame *f = &e->frame;
        if (f->cmd == CMD_HELLO) {
            log_info("[HSM] HANDSHAKE: HELLO received");
            handshake_done = 1;
            hsm_transition(st_cmd_idle, st_top);
            return 1;
        } else {
            log_info("[HSM] HANDSHAKE: unexpected CMD 0x%02X", f->cmd);
            return 1;
        }
    }
    return 0;
}

/* 命令處理狀態：只有 handshake 完成後才會進來 */
static int st_cmd_idle(const AppEvent *e)
{
    if (e->type == EVT_RX_FRAME) {
        const AppFrame *f = &e->frame;

        if (!handshake_done) {
            log_info("[HSM] CMD ignored, handshake not done");
            return 1;
        }

        if (f->cmd == CMD_SET_GPIO) {
            if (f->len < 1) {
                log_error("[HSM] SET_GPIO: payload too short");
                return 1;
            }
            uint8_t v = f->payload[0] ? 1 : 0;
            gpio_set(v);
            log_info("[HSM] SET_GPIO: %u", v);
            return 1;
        } else if (f->cmd == CMD_HELLO) {
            // 若 client 再送一次 HELLO，就當作 keep-alive
            log_info("[HSM] CMD: HELLO (keep-alive)");
            return 1;
        } else {
            log_info("[HSM] CMD: unknown 0x%02X, len=%u", f->cmd, f->len);
            return 1;
        }
    }
    return 0;
}

