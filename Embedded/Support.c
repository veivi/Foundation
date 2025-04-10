#include "StaP.h"
#include "Console.h"

struct StapError {
  uint8_t code;
  const char *text;
};

static struct StapError stapErrorTable[] = {
  { STAP_ERR_HALT, "HALT" },
  { STAP_ERR_MALLOC_FAIL, "MALLOC_FAIL" },
  { STAP_ERR_MUTEX_CREATE, "MUTEX_CREATE" },
  { STAP_ERR_MUTEX, "MUTEX" },
  { STAP_ERR_ISR_USARTRX_HW, "ISR_USARTRX_HW" },
  { STAP_ERR_ISR_USARTRX_LINK, "ISR_USARTRX_LINK" },
  { STAP_ERR_ISR_USARTTX_HW, "ISR_USARTTX_HW" },
  { STAP_ERR_ISR_USARTTX_LINK, "USARTTX_LINK" },
  { STAP_ERR_SPURIOUS_INT, "SPURIOUS_INT" },
  { STAP_ERR_TX_TOO_BIG, "TX_TOO_BIG" },
  { STAP_ERR_NO_UART, "NO_UART" },
  { STAP_ERR_TASK_INVALID, "TASK_INVALID" },
  { STAP_ERR_THREADING, "THREADING," },
  { STAP_ERR_ASSERT_KERNEL, "ASSERT_KERNEL" },
  { STAP_ERR_NO_SIGNAL, "NO_SIGNAL" },
  { STAP_ERR_NO_LINK, "NO_LINK" },
  { STAP_ERR_TASK_TYPE, "TASK_TYPE" },
  { STAP_ERR_TASK_CODE, "TASK_CODE" },
  { STAP_ERR_TX_TIMEOUT0, "TX_TIMEOUT0" },
  { STAP_ERR_DATAGRAM, "DATAGRAM" },
  { STAP_ERR_RX_OVERRUN_H, "RX_OVERRUN_H" },
  { STAP_ERR_RX_OVERRUN_S, "RX_OVERRUN_S" },
  { STAP_ERR_RX_NOISE, "RX_NOISE" },
  { STAP_ERR_TIME, "TIME" },
  { STAP_ERR_I2C, "I2C" },
  { STAP_ERR_TX_TIMEOUT1, "TX_TIMEOUT1" },
  { STAP_ERR_TX_TIMEOUT2, "TX_TIMEOUT2" },
  { STAP_ERR_APPLICATION, "APPLICATION" }
};

const char *STAP_ErrorDecode(uint8_t code)
{
  int i = 0;
  
  for(i = 0; i < sizeof(stapErrorTable)/sizeof(struct StapError); i++)
    if(code == stapErrorTable[i].code)
      return stapErrorTable[i].text;

  return NULL;
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
	  consoleNotefLn("StaP error %s", text);
	else
	  consoleNotefLn("StaP error (%d)", i);
      }
    }
  }
  
  StaP_ErrorState = 0UL;

  return value;
}

