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
#ifdef USE_IMLIB
#include <gdk_imlib.h>
#endif
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <locale.h>

#include "myintl.h"
#include "png_io.h"
#include "geometry.h"
#include "strasse.h"
#include "findpath.h"
#include "mapconfig_data.h"
#include "gps.h"
#include "mapdrawing.h"
#include "png2ps.h"
#include "printdlg.h"
#include "create_connection.h"
#include "gui_common.h"
#include "osm_view.h"
#include "osm_upload.h"
#include "trip_stats.h"
#include "startposition.h"

#include "zoomin.xpm"
#include "zoomout.xpm"
#include "linemode.xpm"
#include "panmode.xpm"

//#define MAP_TILE_WIDTH 1518
//#define MAP_TILE_HEIGHT 1032
//#define TILES_HORIZ 100
//#define TILES_VERT 100

#define MAP_LINE_PRO_MM 10
#define PAPER_WIDTH (get_paper_width()-30)
#define PAPER_HEIGHT (get_paper_height()-40)
#define MY_ABS(x) ((x)>0?(x):(-(x)))


#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif
#ifndef M_PI_2
#define M_PI_2         1.57079632679489661923
#endif
#ifdef _WIN32
#define snprintf _snprintf
#endif
int img_w,img_h;
GdkGC *mygc;
int anz_pfade;


GHashTable *orts_hash;
const char endchars[]=",\n";
static int baum_count;
static int mark_dim;
static GdkColor mark_red;
static GdkColor mark_white;
static GdkColor crosshair_color;
static GdkColor gps_color;
static GdkColor speedcolor[256];
static char *clip_coord_buf;
static char last_coord_buf[80];

struct t_verbindung {
  int von;
  int nach;
  int laenge;
  int typ;
  char *name;
};

struct t_ort {
  char *name;
  char *kreis;
  int breite;
  int laenge;
  int typ;
  int land;
  int vor;
  int entf;
  int zeit;
  int ventf;
  char *vname;
  struct t_verb *verb;
}; 



struct ort_baum {
  struct ort_baum *next[16];
  struct t_ort *entry;
};

static struct ort_baum ortbaum;



struct t_mark_rect *rect_to_edit=NULL;
int start_edit_x;
int start_edit_y;

void draw_marks(struct mapwin *mw); 
static void add_path_to_mark_list(GList **l, int krid);
static void recalc_mark_length(int offset, struct mapwin *mw);
static int find_nearst_point(GList *l, int x, int y);
static void add_pkt_to_list(GList **l, int x, int y);
static void zoom_in(struct mapwin *mw);
static void zoom_out(struct mapwin *mw);
static int gps_timer_first(void *data);
static void handle_pan_release(struct mapwin *mw, GdkEventButton *event,
			       int x, int y);
static void change_sidebar_cb(gpointer callback_data,
			      guint callback_action,
			      GtkWidget *w);

#define PATH_DISP_SEARCH_N N_("/View/Show place searchbox")
#define PATH_DISP_CROSSHAIR_N N_("/View/Show crosshairs")
#define PATH_DISP_HEADING_ARROW_N N_("/View/Show heading arrow")
#define PATH_FOLLOW_GPS_N N_("/View/Follow GPS")
#define PATH_ZOOM_OUT_N N_("/View/Zoom out")
#define PATH_ZOOM_IN_N N_("/View/Zoom in")
#define PATH_DISP_COLOR_N N_("/View/Color tracks by velocity")
#define PATH_DISP_SEARCH _(PATH_DISP_SEARCH_N)
#define PATH_DISP_CROSSHAIR  _(PATH_DISP_CROSSHAIR_N)
#define PATH_DISP_HEADING_ARROW _(PATH_DISP_HEADING_ARROW_N)
#define PATH_FOLLOW_GPS _(PATH_FOLLOW_GPS_N)
#define PATH_ZOOM_OUT _(PATH_ZOOM_OUT_N)
#define PATH_ZOOM_IN _(PATH_ZOOM_IN_N)
#define PATH_DISP_COLOR _(PATH_DISP_COLOR_N)

enum mouse_state_t {
  START_WAY,
  IN_WAY,
  PRINT_NEW,
  PRINT_DEL,
  PRINT_MOVE,
  PRINT_SEL,
  PAN,
  PANNING
};

enum mouse_state_t mouse_state;




struct ort_baum *neuer_baum(struct ort_baum *nb,int n_ziffer)
{
  if (!nb->next[n_ziffer])
    {
      nb->next[n_ziffer]=calloc(1,sizeof(struct ort_baum));
      if (!nb->next[n_ziffer])
	{
	  printf("Speichermangel\n");
	  exit(0);
	}
      baum_count+=sizeof(struct ort_baum);
    }
  return nb->next[n_ziffer];
}


void fuege_hinzu(int n,struct ort_baum *nb,struct t_ort *neuer_eintrag)
{
  int n_ziffer;
  struct t_ort *eintrag;

  if ((nb->entry)&&(nb->entry->name[n/2]))
    {
      eintrag=nb->entry;
      if (n&1)
	{
	  n_ziffer=eintrag->name[n/2]&0xf;
	}
      else 
	{
	  n_ziffer=eintrag->name[n/2];
	  n_ziffer=n_ziffer&0xf0;
	  n_ziffer=n_ziffer/16;
	}
      nb->entry=NULL;
      fuege_hinzu(n+1,neuer_baum(nb,n_ziffer),eintrag);
    }
  if (n&1)
    {
      n_ziffer=neuer_eintrag->name[n/2]&0xf;
    }
  else 
    {
      n_ziffer=neuer_eintrag->name[n/2];
      n_ziffer=n_ziffer&0xf0;
      n_ziffer=n_ziffer/16;
    }
  if ((!neuer_eintrag->name[n/2])||
      ((!nb->entry)&&(!nb->next[n_ziffer])))
    {
      eintrag=nb->entry;
      if (eintrag)
	return ;
      nb->entry=neuer_eintrag;
      
    }
  else
    {
      fuege_hinzu(n+1,neuer_baum(nb,n_ziffer),neuer_eintrag);
    }
}

int lese_zahl(char **s)
{
  char *alts;
  char *endstr;
  if ((endstr=strpbrk(*s,endchars)))
    {
      endstr[0]=0;
      alts=*s;
      *s=endstr+1;
      return atoi(alts);
    }
  *s=NULL;
  return 0;
}
char * lese_zkette(char **s)
{
  char *alts;
  char *neustr;
  char *endstr;
  alts=*s;
  if ((endstr=strpbrk(*s,endchars)))
    {
      endstr[0]=0;
      neustr=(char *)malloc(strlen(alts)+1);
      strcpy(neustr,alts);
      *s=endstr+1;
      return neustr;
    }
  *s=NULL;
  return NULL;
}



void center_ort(struct mapwin *mw,char *name)
{
  
  struct t_ort *ort=g_hash_table_lookup(orts_hash,name);
  if (!ort)
    return;
  center_map(mw,((double)ort->laenge)/3600.0,((double)ort->breite)/3600.0);
}


int init_ort(struct t_ort *ort,char *s)
{
  char *endstr;
  endstr=s;
  ort->name=lese_zkette(&s);
  if (!s) return 0;
  ort->kreis=lese_zkette(&s);
  if (!s) return 0;
  ort->laenge=lese_zahl(&s);
  if (!s) return 0;
  ort->breite=lese_zahl(&s);
  ort->verb=NULL;
  
  return 1;
}

static int read_places(char *fn)
{
  FILE *f;
  char tmp[200];
  f=fopen(fn,"r");
  if (!f)
    return 0;
  while (fgets(tmp,199,f))
    {
      struct t_ort *ort;
      ort=malloc(sizeof(struct t_ort));
      if (init_ort(ort,tmp))
	{
	  g_hash_table_insert(orts_hash,ort->name,ort);
	  fuege_hinzu(0,&ortbaum,ort);
	}
      else
	free(ort);
    }
  fclose(f);
  return 1;
}




#define IS_IN(a,b,c) (((a) >= (b)) && ((a) <= (c)))
#define IS_IN2(a,b,c) ((a<b)?-1:((a>c)?1:0))
static void display_text_box(char *b)
{
  GtkWidget *dialog;
  GtkWidget *button;
  GtkWidget *label;
  dialog=gtk_dialog_new();
  button=gtk_button_new_with_label(_("OK"));
  label=gtk_label_new(b);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		     label,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area),
		     button,FALSE,TRUE,0);
  gtk_signal_connect_object(GTK_OBJECT(button),
			    "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
			    (void*)dialog);
  gtk_signal_connect_object(GTK_OBJECT(dialog),
			    "delete-event",GTK_SIGNAL_FUNC(gtk_widget_destroy),
			    (void*)dialog);
  gtk_widget_show_all(dialog);
}

static void draw_heading_arrow(struct mapwin *mw)
{
  int x2,y2;
  float heading;
  if (1&mw->last_nmea.time)
    return;
  if (!mw->follow_gps)
    return;
  if (mw->last_nmea.heading == INVALID_HEADING)
    return;
  heading=mw->last_nmea.heading/180.0*M_PI;
  gdk_gc_set_foreground(mygc,&crosshair_color);
  
  gdk_gc_set_line_attributes(mygc,5,
			     GDK_LINE_SOLID,
			     GDK_CAP_BUTT,
			     GDK_JOIN_BEVEL);
  x2=mw->page_width/2+70.0*sin(heading);
  y2=mw->page_height/2-70.0*cos(heading);
  gdk_draw_line(mw->map->window,mygc,mw->page_width/2,mw->page_height/2,
		x2,y2);
  gdk_draw_line(mw->map->window,mygc,x2,y2,
		x2+20.0*sin(heading-2.8),y2-20.0*cos(heading-2.8));
  gdk_draw_line(mw->map->window,mygc,x2,y2,
		x2+20.0*sin(heading+2.8),y2-20.0*cos(heading+2.8));
}


static void draw_crosshair(struct mapwin *mw)
{
  gdk_gc_set_foreground(mygc,&crosshair_color);
  
  gdk_gc_set_line_attributes(mygc,3,
			     GDK_LINE_SOLID,
			     GDK_CAP_BUTT,
			     GDK_JOIN_BEVEL);
  gdk_draw_line(mw->map->window,mygc,mw->page_width/2-10,mw->page_height/2,
		mw->page_width/2+10,mw->page_height/2);
  gdk_draw_line(mw->map->window,mygc,mw->page_width/2,mw->page_height/2-10,
		mw->page_width/2,mw->page_height/2+10);
}

static void get_mark_xywh(struct t_mark_rect *rc,
			  int *x, int *y, int *w, int *h, double *dim)
{
  double xd,yd;
  double dd;
  geosec2point(&xd,&yd,rc->longg,rc->latt);
  *x=xd;
  *y=yd;
  if (!globalmap.is_utm) {
    dd=cos(rc->latt/180.0*M_PI)*M_PI*6371221.0*1000.0;
    /* pixels per mm */
    dd=globalmap.xfactor*180.0/dd;
    dd*=1000.0*rc->dim;
  } else {
    dd=MAP_LINE_PRO_MM*rc->dim;
    dd=dd*globalmap.xfactor/6.198382541/50.0;
  }
  *dim=dd;
  if (rc->width_gt_height) {
    *h=dd*PAPER_WIDTH;
    *w=dd*PAPER_HEIGHT;
  } else {
    *w=dd*PAPER_WIDTH;
    *h=dd*PAPER_HEIGHT;
  }
 
}

/* draw all printing rectangles */
/* draw one rectangle mark for printing */
void draw_marks_cb(gpointer data, gpointer user_data)
{
  int x,y,w,h;
  double dim;
  struct mapwin *mw=(struct mapwin *)user_data;
  struct t_mark_rect *rc=(struct t_mark_rect *)data;
  get_mark_xywh(rc,&x,&y,&w,&h,&dim);
  x-=mw->page_x;
  y-=mw->page_y;
  if ((x<-20000)||(x>20000))
    return;
  if ((y<-20000)||(y>20000))
    return;
  gdk_draw_rectangle(mw->map->window,mygc,FALSE,x,y,
		     w,h);
}
void draw_marks(struct mapwin *mw)
{
  gdk_gc_set_foreground(mygc,&mark_red);
  gdk_gc_set_line_attributes(mygc,5,
			     GDK_LINE_SOLID,
			     GDK_CAP_BUTT,
			     GDK_JOIN_BEVEL);
  g_list_foreach(mw->rect_list,draw_marks_cb,
		 mw);
  if (mouse_state == PRINT_MOVE) {
    if (rect_to_edit) {
      gdk_gc_set_foreground(mygc,&mark_white);
      draw_marks_cb(rect_to_edit,mw);
    }
  }
  gdk_gc_set_foreground(mygc,&mark_red);
  draw_line_list(mw,mygc,*mw->mark_line_list,(mw->color_line)?speedcolor:NULL);
}

/* called on mouse move */

