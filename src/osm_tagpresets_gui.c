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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include "gui_common.h"
#include "mapconfig_data.h"
#include "osm_tagpresets_data.h"
#include "osm_tagpresets_gui.h"

static GHashTable *pixmap_ht;

struct pixmap_ht_entry {
  GdkBitmap *bm;
  GdkPixmap *pm;
};

struct osm_preset_menu_running {
  void (*set_tag)(char *,char *,void *,void *);
  struct osm_preset_menu_sect *sect;
  void *set_tag_list;
  void *set_tag_list_data;
  GList *textwidgets;
};

static void preset_clicked(GtkWidget *w, gpointer user_data)
{

  struct osm_preset_menu_running *pr=
    (struct osm_preset_menu_running *)gtk_object_get_data(GTK_OBJECT(w),"menu_info");
  struct osm_preset_menu_sect *sect=((struct osm_presetitem *)
				     gtk_object_get_data(GTK_OBJECT(w),"item"))->menu;
  GList *l;
 
  for(l=g_list_first(pr->textwidgets);l;l=g_list_next(l)) {
    struct osm_presetitem *pi=gtk_object_get_data(GTK_OBJECT(l->data),
						  "item");
    char *v;
    v=gtk_editable_get_chars(GTK_EDITABLE(l->data),0,-1);
    if (pr&&v&&strlen(v)) {
      if (pr->set_tag) {
	pr->set_tag(pi->tagname,v,pr->set_tag_list,pr->set_tag_list_data);
      } else {
	char *tdst=malloc(strlen(pi->tagname)+strlen(v)+2);
	GList **taglist = (GList **)pr->set_tag_list;
	strcpy(tdst,pi->tagname);
	*taglist=g_list_append(*taglist,tdst);
	tdst+=strlen(tdst);
	tdst++;
	strcpy(tdst,v);
      }
    }
    g_free(v);
  }
  if (!sect)
    return;
  gtk_widget_destroy(GTK_WIDGET(user_data));
 
  osm_choose_tagpreset(sect,pr->set_tag,
		       pr->set_tag_list,pr->set_tag_list_data);
}

static GtkWidget *load_pixmap(GtkWidget *win,char *fname)
{
  char fbuf[512];
  GdkBitmap *bm=NULL;
  GdkPixmap *gdkpm=NULL;;
  GtkWidget *gtkpm;
  struct pixmap_ht_entry *hte;
  if (!pixmap_ht) {
    pixmap_ht=g_hash_table_new(g_str_hash,g_str_equal);
  }
  hte=(struct pixmap_ht_entry *)
    g_hash_table_lookup(pixmap_ht,fname);
  if (hte) {
    gdkpm=hte->pm;
    bm=hte->bm;
  }
  if (!gdkpm)
    gdkpm=my_gdk_pixmap_create_from_gfx(win->window,&bm,fname);
  if (!gdkpm) {
    snprintf(fbuf,sizeof(fbuf),MUMPOT_DATADIR "/pixmaps/%s",fname);
    gdkpm=my_gdk_pixmap_create_from_gfx(win->window,&bm,fbuf);
  }
  if (!gdkpm) {
    char *h;
    snprintf(fbuf,sizeof(fbuf),"~/.mumpot/pixmaps/%s",fname);
    h=expand_home(fbuf);
    gdkpm=gdk_pixmap_create_from_xpm(win->window,&bm,NULL,h);
    if (h!=fbuf)
      free(h);
  }
  if (!gdkpm) {
    snprintf(fbuf,sizeof(fbuf),"/usr/share/icons/openstreetmap/square.big/%s",fname);
    gdkpm=my_gdk_pixmap_create_from_gfx(win->window,&bm,fbuf);
  }
  if (!gdkpm) {
    snprintf(fbuf,sizeof(fbuf),"/usr/share/icons/openstreetmap/classic.big/%s",fname);
    gdkpm=my_gdk_pixmap_create_from_gfx(win->window,&bm,fbuf); 
  }
  if (gdkpm) {
    if (!hte) {
      hte=g_new0(struct pixmap_ht_entry,1);
      hte->pm=gdkpm;
      hte->bm=bm;
      g_hash_table_insert(pixmap_ht,fname,
			  hte);
    }
    return gtk_pixmap_new(gdkpm,bm);
  }
  return NULL;
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
    struct osm_preset_menu_running *pr=g_new0(struct osm_preset_menu_running,1);
    int x=0;
    int y=0;
    GList *l=sect->items;
    GtkWidget *win;
    GtkWidget *box;
    GtkWidget *table;
    pr->sect=sect;
    pr->set_tag=set_tag;
    pr->set_tag_list=data;
    pr->set_tag_list_data=user_data;
    for(l=g_list_first(sect->items);l;l=g_list_next(l)) {
      struct osm_presetitem *pi = (struct osm_presetitem *)l->data;
      if (pi->x>x)
	x=pi->x;
      if (pi->y>y)
	y=pi->y;
    }
    win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_realize(win);
    box=gtk_vbox_new(FALSE,0);
    table=NULL;
    if ((x>0)||(y>0)) {
      table=gtk_table_new(y,x,TRUE);
      gtk_box_pack_end(GTK_BOX(box),table,FALSE,FALSE,0);
    }
    for(l=g_list_first(sect->items);l;l=g_list_next(l)) {
      struct osm_presetitem *pi = (struct osm_presetitem *)l->data;
      GtkWidget *but=NULL;
      GtkWidget *hbox=NULL;
      GtkWidget *label=NULL;
      if (pi->img) {
	label=load_pixmap(win,pi->img);
      }
      if (!label) {
	label=gtk_label_new(pi->name);
      }
      switch(pi->type) {
      case BUTTON:
	but=gtk_button_new();
	gtk_container_add(GTK_CONTAINER(but),label);
	gtk_signal_connect(GTK_OBJECT(but),"clicked",
			   GTK_SIGNAL_FUNC(preset_clicked),
			   win);
	gtk_table_attach(GTK_TABLE(table),but,pi->x,pi->x+1,pi->y,pi->y+1,
			 GTK_EXPAND|GTK_FILL,GTK_EXPAND|GTK_FILL,
			 0,0);
	break;
      case TEXT:
	but=gtk_entry_new();
	hbox=gtk_hbox_new(FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
	gtk_box_pack_start(GTK_BOX(hbox),but,TRUE,TRUE,0);
	gtk_box_pack_start(GTK_BOX(box),hbox,FALSE,FALSE,0);
	if (pi->preset)
	  gtk_entry_set_text(GTK_ENTRY(but),pi->preset);
	pr->textwidgets=g_list_append(pr->textwidgets,but);
	break;
      }
      gtk_object_set_data(GTK_OBJECT(but),"menu_info",pr);
      gtk_object_set_data(GTK_OBJECT(but),"item",pi);
    }
    gtk_container_add(GTK_CONTAINER(win),box);
    gtk_widget_show_all(win);
  }
}
