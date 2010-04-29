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
#include <gtk/gtk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "myintl.h"
#include "gui_common.h"
#include "mapconfig_data.h"
#include "startposition.h"

struct startposition_dialog {
  GtkWidget *win;
  GtkWidget *lonentry;
  GtkWidget *latentry;
  GtkWidget *currentposbut;
  GtkWidget *zoomlvl;
  GtkObject *zoomlvladj;
  GtkWidget *mapdefaultcheck;
  GtkWidget *entrycheck;
  GtkWidget *restart_where_closedcheck;
  GtkWidget *restart_last_gpsfixcheck;
  GtkWidget *errorlabel;
  GtkWidget *possettable;
  double lat_now;
  double lon_now;
  int zoomlvlval;
};

typedef enum {
  MAPDEFAULT,
  FIXEDPOS,
  LAST_POS,
  LAST_GPS
} startpos_t;

static void close_startposition_dialog(struct startposition_dialog *spd)
{
  gtk_widget_destroy(spd->win);
  free(spd);
}

static void delete_cb(GtkWidget *w, GdkEventAny *ev, gpointer data)
{
  close_startposition_dialog((struct startposition_dialog *)data);
}

static void cancel_cb(GtkWidget *w, gpointer data)
{
  close_startposition_dialog((struct startposition_dialog *)data);
}

static void set_startpos_cfg(startpos_t st)
{
  char *st_str;
  st_str="mapdefault";
  switch(st) {
  case FIXEDPOS: st_str="fixed"; break;
  case LAST_POS: st_str="lastpos"; break;
  case LAST_GPS: st_str="lastgps"; break;
  default: break;
  }
  cfg_set_string("startposmode",st_str);
}

static int save_startposition_cfg(struct startposition_dialog *spd)
{
  startpos_t st;
  char *stposstr;
  int ret=1;
  char *lontext,*lattext;
  st=MAPDEFAULT;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spd->entrycheck)))
    st=FIXEDPOS;
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spd->restart_where_closedcheck)))
    st=LAST_POS;
  else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spd->restart_last_gpsfixcheck)))
    st=LAST_GPS;

  if (st==FIXEDPOS) {
    double lon,lat;
    char buf[10];
    lon=lat=0;
    lontext=gtk_editable_get_chars(GTK_EDITABLE(spd->lonentry),0,-1);
    lattext=gtk_editable_get_chars(GTK_EDITABLE(spd->latentry),0,-1);
    stposstr=g_strdup_printf("%s %s",lattext,lontext);
    if (parse_coords(stposstr,&lat,&lon)) {
      cfg_set_string("startpos",stposstr);
      snprintf(buf,sizeof(buf),"%d",(int)GTK_ADJUSTMENT(spd->zoomlvladj)->value);
      cfg_set_string("zoomlvl",buf);
    } else
      ret=0;
    free(stposstr);
    free(lontext);
    free(lattext);

  }
  if (ret) {
    set_startpos_cfg(st);
    cfg_write_out();
  }
  return ret;
}

static void ok_cb(GtkWidget *w, gpointer data)
{
  struct startposition_dialog *spd=
    (struct startposition_dialog *)data;
  if (save_startposition_cfg(spd)) {
    close_startposition_dialog(spd);
  } else {
    gtk_label_set_text(GTK_LABEL(spd->errorlabel),
		       _("invalid coordinates!"));
  }
}

static void radiobutton_updater_cb(GtkWidget *w, gpointer data)
{
  struct startposition_dialog *spd=(struct startposition_dialog *)
    data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spd->entrycheck))) {
    gtk_widget_set_sensitive(spd->possettable,1);
    gtk_widget_show(spd->errorlabel);
  } else {
    gtk_widget_set_sensitive(spd->possettable,0);
    gtk_widget_hide(spd->errorlabel);
  }
}

startpos_t get_startpos_cfg()
{
  char *spstr=cfg_get_string("startposmode");
  if (!spstr)
    return MAPDEFAULT;
  if (!strcmp(spstr,"fixed"))
    return FIXEDPOS;
  else if (!strcmp(spstr,"lastpos"))
    return LAST_POS;
  else if (!strcmp(spstr,"lastgps"))
    return LAST_GPS;
  else
    return MAPDEFAULT;
}