static gboolean map_move_cb(gpointer user_data)
{
  struct mapwin *mw=(struct mapwin *)user_data;
  GdkEventMotion *event=&mw->motionev;
  GtkWidget *w=mw->map;
  struct t_punkt32 *p;
  GList *l;
  int mx,my;
  int x1;
  int y1;
  double longg;
  double latt;
  int last_point_available=0;
  char buf[80];
  mx=mw->page_x+(int)event->x;
  my=mw->page_y+(int)event->y;
  point2geosec(&longg,&latt,(double)mx,(double)my);
  snprintf(last_coord_buf, sizeof(last_coord_buf),"%.6f %.6f",
			 latt,longg);

  mw->mouse_moved=0;

  snprintf(buf,sizeof(buf),"x:%d y:%d\n %s",mx,my,last_coord_buf);
  if (mw->disp_search)
  gtk_label_set_text(GTK_LABEL(mw->koords_label),buf);
  if (mouse_state==PANNING) {
    GTK_ADJUSTMENT(mw->hadj)->value-=(mx-start_edit_x);
    GTK_ADJUSTMENT(mw->vadj)->value-=(my-start_edit_y);
    gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->hadj));
    gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->vadj));
    return FALSE;
  } 
  if (!((GTK_WIDGET_MAPPED(mw->linemode_but))&&
        (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->linemode_but)))))
    return FALSE;
  gdk_gc_set_foreground(mygc,&mark_red);
  gdk_gc_set_line_attributes(mygc,5,
			     GDK_LINE_SOLID,
			     GDK_CAP_BUTT,
			     GDK_JOIN_BEVEL);
  last_point_available=0;
  x1=0;
  y1=0;
  if ((mw->line_drawing)&&(g_list_length(*mw->mark_line_list)!=0)) {
    l=g_list_last(*mw->mark_line_list);
    p=(struct t_punkt32 *)l->data;
    x1=(p->x>>globalmap.zoomshift)-mw->page_x;
    y1=(p->y>>globalmap.zoomshift)-mw->page_y;
    if ((x1<-20000)||(x1>20000)||(y1<-20000)||(y1>20000)) {
      last_point_available=0;
    } else {
      last_point_available=1;
    }
  }
  if (event->state&GDK_SHIFT_MASK) {
    int x2,y2;
    int is_anfang;
    struct strasse *str;
   
    if (!last_point_available)
      return FALSE;
    
    x2=mw->page_x+(int)event->x;
    y2=mw->page_y+(int)event->y;
    str=suche_naechste_str(&x2,&y2,&is_anfang);
    
    if ((str)&&(str==mw->mouse_move_str))
      return FALSE;
    mw->mouse_move_str=str;
    if ((str)||((!mw->mark_str)&&(mw->has_path))) {
      gdk_draw_pixmap(mw->map->window,mygc,mw->map_store,0,0,0,0,mw->page_width,
		      mw->page_height);
      if (mw->osm_main_file)
	draw_osm(mw,mw->osm_main_file,mygc);
      draw_marks(mw);



      if ((!mw->mark_str)&&(!mw->has_path)) {
	gdk_draw_line(w->window,mygc,x1,y1,
		      x2-mw->page_x,y2-mw->page_y);
      } else {
	GList *l=NULL;
	if (mw->has_path) {
	  if (mw->mark_str) {
	    add_path_to_mark_list(&l,is_anfang?str->anfang_krid:str->end_krid);
	  } else if (mw->osm_main_file) {
	    osmroute_add_path(mw,mw->osm_main_file,path_to_lines,x2,y2,&l);
	  }
	  draw_line_list(mw,mygc,l,NULL);
	  free_line_list(l);
	}
      }
    }
  } else if (event->state&GDK_CONTROL_MASK) {
    int offset;
    offset=find_nearst_point(*mw->mark_line_list,mw->page_x+(int)event->x,
			     mw->page_y+(int)event->y);
    recalc_mark_length(offset<0?0:offset,mw);
  } else {
    if ((mouse_state!=PRINT_NEW)&&(mouse_state!=PRINT_MOVE)&&(!last_point_available))
      return FALSE;
    gdk_draw_pixmap(mw->map->window,mygc,mw->map_store,0,0,0,0,mw->page_width,
		    mw->page_height);
    if (mw->osm_main_file)
      draw_osm(mw,mw->osm_main_file,mygc);
    draw_marks(mw);
    if (mouse_state==PRINT_MOVE) {
      int sx,sy,w,h;
      double dim;
      gdk_gc_set_foreground(mygc,&mark_red);
      get_mark_xywh(rect_to_edit,&sx,&sy,&w,&h,&dim);
      sx+=mx-start_edit_x;
      sy+=my-start_edit_y;
      gdk_draw_rectangle(mw->map->window,mygc,FALSE,
			 sx-mw->page_x,
			 sy-mw->page_y,
			 w,h);
    } else if (mouse_state==PRINT_NEW) {
      struct t_mark_rect rc;
      point2geosec(&rc.longg,&rc.latt,(double)mx,(double)my);
      rc.dim=mark_dim;
      rc.width_gt_height=0;
      draw_marks_cb(&rc,mw);
    } else if (last_point_available)
      gdk_draw_line(w->window,mygc,x1,y1,
		    (int)event->x,(int)event->y);
    
  }
  return FALSE;
}

gboolean map_move(GtkWidget *w, GdkEventMotion *event, gpointer user_data)
{
  struct mapwin *mw=(struct mapwin *)user_data;
  if (!mw->mouse_moved)
    g_idle_add(map_move_cb,mw);
  mw->motionev=*event;
  mw->mouse_moved=1;
  return TRUE;
}


/* find nearest point which is on a street */
static int find_nearst_point(GList *l, int x, int y)
{
  int i,len;
  int dx,dy;
  int best_pt;
  int min_dist;
  min_dist=1<<30;
  best_pt=-1;
  len=g_list_length(l);
  l=g_list_first(l);
    for(i=0;i<len;i++,l=g_list_next(l)) {
    dx=(((struct t_punkt32 *)l->data)->x>>globalmap.zoomshift)-x;
    dy=(((struct t_punkt32 *)l->data)->y>>globalmap.zoomshift)-y;
    if ((MY_ABS(dx)>1000)||(MY_ABS(dy)>1000)) {
      continue;
    }
    dx*=dx;
    dy*=dy;
    dx+=dy;
    if (dx<min_dist) {
      min_dist=dx;
      best_pt=i+1;
    }
  }
  return best_pt;
}

/* find nearest point which is on a street */
static int find_nearst_point_latlon(GList *l, struct nmea_pointinfo *p)
{
  int i,len;
  int dx,dy;
  int best_pt;
  int min_dist;
  min_dist=1<<30;
  best_pt=-1;
  len=g_list_length(l);
  l=g_list_first(l);
    for(i=0;i<len;i++,l=g_list_next(l)) {
      dy=3600.0*(((struct t_punkt32 *)l->data)->latt-p->lat);
      dx=3600.0*(((struct t_punkt32 *)l->data)->longg-p->lon);
    if ((MY_ABS(dx)>1000)||(MY_ABS(dy)>1000)) {
      continue;
    }
    dx*=dx;
    dy*=dy;
    dx+=dy;
    if (dx<min_dist) {
      min_dist=dx;
      best_pt=i+1;
    }
  }
  return best_pt;
}

static double remaining_length(int offset, struct mapwin *mw, struct t_punkt32 *p)
{
  struct t_punkt32 *p1;
  struct t_punkt32 *p2;
  char buf[80];
  double entf;
  double dx,dy;
  int i,len;
  GList *l;
  entf=0;
  l=*mw->mark_line_list;
  p1=NULL;
  if (l == NULL)
    return 0;
  len=g_list_length(l);
  if (offset >= len)
    return 0;
  l = g_list_first(l);
  for(i = 0; (i < offset) && (l != NULL); i++) {
    l = g_list_next(l);
  }
  p1 = p;
  while(l != NULL) {
    p2=(struct t_punkt32 *)l->data;
    dx=p2->x-p1->x;
    dy=p2->y-p1->y;
    entf+=sqrt(dx*dx+dy*dy);
    p1=p2;
    l = g_list_next(l);
  }
  entf/=(1<<globalmap.zoomshift);
  if ((p1)&&(!globalmap.is_utm)) {
    double dd;
    dd=cos(p1->latt/180.0*M_PI)*M_PI*6371221.0*1000.0;
    /* pixels per mm */
    dd=globalmap.xfactor*180.0/dd;
    entf=entf/dd/1000000.0;
  } else {
    if (globalmap.proj) {
      entf /= globalmap.xfactor;
      return entf;
    }
    entf=entf/200.0/globalmap.xfactor*6.198382541;
    entf*=3600;
  }
  return 1000.0*entf;
}

/* recalculate the length of the marked route */
static void recalc_mark_length(int offset, struct mapwin *mw)
{
  struct t_punkt32 *p1;
  struct t_punkt32 *p2;
  char buf[80];
  double entf;
  double dx,dy;
  int i,len;
  GList *l;
  entf=0;
  l=*mw->mark_line_list;
  p1=NULL;
  if (l) {
    len=g_list_length(l);
    if (len>offset)
      len=offset;
    l=g_list_first(l);
    p1=(struct t_punkt32 *)l->data;
    l=g_list_next(l);
    for(i=1;i<len;i++,l=g_list_next(l)) {
      p2=(struct t_punkt32 *)l->data;
      dx=p2->x-p1->x;
      dy=p2->y-p1->y;
      entf+=sqrt(dx*dx+dy*dy);
      p1=p2;
    }
  }
  entf/=(1<<globalmap.zoomshift);
  if ((p1)&&(!globalmap.is_utm)) {
    double dd;
    dd=cos(p1->latt/180.0*M_PI)*M_PI*6371221.0*1000.0;
    /* pixels per mm */
    dd=globalmap.xfactor*180.0/dd;
    entf=entf/dd/1000000.0;
  } else {
    if (globalmap.proj) {
      entf /= globalmap.xfactor;
      entf /= 1000;
    } else {
      entf=entf/200.0/globalmap.xfactor*6.198382541;
      entf *= 3600;
    }
  }
  if (entf==0) {
    gtk_label_set_text(GTK_LABEL(mw->entf_label),"");
  } else {
    char tbuf[80];
    tbuf[0]=0;
    if ((p1)&&(p1->time)) {
      time_t t=p1->time;
      struct tm *tm=localtime(&t);
      strftime(tbuf,sizeof(tbuf),"%Y-%m-%d %a %H:%M:%S",tm);
    }
    if (p1->speed!=0) {
      snprintf(buf,sizeof(buf),"%.3f km %.f km/h %s",entf,p1->speed*1.852,
	       tbuf);
    } else {
      snprintf(buf,sizeof(buf),"%.3f km %s",entf,
	       tbuf);
    }
    gtk_label_set_text(GTK_LABEL(mw->entf_label),buf);
  }
}


/* handle mouse clicks with road enforcement */
static void handle_route_click(struct mapwin *mw, int x, int y)
{
  /* first click on a road */
  if (!mw->has_path) {
    struct strasse *str;
    str=suche_naechste_str(&x,&y,&(mw->str_is_anfang));
    if (str) {
      add_pkt_to_list(mw->mark_line_list,x,y);
      mw->mark_str=str;
      
      mw->has_path=findpath(mw->str_is_anfang?str->anfang_krid:str->end_krid);
    } else {
      mw->has_path=osmroute_start_calculate_nodest(mw,mw->osm_main_file,x,y);
      mw->mark_str=NULL;
    }
  } else {
    int is_anfang;
    struct strasse *str=suche_naechste_str(&x,&y,&is_anfang);
    if (mw->has_path) {
      if (mw->mark_str) {
	add_path_to_mark_list(mw->mark_line_list,
			      is_anfang?str->anfang_krid:str->end_krid);
      } else {
	GList *l=NULL;
	osmroute_add_path(mw,mw->osm_main_file,path_to_lines,x,y,&l);
	*mw->mark_line_list=g_list_concat(*mw->mark_line_list,l);
      }
      reset_way_info();
      mw->has_path=0;
    }
    mw->mark_str=NULL;
  }
}

static void remove_last_route_point(struct mapwin *mw)
{
  if (*mw->mark_line_list) {
    GList *glast=g_list_last(*mw->mark_line_list);
    free(glast->data);
    *mw->mark_line_list=g_list_remove_link(*mw->mark_line_list,glast);
    gdk_draw_pixmap(mw->map->window,mygc,mw->map_store,0,0,0,0,mw->page_width,
		    mw->page_height);
  }
}


struct t_mark_rect *mark_is_clicked(struct mapwin *mw, int x, int y)
{
  GList *l;
  for(l=g_list_first(mw->rect_list);l;l=g_list_next(l)) {
    struct t_mark_rect *rc=(struct t_mark_rect *)l->data;
    int mx,my,w,h;
    double dim;
    get_mark_xywh(rc,&mx,&my,&w,&h,&dim);
    if ((((MY_ABS(mx-x)<3) || (MY_ABS(mx-x+w)<3)) && (y>=my) && (y<=my+h)) ||
	(((MY_ABS(my-y)<3) || (MY_ABS(my-y+h)<3)) && (x>=mx) && (x<=mx+w))) {
      return rc;
    }
  }
  return NULL;
}


