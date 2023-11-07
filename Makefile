
OO_DIR = /e/AGRV/AgRV_pio/packages/tool-agrv_logic/openocd/
OPENOCD = $(OO_DIR)/bin/openocd.exe -c "set CONNECT_UNDER_RESET 1" -f $(OO_DIR)/agrv2k.cfg 
#OPENOCD = $(OO_DIR)/bin/openocd.exe -c "set ADAPTER cmsis-dap; set CONNECT_UNDER_RESET 1" -f $(OO_DIR)/agrv2k.cfg 
CROSS = riscv64-unknown-elf-

CC = $(CROSS)gcc
AS = $(CROSS)gcc
LD = $(CROSS)gcc
GDB = $(CROSS)gdb
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump


CFLAGS = -Wall -g -O3 -I. -Iinc -march=rv32imafc -mabi=ilp32f -fno-builtin
LDFLAGS  = -fno-builtin -nostartfiles -nodefaultlibs -march=rv32imafc -mabi=ilp32f
LIBS = -lgcc

# SRAM
#LDFLAGS += -T ld_ram.S -Wl,--defsym,TEXT_START=0x20000000
#ASFLAG  += -DRUN_SRAM
# FLASH
LDFLAGS += -T ld.S -Wl,--defsym,TEXT_START=0x80000000
ASFLAG  += -DRUN_FLASH


TARGET = agrv32.elf

OBJS = start.o main.o shell.o printk.o string.o spi_sd.o

#coremark
ifeq (1,0)
CMK_CFLAGS += -funroll-all-loops -finline-limit=600 -ftree-dominator-opts -fno-if-conversion2 -fselective-scheduling -fno-code-hoisting -fno-common -funroll-loops -finline-functions -falign-functions=4 -falign-jumps=4 -falign-loops=4
CMK_OBJS += coremark/core_list_join.o
CMK_OBJS += coremark/core_main.o
CMK_OBJS += coremark/core_matrix.o
CMK_OBJS += coremark/core_portme.o
CMK_OBJS += coremark/core_state.o
CMK_OBJS += coremark/core_util.o
$(CMK_OBJS): CFLAGS += $(CMK_CFLAGS)

CFLAGS += -DRUN_COREMARK
OBJS += $(CMK_OBJS)
endif

###############################################################


all: $(TARGET)

$(TARGET): $(OBJS) ld.S
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	$(OBJDUMP) -xds $@ >$@.dump
	$(OBJCOPY) -O binary -R .comment -R .note* $@ $@.bin

clean:
	rm -f $(TARGET) $(OBJS) *.bin *.dump

dl: $(TARGET)
	$(OPENOCD) -c "flash write_image erase agrv32.elf.bin 0x80000000" -c "reset" -c "exit"

gdb:
	start $(OPENOCD)
	$(GDB) --tui -ex "target remote 127.0.0.1:3333" agrv32.elf

oohlp:
	$(OPENOCD) -c "help" -c "exit"


%.o: %.c
	$(CC)  $(CFLAGS)   -c $< -o $@

%.o: %.S
	$(CC)  -D__ASSEMBLY__ $(CFLAGS) $(ASFLAG) -c $< -o $@

