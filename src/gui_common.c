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
#include <gtk/gtk.h>
#include <glib.h>
#include <stdio.h>
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


