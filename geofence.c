/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Nigel Smart <nigel@projectswift.co.uk>            */
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
#include <stdio.h>

float geofence[28*2] =

{ 
 49.338875,-7.014206,
 51.341161,-5.820586,
 53.598303,-5.700217,
 53.788614,-7.128783,
 53.730822,-8.456011,
 53.983425,-8.771233,
 54.786247,-9.839442,
 56.895217,-8.716811,
 58.769241,-8.011142,
 59.773364,-3.983625,
 61.071428,0.954028,
 59.979922,1.354522,
 58.441186,0.023664,
 57.285814,0.230053,
 56.524919,1.824814,
 56.201344,0.129853,
 55.293828,0.194903,
 54.095494,0.916847,
 53.455828,1.658539,
 53.231033,2.824433,
 52.587586,3.126731,
 51.152375,1.929497,
 50.255347,0.344814,
 49.922331,-1.055233,
 49.619639,-3.508547,
 49.543289,-4.647119,
 49.385614,-5.852136,
 49.359211,-6.152422,
};

int pointinpoly(float *poly, int points, float x, float y)
{
        float *p = poly;
        float *l = &poly[points * 2 - 2];
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

int main(void)
	{
		if(pointinpoly(geofence, 28, 49.521481, -6.588242))
		
		//if(pointinpoly(geofence, 28, 65.1255, -5.3669))
		{
		printf("Geofence Active APRS Disabled\n");
		}
		
		else 

		{
		printf("Outside Geofence Turning On APRS\n");
		}
	}
