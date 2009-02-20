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

#ifndef K_TRIP_STATS_H
#define K_TRIP_STATS_H
struct trip_stats;

struct trip_stats * trip_stats_new();
void trip_stats_update(struct trip_stats *ts, struct nmea_pointinfo *nmea);
void trip_stats_line(struct trip_stats *ts,
		     char *buf, int len,int current);

#endif
