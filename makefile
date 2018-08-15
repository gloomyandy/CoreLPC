## Cross-compilation commands 
CC      = arm-none-eabi-gcc
CXX     = arm-none-eabi-g++
LD      = arm-none-eabi-gcc
AR      = arm-none-eabi-ar
AS      = arm-none-eabi-as
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE    = arm-none-eabi-size




PROCESSOR = LPC17xx

#BOARD = AZTEEGX5MINI
#BOARD = SMOOTHIEBOARD
#BOARD = REARM
#BOARD = AZSMZ
#BOARD = MKSBASE
BOARD = MBED
BUILD_DIR = $(PWD)/build

#BUILD = Debug
BUILD = Release

#compile in Ethernet Networking?
NETWORKING = true
#NETWORKING = false


#enable DFU
USE_DFU = true
#USE_DFU = false

#Comment out to show compilation commands (verbose)
V=@


#Sources Locations
RRF_SRC_BASE  = RepRapFirmware/src
CORE = CoreLPC2

#RTOS location
RTOS_SRC = FreeRTOS/src
RTOS_INCLUDE = $(RTOS_SRC)/include/
RTOS_SRC_PORTABLE = $(RTOS_SRC)/portable/GCC/ARM_CM3

RTOSPLUS_SRC = FreeRTOS

RTOS_CONFIG_INCLUDE = FreeRTOSConfig

OUTPUT_NAME=firmware
$(info Saving Binary to $(OUTPUT_NAME))

#list 1768 based boards here otherwise 1769 will be assumed
LPC1768_BOARDS = REARM MBED MKS_SBASE

#SmoothieBoard, Azteeg 1769
#MBED and ReArm is 1768

#RRF uses VARIANT_MCK for a few calcs, and are constexpr so will pass here the speeds of the CPU instead of using SystemCoreClock

#find the Clock Speed for the selected Board
ifneq ($(filter $(BOARD),$(LPC1768_BOARDS)),)
	VARIANT_MCK = 100000000
else
	#default to 120MHz
	VARIANT_MCK = 120000000
endif

$(info $(BOARD) MCK set to $(VARIANT_MCK))



ifeq ($(BOARD), AZTEEGX5MINI)
	NETWORKING = false #AzteegX5Mini has no Networking, Ensure its disabled
endif
$(info Building Network Support: $(NETWORKING))



ifeq ($(BUILD),Debug)
	DEBUG_FLAGS = -Og -g -gdwarf-3
else
	DEBUG_FLAGS = -Os#-O2
endif
	

COMBINE_AHBRAM = true

#select correct linker script
ifeq ($(BOARD), MBED)
	#No bootloader for MBED
	LINKER_SCRIPT_BASE = $(CORE)/variants/LPC/linker_scripts/gcc/LPC17xx_direct
else 
	#Linker script to avoid Smoothieware Bootloader
 	LINKER_SCRIPT_BASE = $(CORE)/variants/LPC/linker_scripts/gcc/LPC17xx_smoothie
endif


#Path to the linker Script
LINKER_SCRIPT  = $(LINKER_SCRIPT_BASE)_combined.ld



MKDIR = mkdir -p


#Flags common for Core in c and c++
FLAGS  = -D__$(PROCESSOR)__ -D__$(BOARD)__ -DVARIANT_MCK=$(VARIANT_MCK)
#lpcopen Defines
FLAGS += -DCORE_M3
#RTOS + enable mods to RTOS+TCP for RRF
FLAGS += -DRTOS -DFREERTOS_USED -DRRF_RTOSPLUS_MOD
#
FLAGS +=  -Wall -c -mcpu=cortex-m3 -mthumb -ffunction-sections -fdata-sections -march=armv7-m 
FLAGS += -nostdlib -Wdouble-promotion -fsingle-precision-constant
FLAGS += $(DEBUG_FLAGS)

ifeq ($(NETWORKING), true)
        FLAGS += -DLPC_NETWORKING
endif


