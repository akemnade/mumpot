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
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <math.h>
#include "png_io.h"
#include "png2ps.h"
#include "strasse.h"
#include "findpath.h"
#include "mapconfig_data.h"
#include "gps.h"
#include "mapdrawing.h"
#include "geometry.h"

static void write_osm_xml_head(FILE *f)
{
  fprintf(f,"<?xml version='1.0' encoding='UTF-8'?>\n<osm version='0.5' generator='nmea2osm'>\n");
}

static void write_osm_xml_foot(FILE *f)
{
  fprintf(f,"</osm>\n");
}

static void write_osm_xml(FILE *f, GList *lines, int *start_nodenum)
{
  int pointcount = g_list_length(lines);
  GList *l;
  int nodenum=*start_nodenum;
  int i;

  for(l=g_list_first(lines);l;l=g_list_next(l),nodenum--) {
    struct t_punkt32 *p = (struct t_punkt32 *)l->data;
    fprintf(f,"<node id='%d' visible='true' lat='%f' lon='%f' />\n",
            nodenum,p->latt/3600.0, p->longg/3600.0);
  }
  fprintf(f,"<way id='%d' visible='true'>\n",nodenum);
  for(i=0;i<pointcount;i++) {
    fprintf(f,"  <nd ref='%d' />\n",(*start_nodenum)-i);
  }
  fprintf(f,"  <tag k='created_by' v='nmea2osm' />\n</way>\n");
  nodenum--;
  *start_nodenum=nodenum;
  
}

int main(int argc, char **argv)
{
  double precision=1;
  GList *lines=NULL;
  GList *simp_lines;
  int nodenum=-1;
  int mergeosm=0;
  int o;
  if (argc == 2) {
    if (!strcmp(argv[1],"--version")) {
      printf("%s (%s %s)\n""Copyright (C) 2008 Andreas Kemnade\n"
      "This is free software.  You may redistribute copies of it under the terms of\n"
      "the GNU General Public License version 3 or any later version <http://www.gnu.org/licenses/gpl.html>\n"
      "There is NO WARRANTY, to the extent permitted by law.\n",argv[0],PACKAGE,VERSION);
      return 0;
    } else if (!strcmp(argv[1],"--help")) {
      printf("Usage: %s [-p mindist] [-n negnodenum] [-o] nmea-files ...\n\n"
	     "Simplifies NMEA data\n\n"
"  -p mindist gives the limit for simplification, the resulting trace does not differ mindist arc seconds from the original.\n"
 "  -o switch to osm output: the trace is converted to an osm way, with node numbers starting by -1 or if specified by negnodenum\n",argv[0]);

      return 0;
    }
  }
  while(0<(o=getopt(argc,argv,"hop:n:"))) {
    switch(o) {
    case 'p':
      precision=atof(optarg); break;
    case 'n':
      nodenum=-atoi(optarg); break;
    case 'o':
      mergeosm=1;  break;
    default:
      fprintf(stderr,"Usage: %s [-p mindist] [-n negnodenum] [-o] nmea-files ...\n",argv[0]);
      return 1;
    }
  }
  if (!mergeosm) {
    load_gps_line_noproj(argv[optind],&lines);
    if (!lines)
        return 1;
    simp_lines = simplify_lines(g_list_first(lines),g_list_last(lines),atof(argv[2]));
    simp_lines = g_list_append(simp_lines,g_list_last(lines)->data);
    save_nmea(stdout,simp_lines);
  } else {
    int i;
    write_osm_xml_head(stdout);
    for(i=optind;i<argc;i++) {
      lines=NULL;
      load_gps_line_noproj(argv[i],&lines);
      if (!lines)
          continue;
      simp_lines = simplify_lines(g_list_first(lines),g_list_last(lines),atof(argv[2]));
      simp_lines = g_list_append(simp_lines,g_list_last(lines)->data);
      write_osm_xml(stdout,simp_lines,&nodenum);
    }
    write_osm_xml_foot(stdout);
  }
 
  return 1;
}
