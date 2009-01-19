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

#ifndef K_MAPDRAWING_H
#define K_MAPDRAWING_H

struct connection_dialog;
struct osm_file;
struct osm_info;
extern int tile_request_mode;
#define MAX_LINE_LIST 5

struct sidebar_mode {
  char *name;
  GtkWidget *w;
};

struct mapwin {
  int line_drawing;
  long long int page_width,page_height;
  long long int page_x,page_y;
  GList *rect_list;
  GList **mark_line_list;
  GList *all_line_lists[MAX_LINE_LIST];
  GList **gps_line_list;
  struct strasse *mark_str;
  GdkEventMotion motionev;
  int mouse_moved;
  struct strasse *mouse_move_str;
  int str_is_anfang;
  int has_path;
  GdkPixmap *map_store;
  struct connection_dialog *cdlg;
  GtkObject *hadj;
  GtkObject *vadj;
  GtkWidget *mainwin;
  GtkWidget *list;
  GtkWidget *hscr;
  GtkWidget *vscr;
  GtkWidget *bottom_box;
  GtkWidget *entf_label;
  GtkWidget *map;
  GtkWidget *koords_label;
  GtkWidget *gps_label;
  GtkItemFactory *fac;
  GtkWidget *dlabel;
  GtkWidget *entry;
  GtkWidget *searchbox;
  GtkWidget *scrwin;
  GtkWidget *zoomin_but;
  GtkWidget *zoomout_but;
  GtkWidget *panmode_but;
  GtkWidget *linemode_but;
  GList *modelist;
  int disp_search;
  int gpstag;
  int gpstimertag;
  int follow_gps;
  int have_gpspos;
  int mouse_x, mouse_y;
  int draw_crosshair;
  struct osm_info *osm_inf;
  struct osm_file *osm_main_file;
  struct nmea_pointinfo last_nmea;
  struct gpsfile *gpsf;
};

void path_to_lines(double lon, double lat, void *data);
struct t_punkt32 *geosec2pointstruct(double long_sec, double latt_sec);
void geosec2point(double *xr, double *yr,double long_sec, double latt_sec);
void point2geosec(double *longr, double *lattr, double x, double y);
struct pixmap_info *load_image(char *name  /*GdkWindow *win*/);
int mapwin_draw(struct mapwin *mw,GdkGC *mygc,struct t_map *map,
		 int sx,int sy,int dx,int dy,int width,int height);
void draw2pinfo(struct pixmap_info *pinfo, struct t_map *map,
		int x, int y);
struct pixmap_info * get_map_rectangle(int x, int y, int w, int h);
void draw_pinfo(GdkWindow *w, GdkGC *gc,struct pixmap_info *p_info,
		int srcx,int srcy, int destx, int desty,
		int width, int height);
int get_http_file(const char *url,const char *filename,
		  void (*finish_cb)(const char *,const char*,void *),
		  void (*fail_cb)(const char *,const char*,void *),
		  int (*size_check)(const char *,void *,int),
		  void *data);
GtkWidget *make_pixmap_button(struct mapwin *mw,char **xpmdata);
GtkWidget *make_pixmap_toggle_button(struct mapwin *mw,char **xpmdata);
void calc_mapoffsets();
/* draw_route_lines */
void draw_line_list(struct mapwin *mw, GdkGC *mygc, GList *l);
void load_gps_line(const char *fname, GList **mll);
void draw_marks_to_ps(GList *mark_line_list, int mx, int my,
		      int w, int h, int fd);
void free_line_list(GList *l);
int tile_requests_processed();
#endif