gboolean map_click_release(GtkWidget *widget, GdkEventButton *event,
			   gpointer user_data)
{
  int x,y;
  struct mapwin *mw=(struct mapwin *)user_data;
  x=(int)event->x;
  y=(int)event->y;
  x+=mw->page_x;
  y+=mw->page_y;
  if (osm_mouse_handler(mw,x,y,event->time,0,event->state))
    return TRUE;
  if (mouse_state==PANNING) {
    handle_pan_release(mw,event,x,y);
  } else if (mouse_state==IN_WAY) {
    int wd=x-mw->mouse_x;
    int hd=y-mw->mouse_y;
    hd=hd/(mw->page_height/4);
    wd=wd/(mw->page_width/4);
    if ((wd<=-2)&&(hd==0)) {
      mw->mark_str=NULL;
      reset_way_info();
      mw->has_path=0;
      remove_last_route_point(mw);
      remove_last_route_point(mw);
      gtk_widget_queue_draw_area(mw->map,0,0,mw->page_width,mw->page_height);
  
    } else if ((wd==0)&&(hd<=-2)) {
      mw->line_drawing=0;
      free_line_list(*mw->mark_line_list);
      *mw->mark_line_list=NULL;
      reset_way_info();
      mw->has_path=0;
      mouse_state=START_WAY;
      gtk_widget_queue_draw_area(mw->map,0,0,mw->page_width,mw->page_height);
  
    }
  }
  return TRUE;
}



/* handle clicks when the click could start a way */
static void handle_start_way_click(struct mapwin *mw, GdkEventButton *event, int x, int y)
{
  struct t_mark_rect *rc;
  if ((event->type==GDK_BUTTON_PRESS)&&((rc=mark_is_clicked(mw,x,y)))) {
    
    rect_to_edit=rc;
    start_edit_x=x;
    start_edit_y=y;
    mouse_state=PRINT_MOVE;
    return;
  }
  if (event->button==1) {
    mw->line_drawing=1;
    free_line_list(*mw->mark_line_list);
    *mw->mark_line_list=NULL;
    add_pkt_to_list(mw->mark_line_list,x,y);
    mw->mark_str=NULL;
    reset_way_info(); /* free the previous route info */
    mw->has_path=0;
    mouse_state=IN_WAY;
    if (event->state&GDK_SHIFT_MASK) { /* shift pressed ->route click */
      handle_route_click(mw,x,y);
    }
  }
  if (event->button==3) {
    remove_last_route_point(mw);
  }
  recalc_mark_length(1<<30,mw);
}

static void handle_pan_release(struct mapwin *mw, GdkEventButton *event,
			       int x, int y)
{
  
  GTK_ADJUSTMENT(mw->hadj)->value-=(x-start_edit_x);
  GTK_ADJUSTMENT(mw->vadj)->value-=(y-start_edit_y);
  gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->hadj));
  gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->vadj));
  mouse_state=PAN;
}

static void handle_pan_click(struct mapwin *mw, GdkEventButton *event,
			     int x, int y)
{

  start_edit_x=x;
  start_edit_y=y;
  mouse_state=PANNING;

}

static void handle_print_move_click(struct mapwin *mw, GdkEventButton *event, int x, int y)
{
  if (rect_to_edit) {
    int mx,my,w,h;
    double dim;
    get_mark_xywh(rect_to_edit,&mx,&my,&w,&h,&dim);
    mx+=x-start_edit_x;
    my+=y-start_edit_y;
    point2geosec(&rect_to_edit->longg,&rect_to_edit->latt,
		 (double)mx,(double)my);
    
    rect_to_edit=NULL;
    gdk_draw_pixmap(mw->map->window,mygc,mw->map_store,0,0,0,0,mw->page_width,
		    mw->page_height);
    if (mw->osm_main_file)
      draw_osm(mw,mw->osm_main_file,mygc);
    draw_marks(mw);
  }
  mouse_state=START_WAY;
}

static void handle_print_del_click(struct mapwin *mw, GdkEventButton *event, int x, int y)
{
  struct t_mark_rect *rc;
  if ((rc=mark_is_clicked(mw,x,y))) {
    mw->rect_list=g_list_remove(mw->rect_list,rc);
    free(rc);
    gdk_draw_pixmap(mw->map->window,mygc,mw->map_store,0,0,0,0,mw->page_width,
		    mw->page_height);
    if (mw->osm_main_file)
      draw_osm(mw,mw->osm_main_file,mygc);
    draw_marks(mw);
    
  }
  mouse_state=START_WAY;
}

static void handle_print_sel_click(struct mapwin *mw, GdkEventButton *event,
				   int x, int y)
{
  mouse_state=START_WAY;
}

static void handle_in_way_click(struct mapwin *mw, GdkEventButton *event, int x, int y)
{
  struct t_mark_rect *rc;
  if ((event->type==GDK_BUTTON_PRESS)&&((rc=mark_is_clicked(mw,x,y)))) {
    
    rect_to_edit=rc;
    start_edit_x=x;
    start_edit_y=y;
    mouse_state=PRINT_MOVE;
    return;
  }
  if ((event->button==1)||(event->button==2)) {

    if (event->state&GDK_SHIFT_MASK) { /* shift pressed ->route click */
      handle_route_click(mw,x,y);
    } else {
      add_pkt_to_list(mw->mark_line_list,x,y);
      mw->mark_str=NULL;
      reset_way_info(); /* free the previous route info */
      mw->has_path=0;
      if (event->button==2) {
	mw->line_drawing=0;
	mouse_state=START_WAY;
      }
    }
    recalc_mark_length(1<<30,mw);
  } else if (event->button==3) {
     mw->mark_str=NULL;
     reset_way_info();
     mw->has_path=0;
     remove_last_route_point(mw);
  }
  recalc_mark_length(1<<30,mw);
}

static void handle_print_new_click(struct mapwin *mw, GdkEventButton *event, int x, int y)
{
  struct t_mark_rect *rc;
  if (mark_dim!=0) {
    rc=malloc(sizeof(struct t_mark_rect));
    point2geosec(&rc->longg,&rc->latt,(double)x,(double)y);
    rc->dim=mark_dim;
    if (event->button==3) {
      rc->width_gt_height=1;
    } else {
      rc->width_gt_height=0;
    }
    mw->rect_list=g_list_append(mw->rect_list,rc);
    mark_dim=0;
  }
  mouse_state=START_WAY;
}

static void paste_coords(GtkWidget *widget,
        GtkSelectionData *data, guint inf, guint t)
{
  if (clip_coord_buf) {
#ifdef USE_GTK2
    gtk_selection_data_set_text(data,clip_coord_buf,-1);
#else
    gtk_selection_data_set(data,GDK_SELECTION_TYPE_STRING,
			 8,(guchar *)clip_coord_buf,strlen(clip_coord_buf));
#endif
  }
}


/* map focus in */
static gboolean map_focus_in(GtkWidget *w, GdkEventFocus *event, gpointer user_data)
{
  GTK_WIDGET_SET_FLAGS(w,GTK_HAS_FOCUS);
#ifndef USE_GTK2
  gtk_widget_draw_focus(w);
#endif
  return FALSE;
}

/* map focus out */
static gboolean map_focus_out(GtkWidget *w, GdkEventFocus *event, gpointer user_data)
{
  GTK_WIDGET_UNSET_FLAGS(w,GTK_HAS_FOCUS);
#ifndef USE_GTK2
  gtk_widget_draw_focus(w);
#endif
  return FALSE;
}

#ifdef USE_GTK2
/* handle scrollwheel events */
gboolean map_scrollwheel(GtkWidget *widget, GdkEventScroll *event, gpointer user_data)
{
  if (event->direction==GDK_SCROLL_UP) {
    zoom_in((struct mapwin *)user_data);
    return TRUE;
  } else if (event->direction==GDK_SCROLL_DOWN) {
    zoom_out((struct mapwin *)user_data);
    return TRUE;
  }
  return FALSE;
}
#endif

/* hanlde map clicks */
gboolean map_click(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  int x,y;
  struct mapwin *mw=(struct mapwin *)user_data;
  if (event->button==4) {
    zoom_in(mw);
    return TRUE;
  }
  if (event->button==5) {
    zoom_out(mw);
    return TRUE;
  } 
  x=(int)event->x;
  y=(int)event->y;
  x+=mw->page_x;
  y+=mw->page_y;
  mw->mouse_x=x;
  mw->mouse_y=y;
  mw->mouse_move_str=NULL;
  if (!GTK_WIDGET_HAS_FOCUS(mw->map)) {
    gtk_widget_grab_focus(widget);
    return TRUE;
  }
  if ((event->state&GDK_SHIFT_MASK)&&(event->button==3)) {
    x=x-mw->page_width/2;
    y=y-mw->page_height/2;
    GTK_ADJUSTMENT(mw->hadj)->value=x;
    GTK_ADJUSTMENT(mw->vadj)->value=y;
    gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->hadj));
    gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->vadj));
    return TRUE;
  } else if ((event->state & GDK_CONTROL_MASK)&&(event->button==1)) {
    if (clip_coord_buf) {
      free(clip_coord_buf);
    }
    clip_coord_buf=strdup(last_coord_buf);
    gtk_selection_owner_set(mw->map,GDK_SELECTION_PRIMARY,GDK_CURRENT_TIME);
    return TRUE;
  }
  if (osm_mouse_handler(mw,x,y,event->time,1,event->state))
    return TRUE;
  switch(mouse_state) {
  case START_WAY: handle_start_way_click(mw,event,x,y); break;
  case IN_WAY: handle_in_way_click(mw,event,x,y); break;
  case PRINT_NEW: handle_print_new_click(mw,event,x,y); break;
  case PRINT_MOVE: handle_print_move_click(mw,event,x,y); break;
  case PRINT_SEL: handle_print_sel_click(mw,event,x,y); break;
  case PRINT_DEL: handle_print_del_click(mw,event,x,y); break;
  case PAN: handle_pan_click(mw,event,x,y); break;
  default: break;
  }
  if (mw->osm_main_file)
    draw_osm(mw,mw->osm_main_file,mygc);
  draw_marks(mw);
  
  return TRUE;
}

/* redraw the mapwin */
gboolean
expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  int i;
  struct mapwin *mw=data;
  if (!mygc)
    return TRUE;
  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state],
                             &event->area);
  if (!globalmap.first) {
    gdk_gc_set_foreground(mygc,&mark_white);
    gdk_draw_rectangle(widget->window,mygc,1,event->area.x,
		       event->area.y,event->area.width,event->area.height);
  } else {
    gdk_draw_pixmap(widget->window,mygc,mw->map_store,0,0,0,0,mw->page_width,
		    mw->page_height);
  }
  /*  mapwin_draw(mw,widget->style->fg_gc[widget->state]); */
  gdk_gc_set_foreground(mygc,&gps_color);
  for(i=0;i<MAX_LINE_LIST;i++) {
    if (mw->all_line_lists[i]==*(mw->mark_line_list))
      continue;
    draw_line_list(mw,mygc,mw->all_line_lists[i],NULL);
  }  
  if (mw->osm_main_file)
    draw_osm(mw,mw->osm_main_file,mygc);
  draw_marks(mw);
  if (mw->draw_crosshair) {
    draw_crosshair(mw);
  }
  if (mw->draw_heading_arrow) {
    draw_heading_arrow(mw);
  }
  osm_center_handler(mw,mygc,mw->page_x+mw->page_width/2,mw->page_y+mw->page_height/2);
  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state],
                             NULL);

  return TRUE;
}

