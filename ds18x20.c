
/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Philip Heron <phil@sanslogic.co.uk>               */
/*                     Nigel Smart <nigel@projectswift.co.uk>            */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include "config.h"
#include <stdint.h>
#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include <util/crc16.h>
#include "ds18x20.h"

/* The port and pin to use for 1-wire */
#define PIN  _BV(3)
#define OUT  DDRA
#define IN   PINA
#define PORT PORTA

/* Helpers for 1-wire communications */
#define PIN_LOW()  (PORT &= ~PIN)
#define PIN_HIGH() (PORT |= PIN)

#define PIN_IN()   (OUT &= ~PIN)
#define PIN_OUT()  (OUT |= PIN)

#define READ_PIN() (IN & PIN)

/* Delay times */
#define RESET_PULSE     (480)
#define RESET_WAIT_LOW  (80)
#define BEGIN_TIMESLOT  (5)
#define DELAY_TIMESLOT  (5)
#define TIMESLOT        (60)
#define RECOVERY        (10)

/* DS18x20 sensor IDs */
#define DS18S20_ID (0x10)
#define DS1822_ID  (0x22)
#define DS18B20_ID (0x28)

static int ds_reset()
{
	/* Reset pulse - pull line low for at least 480us */
	PIN_LOW();
	PIN_OUT();
	_delay_us(RESET_PULSE);
	PIN_IN();
	
	/* The sensor should respond by pulling the line low
	 * after 15us to 60us for 60us to 240us. Wait for
	 * 80us for the line to go low */
	_delay_us(RESET_WAIT_LOW);
	if(READ_PIN()) return(DS_TIMEOUT);
	
	_delay_us(RESET_PULSE - RESET_WAIT_LOW);
	if(!READ_PIN()) return(DS_TIMEOUT);
	
	return(DS_OK);
}

static void ds_write_bit(uint8_t b)
{
	/* Begin a read/write time slot
	 * pull line low for between 1us and 15us */
	PIN_LOW();
	PIN_OUT();
	_delay_us(BEGIN_TIMESLOT);
	
	/* "1" is written by releasing the line immediatly */
	if(b) PIN_IN();
	
	/* Wait for the write slot to end and then release the line */
	_delay_us(TIMESLOT - BEGIN_TIMESLOT);
	PIN_IN();
	
	_delay_us(RECOVERY);
}

static uint8_t ds_read_bit()
{
	uint8_t i, b = 1;
	
	/* Begin a read/write time slot
	 * pull line low for between 1us and 15us */
	PIN_LOW();
	PIN_OUT();
	_delay_us(BEGIN_TIMESLOT);
	PIN_IN();
	
	_delay_us(DELAY_TIMESLOT);
	
	/* If the sensor pulls the line low within
	 * the time slot the bit is 0 */
	for(i = 0; i < TIMESLOT - BEGIN_TIMESLOT; i++)
	{
		if(!READ_PIN()) b = 0;
		_delay_us(1);
	}
	
	_delay_us(RECOVERY);
	
	return(b);
}

static void ds_write_byte(uint8_t b)
{
	uint8_t i;
	
	for(i = 0; i < 8; i++)
	{
		ds_write_bit(b & 1);
		b >>= 1;
	}
}

static uint8_t ds_read_byte()
{
	uint8_t i, b = 0;
	
	for(i = 0; i < 8; i++)
	{
		b >>= 1;
		if(ds_read_bit()) b |= 1 << 7;
	}
	
	return(b);
}

int ds_search_rom(uint8_t *id, uint8_t mask)
{
	uint8_t r, i, j;
	uint8_t crc, b1, b2;
	
	/* Reset the sensor(s) */
	r = ds_reset();
	if(r != DS_OK) return(r);
	
	/* Transmit the SEARCH ROM command */
	ds_write_byte(0xF0);
	
	/* An ID is made up of 8 bytes (7 + CRC) */
	for(crc = i = 0; i < 8; i++)
	{
		*id = 0;
		
		for(j = 0; j < 8; j++)
		{
			/* Read the bit and its complement */
			b1 = ds_read_bit();
			b2 = ds_read_bit();
			
			/* Both bits should never be 1 */
			if(b1 && b2) return(DS_ERROR);
			
			/* Both bits are 0 when two or more sensors
			 * respond with a different value */
			if(b1 == b2)
			{
				b1 = mask & 1;
				mask >>= 1;
				
				/* Test if this is the last ID on the bus */
				if(!b1) r = DS_MORE;
			}
			
			/* Let the sensors know which direction we're going */
			ds_write_bit(b1);
			
			*id >>= 1;
			if(b1) *id |= 1 << 7;
		}
		
		/* Update the CRC, compare with last byte */
		if(i != 7) crc = _crc_ibutton_update(crc, *id);
		else if(crc != *id) return(DS_BADCRC);
		
		id++;
	}
	
	return(r);
}

static int ds_match_rom(uint8_t *id)
{
	uint8_t i;
	
	/* Reset the sensor(s) */
	i = ds_reset();
	if(i != DS_OK) return(i);
	
	/* Transmit the MATCH ROM command */
	ds_write_byte(0x55);
	
	/* Transmit the ID */
	for(i = 0; i < 8; i++)
		ds_write_byte(id[i]);
	
	return(DS_OK);
}

static int ds_convert()
{
	uint8_t i;
	
	/* Send the command to begin a single temperature conversion */
	ds_write_byte(0x44);
	
	/* Wait until the sensor has finished conversion */
	for(i = 0; ds_read_bit() && i < 100; i++) _delay_ms(1);
	if(i == 100) return(DS_TIMEOUT);
	
	return(DS_OK);
}

static int ds_read_scratchpad(uint8_t *sp)
{
	uint8_t i, crc;
	
	/* Send the command to read the scratchpad */
	ds_write_byte(0xBE);
	
	/* Read the 8 bytes of the scratchpad */
	for(crc = i = 0; i < 8; i++)
		crc = _crc_ibutton_update(crc, *(sp++) = ds_read_byte());
	
	/* Read and check the CRC */
	if(crc != (*sp = ds_read_byte())) return(DS_BADCRC);
	
	return(DS_OK);
}

int ds_read_temperature(int32_t *temperature, uint8_t *id)
{
	uint8_t sp[9], i;
	int16_t temp;
	
	/* Select the sensor */
	i = ds_match_rom(id);
	if(i != DS_OK) return(i);
	
	/* Issue the convert command and wait for completion */
	i = ds_convert(id);
	if(i != DS_OK) return(i);
	
	/* Re-select the sensor */
	i = ds_match_rom(id);
	if(i != DS_OK) return(i);
	
	/* Read the scratchpad data */
	i = ds_read_scratchpad(sp);
	if(i != DS_OK) return(i);
	
	switch(id[0])
	{
	case DS18B20_ID:
		/* Read the result from the scratchpad */
		temp = sp[0] | (sp[1] << 8);
		
		/* Convert to a 4dp fixed-point integer */
		*temperature = temp * 625L;
		break;
	
	case DS18S20_ID:
		/* Read the result from the scratchpad */
		temp = sp[0] | (sp[1] << 8);
		
		/* Convert to a 4dp fixed-point integer */
		*temperature = temp * 5000L;
		break;
	
	case DS1822_ID:
		*temperature = 0;
		break;
	}
	
	return(DS_OK);
}