void get_startposition(double *lat, double *lon, int *zoom)
{
  if (get_startpos_cfg()!=MAPDEFAULT) {
    char *spstr=cfg_get_string("startpos");
    if (spstr) {
      parse_coords(spstr,lat,lon);
      spstr=cfg_get_string("zoomlvl");
      if (spstr) {
	int zl=atoi(spstr);
	if (zl!=0)
	  *zoom=zl;
      }
    }
  }
}

static void set_position_entry(struct startposition_dialog *spd,
			       double lat, double lon, int zoom)
{
  char buf[80];
  int lpos;
  make_nice_coord_string(buf,sizeof(buf),lat,lon);
  lpos=strcspn(buf,"SN");
  if (buf[lpos]!=0) {
    lpos++;
    if (buf[lpos]) {
      buf[lpos]=0;
      lpos++;
      gtk_entry_set_text(GTK_ENTRY(spd->latentry),buf);
      gtk_entry_set_text(GTK_ENTRY(spd->lonentry),buf+lpos);
      if (zoom!=0) {
	gtk_adjustment_set_value(GTK_ADJUSTMENT(spd->zoomlvladj),(double)zoom);
      }
    }
  }
}

static void load_startposcfg(struct startposition_dialog *spd)
{
  startpos_t spos=get_startpos_cfg();
  switch(spos) {
  case MAPDEFAULT:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spd->mapdefaultcheck),
				 1);
    break;
  case FIXEDPOS:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spd->entrycheck),1);
    break;
  case LAST_GPS:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spd->restart_last_gpsfixcheck),1);
    break;
  case LAST_POS:
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(spd->restart_where_closedcheck),1);
    break;
  }
  if (spos==FIXEDPOS) {
    double lon,lat;
    int zoom;
    lon=lat=0;
    get_startposition(&lat,&lon,&zoom);
    set_position_entry(spd,lat,lon,zoom);
  }
  radiobutton_updater_cb(NULL,spd);
}

void startposition_update_lastpos(int is_gps, double lat, double lon,
				  int zoomlvl)
{
  startpos_t spos=get_startpos_cfg();
  char buf[80];
  if (((spos==LAST_GPS)&&is_gps)||((spos==LAST_POS)&&!is_gps)) {
    make_nice_coord_string(buf,sizeof(buf),lat,lon);
    cfg_set_string("startpos",buf);
    snprintf(buf,sizeof(buf),"%d",zoomlvl);
    cfg_set_string("zoomlvl",buf);
  }
  cfg_write_out();
}

static void set_to_current(GtkWidget *w, gpointer data)
{
  struct startposition_dialog *spd=(struct startposition_dialog *)data;
  set_position_entry(spd,spd->lat_now,spd->lon_now,spd->zoomlvlval);
}

static void check_coords(GtkWidget *w, gpointer data)
{
  double lon,lat;
  struct startposition_dialog *spd=(struct startposition_dialog *)
    data;
  char *lontext=gtk_editable_get_chars(GTK_EDITABLE(spd->lonentry),0,-1);
  char *lattext=gtk_editable_get_chars(GTK_EDITABLE(spd->latentry),0,-1);
  char *ll=g_strdup_printf("%s %s",lattext,lontext);
  if (parse_coords(ll,&lat,&lon)) {
    gtk_label_set_text(GTK_LABEL(spd->errorlabel),"");
  } else {
    gtk_label_set_text(GTK_LABEL(spd->errorlabel),_("invalid coordinates!"));
  }
}

