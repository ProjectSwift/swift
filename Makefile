
# Microcontroller
MCU=atmega644p

# Programmer
PROG=stk500v2
TTYPORT=/dev/ttyACM0

#PROG=usbtiny

# Objects
PROJECT=swift
OBJECTS=swift.o rtty.o ax25modem.o gps.o geofence.o ds18x20.o bmp085.o timeout.o ssdv.o rs8encode.o c328.o

# Programs
CC=avr-gcc
OBJCOPY=avr-objcopy
AVRSIZE=avr-size

rom.hex: $(PROJECT).out
	$(OBJCOPY) -O ihex $(PROJECT).out rom.hex

$(PROJECT).out: $(OBJECTS) config.h
	$(CC) -mmcu=$(MCU) -o $(PROJECT).out -ffunction-sections -fdata-sections -Wl,--gc-sections,-Map,$(PROJECT).map $(OBJECTS)
	$(AVRSIZE) -C --mcu=$(MCU) $(PROJECT).out

.c.o:
	$(CC) -Os -Wall -mmcu=$(MCU) -c $< -o $@

clean:
	rm -f *.o *.out *.map *.hex *~

flash: rom.hex
	avrdude -p $(MCU) -B 1 -c $(PROG) -P $(TTYPORT) -U flash:w:rom.hex:i

setfuses:
	# This sets the low fuse to use an external 7.3728MHz or 8MHz crystal,
	# with no prescaler
	avrdude -p $(MCU) -c $(PROG) -P $(TTYPORT)  -U lfuse:w:0xF7:m
	
	# Use the internal 8MHz osccilator, /8 prescaler (Default)
	#avrdude -p $(MCU) -c $(PROG) -P $(TTYPORT) -U lfuse:w:0x62:m
	
	# Use internal 8MHz osccilator, no prescaler
	#avrdude -p $(MCU) -c $(PROG) -P $(TTYPORT) -U lfuse:w:0xE2:m
	
