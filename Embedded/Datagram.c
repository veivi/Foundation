#include <stdint.h>
#include <string.h>
#include "VPTime.h"
#include "Datagram.h"
#include "StaP.h"
#include "CRC16.h"


#define FLAG        ((uint8_t) 0x00)
#define START(n)    ((~FLAG) - (n))
#define START_MASK  ((FLAG - DG_MAX_NODES) & 0xFF)

// #define DG_DEBUG    1

extern DgLink_t *consoleLink;

void datagramLinkInit(DgLink_t *link, uint8_t node,
		      uint8_t *rxStore, size_t rxSize,
		      void *context,
		      void (*rxHandler)(void*, uint8_t node, const uint8_t *data, size_t size),
		void (*rxError)(void*, const char *error, uint16_t code),
		void (*txOut)(void*, const uint8_t *b, size_t l),
		void (*txBegin)(void*),
		void (*txEnd)(void*))
{
  memset((void*) link, '\0', sizeof(DgLink_t));

  link->node = node;
  link->context = context;
  link->rxStore = rxStore;
  link->rxStoreSize = rxSize;
  link->rxHandler = rxHandler;
  link->rxError = rxError;
  link->txOut = txOut;
  link->txBegin = txBegin;
  link->txEnd = txEnd;

  if(rxStore && rxSize > 0x20 && txOut) {
    // Minimum configuration is met
    
#ifdef STAP_MutexCreate
    link->mutex = STAP_MutexCreate;

    if(!link->mutex)
      STAP_Panic(STAP_ERR_MUTEX_CREATE);
#endif
    
    link->initialized = true;
  }
}

bool  datagramLinkAlive(DgLink_t *link)
{
  if(VP_ELAPSED_MILLIS(link->datagramLastRxMillis) > 1500)
    link->alive = false;
  
  return link->alive;
}

static void outputBreak(DgLink_t *link)
{
  const uint8_t buffer[] = { FLAG, FLAG };
  (link->txOut)(link->context, buffer, sizeof(buffer));
}

static void flagRunEnd(DgLink_t *link)
{
  while(link->flagRunLength > 0) {
    const uint16_t piece = link->flagRunLength > 0xFF
      ? 0xFF : link->flagRunLength;
    const uint8_t buffer[] = { FLAG, FLAG + (uint8_t) piece };
    (link->txOut)(link->context, buffer, sizeof(buffer));
    link->totalTxBytesRaw += sizeof(buffer);
    link->totalTxBytes += piece;
    link->flagRunLength -= piece;
  }
}

void datagramTxOutByte(DgLink_t *link, const uint8_t c)
{
  datagramTxOut(link, &c, 1);
}

void datagramTxOut(DgLink_t *link, const uint8_t *data, size_t l)
{
  uint16_t runLength = 0, i = 0;
  const uint8_t *runStart = data;
  
  if(!link->initialized)
    return;
  
  if(l > DG_TRANSMIT_MAX) {
    if(link->rxError)
      (link->rxError)(link->context, "TX_MAX", l);
    return;
  }
  
  for(i = 0; i < l; i++) {
    link->crcStateTx = crc16_update(link->crcStateTx, data[i]);

#if DG_DEBUG > 2
    if(link != consoleLink)
      consolePrintUI8Hex(data[i]);
#endif
    
    if(data[i] == FLAG) {
      if(runLength > 0) {
	(link->txOut)(link->context, runStart, runLength);
	link->totalTxBytesRaw += runLength;
	link->totalTxBytes += runLength;
	runLength = 0;
      }

      link->flagRunLength++;
    } else {
        if(link->flagRunLength > 0) {
//            printf("plop ");
            flagRunEnd(link);
        }

      if(!runLength)
	runStart = &data[i];

      runLength++;
    }
  }

  if(runLength > 0) {
    (link->txOut)(link->context, runStart, runLength);
    link->totalTxBytesRaw += runLength;
    link->totalTxBytes += runLength;
  }
}

