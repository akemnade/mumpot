/********************************************************************** 
 mumpot - Copyright (C) 2008 - Andreas Kemnade
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3, or (at your option)
 any later version.
             
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
***********************************************************************/
#ifndef K_STARTPOSITION_H
#define K_STARTPOSITION_H

void create_startposition_dialog(double lat, double lon,int zoomlvl);
void get_startposition(double *lat, double *lon,int *zoom);
void startposition_update_lastpos(int is_gps, double lat, double lon,
				  int zoomlvl);
#endif
