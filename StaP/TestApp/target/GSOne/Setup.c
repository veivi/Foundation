#include "Setup.h"
#include "AlphaLink.h"
#include "Console.h"
#include <string.h>
#include <ctype.h>

#define SERIAL_BUFSIZE  (1<<6)
#define MAX_DG_SIZE     (1<<8)
#define TX_TIMEOUT      200     // Milliseconds
#define AL_LATENCY      5000    // Microseconds
#define AUX_LATENCY     5000
#define GNSS_LATENCY    50000U
#define GNSS_BITRATE    9600
#define RADIO_BITRATE   57600
// #define AUX_BITRATE     115200

extern bool hostConnected;

//
// Link definitions
//

char hostTxBuffer[SERIAL_BUFSIZE>>1], hostRxBuffer[SERIAL_BUFSIZE];
char auxTxBuffer[SERIAL_BUFSIZE>>2], auxRxBuffer[SERIAL_BUFSIZE>>2];
char gnssTxBuffer[SERIAL_BUFSIZE>>2], gnssRxBuffer[SERIAL_BUFSIZE<<1];
char radioTxBuffer[SERIAL_BUFSIZE>>1], radioRxBuffer[SERIAL_BUFSIZE];
char blueRxBuffer[SERIAL_BUFSIZE];

StaP_LinkRecord_T StaP_LinkTable[StaP_NumOfLinks] = {
  
  //
  // Host interface (debugging)
  //

  [GS_Link_HostRX] =
  { AVRDx_Txcv_UART1, 115200, LINK_MODE_RXTX, VPBUFFER_CONS(hostRxBuffer), GS_SIG_HOSTRX, AL_LATENCY },
  [GS_Link_HostTX] =
  { AVRDx_Txcv_UART1, 115200, LINK_MODE_TX, VPBUFFER_CONS(hostTxBuffer), GS_SIG_HOSTTX },
  
  // Radio link

  [GS_Link_RadioRX] =
  { AVRDx_Txcv_UART4, RADIO_BITRATE, LINK_MODE_RXTX, VPBUFFER_CONS(radioRxBuffer), GS_SIG_RADIORX, AL_LATENCY },
  [GS_Link_RadioTX] =
  { AVRDx_Txcv_UART4, RADIO_BITRATE, LINK_MODE_RXTX, VPBUFFER_CONS(radioTxBuffer), GS_SIG_RADIOTX },

  // GNSS link
  /*
  [GS_Link_GNSSRX] =
  { AVRDx_Txcv_UART2, GNSS_BITRATE, LINK_MODE_RXTX, VPBUFFER_CONS(gnssRxBuffer), GS_SIG_GNSSRX, GNSS_LATENCY },
  [GS_Link_GNSSTX] =
  { AVRDx_Txcv_UART2, GNSS_BITRATE, LINK_MODE_RXTX, VPBUFFER_CONS(gnssTxBuffer), GS_SIG_GNSSTX },

  // BT link, receive-only

  [GS_Link_BTRX] =
  { AVRDx_Txcv_UART0, RADIO_BITRATE, LINK_MODE_RX, VPBUFFER_CONS(blueRxBuffer), GS_SIG_BTRX, AL_LATENCY },
  */
  
  //
  // Aux link
  //

#ifdef AUX_BITRATE
  ,[GS_Link_AuxRX] =
  { AVRDx_Txcv_UART3, AUX_BITRATE, LINK_MODE_RXTX, VPBUFFER_CONS(auxRxBuffer), GS_SIG_AUXRX, AUX_LATENCY },
  [GS_Link_AuxTX] =
  { AVRDx_Txcv_UART3, AUX_BITRATE, LINK_MODE_RXTX, VPBUFFER_CONS(auxTxBuffer), GS_SIG_AUXTX } 
#endif
};


//
// Datagram protocol integration
//

DgLink_t hostLink, telemLink, btLink;
uint8_t datagramRxStore[MAX_DG_SIZE];
uint8_t datagramRxStoreRadio[MAX_DG_SIZE];
uint8_t datagramRxStoreBT[MAX_DG_SIZE];

void dgLinkError(void *context, const char *e, uint16_t v)
{
  STAP_DEBUG(0, "DG %s (%#x)", e, (uint32_t) v);
  STAP_Error(STAP_ERR_DATAGRAM);
}

void dgLinkTxBegin(void *context)
{
  STAP_LinkTalk((StaP_LinkId_T) context);
}

void dgLinkTxEnd(void *context)
{
  STAP_LinkListen((StaP_LinkId_T) context, TX_TIMEOUT);
}

void dgLinkOut(void *context, const uint8_t *b, size_t l)
{
  STAP_LinkPut((StaP_LinkId_T) context, (const char*) b, l, TX_TIMEOUT);
}

void datagramHandlerHost(void *context, uint8_t node, const uint8_t *data, size_t size)
{
  hostLinkHandler(data, size);
}

void datagramHandlerRadio(void *context, uint8_t node, const uint8_t *data, size_t size)
{
  static int num = 0;
  consolePrintfLn("received dg %d (%d bytes)", num++, size);
}

void datagramHandlerBT(void *context, uint8_t node, const uint8_t *data, size_t size)
{
  // We ignore the datagrams, we're only interested in the link status
}

/* This function initializes the ADC module */
void ADC0_init(void)
{
  /* Disable interrupt and digital input buffer on PD0 */
  PORTD.PIN0CTRL &= ~PORT_ISC_gm;
  PORTD.PIN0CTRL |= PORT_ISC_INPUT_DISABLE_gc;
    
  /* Disable pull-up resistor */
  PORTD.PIN0CTRL &= ~PORT_PULLUPEN_bm;

  VREF.ADC0REF = VREF_REFSEL_4V096_gc;  /* Internal 4.096V reference */

    ADC0.CTRLC = ADC_PRESC_DIV4_gc;        /* CLK_PER divided by 4 */    
    ADC0.CTRLA = ADC_ENABLE_bm             /* ADC Enable: enabled */
               | ADC_RESSEL_12BIT_gc      /* 12-bit mode */
               | ADC_LEFTADJ_bm;
    ADC0.MUXPOS = ADC_MUXPOS_AIN0_gc;      /* Select ADC channel AIN0 <-> PD0 */
}

//
// Setup
//

void mainLoopSetup()
{
  datagramLinkInit(&hostLink, 0,
		   datagramRxStore, sizeof(datagramRxStore), 
		   (void*) GS_Link_HostTX, 
		   datagramHandlerHost, dgLinkError,
		   dgLinkOut, dgLinkTxBegin, dgLinkTxEnd);

  consoleLink = &hostLink;

  datagramLinkInit(&telemLink, ALN_TELEMETRY,
		   datagramRxStoreRadio, sizeof(datagramRxStoreRadio), 
		   (void*) GS_Link_RadioTX, 
		   datagramHandlerRadio, dgLinkError,
		   dgLinkOut, dgLinkTxBegin, dgLinkTxEnd);

  // BT link is receive-only
  /*
  datagramLinkInit(&btLink, 0,
		   datagramRxStoreBT, sizeof(datagramRxStoreBT), 
		   NULL, 
		   datagramHandlerBT, dgLinkError,
		   NULL, NULL, NULL);
  */
  
  hostConnected = true;
}

