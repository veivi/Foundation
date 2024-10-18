#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include "StaP.h"
#include "HostLink.h"
#include "AlphaLink.h"
#include "Console.h"
#include "Scheduler.h"
#include "I2CDevice.h"

bool hostConnected;
extern DgLink_t hostLink, telemLink;

#define COMM_BLOCKSIZE      (1<<5)

#if defined(BOARD_GSOne)
#define TestApp_Link_HostRX    GS_Link_HostRX
#define TestApp_Link_AuxRX     GS_Link_RadioRX

#elif defined(BOARD_ABNeo)
#define TestApp_Link_HostRX    ALP_Link_HostRX
#define TestApp_Link_AuxRX     ALP_Link_ALinkRX
#endif

int failCount = 0;

void blinkTask(void)
{
  StaP_ErrorStatus_T status = STAP_Status(true);
  static int i = 0;

  if(status)
    consoleNotefLn("STAP err %#x", status);
  
  if(i & 1)
    STAP_LED1_ON;
  else
    STAP_LED1_OFF;

  i++;
}

int reportType = 0;

void commandHandler(const uint8_t *data, size_t size)
{
  int i = 0, num = 0;

  while(i < size && isdigit(data[i]))
    num = num*10 + (data[i++] - '0');

  reportType = num;
}

void hostLinkHandler(const uint8_t *data, int size)
{
  if(!hostConnected) {
    hostConnected = true;
    consoleNoteLn("Host CONNECTED");
  }

  if(size < 1)
    return;
  
  switch(data[0]) {
  case HL_CONSOLE:
    commandHandler(&data[1], size - 1);
    break;

  case HL_HEARTBEAT:
    break;
    
  default:
    break;
  }
}

VP_TIME_MICROS_T serialTaskHost(void)
{
  uint8_t buffer[COMM_BLOCKSIZE];

  randomEntropyInput(STAP_ENTROPY_SRC);
  
  uint8_t len = STAP_LinkGet(TestApp_Link_HostRX, buffer, sizeof(buffer));
  
  datagramRxInput(&hostLink, (const uint8_t*) buffer, len);

  return 0;
}

VP_TIME_MICROS_T serialTaskRadio(void)
{
  uint8_t buffer[COMM_BLOCKSIZE];

  randomEntropyInput(STAP_ENTROPY_SRC);
  
  uint8_t len = STAP_LinkGet(TestApp_Link_AuxRX, buffer, sizeof(buffer));
  
  datagramRxInput(&telemLink, (const uint8_t*) buffer, len);

  return 0;
}

#if TEST == 1
VP_TIME_MICROS_T textTask(void)
{
  static int i = 0;

  STAP_FailSafe;
  
  for(;;)
    consolePrintfLn("Serial TX FailSafe %d. ", i++);
}
#endif

#if TEST == ALL || TEST == 2
VP_TIME_MICROS_T textTask(void)
{
  int i = 0;

  for(;;)
    consolePrintfLn("Serial TX from a task %d. ", i++);
}
#endif

#if TEST == ALL || TEST == 3
VP_TIME_MICROS_T transmitTestTask(void)
{
  static uint64_t i = 0;

  consolePrintfLn("DG TX test %d", i);
  
  datagramTxStart(&telemLink, ALN_TELEMETRY);
  datagramTxOut(&telemLink, (const uint8_t*) &i, sizeof(i));
  datagramTxEnd(&telemLink);

  i++;
}
#endif

#if TEST == ALL || TEST == 4

I2CDevice_t target = I2CDEVICE_CONS("BMP390", 0x77);

VP_TIME_MICROS_T i2cTestTask(void)
{
  uint8_t status = 0xFF;
  uint8_t chipId = 0;
  
  if((status = I2CDeviceReadByUInt8(&target, 0, 0x0, &chipId, sizeof(chipId))))
    consoleNotefLn("BMP390 read status %#X", status);
  else
    consoleNotefLn("BMP390 chipId = %#X", chipId);

  return 0;
}
#endif

#if TEST == ALL || TEST == 5

#define EEPROM_LINE       (1<<6)
#define EEPROM_I2CADDR    0x50
#define EEPROM_TEST_VALUE(a)  (uint8_t) (13 + (a & 0xFF))

