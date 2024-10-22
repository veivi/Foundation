#include <math.h>
#include "StringFmt.h"

int bufferPrintFP(char *buffer, int size, float v, int w, int p)
{
  int len = 0;
  
  if(isinf(v)) {
    if(size > 3) {
      strcpy(buffer, "inf");
      return 3;
    }

    return 0;
  } else if(isnan(v)) {
    if(size > 3) {
      strcpy(buffer, "nan");
      return 3;
    }

    return 0;
  } else {
    if(v < 0.0f) {
      if(len < size)
	buffer[len++] = '-';
      v = -v;
    }

    float integerPart = 0.0;
    float fraction = modff(v, &integerPart);

    len += bufferPrintUL(&buffer[len], size - len, (unsigned long) integerPart, 10, w);
    
    if(p > 0) {
      if(len < size)
	buffer[len++] = '.';

      fraction *= powf(10.0f, p);
      len += bufferPrintUL(&buffer[len], size - len, (unsigned long) fraction, 10, p);
    }
  }

  return len;
}

int bufferPrintUL(char *buf, int size, unsigned long v, uint8_t base, uint8_t p)
{
  int l = 0;

  while(l < size) {
    uint8_t digit = v % base;
    buf[l++] = digit < 10 ? ('0' + digit) : ('A' + digit - 10);
    v /= base;
    
    if(!v)
      break;
  }

  while(l < p)
    buf[l++] = '0';

  if(l > 0) {
    int i = 0, j = l-1;
    while(i < j) {
      char c = buf[j];
      buf[j--] = buf[i];
      buf[i++] = c;
    }
  }

  return l;
}

int bufferPrintL(char *buf, int size, long v, uint8_t base, uint8_t p)
{
  if(v < 0) {
    buf[0] = '-';
    return 1 + bufferPrintUL(&buf[1], size-1, (unsigned long) -v, base, p);
  } else
    return bufferPrintUL(buf, size, (unsigned long) v, base, p);
}

static void stringPad(char *b, int len, char c)
{
  while(len-- > 0)
    *b++ = c;
}

static int stringAppendPaddedLeft(char *b, int size, int w, const char *field, int fieldLen, char pad)
{
  int padLen = w - fieldLen;

  if(padLen < 0)
    padLen = 0;

  if(padLen > size) {
    stringPad(b, size, pad);
    return size;
  }

  stringPad(b, padLen, pad);    
    
  if(padLen + fieldLen > size) {
    strncpy(&b[padLen], field, size - padLen);
    return size;
  } else
    strncpy(&b[padLen], field, fieldLen);
  
  return fieldLen + padLen;
}

static int stringAppendPaddedRight(char *b, int size, int w, const char *field, int fieldLen)
{
  int padLen = w - fieldLen;

  if(padLen < 0)
    padLen = 0;

  if(fieldLen > size) {
    strncpy(b, field, size);
    return size;
  }
  
  strncpy(b, field, fieldLen);
  stringPad(&b[fieldLen], padLen, ' ');
  
  return fieldLen + padLen;
}

static int stringAppendPadded(char *b, int size, int w, const char *field, int fieldLen, char pad, bool leftJust)
{
  if(leftJust)
    return stringAppendPaddedRight(b, size, w, field, fieldLen);
  else
    return stringAppendPaddedLeft(b, size, w, field, fieldLen, pad);
}

const char *decodeSpec(const char *p, int *value)
{
  *value = 0;
  
  while(isdigit(*p)) {
    *value *= 10;
    *value += *p - '0';
    p++;
  }

  return p;
}

int vStringFmt(char *b, int size, const char *f, va_list args)
{
  char field[PRINT_FMT_FIELD+1];
  int fieldLen = 0, len = 0;
  
  while(len < size && *f != '\0') {
    int w = 0, p = -1;
    char padChar = ' ';
    bool leftJust = false, print0x = false;
    const char *sp = NULL;
    
    while(len < size && *f != '\0' && *f != '%')
      b[len++] = *f++;

    if(*f != '%')
      break;

    // We have a formatting spec

    f++;

    if(*f == '-') {
      // Left justified
      leftJust = true;
      f++;
    }
    
    if(*f == '0') {
      // Pad with zeroes
      padChar = '0';
      f++;
    }
    
    if(*f == '#') {
      // Precede hex numbers by 0x
      print0x = true;
      f++;
    }
    
    if(isdigit(*f))
      f = decodeSpec(f, &w);

    if(*f == '.') {
      f++;
      if(isdigit(*f))
	f = decodeSpec(f, &p);
    }

    switch(*f++) {
    case '%':
      b[len++] = '%';
      break;
      
    case 't':
      // Our own extension - tabulator
      fieldLen = va_arg(args, int);

      if(len < fieldLen)
	len += stringAppendPadded(&b[len], size - len, fieldLen - len, "", 0, padChar, false);
      break;
      
    case 'c':
      field[0] =  (char) va_arg(args, int);
      len += stringAppendPadded(&b[len], size - len, w, field, 1, padChar, leftJust);
      break;

    case 's':
      sp = (const char*) va_arg(args, const char*);
      len += stringAppendPadded(&b[len], size - len, w, sp, strlen(sp), padChar, leftJust);
      break;
	
    case 'd':
      if(p < 0)
	p = 0;
      fieldLen = bufferPrintL(field, PRINT_FMT_FIELD, (long) va_arg(args, int), 10, p);
      len += stringAppendPadded(&b[len], size - len, w, field, fieldLen, padChar, leftJust);
      break;

    case 'x':
    case 'X':
      if(p < 0)
	p = 0;
      if(print0x) {
	field[0] = '0';
	field[1] = 'x';
	fieldLen = 2 + bufferPrintUL(&field[2], PRINT_FMT_FIELD-2, (unsigned long) va_arg(args, int), 16, p);
      } else
	fieldLen = bufferPrintUL(field, PRINT_FMT_FIELD, (unsigned long) va_arg(args, int), 16, p);
	
      len += stringAppendPadded(&b[len], size - len, w, field, fieldLen, padChar, leftJust);
      break;

    case 'u':
      if(p < 0)
	p = 0;
      fieldLen = bufferPrintUL(field, PRINT_FMT_FIELD, (unsigned long) va_arg(args, unsigned int), 10, p);
      len += stringAppendPadded(&b[len], size - len, w, field, fieldLen, padChar, leftJust);
      break;

    case 'p':
      b[len++] = '@';
      if(len < size) {
	if(p < 0)
	  p = 0;
	fieldLen = bufferPrintUL(field, PRINT_FMT_FIELD, (unsigned long) va_arg(args, long), 16, p);
	len += stringAppendPadded(&b[len], size - len, w, field, fieldLen, padChar, leftJust);
      }
      break;

    case 'f':
      if(p < 0)
	p = 5;
      fieldLen = bufferPrintFP(field, size - len, (float) va_arg(args, double), 1, p);
      len += stringAppendPadded(&b[len], size - len, w, field, fieldLen, padChar, leftJust);
      break;

    default:
      strcpy(&b[len], "Blob");
      len += 4;
      break;
    }
  }

  if(len < size)
    // Only add terminator if space remains
    b[len] = '\0';
  
  return len;
}

int stringFmt(char *b, int size, const char *f, ...)
{
  int len = 0;
  va_list args;
  
  va_start(args, f);
  len = vStringFmt(b, size, f, args);
  va_end(args);

  return len;
}

