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
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include "gps.h"
#include "osm_parse.h"
#include "geometry.h"


double point_dist(double reflon, double reflat, double  lon,
		  double lat)
{
  double ldiff, la1, dist,t;
  ldiff=(lon-reflon)/180.0*M_PI;
  la1=reflat/180.0*M_PI;
  lat=lat/180.0*M_PI;
  t=0;
  if (abs(ldiff)<0.002) { /* approximate */
    ldiff=cos(la1)*ldiff;
    lat=la1-lat;
    dist=sqrt(ldiff*ldiff+lat*lat);
    return dist*6371221.0;
  }
  if (lon==reflon) {
    dist=abs(la1-lat); 
  } else { 
    t=sin(la1)*sin(lat)+cos(la1)*cos(lat)*cos(ldiff);
    dist=acos(t);
  }
  if (isnan(dist)) {
    fprintf(stderr,"nan: sin+sin+cos*cos*cos-1: %f\n",t-1); 
    fprintf(stderr,"%.8f/%.8f %.8f/%.8f\n",reflon,reflat,lon,lat);
    dist=0;
  }
  dist=6371221.0*dist;
  return dist;
}


/* split triangleinto two right angle triangles
 * h is the common edge (C is common node), d is part of c of the original triangle belonging to the triangle containing a
 *
 *  h^2 = b^2-d^2=a^2-(c-d)^2
 *  -> d = (c^2+b^2-a^2)/(2*c)
 *
 *  -> dzaehler = (x2-x1)^2+(y2-y1)^2+(x3-x1)^2+(y3-y1)^2-(x3-x2)^2-(y3-y2)^2
 *       = -2*x2*x1+2*x1^2-2*y2*y1+2*y1^2-2*x1*x3-2*y3*y1+2*x2*x3+2*y2*y3
 *       = 2(x1*(x1-x2-x3)+y1(y1-y2-y3)+x2*x3+y2*y3)
 *  -> d = (x1*(x1-x2-x3)+y1(y1-y2-y3)+x2*x3+y2*y3)/(sqrt((x2-x1)^2+(y2-y1)^2))
 */

double get_distance_r(double x1, double y1, double x2, double y2,
                     double x3, double y3)
{
  double dx, dy;
  double anenner;
  double azaehler;
  double d;
  dx=x2-x1;
  dy=y2-y1;
  if ((x1==x2)&&(y1==y2)) {
    return sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1));
  }
  anenner=sqrt(dx*dx+dy*dy);
  d = (x1*(x1-x2-x3)+y1*(y1-y2-y3)+x2*x3+y2*y3)/anenner;
  if (d<0) {
      return sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1));
  }
  if (d>anenner) {
      return sqrt((x3-x2)*(x3-x2)+(y3-y2)*(y3-y2));
  }
  azaehler=dx*(y3-y1)+dy*(x1-x3);
  return fabs(azaehler/anenner);
}

static double get_distance(struct t_punkt32 *firstp, struct t_punkt32 *lastp,
            struct t_punkt32 *refp) {
  return get_distance_r(firstp->longg, firstp->latt, lastp->longg, lastp->latt,
        refp->longg, refp->latt);
}

GList * simplify_lines(GList *lines, GList *linelast, double allowed_distance) {
    GList *max = NULL;
    GList *l = NULL;
    struct t_punkt32 *firstp;
    struct t_punkt32 *lastp;
    double maxdist=0;
    firstp=(struct t_punkt32 *)lines->data;
    lastp=(struct t_punkt32 *)linelast->data;
    if (firstp->time) {
      fprintf(stderr, " %d -> %d\n",firstp->time,lastp->time);
    }
    l = g_list_next(lines);
    if ((lines==linelast)||(l==linelast)) {
      return g_list_append(NULL,lines->data);
    }
    for(;l!=linelast; l=g_list_next(l)) {
      double dist = get_distance(firstp,lastp,(struct t_punkt32 *)l->data);
      if (dist > maxdist) {
        max=l;
        maxdist=dist;
      }
    }

    if ((!max)||(maxdist < allowed_distance)) {
        /* return g_list_append(g_list_append(NULL,firstp),lastp); */
        return g_list_append(NULL,firstp);
    } else {
        GList *head;
        GList *tail;
        fprintf(stderr,"splitting at %d\n",((struct t_punkt32 *)max->data)->time);
        head=simplify_lines(lines,max,allowed_distance);
        tail=simplify_lines(max,linelast,allowed_distance);
        return g_list_concat(head,tail);
    }
}

void move_node_between(double x1, double y1, double x2, double y2, struct osm_node *node)
{
  double x3=node->lon;
  double y3=node->lat;
  double dx, dy;
  double anenner;
  double d;
  dx=x2-x1;
  dy=y2-y1;
  if ((x1==x2)&&(y1==y2)) {
    node->lon=x1;
    node->lat=y2;
  } else {
    anenner=sqrt(dx*dx+dy*dy);
    d = (x1*(x1-x2-x3)+y1*(y1-y2-y3)+x2*x3+y2*y3)/anenner;
    if (d<0) {
      node->lon=x1;
      node->lat=y1;
    } else if (d>anenner) {
      node->lon=x2;
      node->lat=y2;
    } else {
      node->lon=x1+d/anenner*(x2-x1);
      node->lat=y1+d/anenner*(y2-y1);
    }
  }
 
}

#define IS_IN2(a,b,c) ((a<b)?-1:((a>c)?1:0))
int check_crossing(int x1, int y1,
		   int x2, int y2, int width, int height)
{
  int x1p,x2p,y1p,y2p;
  x1p=IS_IN2(x1,0,width);
  y1p=IS_IN2(y1,0,height);
  x2p=IS_IN2(x2,0,width);
  y2p=IS_IN2(y2,0,height);
  if ((!x1p)&&(!y1p))
    return 1;
  if ((!x2p)&&(!y2p))
    return 1;
  if ((x1p==x2p)&&(y1p==y2p))
    return 0;
  if ((x1p==x2p)&&(x1p))
    return 0;
  if ((y1p==y2p)&&(y1p))
    return 0;
  return 1;
}
