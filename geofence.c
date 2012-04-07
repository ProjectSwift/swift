/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Nigel Smart <nigel@projectswift.co.uk>            */
/* And Philip Heron <phil@sanslogic.co.uk>                               */
/* Additional Help from Jon Sowman & Adam Greig of Cambridge University  */
/* Spaceflight (www.cusf.co.uk)                                          */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the          */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program. If not, see <http://www.gnu.org/licenses/>.  */

#include <stdint.h>

static int32_t geofence[50*2] =
{ 
489924043,-36103603,
488138903,-22610257,
491901282,-13252978,
496351217,-10050508,
501037998,11279993,
507722758,16260619,
513189809,25152367,
520765597,35119373,
530454023,42970152,
539001634,49909694,
546830552,44727668,
551230292,34601012,
554336032,29904906,
558418741,26487290,
566806014,24488896,
575931754,16723936,
584164868,11759240,
589641150,4293681,
601679345,-5502415,
602106803,-22911888,
598068675,-31797350,
594741753,-52432139,
590767750,-112927263,
586042329,-125682437,
573560679,-123463450,
568484965,-110679665,
567960819,-94029714,
565955176,-84036460,
562488969,-74621136,
558888574,-68508447,
554623831,-62193300,
554506766,-75560492,
552283624,-86434228,
546950633,-86430795,
539200807,-75704420,
538233232,-70009827,
537741074,-59159398,
534700525,-58290520,
532493620,-58085376,
529591630,-58389442,
522959865,-58365745,
511047160,-84269381,
503317712,-84353354,
496274569,-84025146,
486210038,-81558942,
488754195,-70486501,
490428397,-60384346,
492957603,-52417139,
493083262,-41420783,
489924043,-36103603,
};

static int pointinpoly(int32_t *poly, int points, int32_t x, int32_t y)
{
        int32_t *p = poly;
        int32_t *l = &poly[points * 2 - 2];
        int c = 0;
 
        for(; points; points--, l = p, p += 2)
        {
                if(y < p[1] && y < l[1]) continue;
                if(y >= p[1] && y >= l[1]) continue;
                if(x < p[0] + (l[0] - p[0]) * (y - p[1]) / (l[1] - p[1])) continue;
 
                c = !c;
        }
 
        return(c);
}

int geofence_test(int32_t lat, int32_t lon)
{
	return(pointinpoly(geofence, 50, lat, lon));
}
