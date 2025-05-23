#include <math.h>
#include <string.h>
#include <ctype.h>
#include "Console.h"
#include "Datagram.h"
#include "Buffer.h"
#include "StringFmt.h"
#include "Scheduler.h"

#define CONSOLE_BUFFER     (1<<6)
#if STAP_MACHINE_BIG
#define PRINT_FMT_BUFFER   (1<<8)
#else
#define PRINT_FMT_BUFFER   (1<<7)
#endif

DgLink_t *consoleLink;
bool consoleThrottled;

static VPBuffer_t consoleBuffer;
static char consoleBufferStore[CONSOLE_BUFFER];
static int column;
#ifdef STAP_MutexCreate
static STAP_MutexRef_T mutex = NULL;
#endif

static void mutexObtain(void)
{
  if(failSafeMode)
    return;
	
  STAP_FORBID;
    
  if(!mutex && !(mutex = STAP_MutexCreate))
    STAP_Panic(STAP_ERR_MUTEX_CREATE);

  STAP_PERMIT;

  STAP_MutexObtain(mutex);
}

static void mutexRelease(void)
{
  if(failSafeMode)
    return;
	
  STAP_MutexRelease(mutex);
}

static void consoleFlushUnsafe(void)
{
  char buffer[CONSOLE_BUFFER];
  int8_t s = consoleThrottled ? 16 : sizeof(buffer);

  if(!consoleLink)
    // Not in use
    return;

  s = vpbuffer_extract(&consoleBuffer, buffer, s);

  if(s > 0) {
    datagramTxStart(consoleLink, DG_CONSOLE);
    datagramTxOut(consoleLink, (const uint8_t*) buffer, s);
    datagramTxEnd(consoleLink);
  }
}

void consoleOut(const char *b, int8_t s)
{
  if(failSafeMode) {
    datagramTxStart(consoleLink, DG_CONSOLE);    
    datagramTxOut(consoleLink, (const uint8_t*) b, s);
    datagramTxEnd(consoleLink);
    
    return;
  }

  mutexObtain();
  
  if(!consoleBuffer.mask)
    vpbuffer_init(&consoleBuffer, CONSOLE_BUFFER, consoleBufferStore);
  
  int8_t space = vpbuffer_space(&consoleBuffer);
  
  if(space >= s || consoleThrottled) {
    // There's room in the buffer or we must overwrite anyway

    vpbuffer_insert(&consoleBuffer, b, s, true);
    column += s;

  } else {
    // No room, block and transmit everything in pieces

    do {
      consoleFlushUnsafe();

      int8_t w = vpbuffer_insert(&consoleBuffer, b, s, false);
      b += w;
      s -= w;
      column += w;
    } while(s > 0);
  }

  mutexRelease();
}

void consoleFlush()
{
  mutexObtain();
  consoleFlushUnsafe();
  mutexRelease();
}

void consoleOutChar(char c)
{
  consoleOut(&c, 1);
}

void consoleNL(void)
{
  consoleOutChar('\n');
  if(!consoleThrottled)
    consoleFlush();
  column = 0;
}

void consoleCR(void)
{
  consoleOutChar('\r');
  if(!consoleThrottled)
    consoleFlush();
  column = 0;
}

void consoleTab(int i)
{
  int n = i - column;
  
  while(n-- > 0)
    consoleOutChar(' ');
}

void consoleNote(const char *s)
{
  consolePrint_P(CS_STRING("// "));
  consolePrint(s);
}

void consoleNoteLn(const char *s)
{
  consoleNote(s);
  consoleNL();
}

void consoleError(const char *s, ...)
{
  va_list argp;

  va_start(argp, s);
  consolePrint("!! ");
  consolevPrintf(s, argp);
  va_end(argp);
}

void consoleErrorLn(const char *s, ...)
{
  va_list argp;

  va_start(argp, s);
  consolePrint("!! ");
  consolevPrintf(s, argp);
  va_end(argp);
  consoleNL();
}

void consoleNote_P(const char *s)
{
  consolePrint_P(CS_STRING("// "));
  consolePrint_P(s);
}

void consoleNoteLn_P(const char *s)
{
  consoleNote_P(s);
  consoleNL();
}

void consolevNotef(const char *s, va_list argp)
{
  consolePrint_P(CS_STRING("// "));
  consolevPrintf(s, argp);
}

void consoleNotef(const char *s, ...)
{
  va_list argp;

  va_start(argp, s);
  consolevNotef(s, argp);
  va_end(argp);
}

void consoleNotefLn(const char *s, ...)
{
  va_list argp;

  va_start(argp, s);
  consolevNotef(s, argp);
  va_end(argp);
  
  consoleNL();
}

void consolePrintf(const char *s, ...)
{
  va_list argp;

  va_start(argp, s);
  consolevPrintf(s, argp);
  va_end(argp);
}

void consolePrintfLn(const char *s, ...)
{
  va_list argp;

  va_start(argp, s);
  consolevPrintf(s, argp);
  va_end(argp);

  consoleNL();
}

void consolevPrintf(const char *f, va_list args)
{
  char buffer[PRINT_FMT_BUFFER+1];
  int len = vStringFmt(buffer, PRINT_FMT_BUFFER, f, args);
  consoleOut(buffer, len);
}

