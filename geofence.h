/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Nigel Smart <nigel@projectswift.co.uk>            */
/* And Philip Heron<phil@sanslogic.co.uk>				 */
/* Additional Help from Jon Sowman & Adam Greig of Cambridge University  */
/* Spaceflight (www.cusf.co.uk)                                          */
/*									 */
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
#ifndef __GEOFENCE_H__
#define __GEOFENCE_H__

#include <stdio.h>
#include <stdint.h>

extern int geofence_test(int32_t lat, int32_t lon);

#endif /*__GEOFENCE_H__ */
