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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "osm_tagpresets_data.h"
#include "osm_tagpresets_gui.h"
static void preset_clicked(GtkWidget *w, gpointer user_data)
{
  struct osm_preset_menu_sect *sect=
    (struct osm_preset_menu_sect *)gtk_object_get_data(GTK_OBJECT(w),"sect");
  void *taglist = gtk_object_get_data(GTK_OBJECT(w),"list");
  gtk_widget_destroy(GTK_WIDGET(user_data));
  if (!sect)
    return;
  osm_choose_tagpreset(sect,gtk_object_get_data(GTK_OBJECT(w),"set_tag"),
		       taglist,gtk_object_get_data(GTK_OBJECT(w),"list_data"));
}

void osm_choose_tagpreset(struct osm_preset_menu_sect *sect,
			  void (*set_tag)(char *,char *,void *,void *),
			  void *data, void *user_data)
{
  if (sect->tags) {
    GList *l;
    for(l=g_list_first(sect->tags);l;l=g_list_next(l)) {
      char *tsrc=(char *)l->data;
      char *val;
      int tlen=strlen(tsrc)+1;
      char *tdst;
      val=tsrc+tlen;
      tlen+=strlen(val);
      tlen++;
      if (set_tag) {
	set_tag(tsrc,val,data,user_data);
      } else {
	GList **taglist = (GList **)data;
	tdst=malloc(tlen);
	memcpy(tdst,tsrc,tlen);
	*taglist=g_list_append(*taglist,tdst);
      }
    }
  }
  if (sect->items) {
    int x=0;
    int y=0;
    GList *l=sect->items;
    GtkWidget *win;
    GtkWidget *table;
    for(l=g_list_first(sect->items);l;l=g_list_next(l)) {
      struct osm_presetitem *pi = (struct osm_presetitem *)l->data;
      if (pi->x>x)
	x=pi->x;
      if (pi->y>y)
	y=pi->y;
    }
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    table=gtk_table_new(y,x,TRUE);
    for(l=g_list_first(sect->items);l;l=g_list_next(l)) {
      struct osm_presetitem *pi = (struct osm_presetitem *)l->data;
      GtkWidget *but=gtk_button_new_with_label(pi->name);
      gtk_object_set_data(GTK_OBJECT(but),"sect",pi->menu);
      gtk_object_set_data(GTK_OBJECT(but),"list",data);
      gtk_object_set_data(GTK_OBJECT(but),"list_data",user_data);
      if (set_tag) {
	gtk_object_set_data(GTK_OBJECT(but),"set_tag",set_tag);
      }
      gtk_signal_connect(GTK_OBJECT(but),"clicked",
			 GTK_SIGNAL_FUNC(preset_clicked),
			 win);
      gtk_table_attach(GTK_TABLE(table),but,pi->x,pi->x+1,pi->y,pi->y+1,
		       GTK_EXPAND|GTK_FILL,GTK_EXPAND|GTK_FILL,
		       0,0);
    }
    gtk_container_add(GTK_CONTAINER(win),table);
    gtk_widget_show_all(win);
  }
}
