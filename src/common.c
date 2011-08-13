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

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

time_t gmmktime(struct tm *tm)
{
  time_t t,tref;
  struct tm *tmref;
  t=mktime(tm);  /* t=tm-tz */
  tmref=gmtime(&t); /* tmref=t=tm-tz */
  tref=mktime(tmref); /* tref=tmref-tz=tm-2*tz=t-tz */
  /* tz=t-tref , tm=t+tz=t+(t-tref)*/
  t=t*2-tref;
  return t;
}

time_t parse_xml_time(const char *t)
{
  struct tm tm;
  memset(&tm,0,sizeof(tm));
  sscanf(t,
	 "%04d-%02d-%02dT%02d:%02d:%02d",
	 &tm.tm_year,&tm.tm_mon,&tm.tm_mday,
	 &tm.tm_hour,&tm.tm_min,&tm.tm_sec);
  tm.tm_year-=1900;
  tm.tm_mon--;
  return gmmktime(&tm);
}