ifeq ($(COMBINE_AHBRAM), true)
	FLAGS += -DCOMBINE_AHBRAM
endif

CFLAGS   = $(FLAGS) -std=gnu11 -fgnu89-inline
CXXFLAGS = $(FLAGS) -std=gnu++14  -fno-threadsafe-statics -fno-rtti -fno-exceptions

#RRF c++ flags
RRF_CXXFLAGS  = -DVARIANT_MCK=$(VARIANT_MCK) -D__$(PROCESSOR)__ -D__$(BOARD)__ -D_XOPEN_SOURCE -Wall -c -std=gnu++14 -mcpu=cortex-m3 -mthumb -ffunction-sections -fdata-sections 
RRF_CXXFLAGS += -DCORE_M3
RRF_CXXFLAGS += -fno-threadsafe-statics -fno-rtti -fno-exceptions -nostdlib -Wdouble-promotion -fsingle-precision-constant
RRF_CXXFLAGS += -MMD -MP -march=armv7-m
RRF_CXXFLAGS += $(DEBUG_FLAGS)
RRF_CXXFLAGS += -DRTOS -DFREERTOS_USED

ifeq ($(COMBINE_AHBRAM), true)
	RRF_CXXFLAGS += -DCOMBINE_AHBRAM
endif

ifeq ($(NETWORKING), true)
	RRF_CXXFLAGS += -DLPC_NETWORKING
endif


#Core libraries
CORE_SRC     = $(CORE)/cores $(CORE)/system $(CORE)/variants/LPC
CORE_SRC    += $(CORE)/libraries/WIRE $(CORE)/libraries/SDCard
CORE_SRC    += $(CORE)/libraries/SharedSpi

ifeq ($(USE_DFU), true)
        #enable DFU
        $(info Enabling DFU)
        CXXFLAGS += -DENABLE_DFU
endif



#Includes
CORE_INCLUDES	 = -I$(CORE)
CORE_INCLUDES	+= -I$(CORE)/system/ExploreM3_lib/
CORE_INCLUDES	+= -I$(CORE)/cores/ExploreM3/
CORE_INCLUDES   += -I$(CORE)/cores/arduino/
CORE_INCLUDES   += -I$(CORE)/cores/smoothie/
CORE_INCLUDES   += -I$(CORE)/cores/smoothie/USBDevice/
CORE_INCLUDES   += -I$(CORE)/cores/smoothie/USBDevice/USBDevice/
CORE_INCLUDES   += -I$(CORE)/cores/smoothie/USBDevice/USBSerial/ 
CORE_INCLUDES   += -I$(CORE)/system/CMSIS/CMSIS/Include/
CORE_INCLUDES   += -I$(CORE)/variants/LPC/
CORE_INCLUDES   += -I$(CORE)/libraries/SDCard/ 
CORE_INCLUDES   += -I$(CORE)/libraries/SharedSpi/
CORE_INCLUDES   += -I$(CORE)/libraries/WIRE/

#openlpc 
CORE_INCLUDES  += -I$(CORE)/cores/lpcopen/
CORE_INCLUDES  += -I$(CORE)/cores/lpcopen/inc
CORE_INCLUDES  += -I$(CORE)/cores/lpcopen/board

#Find all c and c++ files for Core
CORE_OBJ_SRC_C	   += $(foreach src, $(CORE_SRC), $(wildcard $(src)/*.c $(src)/*/*.c $(src)/*/*/*.c $(src)/*/*/*/*.c $(src)/*/*/*/*/*.c) )
CORE_OBJ_SRC_CXX   += $(foreach src, $(CORE_SRC), $(wildcard $(src)/*.cpp $(src)/*/*.cpp $(src)/*/*/*.cpp $(src)/*/*/*/*.cpp $(src)/*/*/*/*/*.cpp) )

CORE_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(CORE_OBJ_SRC_C)) $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(CORE_OBJ_SRC_CXX))