static bool datagramTxStartGeneric(DgLink_t *link, uint8_t node, bool canblock)
{
  if(!link->initialized)
    return true;
  
#ifdef STAP_MutexCreate
  if(!failSafeMode) {
    if(canblock) {
      STAP_MutexObtain(link->mutex);
    } else if(!STAP_MutexAttempt(link->mutex))
      return false;
  }
#endif

  VP_TIME_MILLIS_T interDelay = VP_ELAPSED_MILLIS(link->datagramLastTxMillis);

  if(!failSafeMode && interDelay < link->minInterDelay)
    STAP_DelayMillis(link->minInterDelay - interDelay);
  
  uint8_t buffer[] = { START(node), link->txSeq[node]++ };
  
  if(link->txBegin)
    (link->txBegin)(link->context);
  
  if(failSafeMode || link->txBusy || link->datagramLastTxMillis == 0
     || VP_ELAPSED_MILLIS(link->datagramLastTxMillis) > 500U)
    outputBreak(link);

  link->crcStateTx = 0xFFFF;
    link->flagRunLength = 0;
    
#if DG_DEBUG > 2
  if(link != consoleLink)
    consolePrint("DGTX ");
#endif
  
  datagramTxOut(link, buffer, sizeof(buffer));
  link->txBusy = true;
  link->totalTxBytes += sizeof(buffer);

  return true;
}

void datagramTxStart(DgLink_t *link, uint8_t header)
{
  if(!datagramTxStartGeneric(link, link->node, true))
    STAP_Panicf(STAP_ERR_MUTEX, "Blocking start failed");
  
  datagramTxOutByte(link, header);
}

void datagramTxStartNode(DgLink_t *link, uint8_t node, uint8_t header)
{
  if(!datagramTxStartGeneric(link, node, true))
    STAP_Panicf(STAP_ERR_MUTEX, "Blocking start failed");
  
  datagramTxOutByte(link, header);
}

bool datagramTxStartNB(DgLink_t *link, uint8_t header)
{
  bool success = datagramTxStartGeneric(link, link->node, false);
  
  if(success)
    datagramTxOutByte(link, header);

  return success;
}

bool datagramTxStartNodeNB(DgLink_t *link, uint8_t node, uint8_t header)
{
  bool success = datagramTxStartGeneric(link, node, false);

  if(success)
    datagramTxOutByte(link, header);

  return success;
}

void datagramTxEnd(DgLink_t *link)
{
  uint16_t crc = link->crcStateTx;

  if(!link->initialized)
    return;
  
  datagramTxOut(link, (const uint8_t*) &crc, sizeof(crc));
  
  flagRunEnd(link);
  outputBreak(link);
  
#if DG_DEBUG > 2
  if(link != consoleLink)
    consolePrint(" END ");
#endif
  
  if(link->txEnd)
    (link->txEnd)(link->context);

  if(!failSafeMode)
    link->datagramLastTxMillis = vpTimeMillis();
  
  link->txBusy = false;
  link->totalTxDatagrams++;
  
#ifdef STAP_MutexCreate
  if(!failSafeMode)
    STAP_MutexRelease(link->mutex);
#endif
}

static void storeByte(DgLink_t *link, const uint8_t c)
{
  if(link->datagramSize < link->rxStoreSize)
    link->rxStore[link->datagramSize++] = c;
  else {
    link->overflow = true;
  }
}
  
