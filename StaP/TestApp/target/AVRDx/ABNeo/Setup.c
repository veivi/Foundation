#include <string.h>
#include <ctype.h>
#include "Console.h"
#include "AlphaLink.h"
#include "HostLink.h"
//
// Datagram protocol integration
//

#define MAX_DG_SIZE          (3<<5)
#define TX_TIMEOUT           1000 // Milliseconds
#define SERIAL_BUFSIZE       (1<<5)

DgLink_t telemLink, hostLink, alphaLink, alphaLinkMonitor;
uint8_t datagramStoreHost[MAX_DG_SIZE], datagramStoreAlphaLink[MAX_DG_SIZE], datagramStoreAlphaLinkMonitor[MAX_DG_SIZE/4];

char alphaRxBuffer[SERIAL_BUFSIZE], alphaTxBuffer[SERIAL_BUFSIZE>>1], alphaMonBuffer[SERIAL_BUFSIZE];
char hostTxBuffer[SERIAL_BUFSIZE>>1], hostRxBuffer[SERIAL_BUFSIZE];

StaP_LinkRecord_T StaP_LinkTable[StaP_NumOfLinks] = {
  [ALP_Link_ALinkRX] = { AVRDx_Txcv_UART0, AL_BITRATE, LINK_MODE_RX,
			 VPBUFFER_CONS(alphaRxBuffer), ALP_Sig_AlphaRX, 5000 },
  [ALP_Link_ALinkMon] = { AVRDx_Txcv_UART1, AL_BITRATE, LINK_MODE_TX,
			 VPBUFFER_CONS(alphaMonBuffer), ALP_Sig_MonitorRX, 5000 },
  [ALP_Link_ALinkTX] = { AVRDx_Txcv_UART1, AL_BITRATE, LINK_MODE_TX,
			 VPBUFFER_CONS(alphaTxBuffer), ALP_Sig_AlphaTX },
  [ALP_Link_HostRX] = { AVRDx_Txcv_UART2, 115200, LINK_MODE_RXTX,
			VPBUFFER_CONS(hostRxBuffer), ALP_Sig_HostRX, 5000 },
  [ALP_Link_HostTX] = { AVRDx_Txcv_UART2, 115200, LINK_MODE_RXTX,
			VPBUFFER_CONS(hostTxBuffer), ALP_Sig_HostTX } 
};

typedef struct {
  const char *name;
  uint8_t stap;
} Context_t;

Context_t context[] = { { "HOST", ALP_Link_HostTX },
			{ "AL", ALP_Link_ALinkTX },
			{ "MONITOR", ALP_Link_ALinkMon },
			{ "TELE", ALP_Link_TelemTX } };

void datagramRxError(void *cp, const char *error, uint16_t code)
{
  Context_t *context = (Context_t*) cp;
  STAP_Error(STAP_ERR_DATAGRAM);
  consoleNotefLn("[%s] %s (%#x)", context->name, error, code);
}
  
void datagramHandlerHost(void *cp, uint8_t node, const uint8_t *data, size_t size)
{
  hostLinkHandler(data, size);
}

void datagramHandlerAlphaMonitor(void *context, uint8_t node, const uint8_t *data, size_t size)
{
  
}

void datagramHandlerAlpha(void *context, uint8_t node, const uint8_t *data, size_t size)
{
  static int num = 0;
  consolePrintfLn("received dg %d (%d bytes)", num++, size);
}

void dgLinkTxBegin(void *cp)
{
  STAP_LinkTalk(context->stap);
}

void dgLinkTxEnd(void *cp)
{
  STAP_LinkListen(context->stap, TX_TIMEOUT);
}

void dgLinkOut(void *cp, const uint8_t *b, size_t l)
{
  STAP_LinkPut(context->stap, (const char*) b, l, TX_TIMEOUT);
}

//
// Setup and main loop
//

void mainLoopSetup()
{
  // Datagram links
  
  datagramLinkInit(&hostLink, 0,
		   datagramStoreHost, sizeof(datagramStoreHost),
		   (void*) &context[0],
		   datagramHandlerHost,
		   datagramRxError,
		   dgLinkOut,
		   dgLinkTxBegin,
		   dgLinkTxEnd);

  consoleLink = &hostLink;
  
  datagramLinkInit(&alphaLink, 0,
		   datagramStoreAlphaLink, sizeof(datagramStoreAlphaLink),
		   (void*) &context[1],
		   datagramHandlerAlpha,
		   datagramRxError,
		   dgLinkOut,
		   dgLinkTxBegin,
		   dgLinkTxEnd);
  
}

