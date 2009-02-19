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

#ifndef K_OSM_VIEW_H
#define K_OSM_VIEW_H
void osm_clear_data(struct mapwin *mw);
void init_osm_draw(struct mapwin *mw);
void load_osm_gfx(struct mapwin *mw, const char *name);
void free_osm_gfx(struct mapwin *mw, struct osm_file *osmf);
void draw_osm(struct mapwin *mw, struct osm_file *osmf,GdkGC *mygc);
void recalc_node_coordinates(struct mapwin *mw, struct osm_file *osmf);
GtkItemFactoryEntry *get_osm_menu_items(int *n_osm_items);
void append_osm_bottom_line(struct mapwin *mw);
void append_osm_edit_line(struct mapwin *mw,GtkWidget *box);

void osm_map_moved(struct mapwin *mw);
int osmroute_start_calculate_nodest(struct mapwin *mw,
				    struct osm_file *osmf,
				    int x, int y);
int osm_mouse_handler(struct mapwin *mw, int x, int y, int millitime, int state);

int osm_center_handler(struct mapwin *mw, GdkGC *mygc, int x, int y);
void osmroute_add_path(struct mapwin *mw,
		       struct osm_file *osmf, void (*path_to_lines)(double lon, double lat, void *data), int x, int y, void *data);
int osm_save_file(const char *fname, struct osm_file *osmf);
/*
int osm_button_press(struct mapwin *mw);
int osm_button_release(struct mapwin *mw);
*/
#endif
