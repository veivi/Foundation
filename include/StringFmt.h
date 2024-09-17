#ifndef FOUNDATION_STRING_FMT_H
#define FOUNDATION_STRING_FMT_H

#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#define PRINT_FMT_BUFFER   (1<<7)
#define PRINT_FMT_FIELD    (1<<6)

int stringFmt(char *b, int size, const char *f, ...);
int vStringFmt(char *b, int size, const char *f, va_list args);
int bufferPrintFP(char *buffer, int size, float v, int w, int p);
int bufferPrintUL(char *buf, int size, unsigned long v, uint8_t base, uint8_t p);
int bufferPrintL(char *buf, int size, long v, uint8_t base, uint8_t p);

#endif