void create_startposition_dialog(double lat, double lon,int zoomlvl)
{
  struct startposition_dialog *spd=g_new0(struct startposition_dialog,1);
  GtkWidget *table;
  GtkWidget *lab;
  GtkWidget *but;
  static GtkRcStyle *st;
  static GdkColor red;
  static int red_initialized=0;
  spd->win=gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(spd->win),_("Set the startposition"));
  spd->mapdefaultcheck=gtk_radio_button_new_with_label(NULL,_("use map default"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->mapdefaultcheck,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(spd->mapdefaultcheck),"clicked",
		     GTK_SIGNAL_FUNC(radiobutton_updater_cb),spd);
  spd->entrycheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(spd->mapdefaultcheck),_("use given coordinats"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->entrycheck,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(spd->entrycheck),"clicked",
		     GTK_SIGNAL_FUNC(radiobutton_updater_cb),spd);
  table=gtk_table_new(4,2,FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     table,TRUE,TRUE,0);
  lab=gtk_label_new(_("Lattitude"));
  gtk_table_attach(GTK_TABLE(table),lab,0,1,0,1,GTK_FILL,
		   GTK_EXPAND|GTK_FILL,0,0);
  lab=gtk_label_new(_("Longitude"));
  gtk_table_attach(GTK_TABLE(table),lab,0,1,1,2,GTK_FILL,
		   GTK_EXPAND|GTK_FILL,0,0);
  spd->lonentry=gtk_entry_new();
  spd->latentry=gtk_entry_new();
  gtk_table_attach(GTK_TABLE(table),spd->latentry,1,2,0,1,GTK_EXPAND|GTK_FILL,
		   GTK_EXPAND|GTK_FILL,0,0);
  gtk_table_attach(GTK_TABLE(table),spd->lonentry,1,2,1,2,GTK_EXPAND|GTK_FILL,
		   GTK_EXPAND|GTK_FILL,0,0);
  gtk_signal_connect(GTK_OBJECT(spd->lonentry),"changed",
		     GTK_SIGNAL_FUNC(check_coords),spd);
  gtk_signal_connect(GTK_OBJECT(spd->latentry),"changed",
		     GTK_SIGNAL_FUNC(check_coords),spd);
  lab=gtk_label_new(_("zoomlevel"));
  gtk_table_attach(GTK_TABLE(table),lab,0,1,2,3,GTK_FILL,
		   GTK_EXPAND|GTK_FILL,0,0);
 
  spd->zoomlvladj=gtk_adjustment_new(6,1,17,1,1,1);
  spd->zoomlvl=gtk_hscale_new(GTK_ADJUSTMENT(spd->zoomlvladj));
  gtk_scale_set_digits(GTK_SCALE(spd->zoomlvl),0);
  gtk_table_attach(GTK_TABLE(table),spd->zoomlvl,1,2,2,3,GTK_FILL|GTK_EXPAND,
		   GTK_FILL|GTK_EXPAND,0,0);
  spd->currentposbut=gtk_button_new_with_label(_("set to current position"));
  gtk_signal_connect(GTK_OBJECT(spd->currentposbut),
		     "clicked",GTK_SIGNAL_FUNC(set_to_current),spd);
  gtk_table_attach(GTK_TABLE(table),spd->currentposbut,1,2,3,4,
		   GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,0,0);
  
  spd->errorlabel=gtk_label_new("");

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->errorlabel,FALSE,TRUE,0);
  spd->restart_where_closedcheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(spd->mapdefaultcheck),_("start where program was closed"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->restart_where_closedcheck,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(spd->restart_where_closedcheck),"clicked",
		     GTK_SIGNAL_FUNC(radiobutton_updater_cb),spd);
  spd->restart_last_gpsfixcheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(spd->mapdefaultcheck),_("start where last gps fix was"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->restart_last_gpsfixcheck,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(spd->restart_last_gpsfixcheck),"clicked",
		     GTK_SIGNAL_FUNC(radiobutton_updater_cb),spd);
  but=gtk_button_new_with_label(_("OK"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->action_area),
		     but,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(ok_cb),spd);
  but=gtk_button_new_with_label(_("Cancel"));
  gtk_box_pack_end(GTK_BOX(GTK_DIALOG(spd->win)->action_area),
		   but,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(cancel_cb),spd);
  gtk_signal_connect(GTK_OBJECT(spd->win),"delete-event",
		     GTK_SIGNAL_FUNC(delete_cb),spd);
  gtk_widget_show_all(spd->win);
  if (!red_initialized) {
    gdk_color_parse("red",&red);
    gdk_color_alloc(gdk_window_get_colormap(spd->win->window),
		    &red);
    red_initialized=1;
    st=gtk_rc_style_new();
    st->color_flags[spd->errorlabel->state]=GTK_RC_TEXT;
    st->text[spd->errorlabel->state]=red;
  }
  gtk_widget_modify_style(spd->errorlabel,st);
  spd->possettable=table;
  spd->zoomlvlval=zoomlvl;
  spd->lat_now=lat;
  spd->lon_now=lon;
  load_startposcfg(spd);
}
