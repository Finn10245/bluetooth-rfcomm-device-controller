// src/log.c
#include "log.h"
#include <stdarg.h>

static FILE *log_fp = NULL;

void log_init(const char *path)
{
    if (path) {
        log_fp = fopen(path, "a");
        if (!log_fp) {
            log_fp = stderr;
        }
    } else {
        log_fp = stderr;
    }
}

void log_close(void)
{
    if (log_fp && log_fp != stderr) {
        fclose(log_fp);
    }
    log_fp = NULL;
}

static void vlog_print(const char *level, const char *fmt, va_list ap)
{
    if (!log_fp) log_fp = stderr;
    fprintf(log_fp, "[%s] ", level);
    vfprintf(log_fp, fmt, ap);
    fprintf(log_fp, "\n");
    fflush(log_fp);
}

void log_info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_print("INFO", fmt, ap);
    va_end(ap);
}

void log_error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vlog_print("ERROR", fmt, ap);
    va_end(ap);
}

