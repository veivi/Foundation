#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>
#include <stdarg.h>
#include "Datagram.h"

extern DgLink_t *consoleLink;
extern bool consoleThrottled, consoleDebug;
extern uint8_t consoleDebugLevel;

int bufferPrintUL(char *buf, int size, unsigned long v, uint8_t base, uint8_t p);
int bufferPrintL(char *buf, int size, long v, uint8_t base, uint8_t p);

void consoleOut(const char *b, int8_t s);
void consoleOutChar(char c);
void consolePrintULGeneric(unsigned long v, uint8_t base, uint8_t p);
void consoleAssert(bool, const char *);
void consoleFlush(void);
void consoleCR(void);
void consoleNL(void);
void consolePrintC(const char c);
void consoleTab(int i);
void consoleNote_P(const char *s);
void consoleNote(const char *s);
void consolePanic_P(const char *s);
void consolePanic(const char *s);
void consoleError(const char *s, ...);
void consoleErrorLn(const char *s, ...);
void consoleNoteLn_P(const char *s);
void consoleNoteLn(const char *s);
void consolePrint_P(const char *s);
void consolePrint(const char *s);
void consolePrintN(const char *s, int8_t);
void consolePrintFP(float v, int p);
void consolePrintF(float v);
void consolePrintDP(double v, int p);
void consolePrintD(double v);
void consolePrintI(int v);
void consolePrintUI(unsigned int v);
void consolePrintUI8(uint8_t v);
void consolePrintUI8Hex(uint8_t v);
void consolePrintUI16Hex(uint16_t v);
void consolePrintUI32Hex(uint32_t v);
void consolePrintL(long v);
void consolePrintUL(unsigned long v);
void consolePrintLn_P(const char *s);
void consolePrintLn(const char *s);
void consolePrintLnF(float v);
void consolePrintLnFP(float v, int p);
void consolePrintLnD(double v);
void consolePrintLnDP(double v, int p);
void consolePrintLnI(int v);
void consolePrintLnUI(unsigned int v);
void consolePrintLnL(long v);
void consolePrintLnUL(unsigned long v);
void consolePrintPoly(int degree, float *coeff, int p);
int stringFmt(char *b, int size, const char *f, ...);
int vStringFmt(char *b, int size, const char *f, va_list args);
void consolevNotef(const char *s, va_list argp);
void consoleNotef(const char *s, ...);
void consoleNotefLn(const char *s, ...);
void consolevPrintf(const char *s, va_list argp);
void consolePrintf(const char *s, ...);
void consolePrintfLn(const char *s, ...);
void consoleDebugf(uint8_t level, const char *s, ...);
void consolePrintBlob(const char *name, const void *data, size_t size);


#endif
