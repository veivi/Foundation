CORE                  := $(abspath ../..)
ARCH                  := AVRDx

FOUNDATION_REL         := ../../../..
FOUNDATION             := $(abspath $(FOUNDATION_REL))
PLATFORM               := ${FOUNDATION}/StaP

MCU                   := avr128da48
MCU_PINS              := 48
F_CPU                 := 24000000UL

PORT                  := /dev/cu.usbserial-AB0JNQY7

CC                    := xc8-cc
BIN                   := Test

BUILD_DIR             := /tmp/Build/Test_${ARCH}_${TARGET}
BUILD_DIR_ABS         := $(abspath $(BUILD_DIR))

FREERTOS_DIR_REL      := ../../../../../FreeRTOSv202212.01/FreeRTOS
FREERTOS_DIR          := $(abspath $(FREERTOS_DIR_REL))

KERNEL_DIR            := ${FREERTOS_DIR}/Source

INCLUDE_DIRS          := -I${TARGET}/include
INCLUDE_DIRS          += -I${CORE}/include
INCLUDE_DIRS          += -I${FOUNDATION}/include
INCLUDE_DIRS          += -I${PLATFORM}/include
INCLUDE_DIRS          += -I${PLATFORM}/target/${ARCH}/include
INCLUDE_DIRS          += -I${KERNEL_DIR}/include
INCLUDE_DIRS          += -I${KERNEL_DIR}/portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_AVRDx
INCLUDE_DIRS          += -I${FREERTOS_DIR}/Demo/Common/include


SOURCE_FILES          := $(wildcard ${CORE}/*.c)
SOURCE_FILES          += $(wildcard ${TARGET}/*.c)
SOURCE_FILES          += $(wildcard ${FOUNDATION}/*.c)
SOURCE_FILES          += $(wildcard ${PLATFORM}/*.c)
SOURCE_FILES          += $(wildcard ${PLATFORM}/target/${ARCH}/*.c)
SOURCE_FILES          += $(wildcard ${FREERTOS_DIR}/Source/*.c)
# Memory manager (use malloc() / free() )
SOURCE_FILES          += ${KERNEL_DIR}/portable/MemMang/heap_1.c
# Portable layer
SOURCE_FILES          += ${KERNEL_DIR}/portable/ThirdParty/Partner-Supported-Ports/GCC/AVR_AVRDx/*.c

# Demo library.
##SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/AbortDelay.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/BlockQ.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/blocktim.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/countsem.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/death.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/dynamic.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/EventGroupsDemo.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/flop.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/GenQTest.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/integer.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/IntSemTest.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/MessageBufferAMP.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/MessageBufferDemo.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/PollQ.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/QPeek.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/QueueOverwrite.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/QueueSet.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/QueueSetPolling.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/recmutex.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/semtest.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/StaticAllocation.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/StreamBufferDemo.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/StreamBufferInterrupt.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/TaskNotify.c
#SOURCE_FILES          += ${FREERTOS_DIR}/Demo/Common/Minimal/TimerDemo.c

CFLAGS                :=  -DBOARD_${TARGET}=1 -DTEST=${TEST} -mcpu=${MCU} -DF_CPU=${F_CPU}  -DSTAP_MCU_PINS=${MCU_PINS} ${INCLUDE_DIRS}
LDFLAGS               :=  -mcpu=${MCU} -lc -lm

CFLAGS                +=  -DprojCOVERAGE_TEST=0

ifdef PROFILE
  CFLAGS              +=   -pg  -O0
  LDFLAGS             +=   -pg  -O0
else
  CFLAGS              +=   -O1
  LDFLAGS             +=   -O1
endif

ifdef SANITIZE_ADDRESS
  CFLAGS              +=   -fsanitize=address -fsanitize=alignment
  LDFLAGS             +=   -fsanitize=address -fsanitize=alignment
endif

ifdef SANITIZE_LEAK
  LDFLAGS             +=   -fsanitize=leak
endif

CPPFLAGS            +=   -DUSER_DEMO=0


OBJ_FILES = $(SOURCE_FILES:%.c=$(BUILD_DIR)/%.o)

DEP_FILE = $(OBJ_FILES:%.o=%.d)

${BIN} : $(BUILD_DIR_ABS)/$(BIN).hex

${BUILD_DIR_ABS}/${BIN}.elf : ${OBJ_FILES}
	-mkdir -p ${@D}
	$(CC) $^ ${LDFLAGS} -o $@

${BUILD_DIR_ABS}/${BIN}.hex : ${BUILD_DIR_ABS}/${BIN}.elf
	avr-objcopy -O ihex -R .eeprom $< $@

-include ${DEP_FILE}

${BUILD_DIR}/%.o : %.c Makefile
	-mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

flash: ${BUILD_DIR_ABS}/${BIN}.hex
	avrdude -V -p ${MCU} -c serialupdi -P $(PORT) -Uflash:w:${BUILD_DIR_ABS}/${BIN}.hex
	touch flash

.PHONY: clean

clean:
	-rm -rf $(BUILD_DIR)


GPROF_OPTIONS := --directory-path=$(INCLUDE_DIRS)
profile:
	gprof -a -p --all-lines $(GPROF_OPTIONS) $(BUILD_DIR)/$(BIN) $(BUILD_DIR)/gmon.out > $(BUILD_DIR)/prof_flat.txt
	gprof -a --graph $(GPROF_OPTIONS) $(BUILD_DIR)/$(BIN) $(BUILD_DIR)/gmon.out > $(BUILD_DIR)/prof_call_graph.txt