extern uint8_t consoleDebugLevel;

void consoleDebugf(uint8_t level, const char *f, ...)
{
  static int failCount = 0;
  
  if(level <= consoleDebugLevel) {
    char buffer[PRINT_FMT_BUFFER+2];
    int len = 0;
    va_list argp;

    va_start(argp, f);
    len = vStringFmt(buffer, PRINT_FMT_BUFFER, f, argp);
    va_end(argp);

    buffer[len] = '\n';
    
    if(datagramTxStartNB(consoleLink, DG_CONSOLE)) {
      datagramTxOut(consoleLink, (const uint8_t*) "## ", 3);
      
      while(failCount-- > 0)
	datagramTxOut(consoleLink, (const uint8_t*) "~ ", 2);
      
      datagramTxOut(consoleLink, (const uint8_t*) buffer, len+1);
      datagramTxEnd(consoleLink);
    } else
      failCount++;
  }
}

void consolePrint(const char *s)
{
  consoleOut(s, strlen(s));
}

void consolePrintN(const char *s, int8_t maxlen)
{
  size_t len = strlen(s);
  if(len > maxlen)
    len = maxlen;
  consoleOut(s, len);
}

void consolePrint_P(const char *s)
{
  char buffer[CONSOLE_BUFFER];
  CS_STRNCPY(buffer, s, CONSOLE_BUFFER - 1);
  consoleOut(buffer, strlen(buffer));  
}

void consolePrintFP(float v, int p)
{
  char buffer[PRINT_FMT_BUFFER+1];
  int len = bufferPrintFP(buffer, PRINT_FMT_BUFFER, v, 1, p);
  consoleOut(buffer, len);
}

void consolePrintC(const char c)
{
  consoleOutChar(c);
}  

void consolePrintF(float v)
{
  consolePrintFP(v, 2);
}

void consolePrintI(int v)
{
  consolePrintL((long) v);
}

void consolePrintUI(unsigned int v)
{
  consolePrintUL((unsigned long) v);
}

#define MAX_DIGITS    30

void consolePrintULGeneric(unsigned long v, uint8_t base, uint8_t p)
{
  char buf[MAX_DIGITS];
  int l = bufferPrintUL(buf, MAX_DIGITS, v, base, p);
  
  if(l > 0)
    consoleOut(buf, l);
}

void consolePrintLGeneric(long v, uint8_t base, uint8_t p)
{
  char buf[MAX_DIGITS];
  int l = bufferPrintL(buf, MAX_DIGITS, v, base, p);
  
  if(l > 0)
    consoleOut(buf, l);
}

void consolePrintUL(unsigned long v)
{
  consolePrintULGeneric(v, 10, 1);
}

void consolePrintL(long v)
{
  consolePrintLGeneric(v, 10, 1);
}

void consolePrintLP(long v, int p)
{
  consolePrintLGeneric(v, 10, p);
}

void consolePrintULHex(unsigned long v, int d)
{
  consolePrintULGeneric(v, 16, d);
}

void consolePrintUI8(uint8_t v)
{
  consolePrintUI((unsigned int) v);
}

void consolePrintUI8Hex(uint8_t v)
{
  consolePrintULHex((unsigned long) v, 2);
}

void consolePrintUI16Hex(uint16_t v)
{
  consolePrintULHex((unsigned long) v, 4);
}

void consolePrintUI32Hex(uint32_t v)
{
  consolePrintULHex((unsigned long) v, 8);
}

void consolePrintLn(const char *s)
{
  consolePrint(s);
  consoleNL();
}

void consolePrintLn_P(const char *s)
{
  consolePrint_P(s);
  consoleNL();
}

void consolePrintLnF(float v)
{
  consolePrintF(v);
  consoleNL();
}

void consolePrintLnFP(float v, int p)
{
  consolePrintFP(v, p);
  consoleNL();
}

void consolePrintLnI(int v)
{
  consolePrintI(v);
  consoleNL();
}

void consolePrintLnUI(unsigned int v)
{
  consolePrintUI(v);
  consoleNL();
}

void consolePrintLnUI8(uint8_t v)
{
  consolePrintUI8(v);
  consoleNL();
}

void consolePrintLnL(long v)
{
  consolePrintL(v);
  consoleNL();
}

void consolePrintLnUL(unsigned long v)
{
  consolePrintUL(v);
  consoleNL();
}

void consolePrintPoly(int degree, float *coeff, int p)
{
  int i = 0;
  
  for(i = 0; i < degree+1; i++) {
    if(i > 0)
      consolePrint_P(CS_STRING(" + "));
    consolePrintFP(coeff[i], p);
    if(i > 0) {
      consolePrint_P(CS_STRING(" x"));
      if(i > 1) {
	consoleOutChar('^');
	consolePrintI(i);
      }
    }
  }
}

void consolePrintBlob(const char *name, const void *data, size_t size)
{
  int i = 0;

  consolePrintf("  Blob %s = ", name);
  
  for(i = 0; i < size; i++)
    consolePrintf("%.2X", ((const uint8_t*) data)[i]);

  consoleNL();
}
 