#RTOS Sources (selected dirs only)
RTOS_CORE_SRC    += $(RTOS_SRC) $(RTOS_SRC_PORTABLE)
RTOS_CORE_OBJ_SRC_C  += $(foreach src, $(RTOS_CORE_SRC), $(wildcard $(src)/*.c) )
#RTOS Dynamic Memory Management
RTOS_CORE_OBJ_SRC_C  += $(RTOS_SRC)/portable/MemMang/heap_5.c
#RTOS_CORE_OBJ_SRC_C  += $(RTOS_SRC)/portable/MemMang/heap_4.c

CORE_OBJS += $(patsubst %.c,$(BUILD_DIR)/%.o,$(RTOS_CORE_OBJ_SRC_C))

#RTOS Includes
CORE_INCLUDES   += -I$(RTOS_INCLUDE) -I$(RTOS_SRC_PORTABLE)
CORE_INCLUDES   += -I$(RTOS_CONFIG_INCLUDE)


#RTOS-Plus (selected dirs only)
#RTOS +TCP
RTOSPLUS_TCP_SRC = $(RTOSPLUS_SRC)/FreeRTOS-Plus-TCP
RTOSPLUS_TCP_INCLUDE = $(RTOSPLUS_TCP_SRC)/include/


RTOSPLUS_CORE_SRC    += $(RTOSPLUS_TCP_SRC) $(RTOS_SRC_PORTABLE)
RTOSPLUS_CORE_OBJ_SRC_C  += $(foreach src, $(RTOSPLUS_CORE_SRC), $(wildcard $(src)/*.c) )
#** RTOS+TCP Portable**

#(portable) Select Buffer Management (only enable one)
RTOSPLUS_CORE_OBJ_SRC_C  += $(RTOSPLUS_TCP_SRC)/portable/BufferManagement/BufferAllocation_1.c #Static Ethernet Buffers
#RTOSPLUS_CORE_OBJ_SRC_C  += $(RTOSPLUS_TCP_SRC)/portable/BufferManagement/BufferAllocation_2.c #Dynamic Ethernet Buffers

#(portable) Compiler Includes (GCC)
RTOSPLUS_TCP_INCLUDE += -I$(RTOSPLUS_TCP_SRC)/portable/Compiler/GCC

#(portable) NetworkInterface
RTOSPLUS_TCP_INCLUDE += -I$(RTOSPLUS_TCP_SRC)/portable/NetworkInterface/include
RTOSPLUS_TCP_NI_SRC    += $(RTOSPLUS_TCP_SRC)/portable/NetworkInterface/Common
RTOSPLUS_TCP_NI_SRC    += $(RTOSPLUS_TCP_SRC)/portable/NetworkInterface/LPC17xx 
RTOSPLUS_CORE_OBJ_SRC_C  += $(foreach src, $(RTOSPLUS_TCP_NI_SRC), $(wildcard $(src)/*.c) )


CORE_OBJS += $(patsubst %.c,$(BUILD_DIR)/%.o,$(RTOSPLUS_CORE_OBJ_SRC_C))

#RTOS+TCP Includes
CORE_INCLUDES   += -I$(RTOSPLUS_TCP_INCLUDE) -I$(RTOSPLUS_TCP_INCLUDE)




#RepRapFirmware Sources  
RRF_SRC_DIRS  = FilamentMonitors GCodes Heating Movement Movement/BedProbing Movement/Kinematics 
RRF_SRC_DIRS += Storage Tools Libraries/General Libraries/Math Libraries/sha1 Libraries/Fatfs Libraries/Fatfs/port/lpc
RRF_SRC_DIRS += Heating/Sensors Fans
RRF_SRC_DIRS += LPC LPC/MCP4461
#biuld in LCD Support? only when networking is false
ifeq ($(NETWORKING), false)
	#RRF_SRC_DIRS += Display Display/ST7920
endif

#networking support?
ifeq ($(NETWORKING), true)
	RRF_SRC_DIRS += Networking Networking/RTOSPlusTCPEthernet
else
	RRF_SRC_DIRS += LPC/NoNetwork
endif

#Find the c and cpp source files
RRF_SRC = $(RRF_SRC_BASE) $(addprefix $(RRF_SRC_BASE)/, $(RRF_SRC_DIRS))
RRF_OBJ_SRC_C	   += $(foreach src, $(RRF_SRC), $(wildcard $(src)/*.c) ) 
RRF_OBJ_SRC_CXX   += $(foreach src, $(RRF_SRC), $(wildcard $(src)/*.cpp) )


RRF_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(RRF_OBJ_SRC_C)) $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(RRF_OBJ_SRC_CXX))

RRF_INCLUDES = $(addprefix -I, $(RRF_SRC))
RRF_INCLUDES += -I$(RRF_SRC_BASE)/Libraries/

#end RRF


#all Includes (RRF + Core)
INCLUDES = $(CORE_INCLUDES) $(RRF_INCLUDES)




#set Flags for RepRapFirmware Files.
$(RRF_OBJS): CXXFLAGS = $(RRF_CXXFLAGS)


default: all

all: firmware


firmware:  $(BUILD_DIR)/$(OUTPUT_NAME).elf

coreLPC: $(BUILD_DIR)/core.a


$(BUILD_DIR)/core.a: $(CORE_OBJS)

	@echo Building core.a
	$(V)$(AR) rcs $@ $(CORE_OBJS)


$(BUILD_DIR)/$(OUTPUT_NAME).elf: $(BUILD_DIR)/core.a $(RRF_OBJS) 
	@echo Building ELF and BIN
	$(V)$(MKDIR) $(dir $@)
	$(V)$(LD) -L$(BUILD_DIR)/ -Os --specs=nano.specs -u _printf_float -u _scanf_float -Wl,--warn-section-align -Wl,--gc-sections -Wl,--fatal-warnings -march=armv7-m -mcpu=cortex-m3 -T$(LINKER_SCRIPT) -Wl,-Map,$(OUTPUT_NAME).map -o $(OUTPUT_NAME).elf  -mthumb -Wl,--cref -Wl,--check-sections -Wl,--gc-sections -Wl,--entry=Reset_Handler -Wl,--unresolved-symbols=report-all -Wl,--warn-common -Wl,--warn-unresolved-symbols -Wl,--start-group $(BUILD_DIR)/$(CORE)/cores/arduino/syscalls.o $(BUILD_DIR)/core.a $(RRF_OBJS) -Wl,--end-group -lm
	$(V)$(OBJCOPY) --strip-unneeded -O binary $(OUTPUT_NAME).elf $(OUTPUT_NAME).bin
	$(V)$(SIZE) $(OUTPUT_NAME).elf

	@./staticMemStats.sh 

$(BUILD_DIR)/%.o: %.c
	@echo "[$(CC): Compiling $<]"
	$(V)$(MKDIR) $(dir $@)
	$(V)$(CC)  $(CFLAGS) $(DEFINES) $(INCLUDES) -MMD -MP -o $@ $<
	
$(BUILD_DIR)/%.o: %.cpp
	@echo "[$(CXX): Compiling $<]"
	$(V)$(MKDIR) $(dir $@)
	$(V)$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -MMD -MP -o $@ $<


upload: $(BUILD_DIR)/$(OUTPUT_NAME).elf
	@echo "Flashing firmware via DFU"
	dfu-util -d 1d50:6015 -D $(OUTPUT_NAME).bin -R

cleanrrf:
	-rm -f $(RRF_OBJS)
	
cleancore:
	-rm -f $(BUILD_DIR)/core.a $(CORE_OBJS)

clean:
	-rm -f firmware.lst firmware.elf firmware.hex firmware.map firmware.bin firmware.list

distclean: clean cleancore cleanrrf
	-rm -rf $(BUILD_DIR)/core.a $(RRF_OBJS) $(CORE_OBJS)

.PHONY: all  clean distclean
