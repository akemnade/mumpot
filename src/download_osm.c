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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <math.h>
#include "gps.h"

double minlattsec=90*3600;
double maxlattsec=-90*3600;
double minlongsec=180*3600;
double maxlongsec=-180*3600;


static void gps_minmax(struct nmea_pointinfo *nmea, void  *data)
{
  if (nmea->lattsec>maxlattsec)
    maxlattsec=nmea->lattsec;
  if (nmea->lattsec<minlattsec)
    minlattsec=nmea->lattsec;
  if (nmea->longsec>maxlongsec)
    maxlongsec=nmea->longsec;
  if (nmea->longsec<minlongsec)
    minlongsec=nmea->longsec;
}

int main(int argc, char **argv)
{
  int fd;
  struct gpsfile *gpsf;
  char url[256];
  char *apibase;
  if (argc==2) {
    if (!strcmp("--help",argv[1])) {
      printf("Usage: %s nmeafile marginsec\n\n"
	     "downloads osm data\n\n"
	     "A bounding box is constructed around nmeafile with an additional margin of marginsec arc seconds\n"
	     "The downloaded osm data is written to stdout\n", argv[0]);
    } else if (!strcmp("--version",argv[1])) {
      printf("%s (%s %s)\n""Copyright (C) 2008 Andreas Kemnade\n"
	     "This is free software.  You may redistribute copies of it under the terms of\n"
	     "the GNU General Public License version 3 or any later version <http://www.gnu.org/licenses/gpl.html>\n"
	     "There is NO WARRANTY, to the extent permitted by law.\n",argv[0],PACKAGE,VERSION);
      return 0;
    }
  }
  if (argc!=3) {
    fprintf(stderr,"%s nmeafile marginsec\n",argv[0]);
    return 1;
  }
  
  
  fd=open(argv[1],O_RDWR);
  if (fd<0) {
    fprintf(stderr,"cannot open %s\n",argv[1]);
  }
  gpsf=open_gps_file(fd);
  while(0<proc_gps_input(gpsf,gps_minmax,NULL));
  close_gps_file(gpsf,1);
  apibase=getenv("OSMAPIBASE");
  if (!apibase)
      apibase="http://www.openstreetmap.org/api/0.6";
  snprintf(url,sizeof(url),"wget -O - '%s/map?bbox=%f,%f,%f,%f'",apibase,
	   minlongsec/3600,minlattsec/3600,maxlongsec/3600,maxlattsec/3600);
  fprintf(stderr,"executing %s\n",url);
  system(url);
  return 0;   
}
