// src/log.h
#ifndef LOG_H
#define LOG_H

#include <stdio.h>

void log_init(const char *path);   // 若 path 為 NULL 則用 stderr
void log_close(void);

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif
