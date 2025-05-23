#ifndef DATAGRAM_H
#define DATAGRAM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "VPTime.h"
#include "StaP.h"
#include "AlphaLink.h"

#define DG_TRANSMIT_MAX  (1<<9)
#define DG_MAX_NODES     0x40

//
// Generic DG types
//

#define DG_HEARTBEAT       0
#define DG_CONSOLE         1

//
// Application specific datagram type blocks
//

#define DG_TELEMLINK       0x20
#define DG_ALPHALINK       0x80
#define DG_HOSTLINK        0xC0

typedef struct DatagramLink {
  bool initialized, txBusy, rxBusy, alive, overflow;
  uint8_t node;
  uint8_t flagCnt, rxNode;
  uint16_t crcStateTx, crcStateRx;
  uint16_t flagRunLength;
  size_t datagramSize;
  uint8_t rxSeqLast[DG_MAX_NODES];
  uint16_t datagramsGood[DG_MAX_NODES], datagramsLost[DG_MAX_NODES];
  uint8_t txSeq[DG_MAX_NODES];
  uint16_t totalRxBytes, totalRxBytesRaw;
  uint16_t totalTxBytes, totalTxBytesRaw;
  uint16_t totalRxDatagrams, totalTxDatagrams;
  VP_TIME_MILLIS_T datagramLastTxMillis, datagramLastRxMillis;
  uint8_t *rxStore;
  size_t rxStoreSize;
  void *context;
  VP_TIME_MILLIS_T minInterDelay;
#ifdef STAP_MutexCreate
  STAP_MutexRef_T mutex;
#endif
  void (*txBegin)(void*);
  void (*txEnd)(void*);
  void (*txOut)(void*, const uint8_t *b, size_t l);
  void (*rxError)(void*, const char *error, uint16_t code);
  void (*rxHandler)(void*, uint8_t node, const uint8_t *data, size_t size);
} DgLink_t;

void datagramLinkInit(DgLink_t *link, uint8_t node,
		      uint8_t *rxStore, size_t rxSize,
		      void *context,
		      void (*rxHandler)(void*, uint8_t node, const uint8_t *data, size_t),
		      void (*rxError)(void*, const char *error, uint16_t code),
		      void (*txOut)(void*, const uint8_t *b, size_t),
		      void (*txBegin)(void*),
		      void (*txEnd)(void*));

bool datagramLinkAlive(DgLink_t*);
// void datagramTxStartGeneric(DgLink_t*, uint8_t node);
void datagramTxStart(DgLink_t *link, uint8_t header);
void datagramTxStartNode(DgLink_t*, uint8_t node, uint8_t header);
bool datagramTxStartNB(DgLink_t *link, uint8_t header);
bool datagramTxStartNodeNB(DgLink_t *link, uint8_t node, uint8_t header);
void datagramTxOutByte(DgLink_t*, const uint8_t c);
void datagramTxOut(DgLink_t*, const uint8_t *data, size_t l);
void datagramTxEnd(DgLink_t*);
void datagramRxInput(DgLink_t*, const uint8_t *data, size_t s);
void datagramRxStatus(DgLink_t *link, uint8_t node, uint16_t *totalBuf, uint16_t *lostBuf);
void datagramLinkStatus(DgLink_t *link, uint16_t *totalRxBytesBuf, uint16_t *totalTxBytesBuf, uint16_t *totalRxDgBuf, uint16_t *totalTxDgBuf);
void datagramRxInputWithHandler(DgLink_t *link, void (*handler)(void*, uint8_t node, const uint8_t *data, size_t size), const uint8_t *buffer, size_t size);

#endif
