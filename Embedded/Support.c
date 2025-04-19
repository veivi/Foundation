#include "StaP.h"
#include "Console.h"

static StaP_ErrorStatus_T StaP_ErrorState;

static const char *stapErrorTable[STAP_ERR_APPLICATION + 1] = {
  [STAP_ERR_HALT] = "HALT",
  [STAP_ERR_MALLOC_FAIL] = "MALLOC_FAIL",
  [STAP_ERR_MUTEX_CREATE] = "MUTEX_CREATE",
  [STAP_ERR_MUTEX] = "MUTEX",
  [STAP_ERR_ISR_USARTRX_HW] = "ISR_USARTRX_HW",
  [STAP_ERR_ISR_USARTRX_LINK] = "ISR_USARTRX_LINK",
  [STAP_ERR_ISR_USARTTX_HW] = "ISR_USARTTX_HW",
  [STAP_ERR_ISR_USARTTX_LINK] = "USARTTX_LINK",
  [STAP_ERR_SPURIOUS_INT] = "SPURIOUS_INT",
  [STAP_ERR_TX_TOO_BIG] = "TX_TOO_BIG",
  [STAP_ERR_NO_UART] = "NO_UART",
  [STAP_ERR_TASK_INVALID] = "TASK_INVALID",
  [STAP_ERR_THREADING] = "THREADING,",
  [STAP_ERR_ASSERT_KERNEL] = "ASSERT_KERNEL",
  [STAP_ERR_NO_SIGNAL] = "NO_SIGNAL",
  [STAP_ERR_NO_LINK] = "NO_LINK",
  [STAP_ERR_TASK_TYPE] = "TASK_TYPE",
  [STAP_ERR_TASK_CODE] = "TASK_CODE",
  [STAP_ERR_TX_TIMEOUT0] = "TX_TIMEOUT0",
  [STAP_ERR_DATAGRAM] = "DATAGRAM",
  [STAP_ERR_RX_OVERRUN_H] = "RX_OVERRUN_H",
  [STAP_ERR_RX_OVERRUN_S] = "RX_OVERRUN_S",
  [STAP_ERR_RX_NOISE] = "RX_NOISE",
  [STAP_ERR_TIME] = "TIME",
  [STAP_ERR_I2C] = "I2C",
  [STAP_ERR_TX_TIMEOUT1] = "TX_TIMEOUT1",
  [STAP_ERR_TX_TIMEOUT2] = "TX_TIMEOUT2",
  [STAP_ERR_APPLICATION] = "APPLICATION"
};

const char *STAP_ErrorDecode(uint8_t code)
{
  if(code > STAP_ERR_APPLICATION)
    return "-";
  else if(stapErrorTable[code])
    return stapErrorTable[code];
  else
    return NULL;
}

void STAP_Error(uint8_t e)
{
  ForbidContext_T c = STAP_FORBID_SAFE;
  StaP_ErrorState |= (e) < 32 ? (1UL<<(e)) : (1UL<<31);
  STAP_PERMIT_SAFE(c);
}

StaP_ErrorStatus_T STAP_Status(void)
{
  StaP_ErrorStatus_T value = StaP_ErrorState;

  if(!failSafeMode && value) {
    int i = 0;

    for(i = 0; i < 31; i++) {
      if(value & (1UL<<i)) {
	const char *text = STAP_ErrorDecode(i);

	if(text)
	  STAP_DEBUG(0, "StaP error %s", text);
	else
	  STAP_DEBUG(0, "StaP error (%d)", i);
      }
    }
  }
  
  StaP_ErrorState = 0UL;

  return value;
}

