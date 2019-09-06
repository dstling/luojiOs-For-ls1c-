
# 虚拟机里的交叉编译工具链
#CROSS_COMPILE 	=mipsel-linux-
#COPY   = cp
# windows下的交叉编译工具链
CROSS_COMPILE 	=mipsel-linux-
COPY    = copy

#
# Include the make variables (CC, etc...)
#

AS		= $(CROSS_COMPILE)as
LD		= $(CROSS_COMPILE)ld
CC		= $(CROSS_COMPILE)gcc
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
SIZE		= $(CROSS_COMPILE)size


HEADERS = $(wildcard lib/*.h example/*.h app/*.h include/*.h )
SRC_S = $(wildcard lib/*.S os/*.S)

#			os/net/lwip-1.4.1/src/api/*.c \

SRC_C = $(wildcard lib/*.c example/*.c app/*.c libc/*.c libm/*.c \
			os/*.c \
			os/net/*.c \
			os/net/lwip-1.4.1/src/core/*.c\
			os/net/lwip-1.4.1/src/core/ipv4/*.c\
			os/net/lwip-1.4.1/src/api/*.c \
			os/net/lwip-1.4.1/src/netif/*.c\
			os/net/lwip-1.4.1/src/arch/*.c\
			)
			
OBJS = $(patsubst %.S, %.o, $(SRC_S)) $(patsubst %.c, %.o, $(SRC_C))  # 注意汇编文件一定要在前面

#					-Ios/net/lwip-1.4.1/src/arch_include 
#头文件查找路径
INCLUDES = -Iinclude -Ilib -Iexample -Iapp \
					-Ios/include \
					-Ios/net/lwip-1.4.1/src/include \
					-Ios/net/lwip-1.4.1/src/include/ipv4 \
					-Ios/net/lwip-1.4.1/src/arch/include

#libc的头文件所在目录
VPATH += include
#链接库查找路径
LIBS = 

#编译参数
CCFLAGS = -mno-abicalls -fno-pic -g -Wall -Wstrict-prototypes -Wno-uninitialized -Wno-format -Wno-main -O2 -G 0 -mips2 -fno-builtin -nostdinc
AFLAGS  = -mno-abicalls -fno-pic -G 0 -mips2 -Wall -mno-abicalls -fno-builtin
#链接参数
LDFLAGS = -m elf32ltsmip -G 0 -static -n -nostdlib -N

# 最终的目标文件
OUTPUT_ELF = OpenLoongsonLib1c.elf
OUTPUT_DEBUG_ELF = OpenLoongsonLib1c_debug.elf
OUTPUT_BIN = OpenLoongsonLib1c.bin
OUTPUT_MAP = OpenLoongsonLib1c.map

all:$(OUTPUT_BIN)

$(OUTPUT_BIN):$(OBJS)
	$(LD) $(LDFLAGS) -T ld.script -e start -o $(OUTPUT_ELF) $^ -Map $(OUTPUT_MAP)
#	$(COPY) $(OUTPUT_ELF) $(OUTPUT_DEBUG_ELF)
	$(STRIP) -g -S --strip-debug $(OUTPUT_ELF)
	$(OBJCOPY) -O binary $(OUTPUT_ELF) $(OUTPUT_BIN)
	$(SIZE) $(OUTPUT_ELF)
#	cp $(OUTPUT_BIN) /tftpboot/
    
.c.o:
	$(CC) $(CCFLAGS) $(INCLUDES) -c -o $@ $^

.S.o:
	$(CC) $(AFLAGS) $(INCLUDES) -c -o $@ $^
    
clean:
	rm -f $(OBJS) $(OUTPUT_ELF) $(OUTPUT_BIN) $(OUTPUT_MAP)
	
	
