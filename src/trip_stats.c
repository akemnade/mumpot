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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "myintl.h"
#include "geometry.h"
#include "gps.h"
#include "trip_stats.h"
#ifdef USE_GTK2
#define DEG_CHR "\xc2\xb0"
#else
#define DEG_CHR "°"
#endif

#define MAX_FIX_INTERVAL 5
#define MIN_ACTIVE_SPEED (3.0/1.852) /* 2km/h */

struct trip_stats {
  double old_lon;
  double old_lat;
  double speed;
  double spdsum;
  double dist;
  double maxspeed;
  int old_time;
  int travel_time; 
  GtkWidget *trp_stat_win;
  GtkWidget *spdlabel;
  GtkWidget *maxspdlabel;
  GtkWidget *distlabel;
  GtkWidget *coordlabel;
  GtkWidget *averagelabel;
  GtkWidget *traveledtimelabel; 
};

static int ts_delete(GtkWidget *w,
		      GdkEventAny *ev,
		      gpointer data)
{
  gtk_widget_hide(w);
  return TRUE;
}

struct trip_stats * trip_stats_new()
{
#ifdef USE_GTK2
  PangoAttrList *alist;
#endif
  GtkWidget *vbox;
  GtkWidget *label;
  struct trip_stats *ts=(struct trip_stats *)
    malloc(sizeof(struct trip_stats));
  ts->old_lon=0;
  ts->old_lat=0;
  ts->spdsum=0;
  ts->dist=0;
  ts->maxspeed=0;
  ts->travel_time=0;
  ts->trp_stat_win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  vbox=gtk_vbox_new(FALSE,0);
  label=gtk_label_new(_("Speed:"));
  gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
  ts->spdlabel=gtk_label_new("0 km/h");

#ifdef USE_GTK2
  alist=gtk_label_get_attributes(GTK_LABEL(ts->spdlabel));
  if (!alist)
    alist=pango_attr_list_new();
  pango_attr_list_change(alist,pango_attr_size_new(40*PANGO_SCALE));
  gtk_label_set_attributes(GTK_LABEL(ts->spdlabel),alist);
#endif

  gtk_box_pack_start(GTK_BOX(vbox),ts->spdlabel,FALSE,FALSE,0);
  label=gtk_label_new(_("Distance:"));
  gtk_box_pack_start(GTK_BOX(vbox),label,FALSE,FALSE,0);
  ts->distlabel=gtk_label_new("0 km");

#ifdef USE_GTK2
  alist=gtk_label_get_attributes(GTK_LABEL(ts->distlabel));
  if (!alist)
    alist=pango_attr_list_new();
  pango_attr_list_change(alist,pango_attr_size_new(40*PANGO_SCALE));
  gtk_label_set_attributes(GTK_LABEL(ts->distlabel),alist);
#endif

  gtk_box_pack_start(GTK_BOX(vbox),ts->distlabel,FALSE,FALSE,0);
  ts->maxspdlabel=gtk_label_new("max: 0km");
  gtk_box_pack_start(GTK_BOX(vbox),ts->maxspdlabel,FALSE,FALSE,0);
  ts->coordlabel=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(vbox),ts->coordlabel,FALSE,FALSE,0);
  ts->averagelabel=gtk_label_new(_("average:"));
  gtk_box_pack_start(GTK_BOX(vbox),ts->averagelabel,FALSE,FALSE,0);
  ts->traveledtimelabel=gtk_label_new(_("time traveled:\n"));
  gtk_box_pack_start(GTK_BOX(vbox),ts->traveledtimelabel,FALSE,FALSE,0);
  gtk_container_add(GTK_CONTAINER(ts->trp_stat_win),vbox);
  gtk_signal_connect(GTK_OBJECT(ts->trp_stat_win),"delete-event",
		     GTK_SIGNAL_FUNC(ts_delete),
		     ts);
  return ts;
}

void trip_stats_update(struct trip_stats *ts, struct nmea_pointinfo *nmea)
{
  char buf[80];
  int latti,longi;
  char ns='N';
  char ew='E';
  
  if (nmea->speed > ts->maxspeed)
    ts->maxspeed = nmea->speed;
  if (ts->speed > MIN_ACTIVE_SPEED) {
    int tdiff = nmea->time-ts->old_time;
    if (tdiff > MAX_FIX_INTERVAL) {
      tdiff=5;
    }
    ts->travel_time+=tdiff;
    ts->spdsum+=nmea->speed*(double)tdiff;
  }
  ts->old_time=nmea->time;
  if (!nmea->start_new) {
    ts->dist+=point_dist(nmea->longsec/3600.0,nmea->lattsec/3600.0,
			 ts->old_lon,ts->old_lat);
  }
  ts->speed = nmea->speed;
  ts->old_lon = nmea->longsec/3600.0;
  ts->old_lat = nmea->lattsec/3600.0;
  longi=(int)nmea->longsec;
  latti=(int)nmea->lattsec;
  if (latti<0) {
    ns='S';
    latti=-latti;
  }
  if (longi<0) {
    ew='W';
    longi=-longi;
  }
  snprintf(buf,sizeof(buf),"%0d" DEG_CHR "%02d'%02d''%c %0d"DEG_CHR"%02d'%02d''%c",
	   latti/3600,
	   (latti/60)%60,
	   latti%60,
	   ns,
	   longi/3600,
	   (longi/60)%60,
	   longi%60,
	   ew);
  gtk_label_set_text(GTK_LABEL(ts->coordlabel),buf);
  snprintf(buf,sizeof(buf),"%.1f km/h",ts->speed*1.852);
  gtk_label_set_text(GTK_LABEL(ts->spdlabel),buf);
  snprintf(buf,sizeof(buf),"%.2f km",ts->dist/1000.0);
  gtk_label_set_text(GTK_LABEL(ts->distlabel),buf);
  snprintf(buf,sizeof(buf),"max: %.1f km/h",ts->maxspeed*1.852);
  gtk_label_set_text(GTK_LABEL(ts->maxspdlabel),buf);
  snprintf(buf,sizeof(buf),_("average:\n%.1f km/h"),ts->spdsum/((double)ts->travel_time)*1.852);
  gtk_label_set_text(GTK_LABEL(ts->averagelabel),buf);
  snprintf(buf,sizeof(buf),_("travel time:\n%02d:%02d:%02d"),
	   ts->travel_time/3600,(ts->travel_time/60)%60,ts->travel_time%60);
  gtk_label_set_text(GTK_LABEL(ts->traveledtimelabel),
		     buf);
}

void trip_stats_line(struct trip_stats *ts,
		     char *buf, int len,int current)
{
  if (current)
    snprintf(buf,len,"%.1f km/h %.2f km",ts->speed*1.852,
	     ts->dist/1000.0);
  else
    snprintf(buf,len,"%.2f km",
	     ts->dist/1000.0);
}
 
void trip_stats_show(struct trip_stats *ts)
{
  gtk_widget_hide(ts->trp_stat_win);
  gtk_widget_show_all(ts->trp_stat_win);
  gdk_window_raise(ts->trp_stat_win->window);
}

void trip_stats_hide(struct trip_stats *ts)
{
  gtk_widget_hide(ts->trp_stat_win);
}
