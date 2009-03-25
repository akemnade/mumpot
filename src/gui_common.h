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

void check_item_set_state(struct mapwin *mw,char *path,int state);

void menu_item_set_state(struct mapwin *mw,char *path,int state);

void yes_no_dlg(char *txt,GtkSignalFunc yesfunc,GtkSignalFunc nofunc,void *data);
GdkPixmap *my_gdk_pixmap_creyte_from_gfx(GdkWindow *win,GdkBitmap **bm,
					 char *fname);
