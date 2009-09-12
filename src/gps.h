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
#define INVALID_HEADING 1000.0
struct nmea_pointinfo {
  double longsec;
  double lattsec;
  double speed;
  double heading;
  int time;
  char state;
  int hdop;  /* 10 times the hdop */
  unsigned start_new: 1;
  unsigned single_point: 1;
};

struct t_punkt32 {
  int x,y;
  float speed;
  int hdop;
  double longg;
  double latt;
  int time;
  unsigned start_new:1;
  unsigned single_point:1;
};

struct gpsfile;

int save_gpx(const char  *f,GList *save_list);
void  save_nmea(FILE *f,GList *save_list);
void load_gps_line_noproj(const char *fname, GList **mll);
struct gpsfile *open_gps_file(int fd);
void close_gps_file(struct gpsfile *gpsf,int closefd);
int proc_gps_input(struct gpsfile *gpsf,
                   void (*gpsproc)(struct nmea_pointinfo *,void *), void *data);
int gps_writeback(struct gpsfile *gpsf, void *data, int len);
