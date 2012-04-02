/* Project Swift - High altitude balloon flight software                 */
/*=======================================================================*/
/* Copyright 2010-2012 Nigel Smart <nigel@projectswift.co.uk>            */
/* And Philip Heron<phil@sanslogic.co.uk>				 */
/*                                                                       */
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
#ifndef __GEOFENCE_H__
#define __GEOFENCE_H__

#include <stdio.h>
#include <stdint.h>

extern uint32_t pointinpoly(int32_t *poly, int points, int32_t x, int32_t y);

#endif /*__GEOFENCE_H__ */
