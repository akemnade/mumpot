/********************************************************************** 
 mumpot - Copyright (C) 2008 - Andreas Kemnade
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
             
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
***********************************************************************/

#ifndef K_MAPCONFIG_DATA_H
#define K_MAPCONFIG_DATA_H
struct t_map {
  char *name;
  double reflatt;
  double reflong;
  int refx;
  int refy;
  double zerolatt;
  double zerolong;
  double xfactor;  /* relative to the global map factors*/
  double yfactor;
  int xoffset;     
  int yoffset;
  int tilewidth;
  int tileheight;
  char *filepattern;
  char *url;
  struct t_map *next;
};
struct t_globalmap {
  double zerolatt;     /* geographic coordinates (seconds) of the point (0,0) */
  double zerolong;
  double xoffset;      /* utm offsets in radians (calculated during init */
  double yoffset;
  int startlatt;       /* start point */
  int startlong;
  int is_utm;
  double xfactor;  
  double yfactor;
  int fullwidth;
  int fullheight;
  int zoomable;
  int zoomfactor;
  int zoomshift;
  double orig_xfactor;
  double orig_yfactor;
  char *startplace;    /* start place name */
  struct t_map *first;
  struct t_map *last;
  GList *placefilelist;
};

struct t_mark_rect {
  double longg;
  double latt;
  int dim;
  int width_gt_height;
};

extern int kconflex();
extern struct t_globalmap globalmap;
int parse_mapconfig(const char *fname);
int parse_coords(char *coordstr, double *lattitude, double *longitude);
struct t_map *map_new(char *name);

#endif
