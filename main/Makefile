

################################################################


LDIR = main

LOBJS  = start.o cpu_agrv.o main.o shell.o spi_sd.o
LOBJS += xos_core.o heap.o printk.o string.o

OBJS += $(addprefix $(LDIR)/, $(LOBJS))


################################################################


INCDIR  += -Imain
DEFS    += -march=rv32imafc -mabi=ilp32f -fno-builtin -DARCH_RISCV
LDFLAGS += -T $(LDIR)/ld.S -march=rv32imafc -mabi=ilp32f -Wl,--defsym,TEXT_START=0x80000000
ASFLAG  += -DRUN_FLASH

################################################################


