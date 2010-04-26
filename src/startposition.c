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
};

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

static void ok_cb(GtkWidget *w, GdkEventAny *ev, gpointer data)
{
  close_startposition_dialog((struct startposition_dialog *)data);
}

void create_startposition_dialog()
{
  struct startposition_dialog *spd=g_new0(struct startposition_dialog,1);
  GtkWidget *table;
  GtkWidget *lab;
  GtkWidget *but;
  
  spd->win=gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(spd->win),_("Set the startposition"));
  spd->mapdefaultcheck=gtk_radio_button_new_with_label(NULL,_("use map default"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->mapdefaultcheck,TRUE,TRUE,0);
  spd->entrycheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(spd->mapdefaultcheck),_("use given coordinats"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->entrycheck,TRUE,TRUE,0);
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
  lab=gtk_label_new(_("zoomlvl"));
  gtk_table_attach(GTK_TABLE(table),lab,0,1,2,3,GTK_FILL,
		   GTK_EXPAND|GTK_FILL,0,0);
 
  spd->zoomlvladj=gtk_adjustment_new(6,1,17,1,1,1);
  spd->zoomlvl=gtk_hscale_new(GTK_ADJUSTMENT(spd->zoomlvladj));
  gtk_table_attach(GTK_TABLE(table),spd->zoomlvl,1,2,2,3,GTK_FILL|GTK_EXPAND,
		   GTK_FILL|GTK_EXPAND,0,0);
  spd->currentposbut=gtk_button_new_with_label(_("set to current position"));
  gtk_table_attach(GTK_TABLE(table),spd->currentposbut,1,2,3,4,
		   GTK_FILL|GTK_EXPAND,GTK_FILL|GTK_EXPAND,0,0);
  
  spd->errorlabel=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->errorlabel,FALSE,TRUE,0);
  spd->restart_where_closedcheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(spd->mapdefaultcheck),_("start where mumpot was closed"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->restart_where_closedcheck,TRUE,TRUE,0);
  spd->restart_last_gpsfixcheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(spd->mapdefaultcheck),_("start where last gps fix was"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(spd->win)->vbox),
		     spd->restart_last_gpsfixcheck,TRUE,TRUE,0);
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
}