/* resize the mapwin */
gboolean 
config_event(GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
  struct mapwin *mw=data;
  if ((mw->page_width==event->width)&&(mw->page_height==event->height))
    return TRUE;
  mw->page_width=event->width;
  mw->page_height=event->height;
  printf("w: %d h: %d\n",event->width,event->height);
  /*  if (mw->map_store) 
      gdk_pixmap_unref(mw->map_store); */
  mw->map_store=gdk_pixmap_new(mw->map->window,mw->page_width,
			       mw->page_height,-1);
  GTK_ADJUSTMENT(mw->hadj)->page_size=event->width;
  GTK_ADJUSTMENT(mw->hadj)->page_increment=event->width;
  GTK_ADJUSTMENT(mw->hadj)->upper=globalmap.fullwidth*(1<<(globalmap.zoomfactor-1));
  GTK_ADJUSTMENT(mw->vadj)->page_size=event->height;
  GTK_ADJUSTMENT(mw->vadj)->page_increment=event->height;
  GTK_ADJUSTMENT(mw->vadj)->upper=globalmap.fullheight*(1<<(globalmap.zoomfactor-1));
  gtk_adjustment_changed(GTK_ADJUSTMENT(mw->hadj));
  gtk_adjustment_changed(GTK_ADJUSTMENT(mw->vadj));
  if (!mygc) {
    return TRUE;
  }
  if (mygc)
     mapwin_draw(mw,mygc,globalmap.first,mw->page_x,mw->page_y,
		 0,0,mw->page_width,mw->page_height);
  gdk_draw_pixmap(widget->window,mygc,mw->map_store,0,0,0,0,mw->page_width,
		  mw->page_height);
  if (mw->osm_main_file)
    draw_osm(mw,mw->osm_main_file,mygc);
  draw_marks(mw);
  if (mw->draw_crosshair) {
    draw_crosshair(mw);
  }
  return TRUE;
}
/* scroll callback */
void scrollbar_moved(GtkWidget *w,gpointer data)
{
  struct mapwin *mw=data;
  int oldpx=mw->page_x;
  int oldpy=mw->page_y;
  int dx,dy;
  mw->page_x=GTK_ADJUSTMENT(mw->hadj)->value;
  mw->page_y=GTK_ADJUSTMENT(mw->vadj)->value;
  dx=mw->page_x-oldpx;
  dy=mw->page_y-oldpy;
  if ((dx==0)&&(dy==0))
    return;
  printf("dx: %d dy: %d\n",dx,dy);
  gtk_widget_queue_draw_area(mw->map,0,0,mw->page_width,mw->page_height);
  if ((MY_ABS(dx)<mw->page_width)&&(MY_ABS(dy)<mw->page_height)) {
#ifndef USE_GTK2
    gdk_draw_pixmap(mw->map_store,mw->map->style->fg_gc[mw->map->state],mw->map_store,
		    (dx>0)?dx:0,(dy>0)?dy:0,(dx>0)?0:-dx,(dy>0)?0:-dy,
		    mw->page_width-MY_ABS(dx),mw->page_height-MY_ABS(dy));
#else
    gdk_draw_drawable(mw->map_store,mw->map->style->fg_gc[mw->map->state],mw->map_store,
		      (dx>0)?dx:0,(dy>0)?dy:0,(dx>0)?0:-dx,(dy>0)?0:-dy,
		      mw->page_width-MY_ABS(dx),mw->page_height-MY_ABS(dy));
#endif
    if (dx) {
      int dxoff=dx>0?(mw->page_width-dx):0;
      
      mapwin_draw(mw,mw->map->style->fg_gc[mw->map->state],globalmap.first,
		  mw->page_x+dxoff,mw->page_y,dxoff,0,MY_ABS(dx),mw->page_height);
      
    }
    if (dy) {
      int dyoff=dy>0?(mw->page_height-dy):0;
      mapwin_draw(mw,mw->map->style->fg_gc[mw->map->state],globalmap.first,
		  mw->page_x,mw->page_y+dyoff,0,dyoff,
		  mw->page_width,MY_ABS(dy));
    }
    
  } else {
    mapwin_draw(mw,mw->map->style->fg_gc[mw->map->state],globalmap.first,
		mw->page_x,mw->page_y,0,0,mw->page_width,mw->page_height);
  }
  /*
  if (mw->osm_main_file)
  draw_osm(mw,mw->osm_main_file,mygc);
  draw_marks(mw);
  if (mw->draw_crosshair) {
    draw_crosshair(mw);
  }
  */
}

void suche_ort_cb(GtkWidget *w,gpointer data)
{
  struct mapwin *mw=data;
  char *ort;
  ort=gtk_editable_get_chars(GTK_EDITABLE(mw->entry),0,-1);
  if (strchr(ort,'=')) {
    osmedit_search(mw,ort);
  } else {
    center_ort(mw,ort);
  }
  g_free(ort);
  
}

void gebe_baum_aus(struct mapwin *mw,struct ort_baum *baum, int *anz_erg)
{
  int i;
  if ((*anz_erg)>100)
    return ;
  for (i=0;i<16;i++)
    {
      if (baum->next[i])
	gebe_baum_aus(mw,baum->next[i],anz_erg);
    }
  if (baum->entry) {
    char *le[2];
    int row;
    le[0]=baum->entry->name;
    le[1]=baum->entry->kreis;
    row=gtk_clist_append(GTK_CLIST(mw->list),le);
    gtk_clist_set_row_data(GTK_CLIST(mw->list),row,baum->entry);
    (*anz_erg)++;
  }
}

void list_orte(GtkWidget *w,gpointer data)
{
  int anz_erg;
  struct ort_baum *baum;
  int len,i;
  char *ortsname;
  struct mapwin *mw=data;
  ortsname=gtk_editable_get_chars(GTK_EDITABLE(w),0,-1);
  len=strlen(ortsname)*2;
  if (len<3)
    return;
  baum=&ortbaum;
  for (i=0;i<len;i++)
    {
      int n_ziffer;
      if (i&1)
	{
	  n_ziffer=ortsname[i/2]&0xf;
	}
      else 
	{
	  n_ziffer=ortsname[i/2];
	  n_ziffer=n_ziffer&0xf0;
	  n_ziffer=n_ziffer/16;
	}
      if (!baum->next[n_ziffer])
	break;
      baum=baum->next[n_ziffer];
      
    }

  gtk_clist_freeze(GTK_CLIST(mw->list));
  gtk_clist_clear(GTK_CLIST(mw->list));
  if (i<len)
    {
      if ((baum->entry)&&(!strncmp(baum->entry->name,ortsname,len/2)))
	{
	  char *le[2];
	  int row;
	  le[0]=baum->entry->name;
	  le[1]=baum->entry->kreis;
	  row=gtk_clist_append(GTK_CLIST(mw->list),le);
	  gtk_clist_set_row_data(GTK_CLIST(mw->list),row,baum->entry);
	}
    }
  else
    {
      anz_erg=0;
      gebe_baum_aus(mw,baum,&anz_erg);
    }
  gtk_clist_thaw(GTK_CLIST(mw->list));
}



void ort_select(GtkCList *clist,
		gint row,
		gint column,
		GdkEventButton *event,
		gpointer user_data)
{
  struct mapwin *mw=user_data;
  struct t_ort *ort=gtk_clist_get_row_data(clist,row);
  if (ort)
    center_map(mw,ort->laenge/3600.0,ort->breite/3600.0);
}

/* prepare to mark a rectangle for printing */
static void mark_for_print(gpointer callback_data,
			   guint callback_action,
			   GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  mark_dim=callback_action;
  //mark_width=globalmap.xfactor/6.198382541*mark_dim*PAPER_WIDTH;
  //mark_height=globalmap.yfactor/6.181708885*mark_dim*PAPER_HEIGHT;
 
  change_sidebar_cb(callback_data,0,NULL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->linemode_but),1);
  gtk_toggle_button_toggled(GTK_TOGGLE_BUTTON(mw->linemode_but));
  mouse_state=PRINT_NEW;
}

/* delete the print rectangles */
static void mark_del_cb(gpointer data,
			gpointer user_data)
{
  free(data);
}

static void mark_del(gpointer callback_data,
		     gpointer callback_action,
		     GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  g_list_foreach(mw->rect_list,mark_del_cb,NULL);
  g_list_free(mw->rect_list);
  mw->rect_list=NULL;
}

static void mark_del_one(gpointer callback_data,
			 gpointer callback_action,
			 GtkWidget *w)
{
  mouse_state=PRINT_DEL;
}





/* print the marked rectangles */
static int mark_print_page(int fd, struct t_mark_rect *rc,
			   struct mapwin *mw,int start_page,int pnum)
{
  char cmd_tpl[200];
  char buf[512];
  int x,y,w,h;
  double dim;
  get_mark_xywh(rc,&x,&y,&w,&h,&dim);
  struct pixmap_info *pinfo=get_map_rectangle(x,y,w,h);
  if (!tile_requests_processed()) {
    free_pinfo(pinfo);
    return 0;
  }
    
  dim=dim*25.4;

  /*  strcpy(tmp_tpl,"/tmp/mapprint.XXXXXX");
      mktemp(tmp_tpl); */
  /*  save_pinfo(tmp_tpl,pinfo); */

  setlocale(LC_NUMERIC,"C");
  snprintf(buf,sizeof(buf),"%%%%Page: %d %d\n",start_page,start_page+pnum);
  write(fd,buf,strlen(buf));
  snprintf(cmd_tpl,sizeof(cmd_tpl),
           "gsave 50 50 translate 72 72 scale 1 %f div dup scale 0 %lu %s\n",
	   dim,pinfo->height,rc->width_gt_height?"pop pop 90 rotate":"translate");
  write(fd,cmd_tpl,strlen(cmd_tpl));
  colordump_ps(fd,pinfo,0);
  {
    char *t="1 -1 scale\n";
    write(fd,t,strlen(t));
  }
  if (*mw->mark_line_list) {
    int mx,my,w,h;
    get_mark_xywh(rc,&mx,&my,&w,&h,&dim); 
    draw_marks_to_ps(*mw->mark_line_list,mx,my,w,h,fd);
  }
  { 
    char *t="grestore showpage\n";
    write(fd,t,strlen(t));
  }
  free_pinfo(pinfo);
  return 1;
}

static void print_dlg_cancel(void *data)
{
}

static gboolean print_timer_cb(void *data)
{
  char buf[512];
  struct print_job *pj=(struct print_job *)data;
  int fd=pj->fd;
  while (pj->page_data) {
    if (!tile_requests_processed()) {
      return TRUE;
    }
    if (mark_print_page(fd,(struct t_mark_rect *)g_list_first(pj->page_data)->data,
			(struct mapwin *)pj->data,pj->start_page,pj->page)) {
      pj->page++;
      free(g_list_first(pj->page_data)->data);
      pj->page_data=g_list_remove(pj->page_data,
				       g_list_first(pj->page_data)->data);
    }
  } 
  snprintf(buf,sizeof(buf),"%%EOF\n");
  write(fd,buf,strlen(buf));
  delete_print_job(pj);

  return FALSE;
}

static void print_dlg_ok(void *data, struct print_job *pj)
{
  struct mapwin *mw=(struct mapwin *)data;
  int i;
  GList *l;
  char buf[512];
  snprintf(buf,sizeof(buf),"%%!PS-Adobe-2.0\n%%%%Pages %d\n%%%%PageOrder: Ascend\n%%%%DocumentPaperSizes: %s\n%%%%EndComments\n",pj->end_page-pj->start_page+1,get_paper_name());
  write(pj->fd,buf,strlen(buf));
  for(i=1,l=g_list_first(mw->rect_list);l&&(i<pj->start_page);i++,l=g_list_next(l));
  for(l=g_list_first(mw->rect_list);l&&(i<=pj->end_page);l=g_list_next(l)) {
    struct t_mark_rect *mr=g_new0(struct t_mark_rect,1);
    *mr=*(struct t_mark_rect *)l->data;
    pj->page_data=g_list_append(pj->page_data,mr);
  }
  g_timeout_add(1000,print_timer_cb,pj);
  
}

static void mark_print(gpointer callback_data,
		       guint callback_action,
		       GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  create_printdlg(g_list_length(mw->rect_list), print_dlg_cancel, print_dlg_ok,
		  mw); /*
  g_list_foreach(mw->rect_list,mark_print_cb,callback_data);
  mark_del(callback_data,callback_action,w); */
}

/* save a point from the mark line */
void save_mark_item(gpointer data, gpointer user_data)
{
}



/* save the mark line */
static void save_mark_line(GtkWidget *w,
			   gpointer data)
{
  const char *fname;
  FILE *f;
  struct mapwin *mw;
  GtkWidget *combo;
  char *txt;
  mw=(struct mapwin *)gtk_object_get_user_data(GTK_OBJECT(data));
  fname=gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
  if (!*mw->mark_line_list)
    return;
  combo=GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(data),"format"));
#ifdef USE_GTK2
  txt=gtk_combo_box_get_active_text(GTK_COMBO_BOX(combo));
#else
  txt=gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(combo)->entry),
			    0,-1);
#endif
  if (!strcasecmp("GPX",txt)) {
    save_gpx(fname,*mw->mark_line_list);
  } else {
    f=fopen(fname,"w");
    if (!f)
      return;
    save_nmea(f,*mw->mark_line_list);
    fclose(f);
  }
}

static void save_mark_line_menucb(gpointer callback_data,
			     guint callback_action,
			     GtkWidget *w)
{
  GtkWidget *fs;
  GtkWidget *combo;
  fs=gtk_file_selection_new(_("select a name for the new nmea file"));
#ifdef USE_GTK2
  combo=gtk_combo_box_new_text();
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),"NMEA");
  gtk_combo_box_append_text(GTK_COMBO_BOX(combo),"GPX");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combo),1);
#else
  combo=gtk_combo_new();
  GList *l=g_list_append(g_list_append(NULL,"gpx"),"nmea");
  gtk_combo_set_popdown_strings(GTK_COMBO(combo),l);
  g_list_free(l);
  gtk_combo_set_value_in_list(GTK_COMBO(combo),TRUE,FALSE);
#endif
  gtk_box_pack_end(GTK_BOX(GTK_FILE_SELECTION(fs)->main_vbox),
		   combo,FALSE,TRUE,0);
  gtk_object_set_user_data(GTK_OBJECT(fs),callback_data);
  gtk_object_set_data(GTK_OBJECT(fs),"format",combo);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
			    "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),(void*)fs);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),"clicked",
		     GTK_SIGNAL_FUNC(save_mark_line),(void *)fs);
  gtk_signal_connect_object_after(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
				  "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  (void *)fs);
  gtk_widget_show_all(fs);
				
}


