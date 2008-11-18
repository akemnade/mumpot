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

#include <assert.h>
#include <string.h>
#include <glib.h>
#include <stdio.h>
#include "strasse.h"
#include "findpath.h"

static int p_fifo[32768];
static int readp,writep;

static void ways2fifo(int s)
{
  int i,j,l2,l;
  int dummy,dummy2;
  struct kreuzung *kr1;
  struct strasse *str;
  kr1=get_kr_id(s);
  if (!kr1)
    return;
  get_way_info(s,&l,&dummy);
  for(i=0;i<kr1->n;i++) {
    j=kr1->abzweigungen[i];
    if (j<0)
      continue;
    str=get_str_id(j);
    if (!str)
      continue;
    if (str->end_krid==kr1->id)
      j=str->anfang_krid;
    else if (str->anfang_krid==kr1->id)
      j=str->end_krid;
    else {
      printf("data incorrect: strid: %d krid: %d\n",str->id,kr1->id);
      continue;
    }
    if (j<0)
      continue;
    l2=str->len+l+1;
    get_way_info(j,&dummy,&dummy2);
    if ((dummy<0)||(l2<dummy)) {
      get_way_info(s,&dummy,&dummy2);
      if (dummy2==str->id) {
	printf("detected circle (strid: %d)\n",str->id);
	return;
      }
      put_way_info(j,l2,str->id);
      p_fifo[writep]=j;
      writep++;
      writep=writep&32767;
      assert(writep!=readp);
    }
  }
}

int findpath(int id_start)
{
  if (id_start<0)
    return 0;
  readp=0;
  writep=0;
  memset(p_fifo,-1,sizeof(p_fifo));
  reset_way_info();
  put_way_info(id_start,0,-1);
  ways2fifo(id_start);
  while(readp!=writep) {
    ways2fifo(p_fifo[readp]);
    readp++;
    readp=readp&32767;
  }
  return 1;
}