VP_TIME_MICROS_T serialEEPROMTestTask(void)
{
  static uint16_t addr = 0;
  uint8_t data[EEPROM_LINE];
  StaP_TransferUnit_t readTransfer[] = {
    { .data.tx = (const uint8_t*) &addr, .size = sizeof(addr), .dir = transfer_dir_transmit }, 
      { .data.rx = data, .size = sizeof(data), .dir = transfer_dir_receive } },
    writeTransfer[] = { 
      { .data.tx = (const uint8_t*) &addr, .size = sizeof(addr), .dir = transfer_dir_transmit }, 
      { .data.tx = (const uint8_t*) data, .size = sizeof(data), .dir = transfer_dir_transmit } };
  uint8_t status = 0xFF;
  int i = 0;

  // Read a line from 0

  status = STAP_I2CTransfer(EEPROM_I2CADDR, readTransfer, sizeof(readTransfer)/sizeof(StaP_TransferUnit_t));

  consoleNotefLn("EEPROM read status %#X", status);
  
  if(status) {
    failCount++;
    return 0;
  }

  // Write a line

  for(i = 0; i < sizeof(data); i++)
    data[i] = EEPROM_TEST_VALUE(addr+i);
  
  status = STAP_I2CTransfer(EEPROM_I2CADDR, writeTransfer, sizeof(writeTransfer)/sizeof(StaP_TransferUnit_t));
  
  consoleNotefLn("EEPROM write status %#X", status);
  
  if(status) {
    failCount++;
    return 0;
  }

  // Wait for write to complete, read back and compare

  STAP_DelayMillis(6);
  memset((void*) data, 0, sizeof(data));
  
  status = STAP_I2CTransfer(EEPROM_I2CADDR, readTransfer, sizeof(readTransfer)/sizeof(StaP_TransferUnit_t));

  consoleNotefLn("EEPROM read status %#X", status);
  
  if(status) {
    failCount++;
    return 0;
  }

  for(i = 0; i < sizeof(data); i++) {
    if(data[i] != EEPROM_TEST_VALUE(addr+i)) {
      consoleNotefLn("EEPROM read data %#X, expected %#X", data[i], EEPROM_TEST_VALUE(addr+i));
      failCount++;
      return 0;
    }  
  }

  consoleNotefLn("Test PASSED (%#x) total fails = %d", addr, failCount);

  addr += EEPROM_LINE;
  return 0;
}
#endif

#if TEST == ALL || TEST == 6

VP_TIME_MICROS_T idleTestTask(void)
{
  consoleNotefLn("idle = %.1f%%", (float) STAP_CPUIdlePermille()/10.0f);
  return 0;
}

#endif

VP_TIME_MICROS_T flushTask(void)
{
  consoleFlush();
}

VP_TIME_MICROS_T debugTask(void)
{
  static int i = 0;
  consoleDebugf(0, "DEBUG %d", i++);
}

struct TaskDecl StaP_TaskList[] = {
  TASK_BY_FREQ("Blink", 0, blinkTask, 2, 1<<8),
  TASK_BY_FREQ("Flush", 0, flushTask, 5, 1<<8)

#if TEST == ALL || TEST == 1 || TEST == 2
  ,TASK_BY_FREQ("Text", 0, textTask, 1, 1<<8)
  ,TASK_BY_FREQ("Debug", 1, debugTask, 247, 1<<8)
#endif

#if TEST == ALL || TEST == 3
  ,TASK_BY_FREQ("TxTest", 0, transmitTestTask, 100, 1<<8)
#endif

#if TEST == ALL || TEST == 4
  ,TASK_BY_FREQ("I2C", 0, i2cTestTask, 1, 1<<8)
#endif

#if TEST == ALL || TEST == 5
  ,TASK_BY_FREQ("EEPROM", 0, serialEEPROMTestTask, 1000, 1<<8)
  ,TASK_BY_FREQ("Debug", 0, debugTask, 147, 1<<8)
#endif

#if TEST == ALL && TEST != 1
  ,TASK_BY_SERIAL("HostRX", 2, serialTaskHost, TestApp_Link_HostRX, 3<<8)
  ,TASK_BY_SERIAL("TelemRX", 2, serialTaskRadio, TestApp_Link_AuxRX, 3<<8)
#endif

#if TEST == ALL || TEST == 6
  ,TASK_BY_FREQ("Idle", 0, idleTestTask, 10, 1<<8)
#endif

};

const int StaP_NumOfTasks
  = sizeof(StaP_TaskList)/sizeof(struct TaskDecl);