static void gps_filesel(GtkWidget *w, gpointer data)
{
  struct mapwin *mw;
  const char *f;
  GList *l;
  mw=(struct mapwin *)gtk_object_get_user_data(GTK_OBJECT(data));
  f=gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
  if (!f)
    return;
  load_gps_line(f,mw->mark_line_list);
  mouse_state=IN_WAY;
  mw->line_drawing=1;
  change_sidebar_cb(mw,0,NULL);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->linemode_but),1);
  l=g_list_last(*mw->mark_line_list);
  if (l) {
    struct t_punkt32 *p;
    p=(struct t_punkt32 *)l->data; 
    if (!p->single_point)
      center_map(mw,p->longg,p->latt);
  } 
}

static void osm_filesel(GtkWidget *w, gpointer data)
{
  struct mapwin *mw;
  const char *f;
  mw=(struct mapwin *)gtk_object_get_user_data(GTK_OBJECT(data));
  f=gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
  if (!f)
    return;

  load_osm_gfx(mw,f);
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);

}

static gboolean map_key_press(GtkWidget *w, GdkEventKey *ev, gpointer data)
{
  struct mapwin *mw=(struct mapwin *)data;
  int follow_gps=0;
  printf("key press %d\n",ev->keyval);
  if (GTK_WIDGET_HAS_FOCUS(mw->map)) {
  switch(ev->keyval) {
    case GDK_Up: GTK_ADJUSTMENT(mw->vadj)->value-=50; 
        gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->vadj)); break;
    case GDK_Down: GTK_ADJUSTMENT(mw->vadj)->value+=50;
        gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->vadj)); break;
    case GDK_Left: GTK_ADJUSTMENT(mw->hadj)->value-=50;
        gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->hadj)); break;
    case GDK_Right: GTK_ADJUSTMENT(mw->hadj)->value+=50; 
        gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->hadj)); break;
    case GDK_KP_Enter:
    case GDK_Return:
        follow_gps=1;
        break;

    default: return FALSE;
  }
  if (mw->follow_gps != follow_gps) {
    mw->follow_gps=follow_gps;
    check_item_set_state(mw,PATH_FOLLOW_GPS,mw->follow_gps);
  }
#ifndef USE_GTK2
  gtk_signal_emit_stop_by_name(GTK_OBJECT(w),"key_press_event");
#endif
  return TRUE;
  }
  return FALSE;
}


static void load_gps_menucb(gpointer callback_data,
			    guint callback_action,
			    GtkWidget *w)
{
  GtkWidget *fs;
  fs=gtk_file_selection_new(_("select a gps file"));
  gtk_object_set_user_data(GTK_OBJECT(fs),callback_data);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
			    "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),(void*)fs);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),"clicked",
		     GTK_SIGNAL_FUNC(gps_filesel),(void *)fs);
  gtk_signal_connect_object_after(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
				  "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  (void *)fs);
  gtk_widget_show_all(fs);
				
}

static void osm_filesel_save(GtkWidget *w, gpointer data)
{
  struct mapwin *mw;
  const char *f;
  mw=(struct mapwin *)gtk_object_get_user_data(GTK_OBJECT(data));
  f=gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
  if (!f)
    return;
  if (mw->osm_main_file) {
    osm_save_file(f,mw->osm_main_file,0);
  }
}

static void osm_filesel_savechanges(GtkWidget *w, gpointer data)
{
  struct mapwin *mw;
  const char *f;
  mw=(struct mapwin *)gtk_object_get_user_data(GTK_OBJECT(data));
  f=gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
  if (!f)
    return;
  if (mw->osm_main_file) {
    osm_save_file(f,mw->osm_main_file,1);
  }
}





static void load_osm_menucb(gpointer callback_data,
                            guint callback_action,
                            GtkWidget *w)
{
  GtkWidget *fs;
  fs=gtk_file_selection_new(_("select an osm file"));
  gtk_object_set_user_data(GTK_OBJECT(fs),callback_data);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
                            "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),(void*
)fs);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),"clicked",
                     GTK_SIGNAL_FUNC(osm_filesel),(void *)fs);
  gtk_signal_connect_object_after(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
                                  "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
                                  (void *)fs);
  gtk_widget_show_all(fs);
                                
}



static void clear_osm_menucb(gpointer callback_data,
			     guint callback_action,
			     GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  osm_clear_data(mw);
}


static void save_osmchange_menucb(gpointer callback_data,
				  guint callback_action,
				  GtkWidget *w)
{
  GtkWidget *fs;
  fs=gtk_file_selection_new(_("save OSM changes as OSC file"));
  gtk_object_set_user_data(GTK_OBJECT(fs),callback_data);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
			    "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),(void*)fs);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),"clicked",
		     GTK_SIGNAL_FUNC(osm_filesel_savechanges),(void *)fs);
  gtk_signal_connect_object_after(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
				  "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  (void *)fs);
  gtk_widget_show_all(fs);

}

#ifdef ENABLE_OSM_UPLOAD
struct upload_msg_dlg {
  GtkWidget *win;
  GtkWidget *entry;
  GtkWidget *username;
  GtkWidget *password;
  GtkWidget *savecheckbox;
  GtkWidget *cancelbut;
  GtkWidget *okbut;
  GtkWidget *msg;
  struct mapwin *mw;
  int uploading;
};

static void upl_check_close_cancel(GtkWidget *w,
                                   gpointer data)
{
  struct upload_msg_dlg *umd=(struct upload_msg_dlg *)data;
  if (!umd->uploading) {
#ifdef USE_GTK2
    gtk_grab_remove(umd->win);
#endif
    gtk_widget_destroy(umd->win);
  }
}                             

static int upld_finished(gpointer data)
{
  upl_check_close_cancel(NULL,data);
  return 0;   
}

static void disp_upload_msg(void *data, char *msg,
                             int finished)
{
  struct upload_msg_dlg *umd=(struct upload_msg_dlg *)data;
  gtk_label_set_text(GTK_LABEL(umd->msg),msg);
  if (finished) {
    umd->uploading=0;
    g_timeout_add(2000,upld_finished,umd);
  }
}

static void ok_osm_upload_cb(GtkWidget *w,
			     gpointer data)
{
  struct upload_msg_dlg *umd=(struct upload_msg_dlg *)data;
  char *txt=gtk_editable_get_chars(GTK_EDITABLE(umd->entry),0,-1);
  if (umd->mw->osm_main_file) {
    char *username;
    char *pw;
    username=gtk_editable_get_chars(GTK_EDITABLE(umd->username),0,-1);
    pw=gtk_editable_get_chars(GTK_EDITABLE(umd->password),0,-1);
    if ((strlen(username)>0)&&(strlen(pw)>0)) {
      umd->uploading=1;
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(umd->savecheckbox))) {
	cfg_set_string("osm_user",username);
	cfg_set_string("osm_password",pw);
	cfg_write_out();
      }
#ifdef USE_GTK2
      gtk_grab_add(umd->win);
#endif
      start_osm_upload(txt,username,pw,umd->mw->osm_main_file,disp_upload_msg,umd);
    } else {
      display_text_box(_("Please enter your OSM username and password!"));
      return;
    }
    g_free(username);
    g_free(pw);
  }
  gtk_widget_set_sensitive(umd->okbut,FALSE);
  gtk_widget_set_sensitive(umd->cancelbut,FALSE);
  //gtk_widget_destroy(umd->win);
}

static void upl_check_close(GtkWidget *w, GdkEventAny *event, gpointer data)
{
   upl_check_close_cancel(w,data);
}

static void upload_osm_menucb(gpointer callback_data,
			      guint callback_action,
			      GtkWidget *w)
{
  GtkWidget *label;
  char *cfgstr;
  struct mapwin *mw=(struct mapwin *)callback_data;;
  struct upload_msg_dlg *umd;
  if (!mw->osm_main_file)
    return;
  umd=malloc(sizeof(struct upload_msg_dlg));
  umd->win=gtk_dialog_new();
  umd->mw=mw;
  umd->uploading=0;
  label=gtk_label_new(_("Username"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
		     label,FALSE,FALSE,0);
  umd->username=gtk_entry_new();
  cfgstr=cfg_get_string("osm_user");
  if (cfgstr)
    gtk_entry_set_text(GTK_ENTRY(umd->username),cfgstr);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
		     umd->username,FALSE,TRUE,0);
  label=gtk_label_new(_("Password"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
		     label,FALSE,TRUE,0);
  umd->password=gtk_entry_new();
  cfgstr=cfg_get_string("osm_password");
  if (cfgstr)
    gtk_entry_set_text(GTK_ENTRY(umd->password),cfgstr);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
		     umd->password,FALSE,TRUE,0);
  gtk_entry_set_visibility(GTK_ENTRY(umd->password),FALSE);
  umd->savecheckbox=gtk_check_button_new_with_label(_("Save osm account data"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
                     umd->savecheckbox,FALSE,TRUE,0);
  label=gtk_label_new(_("Upload comment"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
                    label,FALSE,TRUE,0);
  umd->entry=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
		     umd->entry,FALSE,TRUE,0);
  umd->msg=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->vbox),
                     umd->msg,TRUE,TRUE,0);
  umd->okbut=gtk_button_new_with_label(_("OK"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->action_area),
		     umd->okbut,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(umd->okbut),"clicked",
		     GTK_SIGNAL_FUNC(ok_osm_upload_cb),
		     umd);
  umd->cancelbut=gtk_button_new_with_label(_("Cancel"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(umd->win)->action_area),
		     umd->cancelbut,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(umd->cancelbut),"clicked",
		     GTK_SIGNAL_FUNC(upl_check_close_cancel),umd);
  gtk_signal_connect(GTK_OBJECT(umd->win),"delete-event",
                     GTK_SIGNAL_FUNC(upl_check_close),umd);
  gtk_widget_show_all(umd->win);
}
#endif
static void save_osm_menucb(gpointer callback_data,
			    guint callback_action,
			    GtkWidget *w)
{
  GtkWidget *fs;
  fs=gtk_file_selection_new(_("save OSM data"));
  gtk_object_set_user_data(GTK_OBJECT(fs),callback_data);
  gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(fs)->cancel_button),
			    "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),(void*)fs);
  gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),"clicked",
		     GTK_SIGNAL_FUNC(osm_filesel_save),(void *)fs);
  gtk_signal_connect_object_after(GTK_OBJECT(GTK_FILE_SELECTION(fs)->ok_button),
				  "clicked",GTK_SIGNAL_FUNC(gtk_widget_destroy),
				  (void *)fs);
  gtk_widget_show_all(fs);
				
}
static gboolean set_gps_position(gpointer data) 
{
  char buf[80];
  char buf2[80];
  struct mapwin *mw = (struct mapwin *)data;
  struct nmea_pointinfo *nmea=&mw->last_nmea;
  if (mw->follow_gps) {
    center_map(mw,nmea->lon,nmea->lat);
  }
  if (mw->draw_heading_arrow) {
    gtk_widget_queue_draw_area(mw->map,0,0,
			       mw->page_width,
			       mw->page_height);
  }
  trip_stats_line(mw->stats,buf2,sizeof(buf2),1);
  snprintf(buf,sizeof(buf),"%c %s",nmea->state,buf2);
  gtk_label_set_text(GTK_LABEL(mw->gps_label),buf);
  return FALSE;
}



static void got_gps_position(struct nmea_pointinfo *nmea,
			     void *data)
{
  struct mapwin *mw = (struct mapwin *)data;
  int offset;
  struct t_punkt32 *p_new;
  mw->have_gpspos|=1;
  p_new=geosec2pointstruct(nmea->lon,nmea->lat);
  p_new->time=nmea->time;
  p_new->speed=nmea->speed;
  p_new->hdop=nmea->hdop;
  p_new->single_point=nmea->single_point;
  p_new->start_new=nmea->start_new;
  *mw->gps_line_list = g_list_append(*mw->gps_line_list,p_new);
  mw->last_nmea=*nmea;
 
  offset=find_nearst_point_latlon(*mw->mark_line_list, nmea);
  trip_stats_update(mw->stats, nmea, remaining_length(offset, mw, p_new));
  g_idle_add(set_gps_position,mw);
}

static void close_gps(struct mapwin *mw)
{
  if (mw->gpsf) {
    close_gps_file(mw->gpsf,1);
    gdk_input_remove(mw->gpstag);
  }
  mw->gpsf=NULL;
  gtk_label_set_text(GTK_LABEL(mw->gps_label),"");
}

static void got_gpsinput(gpointer data, int fd,
			 GdkInputCondition cond)
{
  struct mapwin *mw =(struct mapwin *)data;
  
  if (proc_gps_input(mw->gpsf,
		     got_gps_position, mw)<=0) {
    close_gps(mw);
    if (mw->gpstimertag)
      g_source_remove(mw->gpstimertag);
    mw->gpstimertag=0; 
    display_text_box(_("Lost GPS connection"));
  } else { 
    if (mw->gpstimertag) {
      g_source_remove(mw->gpstimertag);
      mw->gpstimertag=g_timeout_add_full(0,3000,gps_timer_first,mw,NULL);
    }
  }
}

