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
#include <getopt.h>

#include "png_io.h"
#include "png2ps.h"
#include "strasse.h"
#include "findpath.h"
#include "mapconfig_data.h"
#include "gps.h"
#include "mapdrawing.h"

#define OPT_XOFF 1
#define OPT_YOFF 2
#define OPT_W 4
#define OPT_H 8

GdkColor speedcolor[256];

static GMainLoop *mainloop;

static gboolean poll_requests(gpointer data)
{
  if (tile_requests_processed()) {
    g_main_quit(mainloop);
    return FALSE;
  }
  return TRUE;
}

int main(int argc, char **argv)
{
  double x;
  double y;
  int xoffset;
  int yoffset;
  int width,height;
  int fd;
  int o_found=0;
  int output_ps=0;
  int output_arrow=0;
  int zoomlevel=-1;
  double latt, longg;
  struct pixmap_info *pinfo;
  int o;
  char cfgfile[256];
  char *nmea_file=NULL;
  char *configfile=cfgfile;
  snprintf(cfgfile,sizeof(cfgfile),"%s/.map.conf",getenv("HOME"));
  /* if (argc!=6) {
    printf("Usage: %s configfile pngfile coords width height",
	    argv[0]);
    return 1;
    } */
  if (argc == 2) {
    if (!strcmp(argv[1],"--help")) {
      printf("Usage: %s -x xoffset -y yoffset -w width -h height [-c configfile] [-p] [-n nmeafile] [-z zoomlevel] [.a] filename coords\n\n"
      "Extracts one rectangle from the tile cache.\n\n"
      "coords are the geographical coordinates in the form like \"49°22'33''N 7°11'44''E\" \n"
      "-x/-y/-w/-h specify the area around the coords to extract in pixels\n"
      "-p turns on eps output instead of png output\n"
      "-c gives the name of a configfile for mumpot (like /usr/bin/mumpot-tah)\n"
      "-n gives an nmea track which is drawn on top of the map in eps mode\n"
      "-z zoomlevel sets the zoomlevel\n"
      "-a draws an arrow to the place specified by coords\n",argv[0]);
       return 0;
    } else if (!strcmp(argv[1],"--version")) {
      printf("%s (%s %s)\n""Copyright (C) 2008 Andreas Kemnade\n"
      "This is free software.  You may redistribute copies of it under the terms of\n"
      "the GNU General Public License version 3 or any later version <http://www.gnu.org/licenses/gpl.html>\n"
      "There is NO WARRANTY, to the extent permitted by law.\n",argv[0],PACKAGE,VERSION);
      return 0;
    }
  }
  while(0<(o=getopt(argc,argv,"az:x:w:y:h:c:pn:"))) {
    switch(o) {
    case 'x': xoffset=atoi(optarg); o_found |= OPT_XOFF; break;
    case 'y': yoffset=atoi(optarg); o_found |= OPT_YOFF; break;
    case 'w': width=atoi(optarg); o_found |= OPT_W; break;
    case 'h': height=atoi(optarg); o_found |= OPT_H; break;
    case 'c': configfile=strdup(optarg); break;
    case 'p': output_ps=1; break;
    case 'n': nmea_file=strdup(optarg);  break;
    case 'a': output_arrow=1; break;
    case 'z': zoomlevel=atoi(optarg);
    }
    
  }
  if ((argc-optind != 2)&&(o_found != (OPT_XOFF | OPT_YOFF | OPT_W | OPT_H))) {
    fprintf(stderr,"missing options\n");
    return 1;
  }
  /* initialize map data */
  parse_mapconfig(configfile);
  calc_mapoffsets();
  if (zoomlevel > 0) {
    globalmap.zoomfactor=zoomlevel;
    calc_mapoffsets();
  }
  parse_coords(argv[optind+1],&latt,&longg);
  /* convert coordinates to x y */
  geosec2point(&x, &y, longg, latt);
  /* read the map and copy the rectangle */
  pinfo=get_map_rectangle((int)x+xoffset,(int)y+yoffset,width, height);
  if (!tile_requests_processed()) {
    g_timeout_add(1000,poll_requests,NULL);
    mainloop=g_main_new(TRUE);
    g_main_run(mainloop);
    if (pinfo)
      free_pinfo(pinfo);
    pinfo=get_map_rectangle((int)x+xoffset,(int)y+yoffset,width, height);
  }
  if (pinfo) {
    char pshead[512];
    if (output_ps) {
      fd=open(argv[optind],O_WRONLY|O_CREAT|O_TRUNC,0666);
      snprintf(pshead,sizeof(pshead),"%%!PS-Adobe-2.0 EPSF-3.0\n%%%%BoundingBox: 0 0 %d %d\n%%%%EndComments\ngsave 0 %u translate\n",width,height,height);
      write(fd,pshead,strlen(pshead));
      if (pinfo->row_len > 0)
      colordump_ps(fd,pinfo,0);
      {
	char *t="1 -1 scale\n"
	  "/dup2 { dup 3 2 roll dup 4 1 roll exch } bind def\n"
	  "/arrowlen { 30 } bind def\n"
	  "/pol2eu { dup2 exch cos mul 3 1 roll exch sin mul } bind def\n"
	  "/arrowdeg { 15 } bind def\n"
	  "/arrowheadr { dup2 moveto 180 arrowdeg sub arrowlen pol2eu rlineto stroke moveto 180 arrowdeg add arrowlen pol2eu rlineto stroke } bind def\n";

	write(fd,t,strlen(t));
	if (nmea_file) {
	  GList *lines=NULL;
	  load_gps_line(nmea_file,&lines);
	  draw_marks_to_ps(lines,(int)x+xoffset,(int)y+yoffset,width,height,fd);
	}
	if (output_arrow) {
	  snprintf(pshead,sizeof(pshead)," 0 0 0 setrgbcolor 8 setlinewidth %d %d dup2 exch -70 add exch moveto dup2 lineto stroke arrowheadr\n",
		   -xoffset,-yoffset);
	  write(fd,pshead,strlen(pshead));
	}
	t="grestore showpage\n";
	write(fd,t,strlen(t));
      }
      close(fd);
    } else {
      if (pinfo->row_len > 0)
      save_pinfo(argv[optind],pinfo);
    }
  }
  return 0;
}
