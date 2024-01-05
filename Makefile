

################################################################

OO_DIR = /e/AGRV/AgRV_pio/packages/tool-agrv_logic/openocd/
OPENOCD = $(OO_DIR)/bin/openocd.exe -c "set ADAPTER cmsis-dap; set CONNECT_UNDER_RESET 1" -f $(OO_DIR)/agrv2k.cfg 
CROSS = riscv64-unknown-elf-

CC = $(CROSS)gcc
AS = $(CROSS)gcc
LD = $(CROSS)gcc
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump
GDB = $(CROSS)gdb


TARGET = agrv32.elf

modules  = main cherryusb tntfs

################################################################

make_module_dirs := $(shell mkdir -p $(addprefix objs/, $(modules)) )


OBJS    :=
INCDIR  := -Iinc
DEFS    :=

LDFLAGS := -fno-builtin -nostartfiles -nodefaultlibs

include $(addsuffix /Makefile, $(modules))

CFLAGS  := -Wall -g -MMD -fno-builtin $(DEFS) $(INCDIR)

TOBJS = $(addprefix objs/, $(OBJS))
DEPENDS = $(patsubst %.o, %.d, $(TOBJS))

all: $(TARGET)

clean:
	rm -f $(TOBJS) $(TARGET) $(TARGET).bin $(TARGET).dump


$(TARGET): $(TOBJS)
ifeq ($(V), 1)
	@echo $(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	@echo $(OBJDUMP) -xd $@ >$@.dump
	@echo $(OBJCOPY) -O binary -R .comment -R .note* $@ $@.bin
else
	@echo linking  $@ ...
endif
	@$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	@$(OBJDUMP) -xd $@ >$@.dump
	@$(OBJCOPY) -O binary -R .comment -R .note* $@ $@.bin


-include $(DEPENDS)

objs/%.o: %.c
ifeq ($(V), 1)
	@echo $(CC)  $(CFLAGS) -c $< -o $@
else
	@echo Compile  $< ...
endif
	@$(CC)  $(CFLAGS) -c $< -o $@

objs/%.o: %.S
ifeq ($(V), 1)
	@echo $(CC)  -D__ASSEMBLY__ $(CFLAGS) $(ASFLAG) -c $< -o $@
else
	@echo Compile  $< ...
endif
	@$(CC)  -D__ASSEMBLY__ $(CFLAGS) $(ASFLAG) -c $< -o $@


################################################################


dl: $(TARGET)
	$(OPENOCD) -c "flash write_image erase agrv32.elf.bin 0x80000000" -c "reset" -c "exit"

gdb:
	start $(OPENOCD)
	$(GDB) --tui -ex "target remote 127.0.0.1:3333" agrv32.elf

oohlp:
	$(OPENOCD) -c "help" -c "exit"


