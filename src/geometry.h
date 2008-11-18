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
#ifndef K_GEOMETRY_H
#define K_GEOMETRY_H
struct osm_node;
GList * simplify_lines(GList *lines, GList *linelast, double allowed_distance);
void move_node_between(double x1, double y1, double x2, double y2, struct osm_node *node);
double point_dist(double reflon, double reflat, double  lon,
		  double lat);
int check_crossing(int x1, int y1,
		   int x2, int y2, int width, int height);
#endif
