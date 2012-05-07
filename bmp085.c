
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
#include <avr/io.h>
#include <util/twi.h>
#include <util/delay.h>
#include "bmp085.h"

/* Address of device */
#define BMP085 (0xEE)

#define _INT16(buf, b) (((uint16_t) buf[b] << 8) + buf[b + 1])
#define _INT24(buf, b) (((uint32_t) buf[b] << 16) + ((uint32_t) buf[b + 1] << 8) + buf[b + 2])

/* Two-wire / I2C helpers */
static void _tw_stop()
{
	uint8_t i;
	
	/* Send Stop Condition */
	TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
	for(i = 0; i < 200 && (TWCR & _BV(TWSTO)); i++)
		_delay_us(1);
}

static uint8_t _tw_send(uint8_t flag)
{
	/* Set the flags, wait for completion and return result */
	uint8_t i;
	
	TWCR = _BV(TWINT) | _BV(TWEN) | flag;
	for(i = 0; i < 200; i++)
	{
		if(TWCR & _BV(TWINT)) return(TW_STATUS);
		_delay_us(1);
	}
	
	return(0);
}

static uint8_t _tw_tx(uint8_t b)
{
	TWDR = b;
	return(_tw_send(0));
}

static uint8_t _tw_read(uint8_t addr, uint8_t *ptr, uint8_t len)
{
	uint8_t r;
	
	/* Send Start Condition */
	r = _tw_send(_BV(TWSTA));
	if(r != TW_START && r != TW_REP_START) return(r);
	
	/* Send the BMP085 ID (write) */
	if((r = _tw_tx(BMP085 | TW_WRITE)) != TW_MT_SLA_ACK) return(r);
	
	/* Send the address to begin reading at */
	if((r = _tw_tx(addr)) != TW_MT_DATA_ACK) return(r);
	
	/* Send another Start Condition (repeat start) */
	r = _tw_send(_BV(TWSTA));
	if(r != TW_START && r != TW_REP_START) return(r);
	
	/* Send the BMP085 ID (read) */
	if((r = _tw_tx(BMP085 | TW_READ)) != TW_MR_SLA_ACK) return(r);
	
	/* Read each byte */
	while(len--)
	{
		r = _tw_send(len ? _BV(TWEA) : 0);
		if(r != TW_MR_DATA_ACK &&
		   r != TW_MR_DATA_NACK) return(r);
		
		*(ptr++) = TWDR;
		
		if(r == TW_MR_DATA_NACK) break;
	}
	
	/* Send Stop Condition */
	_tw_stop();
	
	return(-1);
}

static uint8_t _tw_write(uint8_t addr, uint8_t *ptr, uint8_t len)
{
	uint8_t r;
	
	/* Send Start Condition */
	r = _tw_send(_BV(TWSTA));
	if(r != TW_START && r != TW_REP_START) return(r);
	
	/* Send the BMP085 ID (write) */
	r = _tw_tx(BMP085 | TW_WRITE);
	if(r != TW_MT_SLA_ACK) return(r);
	
	/* Send the address to begin writing at */
	r = _tw_tx(addr);
	if(r != TW_MT_DATA_ACK) return(r);
	
	/* Transmit the bytes */
	while(len--)
	{
		r = _tw_tx(*(ptr++));
		if(r != TW_MT_DATA_ACK) return(r);
	}
	
	/* Send Stop Condition */
	_tw_stop();
	
	return(1);
}

/* -------------------------------------------- */

void bmp085_init()
{
	/* Set the I2C speed */
	TWSR = _BV(TWPS1) | _BV(TWPS0); /* /64 prescaler */
	TWBR = 0;
}

int bmp085_read_calibration(bmp085_t *s)
{
	uint8_t buf[22];
	
	/* Read calibration values */
	if(_tw_read(0xAA, buf, 22) == 0) return(BMP_ERROR);
	
	s->ac1 = _INT16(buf, 0);
	s->ac2 = _INT16(buf, 2);
	s->ac3 = _INT16(buf, 4);
	s->ac4 = _INT16(buf, 6);
	s->ac5 = _INT16(buf, 8);
	s->ac6 = _INT16(buf, 10);
	s->b1  = _INT16(buf, 12);
	s->b2  = _INT16(buf, 14);
	/*s->mb  = _INT16(buf, 16);*/ /* This is never used */
	s->mc  = _INT16(buf, 18);
	s->md  = _INT16(buf, 20);
	
	/* TODO: The data communication can be checked by checking
	 * that none of the words has the value 0 or 0xFFFF. */
	
	s->b5 = 0;
	
	return(BMP_OK);
}

int bmp085_sample(bmp085_t *s, uint8_t oversample)
{
	uint8_t buf[3];
	
	/* Store the oversample parameter */
	s->oversample = oversample;
	
	/* Start temperature measurement */
	*buf = 0x2E;
	if(_tw_write(0xF4, buf, 1) == 0) return(BMP_ERROR);
	
	/* Wait at least 4.5ms */
	_delay_ms(5);
	
	/* Read the result */
	if(_tw_read(0xF6, buf, 2) == 0) return(BMP_ERROR);
	s->ut = _INT16(buf, 0);
	
	/* Start pressure measurement */
	*buf = 0x34 + (s->oversample << 6);
	if(_tw_write(0xF4, buf, 1) == 0) return(BMP_ERROR);
	
	/* Wait until measurement complete */
	switch(s->oversample)
	{
	case 0:  _delay_ms(5); break;
	case 1:  _delay_ms(8); break;
	case 2:  _delay_ms(14); break;
	default: _delay_ms(26); break;
	}
	
	/* Read the result */
	if(_tw_read(0xF6, buf, 3) == 0) return(BMP_ERROR);
	
	s->up = _INT24(buf, 0) >> (8 - s->oversample);
	
	return(BMP_OK);
}

int16_t bmp085_calc_temperature(bmp085_t *s)
{
	int32_t x1, x2;
	
	x1 = (((int32_t) s->ut - s->ac6) * s->ac5) >> 15;
	x2 = ((int32_t) s->mc << 11) / (x1 + s->md);
	s->b5 = x1 + x2;
	
	return((s->b5 + 8) >> 4);
}

int32_t bmp085_calc_pressure(bmp085_t *s)
{
	int32_t x1, x2, x3;
	int32_t b3, b6, p;
	uint32_t b4, b7;
	
	if(!s->b5) bmp085_calc_temperature(s);
	
	b6 = s->b5 - 4000;
	
	x1 = (s->b2 * ((b6 * b6) >> 12)) >> 11;
	x2 = (s->ac2 * b6) >> 11;
	x3 = x1 + x2;
	b3 = ((((int32_t) s->ac1 * 4 + x3) << s->oversample) + 2) / 4;
	
	x1 = (s->ac3 * b6) >> 13;
	x2 = (s->b1 * ((b6 * b6) >> 12)) >> 16;
	x3 = ((x1 + x2) + 2) >> 2;
	b4 = (s->ac4 * (uint32_t) (x3 + 32768)) >> 15;
	
	b7 = (s->up - b3) * (50000 >> s->oversample);
	if(b7 < 0x80000000) p = (b7 * 2) / b4;
	else p = (b7 / b4) * 2;
	
	x1 = (p >> 8) * (p >> 8);
	x1 = (x1 * 3038) >> 16;
	x2 = (-7357 * p) >> 16;
	return(p += (x1 + x2 + 3791) >> 4);
}

