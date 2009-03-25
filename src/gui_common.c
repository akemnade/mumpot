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
#include <glib.h>
#include <stdio.h>
#include "myintl.h"
#include "png_io.h"
#include "gps.h"
#include "mapconfig_data.h"
#include "mapdrawing.h"
#include "gui_common.h"
void check_item_set_state(struct mapwin *mw,char *path,int state)
{
    GtkWidget *w=gtk_item_factory_get_item(GTK_ITEM_FACTORY(mw->fac),path);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(w),state);
}

void menu_item_set_state(struct mapwin *mw,char *path,int state)
{
  GtkWidget *w=gtk_item_factory_get_item(GTK_ITEM_FACTORY(mw->fac),path);
  gtk_widget_set_sensitive(w,state);
}

void yes_no_dlg(char *txt,GtkSignalFunc yesfunc,GtkSignalFunc nofunc,void *data)
{
  GtkWidget *dialog;
  GtkWidget *but;
  GtkWidget *label;
  dialog=gtk_dialog_new();
  but=gtk_button_new_with_label(_("Yes"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),but,
		     TRUE,TRUE,0);
  if (yesfunc)
     gtk_signal_connect(GTK_OBJECT(but),"clicked",
			yesfunc,data);
  gtk_signal_connect_object_after(GTK_OBJECT(but),"clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  dialog);

  but=gtk_button_new_with_label(_("No"));
  gtk_box_pack_end(GTK_BOX(GTK_DIALOG(dialog)->action_area),but,
		   TRUE,TRUE,0);
  if (nofunc)
    gtk_signal_connect(GTK_OBJECT(but),"clicked",
		       nofunc,data);
  gtk_signal_connect_object_after(GTK_OBJECT(but),"clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  dialog);
  

  label=gtk_label_new(txt);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),label,
		     FALSE,FALSE,0);
  gtk_widget_show_all(dialog);
}

GdkPixmap *my_gdk_pixmap_creyte_from_gfx(GdkWindow *win,GdkBitmap **bm,
					 char *fname)
{
  GdkPixmap *pm;
  GdkGC *mygc;
  struct pixmap_info *pinfo=load_gfxfile(fname);
  if (!pinfo)
    return NULL;
  pm=gdk_pixmap_new(win,pinfo->width,pinfo->height,-1);
  mygc=gdk_gc_new(win);
  if ((bm)&&(pinfo->row_mask_pointers)) {
    GdkGC *bmgc;
    *bm=gdk_pixmap_new(NULL,pinfo->width,pinfo->height,1);
    bmgc=gdk_gc_new(*bm);
    gdk_draw_gray_image(*bm,mygc,0,0,pinfo->width,pinfo->height,
			GDK_RGB_DITHER_NONE,pinfo->row_mask_pointers[0],
			pinfo->row_mask_len);
    gdk_gc_unref(bmgc);
  } else {
    *bm=NULL;
  }
  if (pinfo->num_palette) {
    GdkRgbCmap* cmap=gdk_rgb_cmap_new(pinfo->gdk_palette,
				      pinfo->num_palette);
    gdk_draw_indexed_image(pm,mygc,0,0,pinfo->width,pinfo->height,
			   GDK_RGB_DITHER_NONE,pinfo->row_pointers[0],
			   pinfo->row_len,cmap);
    gdk_rgb_cmap_free(cmap);
			   
  } else {
    gdk_draw_rgb_image(pm,mygc,0,0,
		      pinfo->width,pinfo->height,
		      GDK_RGB_DITHER_NONE,pinfo->row_pointers[0],
		      pinfo->row_len);
  }
  gdk_gc_unref(mygc);
  return pm;
}