#define GPSD_COMMAND_NEW "?WATCH={\"enable\":true,\"nmea\":true}\n"
/* send enable command for new gpsd */
static int gps_timer_second(void *data)
{
  struct mapwin *mw = (struct mapwin *)data;
  if (mw->gpsf) {
    write(1,GPSD_COMMAND_NEW,strlen(GPSD_COMMAND_NEW));
    gps_writeback(mw->gpsf,GPSD_COMMAND_NEW,strlen(GPSD_COMMAND_NEW));
  }
  return FALSE;
}

/* send r if no nmea data arrives */
static int gps_timer_first(void *data)
{
  struct mapwin *mw = (struct mapwin *)data;
  if (mw->gpsf) {
    gps_writeback(mw->gpsf,"r+\n",3); 
    mw->gpstimertag=g_timeout_add_full(0,3000,gps_timer_second,mw,NULL);
  }
  return FALSE;
}

static int gps_timer(void *data)
{
  struct mapwin *mw = (struct mapwin *)data;
  if (mw->gpsf == NULL) {
    return FALSE;
  }
  if (!mw->have_gpspos) {
    char buf[80];
    buf[0]='V';
    buf[1]=' ';
    trip_stats_line(mw->stats,buf+2,sizeof(buf)-2,0);
    gtk_label_set_text(GTK_LABEL(mw->gps_label),buf);
  } else {
    mw->have_gpspos=0;
  }
  return TRUE;
}

static void gps_connected(struct connection_dialog *cdlg,
			  int fd, void *data)
{
  struct mapwin *mw = (struct mapwin *)data;
  mw->gpsf=open_gps_file(fd);
  mw->gpstag=gdk_input_add(fd, GDK_INPUT_READ,
			   got_gpsinput,mw);
  mw->gpstimertag=g_timeout_add_full(0,3000,gps_timer_first,mw,NULL);
  g_timeout_add_full(0,3000,gps_timer,mw,NULL);
  mw->follow_gps=1;
  check_item_set_state(mw,PATH_FOLLOW_GPS,mw->follow_gps);
  
}
			  

static void connect_gps_cb(gpointer callback_data,
			guint callback_action,
			GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  close_gps(mw);
#ifndef _WIN32
  show_connection_dialog(mw->cdlg);
#endif
}

static void switch_searchdisp(gpointer callback_data,
			      guint callback_action,
			      GtkWidget *w) 
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  mw->disp_search=GTK_CHECK_MENU_ITEM(w)->active;
  if (!mw->disp_search) {
    gtk_widget_hide(mw->searchbox);
  } else {
    gtk_widget_show(mw->searchbox);
  }

}


static void switch_crosshair(gpointer callback_data,
			   guint callback_action,
			   GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  mw->draw_crosshair=GTK_CHECK_MENU_ITEM(w)->active;
}

static void switch_heading_arrow(gpointer callback_data,
				 guint callback_action,
				 GtkWidget*w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  mw->draw_heading_arrow=GTK_CHECK_MENU_ITEM(w)->active;
}

static void switch_followgps(gpointer callback_data,
			     guint callback_action,
			     GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  mw->follow_gps=GTK_CHECK_MENU_ITEM(w)->active;
}

static void close_gps_cb(gpointer callback_data,
                         guint callback_action,
                         GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  close_gps(mw);
}

static void zoom_in(struct mapwin *mw)
{
  if (globalmap.first && (!globalmap.zoomable))
    return;
  double lon,lat,x,y;
  point2geosec(&lon,&lat,(double)(mw->page_x+mw->page_width/2),
	       (double)(mw->page_y+mw->page_height/2));
  if (globalmap.zoomable) {
    globalmap.zoomfactor++;
    globalmap.xfactor=globalmap.orig_xfactor*(double)(1<<(globalmap.zoomfactor-1));
    globalmap.yfactor=globalmap.orig_yfactor*(double)(1<<(globalmap.zoomfactor-1));
  } else {
    globalmap.xfactor*=2;
    globalmap.yfactor*=2;
    globalmap.fullwidth*=2;
    globalmap.fullheight*=2;
    if (!globalmap.fullwidth)
      globalmap.fullwidth++;
    if (!globalmap.fullheight)
      globalmap.fullheight++;
  }
  calc_mapoffsets();
  GTK_ADJUSTMENT(mw->hadj)->upper=globalmap.fullwidth*(double)(1<<(globalmap.zoomfactor-1));
  GTK_ADJUSTMENT(mw->vadj)->upper=globalmap.fullheight*(double)(1<<(globalmap.zoomfactor-1));
  if (mw->osm_main_file)
    recalc_node_coordinates(mw,mw->osm_main_file);
  geosec2point(&x,&y,lon,lat);
  center_map(mw,lon,lat);
  mapwin_draw(mw,mw->map->style->fg_gc[mw->map->state],globalmap.first,
              mw->page_x,mw->page_y,0,0,mw->page_width,mw->page_height);
  gtk_widget_queue_draw_area(mw->map,0,0,mw->page_width,mw->page_height);

}

static void zoom_in_cb(gpointer callback_data,
			guint callback_action,
			GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  zoom_in(mw);
}

static void zoom_in_cbbut(GtkWidget *w, gpointer data)
{
  zoom_in((struct mapwin *)data);
  gtk_widget_grab_focus(((struct mapwin *)data)->map);
}


static void zoom_out(struct mapwin *mw)
{
  if (globalmap.first && (!globalmap.zoomable))
    return;
  double lon,lat,x,y;
  point2geosec(&lon,&lat,(double)(mw->page_x+mw->page_width/2),
	       (double)(mw->page_y+mw->page_height/2));
  if (globalmap.zoomable) {
    if (globalmap.zoomfactor<2)
      return;
    globalmap.zoomfactor--;
    globalmap.xfactor=globalmap.orig_xfactor*(double)(1<<(globalmap.zoomfactor-1));
    globalmap.yfactor=globalmap.orig_yfactor*(double)(1<<(globalmap.zoomfactor-1));
  } else {
    globalmap.xfactor/=2;
    globalmap.yfactor/=2;
    globalmap.fullwidth/=2;
    globalmap.fullheight/=2;
  }
  if (!globalmap.fullwidth)
    globalmap.fullwidth++;
  if (!globalmap.fullheight)
    globalmap.fullheight++;
  GTK_ADJUSTMENT(mw->hadj)->upper=globalmap.fullwidth*(1<<(globalmap.zoomfactor-1));
  GTK_ADJUSTMENT(mw->vadj)->upper=globalmap.fullheight*(1<<(globalmap.zoomfactor-1));
  calc_mapoffsets();
  if (mw->osm_main_file)
    recalc_node_coordinates(mw,mw->osm_main_file);
  geosec2point(&x,&y,lon,lat);
  center_map(mw,lon,lat);
  mapwin_draw(mw,mw->map->style->fg_gc[mw->map->state],globalmap.first,
              mw->page_x,mw->page_y,0,0,mw->page_width,mw->page_height);
  gtk_widget_queue_draw_area(mw->map,0,0,mw->page_width,mw->page_height);
}

static void zoom_out_cb(gpointer callback_data,
			guint callback_action,
			GtkWidget *w)
{
  zoom_out((struct mapwin *)callback_data);
}

static void zoom_out_cbbut(GtkWidget *w, gpointer data)
{
  zoom_out((struct mapwin *)data);
  gtk_widget_grab_focus(((struct mapwin *)data)->map);
}

static void set_start_pos_cb(gpointer callback_data,
			     guint callback_action,
			     GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  double lat,lon;
  point2geosec(&lon,&lat,mw->page_x+mw->page_width/2,
	       mw->page_y+mw->page_height/2);
  create_startposition_dialog(lat,lon,globalmap.zoomfactor);
}


static void switch_requesttile(gpointer callback_data,
			       guint callback_action,
			       GtkWidget *w)
{
  tile_request_mode=callback_action;
}

static void display_about_box(gpointer callback_data,
			      guint callback_action,
			      GtkWidget *w)
{
  char *b;
  b=g_strdup_printf(_("%s %s\n"
"Copyright (C) 2008 Andreas Kemnade\n"
"This is free software.  You may redistribute copies of it under the terms of\n"
"the GNU General Public License version 3 or any later version <http://www.gnu.org/licenses/gpl.html>\n"
		      "There is NO WARRANTY, to the extent permitted by law."),PACKAGE,VERSION);
  
  display_text_box(b);
  g_free(b);
}

static void sel_layer_cb(gpointer callback_data,
			 guint callback_action,
			 GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  if (callback_action >= MAX_LINE_LIST)
    return;
  mw->mark_line_list=mw->all_line_lists+callback_action;
}

static void download_osm_failed_cb(const char *url, const char *filename,
				   void *data)
{
  display_text_box(_("downloading OSM data failed"));

}

static void download_osm_finished_cb(const char *url, const char *filename,
				     void *data)
{
  struct mapwin *mw=(struct mapwin *)data;
  display_text_box(_("downloading OSM data finished"));

  load_osm_gfx(mw,filename);
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
  unlink(filename);
}

static void download_osm_data_cb(gpointer callback_data,
				 guint callback_acction,
				 GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  double maxlon=-180;
  double maxlat=-90;
  double minlon=180;
  double minlat=90;
  char *url;
  char *urlbase;
  GList *l=*mw->mark_line_list;
  for(l=g_list_first(l);l;l=g_list_next(l)) {
    struct t_punkt32 *p=(struct t_punkt32 *)l->data;
    double lon=p->longg;
    double lat=p->latt;
    if (lon>maxlon)
      maxlon=lon;
    if (lon<minlon)
      minlon=lon;
    if (lat>maxlat)
      maxlat=lat;
    if (lat<minlat)
      minlat=lat;
  }
  if ((maxlat<minlat)||(maxlon<minlon)) {
    display_text_box(_("No area selected,\n draw some lines around the area \nfor which you want the osm data\n or select the correct line layer"));
    return;
  }
  setlocale(LC_NUMERIC,"C");
  urlbase=getenv("OSMAPIURL");
  if (!urlbase)
    urlbase="http://www.openstreetmap.org/api/0.6";
#ifndef _WIN32
  url=g_strdup_printf("%s/map?bbox=%f,%f,%f,%f",urlbase,
		      minlon,minlat,maxlon,maxlat);
#else
/* there seems to be broken g_strdup_printf()s out there which ignore LC_NUMERIC */
  {
    char b[512];
    _snprintf(b,sizeof(b),"%s/map?bbox=%f,%f,%f,%f",minlon,minlat,maxlon,maxlat);
    url=strdup(b); 
  }
#endif
  get_http_file(url,NULL,download_osm_finished_cb,
		download_osm_failed_cb,NULL,mw);
}

static void switch_draw_all(gpointer callback_data,
                            guint callback_action,
                            GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  mw->color_line=GTK_CHECK_MENU_ITEM(w)->active;
}


static void tripstats_cb(gpointer callback_data,
			 guint callback_action,
			 GtkWidget *w)
{
  struct mapwin *mw=(struct mapwin *)callback_data;
  trip_stats_show(mw->stats);
}

static void pagesize_cb(gpointer callback_data,
			guint callback_action,
			GtkWidget *w)
{
  select_pagesize();
}

static void change_sidebar_cb(gpointer callback_data,
			      guint callback_action,
			      GtkWidget *w)
{
  GList *l;
  struct sidebar_mode *sm;
  struct mapwin *mw=(struct mapwin *)callback_data;
  for(l=g_list_first(mw->modelist);l;l=g_list_next(l)) {
    sm=(struct sidebar_mode *)l->data;
    gtk_widget_hide(sm->w);
  }
  l=g_list_nth(mw->modelist,callback_action);
  sm=(struct sidebar_mode *)l->data;
  gtk_widget_show(sm->w);
}