static void handleBreak(DgLink_t *link, void (*handler)(void*, uint8_t node, const uint8_t *data, size_t size))
{
  if(link->overflow) {
    if(link->rxError)
      (link->rxError)(link->context, "OVERFLOW", 0);
    return;
  }
  
  if(link->datagramSize >= 1+sizeof(uint16_t)) {
    uint16_t crc = ((uint16_t) link->rxStore[link->datagramSize-1]<<8)
      | link->rxStore[link->datagramSize-2];
    int payload = (int) link->datagramSize - sizeof(crc);
    
    if(crc == crc16(link->crcStateRx, link->rxStore, payload)) {
      uint8_t rxExpected = ((link->rxSeqLast[link->rxNode] + 1) & 0xFF);
      uint8_t rxSeq = link->rxStore[0], lost = rxSeq - rxExpected;

      link->datagramsGood[link->rxNode]++;
      link->datagramsLost[link->rxNode] += lost;
      
      link->totalRxBytes += payload;
      link->totalRxDatagrams++;
      
      link->rxSeqLast[link->rxNode] = rxSeq;
      link->datagramLastRxMillis = vpApproxMillis();
      link->alive = true;

      if(handler)
	(*handler)(link->context, link->rxNode, &link->rxStore[1], payload-1);
    } else {
#if DG_DEBUG > 3
    int i = 0;

      for(i = 0; i < link->datagramSize; i++)
	consolePrintUI8Hex(link->rxStore[i]);
      consoleNL();
#endif
      if(link->rxError)
	(link->rxError)(link->context, "CRC", crc);
    }
  }
    
  link->datagramSize = 0;
}

void datagramRxStatus(DgLink_t *link, uint8_t node, uint16_t *totalBuf, uint16_t *lostBuf)
{
    uint16_t lost = link->datagramsLost[node], total = lost + link->datagramsGood[node];
    link->datagramsLost[node] = link->datagramsGood[node] = 0;
        
    if(lostBuf)
        *lostBuf = lost;
    if(totalBuf)
        *totalBuf = total;
}

void datagramLinkStatus(DgLink_t *link, uint16_t *totalRxBytesBuf, uint16_t *totalTxBytesBuf, uint16_t *totalRxDgBuf, uint16_t *totalTxDgBuf)
{
  uint16_t totalRx = link->totalRxBytesRaw, totalTx = link->totalTxBytesRaw,
    totalRxDg = link->totalRxDatagrams, totalTxDg = link->totalTxDatagrams;

  link->totalRxBytesRaw = link->totalTxBytesRaw
    = link->totalRxDatagrams = link->totalTxDatagrams = 0;

  if(totalRxBytesBuf)
    *totalRxBytesBuf = totalRx;
  
  if(totalTxBytesBuf)
    *totalTxBytesBuf = totalTx;
  
  if(totalRxDgBuf)
    *totalRxDgBuf = totalRxDg;
  
  if(totalTxDgBuf)
    *totalTxDgBuf = totalTxDg;
}
  
void datagramRxInputWithHandler(DgLink_t *link, void (*handler)(void*, uint8_t node, const uint8_t *data, size_t size), const uint8_t *buffer, size_t size)
{
  if(!link->initialized)
    return;
  
  while(size-- > 0) {
    uint8_t c = *buffer++;

#if DG_DEBUG > 0
    //    consolePrintUI8Hex(c);
#endif
    if(c != FLAG) {
      if(link->overflow) {
	// Ignore the rest
      } else if(link->rxBusy) {
          if(link->node == 0 || link->rxNode == link->node || link->rxNode == ALN_BROADCAST) {
	          if(link->flagCnt) {
	            while(c-- != FLAG)
	              storeByte(link, FLAG);
            } else {
             storeByte(link, c);
            }
          }
	      
	      link->totalRxBytesRaw++;
      } else {
	//printf("S %x ", c);
	link->rxNode = (~FLAG) - c;
	  
	if(link->rxNode < DG_MAX_NODES) {
	  link->rxBusy = true;
	  link->crcStateRx = crc16_update(0xFFFF, c);
	  link->datagramSize = 0;
	} else if(link->rxError)
	  (link->rxError)(link->context, "BAD", c);
      }
    
      link->flagCnt = 0;
    } else if(++link->flagCnt > 1 && link->rxBusy) {
      if(link->node == 0 || link->rxNode == link->node || link->rxNode == ALN_BROADCAST)
          handleBreak(link, handler);
      link->rxBusy = link->overflow = false;
    }
  }
}

void datagramRxInput(DgLink_t *link, const uint8_t *data, size_t size)
{
  datagramRxInputWithHandler(link, link->rxHandler, data, size);
}



