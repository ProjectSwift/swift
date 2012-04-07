
PROJECT=swift
OBJECTS=swift.o rtty.o gps.o geofence.o

# Serial device used for programming AVR
#TTYPORT=/dev/ttyACM0

CFLAGS=-Os -Wall -mmcu=atmega644p
CC=avr-gcc
OBJCOPY=avr-objcopy

rom.hex: $(PROJECT).out
	$(OBJCOPY) -O ihex $(PROJECT).out rom.hex

$(PROJECT).out: $(OBJECTS) config.h
	$(CC) $(CFLAGS) -o $(PROJECT).out -ffunction-sections -fdata-sections -Wl,--gc-sections,-Map,$(PROJECT).map $(OBJECTS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.out *.map *.hex *~

flash: rom.hex
	avrdude -p m644p -B 1 -c usbtiny -U flash:w:rom.hex:i

setfuses:
	# This sets the low fuse to use an external 7.3728MHz or 8MHz crystal,
	# with no prescaler
	avrdude -p m644p -c usbtiny -U lfuse:w:0xF7:m
	
	# Use the internal 8MHz osccilator, /8 prescaler (Default)
	#avrdude -p m644p -c stk500v2 -P $(TTYPORT) -U lfuse:w:0x62:m
	
	# Use internal 8MHz osccilator, no prescaler
	#avrdude -p m644p -c stk500v2 -P $(TTYPORT) -U lfuse:w:0xE2:m
	
