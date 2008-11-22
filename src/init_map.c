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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "mapconfig_data.h"
#include "mapdrawing.h"



int lese_zahl(FILE *f)
{
  char tmp[20];
  fgets(tmp,19,f);
  return atoi(tmp);
}

void geo2utm(double x, double y, double *utmr, double *utmh)
{
  x=(x-9*3600)/(3600*180)*M_PI;
  y=y/(3600*180)*M_PI;
  *utmh=M_PI_2-atan(cos(x)/tan(y));
  *utmr=asin(sin(x)*cos(y));
  *utmh=(*utmh)*180/M_PI*3600;
  *utmr=(*utmr)*180/M_PI*3600;
}

int main()
{
  double utmr,utmh,utmr2,utmh2;
  double longg,latt,longg2,latt2;
  int x,y,x2,y2;
  double t;
  char buf[80];
  printf("Punkt 1 geogr: ");
  fgets(buf,sizeof(buf),stdin);
  buf[strcspn(buf,"\r\n")]=0;
  parse_coords(buf, &latt, &longg);
  geo2utm(longg,latt,&utmr,&utmh);
  printf("Punkt 1 pixel ");
  printf("x: ");
  x=lese_zahl(stdin);
  printf("y: ");
  y=lese_zahl(stdin);

  printf("Punkt 2 geogr: ");
  fgets(buf,sizeof(buf),stdin);
  buf[strcspn(buf,"\r\n")]=0;
  parse_coords(buf, &latt2, &longg2);
  geo2utm(longg2,latt2,&utmr2,&utmh2);
  printf("Punkt 2 pixel ");
  printf("x: ");
  x2=lese_zahl(stdin);
  printf("y: ");
  y2=lese_zahl(stdin);
  t=x2-x;
  printf("xfactor: %f\n",t/(utmr2-utmr));
  t=y2-y;
  printf("yfactor: %f\n",t/(utmh2-utmh));
  return 0;
}
