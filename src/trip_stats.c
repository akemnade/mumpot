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
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include "geometry.h"
#include "gps.h"
#include "trip_stats.h"

struct trip_stats {
  double old_lon;
  double old_lat;
  double speed;
  double spdsum;
  double dist;
  double maxspeed;
};

struct trip_stats * trip_stats_new()
{
  struct trip_stats *ts=(struct trip_stats *)
    malloc(sizeof(struct trip_stats));
  ts->old_lon=0;
  ts->old_lat=0;
  ts->spdsum=0;
  ts->dist=0;
  ts->maxspeed=0;
  return ts;
}

void trip_stats_update(struct trip_stats *ts, struct nmea_pointinfo *nmea)
{
  if (nmea->speed > ts->maxspeed)
    ts->maxspeed = nmea->speed;
  if (!nmea->start_new) {
    ts->dist+=point_dist(nmea->longsec/3600.0,nmea->lattsec/3600.0,
			 ts->old_lon,ts->old_lat);
  }
  ts->speed = nmea->speed;
  ts->old_lon = nmea->longsec/3600.0;
  ts->old_lat = nmea->lattsec/3600.0;
}

void trip_stats_line(struct trip_stats *ts,
		     char *buf, int len,int current)
{
  if (current)
    snprintf(buf,len,"%.1f km/h %.2f km",ts->speed/1.852,
	     ts->dist/1000.0);
  else
    snprintf(buf,len,"%.2f km",
	     ts->dist/1000.0);
}
 