GtkWidget *create_menu(struct mapwin *mw)
{
  int i;
  GtkAccelGroup *accel_group;
  GtkItemFactoryEntry *allitems;
  GtkItemFactoryEntry *osmitems;
  static GtkItemFactoryEntry enditems[]={
    {N_("/Help/About"),NULL,GTK_SIGNAL_FUNC(display_about_box),0,NULL}};
  static GtkItemFactoryEntry mainitems[]={
    {N_("/Project/Printing/mark page in scale 1:50000"),NULL,GTK_SIGNAL_FUNC(mark_for_print),50,NULL},
    {N_("/Project/Printing/mark page in scale 1:25000"),NULL,GTK_SIGNAL_FUNC(mark_for_print),25,NULL},
    {N_("/Project/Printing/mark page in scale 1:10000"),NULL,GTK_SIGNAL_FUNC(mark_for_print),10,NULL},
    {N_("/Project/Printing/mark page in scale 1:100000"),NULL,GTK_SIGNAL_FUNC(mark_for_print),100,NULL},
    {N_("/Project/Printing/mark page in scale 1:200000"),NULL,GTK_SIGNAL_FUNC(mark_for_print),200,NULL},
    {N_("/Project/Printing/mark page in scale 1:1000000"),NULL,GTK_SIGNAL_FUNC(mark_for_print),1000,NULL},
    {N_("/Project/Printing/delete marked areas"),NULL,GTK_SIGNAL_FUNC(mark_del),0,NULL},
    {N_("/Project/Printing/print marked areas"),NULL,GTK_SIGNAL_FUNC(mark_print),0,NULL},
    {N_("/Project/Printing/delete marked area"),NULL,GTK_SIGNAL_FUNC(mark_del_one),0,NULL},
    {N_("/Project/Printing/select page size..."),NULL,GTK_SIGNAL_FUNC(pagesize_cb),0,NULL},
    {N_("/Project/save route (current layer)"),NULL,GTK_SIGNAL_FUNC(save_mark_line_menucb),0,NULL},
    {N_("/Project/load GPS data"),NULL,GTK_SIGNAL_FUNC(load_gps_menucb),0,NULL},
    {N_("/Project/load OSM data"),NULL,GTK_SIGNAL_FUNC(load_osm_menucb),0,NULL},
    {N_("/Project/download OSM data for selected region"),NULL,GTK_SIGNAL_FUNC(download_osm_data_cb),0,NULL},
    {N_("/Project/save OSM data"),NULL,GTK_SIGNAL_FUNC(save_osm_menucb),0,NULL},
    {N_("/Project/save OSM changes (OSC file)"),NULL,GTK_SIGNAL_FUNC(save_osmchange_menucb),0,NULL},
    {N_("/Project/clear OSM data"),NULL,GTK_SIGNAL_FUNC(clear_osm_menucb),0,NULL},
#ifdef ENABLE_OSM_UPLOAD
    {N_("/Project/upload OSM data"),NULL,GTK_SIGNAL_FUNC(upload_osm_menucb),
     0,NULL},
#endif
#ifndef _WIN32
    {N_("/Project/Connect to GPS receiver"),NULL,GTK_SIGNAL_FUNC(connect_gps_cb),0,NULL},
#endif
    {PATH_DISP_SEARCH_N,NULL,GTK_SIGNAL_FUNC(switch_searchdisp),0,"<CheckItem>"},
    {PATH_DISP_CROSSHAIR_N,NULL,GTK_SIGNAL_FUNC(switch_crosshair),0,"<CheckItem>"},
    {PATH_DISP_HEADING_ARROW_N,NULL,GTK_SIGNAL_FUNC(switch_heading_arrow),0,"<CheckItem>"},
    {PATH_FOLLOW_GPS_N,NULL,GTK_SIGNAL_FUNC(switch_followgps),0,"<CheckItem>"},
    {N_("/View/Trip stats"),NULL,GTK_SIGNAL_FUNC(tripstats_cb),0,NULL},
    {N_("/View/Select line layer/0"),NULL,GTK_SIGNAL_FUNC(sel_layer_cb),0,"<RadioItem>"},
    {N_("/View/Select line layer/1"),NULL,GTK_SIGNAL_FUNC(sel_layer_cb),1,N_("/View/Select line layer/0")},
    {N_("/View/Select line layer/2"),NULL,GTK_SIGNAL_FUNC(sel_layer_cb),2,N_("/View/Select line layer/0")},
    {N_("/View/Select line layer/3"),NULL,GTK_SIGNAL_FUNC(sel_layer_cb),3,N_("/View/Select line layer/0")},
    {N_("/View/Select line layer/4 (live gps)"),NULL,GTK_SIGNAL_FUNC(sel_layer_cb),4,N_("/View/Select line layer/0")},
    {PATH_DISP_COLOR_N,NULL,GTK_SIGNAL_FUNC(switch_draw_all),0,"<CheckItem>"},
    {N_("/View/Request tiles/request missing"),NULL,GTK_SIGNAL_FUNC(switch_requesttile),1,"<RadioItem>"},
    {N_("/View/Request tiles/request never"),NULL,GTK_SIGNAL_FUNC(switch_requesttile),0,N_("/View/Request tiles/request missing")},
    {N_("/View/Request tiles/older than one day"),NULL,GTK_SIGNAL_FUNC(switch_requesttile),86400,N_("/View/Request tiles/request missing")},
    {N_("/View/Disconnect GPS"),NULL,GTK_SIGNAL_FUNC(close_gps_cb),0,NULL},
    {N_("/View/Set start position..."),NULL,GTK_SIGNAL_FUNC(set_start_pos_cb),0,NULL},
    {PATH_ZOOM_OUT_N,NULL,GTK_SIGNAL_FUNC(zoom_out_cb),0,NULL},
    {PATH_ZOOM_IN_N,NULL,GTK_SIGNAL_FUNC(zoom_in_cb),0,NULL},
    {N_("/Project/quit"),NULL,GTK_SIGNAL_FUNC(gtk_main_quit),0,NULL}};
  
  int nitems=sizeof(mainitems)/sizeof(mainitems[0]);
  int n_enditems=sizeof(enditems)/sizeof(enditems[0]);
  int n_osm_items=0;
  osmitems=get_osm_menu_items(&n_osm_items);
  allitems=calloc(n_enditems+nitems+n_osm_items,sizeof(GtkItemFactoryEntry));
  memcpy(allitems,mainitems,sizeof(mainitems));
  if (n_osm_items>0)
    memcpy(allitems+nitems,osmitems,n_osm_items*sizeof(GtkItemFactoryEntry));
  nitems+=n_osm_items;
  memcpy(allitems+nitems,enditems,n_enditems*sizeof(GtkItemFactoryEntry));
  nitems+=n_enditems;
  for(i=0;i<nitems;i++) {
    allitems[i].path=_(allitems[i].path);
    allitems[i].item_type=_(allitems[i].item_type);
  }
  accel_group=gtk_accel_group_new();   
  mw->fac=gtk_item_factory_new(GTK_TYPE_MENU_BAR,"<Main>",accel_group); 
  gtk_item_factory_create_items(mw->fac,nitems,allitems,mw);

  gtk_window_add_accel_group(GTK_WINDOW(mw->mainwin),accel_group); 
  return gtk_item_factory_get_widget(mw->fac,"<Main>");
}

static void add_sidebar_chooser(struct mapwin *mw)
{
  GList *l;
  int i;
  struct sidebar_mode *sm1=(struct sidebar_mode *) g_list_first(mw->modelist)->data;
  char *radiopath=g_strdup_printf(_("/View/Mode/%s"),sm1->name);
  i=0;
  for(l=g_list_first(mw->modelist);l;l=g_list_next(l),i++) {
    GtkItemFactoryEntry ife;
    struct sidebar_mode *sm=(struct sidebar_mode *)l->data;
    ife.path=g_strdup_printf(_("/View/Mode/%s"),sm->name);
    ife.accelerator=NULL;
    ife.callback=change_sidebar_cb;
    ife.callback_action=i;
    ife.item_type=(sm==sm1)?"<RadioItem>":radiopath;
    gtk_item_factory_create_items(mw->fac,1,&ife,mw);
    g_free(ife.path);
  }
  g_free(radiopath);
}

static void set_panmode_cb(GtkWidget *w, gpointer data)

{
  struct mapwin *mw=(struct mapwin *)data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->panmode_but))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->linemode_but),FALSE);
    mouse_state=PAN;
  }
  gtk_widget_grab_focus(mw->map);
}

static void set_linemode_cb(GtkWidget *w, gpointer data)

{
  struct mapwin *mw=(struct mapwin *)data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->linemode_but))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->panmode_but),FALSE);
    if (mw->line_drawing)
      mouse_state=IN_WAY;
    else
      mouse_state=START_WAY;
  }
  gtk_widget_grab_focus(mw->map);
}



struct mapwin * create_mapwin()
{
  int events;
  char *titles[2]={N_("Name"),N_("is in")};
  struct mapwin *w;
  GtkWidget *table1;
  GtkWidget *hb1;
  GtkWidget *okbut;
  GtkWidget *vb1;
  GtkWidget *label2;
  GtkTooltips *tt;
  struct sidebar_mode *sm;
  titles[0]=_(titles[0]);
  titles[1]=_(titles[1]);
  w=calloc(1,sizeof(struct mapwin));
  w->rect_list=NULL;
  w->mark_line_list=w->all_line_lists;
  w->gps_line_list=w->all_line_lists+MAX_LINE_LIST-1;
  w->mark_str=NULL;
  w->has_path=0;
  w->follow_gps=1;
  w->map_store=NULL;
  w->line_drawing=0;
  w->mouse_move_str=NULL;
  w->mouse_moved=0;
  w->stats=trip_stats_new();
  w->mainwin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  tt=gtk_tooltips_new();
  vb1=gtk_vbox_new(FALSE,1);
  gtk_box_pack_start(GTK_BOX(vb1),create_menu(w),FALSE,FALSE,1);
  hb1=gtk_hbox_new(FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb1),hb1,TRUE,TRUE,1);
  w->bottom_box=gtk_hbox_new(FALSE,0);
  gtk_box_pack_start(GTK_BOX(vb1),w->bottom_box,FALSE,FALSE,1);
  w->entf_label=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(w->bottom_box),w->entf_label,FALSE,FALSE,1);
  w->gps_label=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(w->bottom_box),w->gps_label,FALSE,FALSE,1);
  gtk_container_add(GTK_CONTAINER(w->mainwin),vb1);
  table1=gtk_table_new(2,2,FALSE);
  w->page_x=0;
  w->page_y=0;
  w->vadj=gtk_adjustment_new(0,0,globalmap.fullwidth,10,1,
			     1518);
  w->hadj=gtk_adjustment_new(0,0,globalmap.fullheight,10,1,
			     1032);
  w->hscr=gtk_hscrollbar_new(GTK_ADJUSTMENT(w->hadj));
  w->vscr=gtk_vscrollbar_new(GTK_ADJUSTMENT(w->vadj));
  gtk_table_attach(GTK_TABLE(table1),w->vscr,0,1,0,1,GTK_FILL,
		   GTK_EXPAND | GTK_FILL,
		   0,0);
  gtk_table_attach(GTK_TABLE(table1),w->hscr,1,2,1,2,
		   GTK_EXPAND | GTK_FILL,
		   GTK_FILL,
		   0,0);
  w->map=gtk_drawing_area_new();
  gtk_table_attach(GTK_TABLE(table1),w->map,1,2,0,1,
		   GTK_EXPAND | GTK_FILL,
		   GTK_EXPAND | GTK_FILL,
		   0,0);
  vb1=gtk_vbox_new(FALSE,0);
/*
  label=gtk_label_new(_("file name"));
  gtk_box_pack_start(GTK_BOX(vb1),label,FALSE,TRUE,0);
  w->dlabel=gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(vb1),w->dlabel,FALSE,TRUE,0);
*/
  label2=gtk_label_new(_("place name:"));
  gtk_box_pack_start(GTK_BOX(vb1),label2,FALSE,TRUE,10);
  w->entry=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(vb1),w->entry,FALSE,TRUE,0);
  okbut=gtk_button_new_with_label(_("search"));
  gtk_box_pack_start(GTK_BOX(vb1),okbut,FALSE,TRUE,0);
  w->list=gtk_clist_new_with_titles(2,titles);
  gtk_clist_set_column_auto_resize(GTK_CLIST(w->list),0,TRUE);
  gtk_clist_set_column_auto_resize(GTK_CLIST(w->list),1,TRUE);

  w->scrwin=gtk_scrolled_window_new(NULL,NULL);
  gtk_container_add(GTK_CONTAINER(w->scrwin),w->list);
  gtk_box_pack_start(GTK_BOX(vb1),w->scrwin,TRUE,TRUE,5);
  w->koords_label=gtk_label_new(" ");
  gtk_box_pack_start(GTK_BOX(vb1),w->koords_label,FALSE,FALSE,5);
  
  gtk_box_pack_start(GTK_BOX(hb1),vb1,FALSE,TRUE,0);
  w->searchbox=vb1;
  check_item_set_state(w,PATH_FOLLOW_GPS,w->follow_gps);
  w->disp_search=0;
  if (globalmap.placefilelist) {
    w->disp_search=1;
  } else {
    menu_item_set_state(w,PATH_DISP_SEARCH,0);
  }
  check_item_set_state(w,PATH_DISP_SEARCH,w->disp_search);
  
  gtk_box_pack_start(GTK_BOX(hb1),table1,TRUE,TRUE,0);


  gtk_signal_connect(GTK_OBJECT(w->map),"button_press_event",
		     GTK_SIGNAL_FUNC(map_click),w);
  gtk_signal_connect(GTK_OBJECT(w->map),"button_release_event",
		     GTK_SIGNAL_FUNC(map_click_release),w);
#ifdef USE_GTK2
  gtk_signal_connect(GTK_OBJECT(w->map),"scroll_event",
                     GTK_SIGNAL_FUNC(map_scrollwheel),w);
#endif
  gtk_signal_connect(GTK_OBJECT(w->map),"focus_in_event",
                     GTK_SIGNAL_FUNC(map_focus_in),w);
  gtk_signal_connect(GTK_OBJECT(w->map),"focus_out_event",
                     GTK_SIGNAL_FUNC(map_focus_out),w);
  gtk_signal_connect(GTK_OBJECT(w->map),"key_press_event",
                     GTK_SIGNAL_FUNC(map_key_press),w);
