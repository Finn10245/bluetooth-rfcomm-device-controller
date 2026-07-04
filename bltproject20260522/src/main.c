// src/main.c
#include "rfcomm_epoll.h"
#include "app_hsm.h"
#include "log.h"
#include <unistd.h>
#include <stdio.h>

int main(void)
{
    log_init("blt_hsm.log"); // 或 NULL 用 stderr

    log_info("=== RFCOMM + epoll + HSM + GPIO + CRC + handshake ===");

    app_hsm_init();

    if (rfcomm_init() < 0) {
        log_error("rfcomm_init failed");
        return 1;
    }

    while (1) {
        rfcomm_poll();
        usleep(1000);
    }

    rfcomm_close();
    app_hsm_close();
    log_close();
    return 0;
}

