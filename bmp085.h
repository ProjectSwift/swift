
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

#ifndef _BMP085_H
#define _BMP085_H

#define BMP_OK      (0)
#define BMP_ERROR   (1)

typedef struct {
	
	/* Calibration values */
	int16_t ac1, ac2, ac3;
	uint16_t ac4, ac5, ac6;
	int16_t b1, b2;
	int16_t /* mb, */ mc, md;
	
	/* Calculated from temperature */
	int32_t b5;
	
	/* Raw temperature and pressure */
	uint16_t ut;
	uint32_t up;
	
	/* Oversample parameter */
	uint8_t oversample;
	
} bmp085_t;

extern void bmp085_init();
extern int bmp085_read_calibration(bmp085_t *s);
extern int bmp085_sample(bmp085_t *s, uint8_t oversample);
extern int16_t bmp085_calc_temperature(bmp085_t *s);
extern int32_t bmp085_calc_pressure(bmp085_t *s);

#endif