/*  gtk_signal_connect(GTK_OBJECT(w->mainwin),"key_press_event",
                     GTK_SIGNAL_FUNC(map_key_press),w); */
  gtk_signal_connect(GTK_OBJECT(w->map),"motion_notify_event",
		     GTK_SIGNAL_FUNC(map_move),w);
  gtk_signal_connect(GTK_OBJECT(w->map),"expose_event",
		     GTK_SIGNAL_FUNC(expose_event),w);
  gtk_signal_connect(GTK_OBJECT(w->map),"configure_event",
		     GTK_SIGNAL_FUNC(config_event),w);
  gtk_signal_connect(GTK_OBJECT(w->map),"selection_get",
                     GTK_SIGNAL_FUNC(paste_coords),NULL);
  gtk_selection_add_target(w->map, GDK_SELECTION_PRIMARY,
              GDK_SELECTION_TYPE_STRING,1);
  gtk_signal_connect(GTK_OBJECT(w->hadj),"value-changed",
		     GTK_SIGNAL_FUNC(scrollbar_moved),w);
  gtk_signal_connect(GTK_OBJECT(w->vadj),"value-changed",
		     GTK_SIGNAL_FUNC(scrollbar_moved),w);
  gtk_signal_connect(GTK_OBJECT(w->mainwin),"destroy",
		     GTK_SIGNAL_FUNC(gtk_main_quit),NULL);
  gtk_signal_connect(GTK_OBJECT(okbut),"clicked",
		     GTK_SIGNAL_FUNC(suche_ort_cb),w);
  gtk_signal_connect(GTK_OBJECT(w->entry),"changed",
		     GTK_SIGNAL_FUNC(list_orte),w);
  gtk_signal_connect(GTK_OBJECT(w->list),"select_row",
		     GTK_SIGNAL_FUNC(ort_select),w);
  gtk_widget_show(w->hscr);
  gtk_widget_show(w->vscr);

  gtk_widget_show(w->map);
  gtk_widget_show(w->koords_label);
  gtk_widget_show(okbut);
  gtk_widget_show(label2);
/*  gtk_widget_show(label); */
  gtk_widget_show(w->entry);
/*  gtk_widget_show(w->dlabel); */
  gtk_widget_show(w->list);
  gtk_widget_show(w->scrwin);
  gtk_widget_show(vb1);
  gtk_widget_show(table1);
  gtk_widget_show(hb1);
  gtk_widget_show_all(w->mainwin);
  if (!w->disp_search)
    gtk_widget_hide(w->searchbox);
  events=(int)gdk_window_get_events(w->map->window);
  init_osm_draw(w);
  append_osm_edit_line(w,hb1);
  sm=calloc(sizeof(struct sidebar_mode ),1);
  sm->name=_("Move mode");
  sm->w=gtk_vbox_new(FALSE,0);
  w->modelist=g_list_prepend(w->modelist,sm);
  gtk_box_pack_start(GTK_BOX(hb1),sm->w,FALSE,FALSE,0);
  w->panmode_but=make_pixmap_toggle_button(w,panmode);
  gtk_widget_show(w->panmode_but);
  gtk_tooltips_set_tip(tt,w->panmode_but,_("pan mode"),NULL);
  gtk_signal_connect(GTK_OBJECT(w->panmode_but),"clicked",
		     GTK_SIGNAL_FUNC(set_panmode_cb),w);
  gtk_box_pack_start(GTK_BOX(sm->w),w->panmode_but,TRUE,TRUE,0);
  w->linemode_but=make_pixmap_toggle_button(w,linemode);
  gtk_widget_show(w->linemode_but);
  gtk_tooltips_set_tip(tt,w->linemode_but,_("route drawing mode"),NULL);
  gtk_signal_connect(GTK_OBJECT(w->linemode_but),"clicked",
		     GTK_SIGNAL_FUNC(set_linemode_cb),w);
  gtk_box_pack_start(GTK_BOX(sm->w),w->linemode_but,TRUE,TRUE,0);
  w->zoomin_but=make_pixmap_button(w,zoomin);
  gtk_signal_connect(GTK_OBJECT(w->zoomin_but),"clicked",
		     GTK_SIGNAL_FUNC(zoom_in_cbbut),w);
  gtk_widget_show(w->zoomin_but);
  gtk_box_pack_start(GTK_BOX(sm->w),w->zoomin_but,TRUE,TRUE,0);
  w->zoomout_but=make_pixmap_button(w,zoomout);
  gtk_signal_connect(GTK_OBJECT(w->zoomout_but),"clicked",
		     GTK_SIGNAL_FUNC(zoom_out_cbbut),w);
  gtk_box_pack_start(GTK_BOX(sm->w),w->zoomout_but,TRUE,TRUE,0);
  gtk_widget_show(w->zoomout_but);
  gtk_widget_show(sm->w);
  add_sidebar_chooser(w);
  
  gdk_window_set_events(w->map->window,
			(GdkEventMask )(events+GDK_BUTTON_PRESS_MASK+GDK_BUTTON_RELEASE_MASK+
					GDK_POINTER_MOTION_MASK+GDK_KEY_PRESS_MASK));
  GTK_WIDGET_SET_FLAGS(w->map,GTK_CAN_FOCUS | GTK_CAN_DEFAULT);
  return w;
}



static void add_pkt_to_list(GList **l, int x, int y)
{
  struct t_punkt32 *p=calloc(1,sizeof(struct t_punkt32));
  p->x=x<<globalmap.zoomshift;
  p->y=y<<globalmap.zoomshift;
  point2geosec(&p->longg,&p->latt,(double)x,(double)y);
  p->time=0;
  *l=g_list_append(*l,p);
}

static void add_path_to_mark_list(GList **l, int krid)
{
  GList *l2=NULL;
  int last_str,len;
  struct strasse *str;
  if (krid<0)
    return;
  get_way_info(krid,&len,&last_str);
  if (last_str<0)
    return;
  while(len!=0) {
    if (last_str<0) {
      printf("Error in wayfinding\n");
      return;
    }
    str=get_str_id(last_str);
    if (str) {
      int is_anfang;
      is_anfang=(str->anfang_krid==krid);
      str_to_mark_list(add_pkt_to_list,&l2,str,is_anfang);
      krid=is_anfang?str->end_krid:str->anfang_krid;
    } else {
      krid=-1;
    }
    if (krid<0) {
      printf("Error in wayfinding\n");
      break;
    }
    get_way_info(krid,&len,&last_str);
  } 
  l2=g_list_reverse(l2);
  *l=g_list_concat(*l,l2);
}

static void restart_prog(GtkWidget *w,gpointer data)
{
  char *prog=(char *)data;
  execlp(prog,prog,NULL);
}

/* quick hack to choose a configuration */
static void config_chooser()
{
  GtkWidget *win;
  GtkWidget *vbox;
  GtkWidget *but;
  win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(win),_("Choose a map"));
  
  vbox=gtk_vbox_new(TRUE,0);
  gtk_container_add(GTK_CONTAINER(win),vbox);
  but=gtk_button_new_with_label(_("OSM Tiles@home"));
  gtk_box_pack_start(GTK_BOX(vbox),but,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(restart_prog),"mumpot-tah");
  but=gtk_button_new_with_label(_("OSM Mapnik"));
  gtk_box_pack_start(GTK_BOX(vbox),but,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(restart_prog),"mumpot-mapnik");
  but=gtk_button_new_with_label(_("OSM Cyclemap"));
  gtk_box_pack_start(GTK_BOX(vbox),but,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(restart_prog),"mumpot-cyclemap");
  gtk_widget_show_all(win);
  gtk_main();

  exit(1);
}

static void upd_startposition(struct mapwin *mw)
{
  double lon;
  double lat;
  point2geosec(&lon,&lat,(double)(mw->page_x+mw->page_width/2),
		(double)(mw->page_y+mw->page_height/2));
  startposition_update_lastpos(0,lat,lon,globalmap.zoomfactor);
  if (mw->gps_line_list) {
    GList *l=*mw->gps_line_list;
    if (l) {
      startposition_update_lastpos(1,mw->last_nmea.lat,
				   mw->last_nmea.lon,
				   globalmap.zoomfactor);
    }
  }
}

int main(int argc, char **argv)
{
  char buf[512];
  double startlon,startlat;
  GList *l;
  int i;
#ifdef USE_IMLIB
  GdkImlibInitParams imlibinit={
    PARAMS_REMAP | PARAMS_FASTRENDER | PARAMS_HIQUALITY,
    0,NULL,0,0,0,
    1,1,
    0,0,0,0};
#endif
  struct mapwin *mw;
  GdkColormap *cmap;
  const char *configfilename;
#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#ifdef USE_GTK2
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8"); 
#endif
  textdomain (GETTEXT_PACKAGE);
#endif
  gtk_set_locale ();
  gtk_init (&argc, &argv);
  setlocale(LC_NUMERIC,"C");
  memset(&globalmap, 0, sizeof(globalmap));
  globalmap.fullwidth=50000;
  globalmap.fullheight=50000;
  orts_hash=g_hash_table_new(g_str_hash,g_str_equal);
  if (argc>=2) {
    configfilename=argv[1];
    if (strcmp(configfilename,"--help")==0) {
      printf(_("Usage: %s [configfile [gpsfile | osmfile ]]...\n"),argv[0]);
      return 1;
    }
  } else if (argc==1) {
    snprintf(buf,sizeof(buf),"%s/.mumpot/map.conf",getenv("HOME"));
    configfilename=buf;
  } else {
    printf(_("Usage: %s configfile\n"),argv[0]);
    return 1;
  }
  if (!parse_mapconfig(configfilename)) { 
    if (argc==1) {
      config_chooser();
    }
    printf(_("Usage: %s configfile\n"),argv[0]);
    return 1;
  }
#ifdef _WIN32
  {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(1,1),&wsadata);
  }
#endif
  {
    int zoom;
    startlon=globalmap.startlong;
    startlat=globalmap.startlatt;
    zoom=globalmap.zoomfactor;
    get_startposition(&startlat,&startlon,&zoom);
    if (zoom!=0) {
      globalmap.zoomfactor=zoom;
    }
  }
  calc_mapoffsets();
  {
    GList *lcopy=NULL;
    for (l=g_list_first(globalmap.placefilelist);l;l=g_list_next(l)) {
      if (read_places((char *)l->data)) 
	lcopy=g_list_append(lcopy,l->data);
    }
    g_list_free(globalmap.placefilelist);
    globalmap.placefilelist=lcopy;
    
  }
  if (!globalmap.placefilelist) {
    read_places(MUMPOT_DATADIR "/places.txt");
  }
  gdk_rgb_init();
  /* gdk_imlib_init_params(&imlibinit);
     gtk_widget_push_visual(gdk_imlib_get_visual());
     gtk_widget_push_colormap(gdk_imlib_get_colormap()); */
  mw=create_mapwin();
#ifndef _WIN32
  mw->cdlg=create_connection_dialog(gps_connected,NULL,mw);
#endif
  mygc=gdk_gc_new(mw->map->window);
  cmap=gdk_window_get_colormap(mw->map->window);
  gdk_color_parse("red",&mark_red);
  gdk_color_parse("white",&mark_white);
  gdk_color_alloc(cmap,&mark_white);
  gdk_color_alloc(cmap,&mark_red);
  gdk_color_parse("violet",&crosshair_color);
  gdk_color_alloc(cmap,&crosshair_color);
  gps_color = crosshair_color;
  for(i=0;i<256;i++) {
    int hs=i*5/256; /* leave out violet -> red transition */
    int f=(i*5)%256;
    switch (hs) {
    case 0:
      speedcolor[i].red=65535;
      speedcolor[i].green=256*f;
      speedcolor[i].blue=0;
      break;
    case 1:
      speedcolor[i].red=256*(255-f);
      speedcolor[i].green=65535;
      speedcolor[i].blue=0;
      break;
    case 2:
      speedcolor[i].red=0;
      speedcolor[i].green=65535;
      speedcolor[i].blue=256*f;
      break;
    case 3:
      speedcolor[i].red=0;
      speedcolor[i].green=256*(255-f);
      speedcolor[i].blue=65535;
      break;
    case 4:
      speedcolor[i].red=256*f;
      speedcolor[i].green=0;
      speedcolor[i].blue=65535;
      break;
    default:
      speedcolor[i].red=65535;
      speedcolor[i].green=0;
      speedcolor[i].blue=65535;
      
    }
    gdk_color_alloc(cmap,&speedcolor[i]);
  }
  
  if ((globalmap.first)&&(!globalmap.zoomable)) {
    menu_item_set_state(mw,PATH_ZOOM_OUT,0);
    menu_item_set_state(mw,PATH_ZOOM_IN,0);
  }
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->linemode_but),TRUE);
  if (globalmap.startplace)
    center_ort(mw,globalmap.startplace);
  else {
    center_map(mw,startlon,startlat);
  }
  
  if (argc > 2) {
    int i;
    for(i=2;i<argc;i++) {
       if ((!strcasecmp(argv[i]+strlen(argv[i])-4,".osm"))||
           (!strcasecmp(argv[i]+strlen(argv[i])-8,".osm.bz2"))) {
          load_osm_gfx(mw,argv[i]);   
       } else {
          load_gps_line(argv[i],mw->mark_line_list);
       } 
    }
  }
  gtk_main();
  upd_startposition(mw);
  return 0;
}
