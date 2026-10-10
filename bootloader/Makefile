TARGET = bootloader

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

CFLAGS = -mcpu=cortex-m4 \
         -mthumb \
         -mfloat-abi=soft \
         -ffreestanding \
         -fdata-sections \
         -ffunction-sections \
         -Wall \
         -Wextra \
         -Iinc

LDFLAGS = -T linker.ld \
          -nostdlib \
          -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map

C_SOURCES = src/main.c src/flash.c src/image.c src/app.c src/uart.c


ASM_SOURCES = startup/startup.s

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)

OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS)


all: $(TARGET).bin


$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@


$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(OBJECTS) \
	      $(TARGET).elf \
	      $(TARGET).bin \
	      $(TARGET).map