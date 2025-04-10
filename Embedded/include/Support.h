#ifndef STAP_SUPPORT_H
#define STAP_SUPPORT_H

#include <stdint.h>

typedef uint32_t StaP_ErrorStatus_T;

const char *STAP_ErrorDecode(uint8_t code);
StaP_ErrorStatus_T STAP_Status(void);

#endif
