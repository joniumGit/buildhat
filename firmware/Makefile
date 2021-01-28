COPTS:=-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -fsingle-precision-constant -I . -g -Os -Wall -Wno-misleading-indentation -fno-builtin-strcpy -fno-strict-aliasing -nostartfiles

.PHONY: reset

all: up
up: main.bin Makefile
	./flash.sh

upbl: bhbl.bin Makefile
	./flash-bl.sh

reset:
	./reset.sh

# bhbl.bin: bhbl Makefile
# 	arm-none-eabi-objcopy -O binary bhbl bhbl.bin
#
# bhbl.hex: bhbl Makefile
# 	arm-none-eabi-objcopy -O ihex bhbl bhbl.hex
#
# bhbl: bhbl-startup.s bhbl.c Makefile bhbl.lnk
# 	arm-none-eabi-gcc $(COPTS) -nostartfiles -Xlinker -i bhbl-startup.s bhbl.c -o bhbl.o
# 	arm-none-eabi-ld  $(LOPTS) -T bhbl.lnk bhbl.o -o bhbl
# 	arm-none-eabi-objdump -D bhbl >bhbl.dump



%.bin: % Makefile
	arm-none-eabi-objcopy -O binary $< $@

%.hex: % Makefile
	arm-none-eabi-objcopy -O ihex $< $@

%.o: %.s
	arm-none-eabi-gcc -c $(COPTS) $< -o $@

%.o: %.c
	arm-none-eabi-gcc -c $(COPTS) $< -o $@

main: main.lnk main-startup.o main.o debug.o system.o gpio.o ioconv.o adc.o timer.o hardware.o ports.o leds.o control.o command.o device.o pwm_pid.o tests.o message.o
	arm-none-eabi-gcc $(COPTS) -T main.lnk main-startup.o main.o debug.o system.o gpio.o ioconv.o adc.o timer.o hardware.o ports.o leds.o control.o command.o device.o pwm_pid.o tests.o message.o -o main
	arm-none-eabi-objdump -D main >main.dump

bhbl: bhbl.lnk bhbl-startup.o bhbl.o debug.o system.o gpio.o ioconv.o hardware.o control.o
	arm-none-eabi-gcc $(COPTS) -T bhbl.lnk bhbl-startup.o bhbl.o debug.o system.o gpio.o ioconv.o hardware.o control.o -o bhbl
	arm-none-eabi-objdump -D bhbl >bhbl.dump
