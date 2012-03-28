
/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Philip Heron <phil@sanslogic.co.uk>               */
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
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "rtty.h"
#include "gps.h"

int main(void)
{
	uint32_t count = 0;
	int32_t lat, lon, alt;
	char msg[100];
	
	rtx_init();
	gps_setup();
	
	sei();
	
	/* Enable the radio and let it settle */
	rtx_enable(1);
	_delay_ms(1000);
	
	rtx_string_P(PSTR(RTTY_CALLSIGN " starting up"));
	
	while(1)
	{
		if(!gps_get_pos(&lat, &lon, &alt))
		{
			rtx_string_P(PSTR("$$" RTTY_CALLSIGN ",No or invalid GPS response\n"));
			continue;
		}
		
		rtx_wait();
		snprintf(msg, 100, "$$%s,%li,%li,%li,%li\n",
			RTTY_CALLSIGN, count++, lat, lon, alt);
		rtx_string(msg);
	}
}

