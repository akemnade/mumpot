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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include "myintl.h"
#include "mapconfig_data.h"
#include "gps.h"
#include "mapdrawing.h"
#include "osm_parse.h"
#include "gui_common.h"
#include "geometry.h"
#include "osm_tagpresets_data.h"
#include "osm_tagpresets_gui.h"

#include "osm_view.h"
#include "start_way.xpm"
#include "end_way.xpm"
#include "restart_way.xpm"
#include "automerge.xpm"

#include "start_route.xpm"
#include "end_route.xpm"
#include "routestartgps.xpm"
#include "delosmobj.xpm"
#include "selosmobj.xpm"
#include "newosmway.xpm"

#define MENU_VIEW_OSM_DATA_N N_("/OSM/View OSM Layer")
#define MENU_VIEW_OSM_DATA _(MENU_VIEW_OSM_DATA_N)
#define MENU_OSM_AUTO_SELECT_N N_("/OSM/Auto-Select way in center")
#define MENU_OSM_AUTO_SELECT _(MENU_OSM_AUTO_SELECT_N)
#define MENU_OSM_DISPLAY_NODES_N N_("/OSM/Display nodes")
#define MENU_OSM_DISPLAY_NODES _(MENU_OSM_DISPLAY_NODES_N)
#define MENU_OSM_DISPLAY_STREET_BORDERS_N N_("/OSM/Display street borders")
#define MENU_OSM_DISPLAY_STREET_BORDERS _(MENU_OSM_DISPLAY_STREET_BORDERS_N)
#define MENU_OSM_DISPLAY_ALL_WAYS_N N_("/OSM/Display non-street ways")
#define MENU_OSM_DISPLAY_ALL_WAYS MENU_OSM_DISPLAY_ALL_WAYS_N
#define MENU_OSM_DISPLAY_TAGS_N N_("/OSM/Display tags")
#define MENU_OSM_DISPLAY_TAGS _(MENU_OSM_DISPLAY_TAGS_N)


struct way_colors {
  char *class_name;
  char *color_name;
  GdkColor color;
};

static GTree *ptree;
static GdkColor node_color;
static GdkColor black_color;

struct osm_info {
  int display_osm;
  int display_street_borders;
  int display_nodes;
  int autoselect_center;
  int display_tags;
  int display_editbar;
  int display_all_ways;
  int has_dest;
  double min_distance;
  struct osm_object *selected_object;
  struct osm_node *selected_node;
  GList *newwaypoints_start;
  GList *newway_tags;
  char *newway_hwyclass;
  int merge_first_node;
  GtkWidget *tag_combo;
  GtkWidget *tag_value;
  GtkWidget *tag_set;
  GHashTable *tag_hash;
  GtkWidget *editbar;
  GtkWidget *routebar;
  GtkWidget *meditbar;

  struct {
    GtkWidget *startwaybut;
    GtkWidget *endwaybut;
    GtkWidget *restartwaybut;
    GtkWidget *automergebut;
  } liveeditb;
  struct {
    GtkWidget *start_route;
    GtkWidget *end_route;
    GtkWidget *set_destination;
   
  } routeb;
  GtkWidget *hwypopup;
  struct {
    GtkWidget *selbut;
    GtkWidget *addwaybut;
    GtkWidget *delobjbut;
    GtkWidget *joinbut;
    GtkWidget *splitbut;
  } editb;
  int clicktime;
  int moving_node;
  int route_start_node;
  struct osm_way *way_to_edit;
};

static GHashTable *color_hash;

static struct way_colors all_colors[]={
  {"default","gray50"},
  {"motorway","blue"},
  {"motorway_link","blue"},
  {"primary","red"},
  {"secondary","orange"},
  {"tertiary","yellow"},
  {"unclassified","LightGray"},
  {"residential","LightGray"},
  {"track","brown"},
  {"path","sandy brown"},
  {"living_street","PaleGreen"},
  {"cycleway","violet"},
  {"footway","green"},
  {"service","LightGray"},
  {"pedestrian","SpringGreen"},
  {"steps","green"},
				       
};

struct way_render_data {
  GdkColor *color;
  int processed;
};

struct node_render_data {
  int x;
  int y;
  int prevnode;
  double totdist;
};

static int find_nearest_node(struct mapwin *mw,
			     struct osm_file *osmf,
			     int x, int y, int only_with_way);

static void start_way_menu_cb(GtkWidget *w, gpointer data);
static double get_distance_r(double x1, double y1, double x2, double y2,
                     double x3, double y3)
{
  double dx, dy;
  double anenner;
  double azaehler;
  double d;
  dx=x2-x1;
  dy=y2-y1;
  if ((x1==x2)&&(y1==y2)) {
    return sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1));
  }
  anenner=sqrt(dx*dx+dy*dy);
  d = (x1*(x1-x2-x3)+y1*(y1-y2-y3)+x2*x3+y2*y3)/anenner;
  if (d<0) {
      return sqrt((x3-x1)*(x3-x1)+(y3-y1)*(y3-y1));
  }
  if (d>anenner) {
      return sqrt((x3-x2)*(x3-x2)+(y3-y2)*(y3-y2));
  }
  azaehler=dx*(y3-y1)+dy*(x1-x3);
  return fabs(azaehler/anenner);
}

static void init_node_render_data(struct  osm_node *node)
{
  struct node_render_data *nrd;
  if (!node->user_data) {
    node->user_data = malloc(sizeof(struct node_render_data));
    nrd = (struct node_render_data*)node->user_data;
    nrd->totdist = DBL_MAX;
    nrd->prevnode = 0;
  } else {
    nrd = (struct node_render_data*)node->user_data;
  }
  double x,y;
  geosec2point(&x,&y,3600.0*node->lon,3600.0*node->lat);
  nrd->x = (int)x;
  nrd->y = (int)y;
}


static struct way_render_data *get_way_render_data(struct osm_way *way)
{
  if (!way->user_data) {
    char *hwyclass;
    struct way_render_data *wrd = malloc(sizeof(struct way_render_data));
    way->user_data = wrd;
    wrd->processed=0;

    wrd->color=NULL;
    if (way->street) {
      hwyclass = get_tag_value(&way->head,"highway");
      if (hwyclass)
	wrd->color = (GdkColor *)g_hash_table_lookup(color_hash, hwyclass);
      if (!wrd->color) {
	wrd->color = &all_colors[0].color;
	fprintf(stderr,"no color for %s\n",hwyclass);
      }
    } else {
      wrd->color = &all_colors[0].color;
    }
  }
  return (struct way_render_data *)way->user_data;
}

static void draw_way(struct mapwin *mw, struct osm_way *way,GdkGC *osmgc,
		     int set_color)
{
  struct osm_node *node;
  struct way_render_data *wrd=get_way_render_data(way);
  struct node_render_data *nrd;
  int x1,x2,y1,y2;
  int i;
  int firstrun=set_color;
  int editing=mw->osm_inf->display_tags&&((GTK_WIDGET_HAS_FOCUS(mw->osm_inf->tag_value))||(GTK_WIDGET_HAS_FOCUS(mw->osm_inf->tag_combo)));
  int autosel=(mw->osm_inf->autoselect_center)&&(!editing);
    
  if (wrd->processed)
    return;
  if (!way->street) {
    if (mw->osm_inf->selected_object != &way->head) {
      if (!set_color)
	return;
      if (!mw->osm_inf->display_all_ways) 
	return;
    }
  }
  wrd->processed=1;
 
  node = get_osm_node(way->nodes[0]);
  if (!node)
    return;
  if (!node->user_data)
    init_node_render_data(node);
  nrd = (struct node_render_data *)node->user_data;
  x2=nrd->x-mw->page_x;
  y2=nrd->y-mw->page_y;
  for(i=1;i<way->nr_nodes;i++) {
    double dist;
    x1=x2;
    y1=y2;
    node = get_osm_node(way->nodes[i]);
    if (!node)
      break;
    if (!node->user_data)
      init_node_render_data(node);
    nrd = (struct node_render_data *)node->user_data;
    x2=nrd->x-mw->page_x;
    y2=nrd->y-mw->page_y;
    if (!check_crossing(x1,y1,x2,y2,mw->page_width,mw->page_height))
      continue;
    if (set_color) {
      gdk_gc_set_foreground(osmgc,wrd->color);
      set_color=0;
    }
    if ((firstrun)&&(autosel)) {
      dist = get_distance_r(x1,
			    y1,
			    x2,
			    y2,
			    mw->page_width/2,
			    mw->page_height/2);
      if (dist < mw->osm_inf->min_distance) {
	mw->osm_inf->min_distance = dist;
	mw->osm_inf->selected_object = &way->head;
      }
    }
    gdk_draw_line(mw->map->window,osmgc,
		  x1,
		  y1,
		  x2,
		  y2);
		  
  }
}

static void set_way_state(struct osm_info *osminf,int state)
{
  gtk_widget_set_sensitive(osminf->liveeditb.endwaybut,state);
  gtk_widget_set_sensitive(osminf->liveeditb.restartwaybut,state);
}


static void reset_processed(struct osm_file *osmf)
{
  GList *lway;
  for(lway = g_list_first(osmf->ways);lway;lway=g_list_next(lway)) {
    struct osm_way *way = (struct osm_way *)lway->data;
    struct way_render_data *wrd = get_way_render_data(way);
    wrd->processed = 0;
  }
}

static void display_tag_value(struct osm_info *osminf,
			      char *key)
{
  char *val;
  if (osminf->tag_hash && (val=g_hash_table_lookup(osminf->tag_hash,key))) {
    gtk_entry_set_text(GTK_ENTRY(osminf->tag_value),val);
  } else {
    gtk_entry_set_text(GTK_ENTRY(osminf->tag_value),"");
  }
}

static void tag_hash_free(gpointer key, gpointer value,
			  gpointer user_data)
{
  free(key);
  free(value);
}

static void display_tags(struct osm_info *osmf,
			 struct osm_object *way)
{
  GList *keys=NULL;
  if (osmf->tag_hash) {
    g_hash_table_foreach(osmf->tag_hash,tag_hash_free,NULL);
    g_hash_table_destroy(osmf->tag_hash);
  }
  osmf->tag_hash=g_hash_table_new(g_str_hash,g_str_equal);
  if (way->tag_list) {
    GList *l=way->tag_list;
    for(;l;l=g_list_next(l)) {
      char *t=(char *)l->data;
      char *k=strdup(t);
      g_hash_table_insert(osmf->tag_hash,k,strdup(t+strlen(t)+1));
      keys=g_list_append(keys,k);
    }
  }
  if (!g_hash_table_lookup(osmf->tag_hash,"name")) {
    keys=g_list_append(keys,strdup("name"));
  }
  if (!g_hash_table_lookup(osmf->tag_hash,"highway")) {
    keys=g_list_append(keys,strdup("highway"));
  }
      
  gtk_combo_set_popdown_strings(GTK_COMBO(osmf->tag_combo),
				keys);
  g_list_free(keys);
  if (g_hash_table_lookup(osmf->tag_hash,"name")) {
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(osmf->tag_combo)->entry),
		       "name");
    display_tag_value(osmf,"name");
  } else if (g_hash_table_lookup(osmf->tag_hash,"ref")) {
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(osmf->tag_combo)->entry),
		       "ref");
    display_tag_value(osmf,"ref");
  } else if (g_hash_table_lookup(osmf->tag_hash,"highway")) {
    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(osmf->tag_combo)->entry),
		       "highway");
    display_tag_value(osmf,"highway");
  }
  return;

}

static int osmroute_get_way_info(int nodenum,double *len,
				  int *prevnode)
{
  struct osm_node *node=get_osm_node(nodenum);
  if (node) {
    struct  node_render_data *nrd = (struct node_render_data*)node->user_data;
    if (prevnode)
      *prevnode=nrd->prevnode;
    if (len)
      *len=nrd->totdist;
    return 1;
  }
  return 0;
}

static void osmroute_put_way_info(int nodenum,double len,
				  int prevnode)
{
  struct osm_node *node=get_osm_node(nodenum);
  if (node) {
    struct  node_render_data *nrd = (struct node_render_data*)node->user_data;
    nrd->prevnode=prevnode;
    nrd->totdist=len;
  }
}

#ifdef TREE_DEBUG
static gboolean check_cons(gpointer key, gpointer value, gpointer data)
{
  struct node_render_data *nrd=(struct node_render_data*)key;
  double *dist=(double *)data;
  if ((*dist)>nrd->totdist) {
    printf("inconsistency %f < %f",nrd->totdist,*dist);
    return TRUE;
  }
  *dist=nrd->totdist;
  return FALSE;
}

static void osmroute_check_consistency()
{
  double dist=0;
#ifdef USE_GTK2
  g_tree_foreach(ptree,check_cons,&dist);
#else
  g_tree_traverse(ptree,check_cons,G_IN_ORDER,&dist);
#endif
}
#endif

static void osmroute_check_node(int nodenum,double newlen, int prevnode)
{
  double oldlen;
  osmroute_get_way_info(nodenum,&oldlen,NULL);
  if (newlen<oldlen) {
    struct osm_node *node=get_osm_node(nodenum);
    struct  node_render_data *nrd = (struct node_render_data*)node->user_data;
    if (nrd->prevnode) {
      g_tree_remove(ptree,nrd); 
    }
    osmroute_put_way_info(nodenum,newlen,prevnode);
    if ((node)&&(node->nr_ways>1)) {
      /* for(i=pfifo_r;i!=pfifo_w;i=(i+1)&32767) {
	if (pfifo[i]==nodenum)
	  return;
	  } */
     
#ifdef TREE_DEBUG
      osmroute_check_consistency();
#endif
      g_tree_insert(ptree,nrd,node);
#ifdef TREE_DEBUG
      osmroute_check_consistency();
#endif
      /*
      pfifo[pfifo_w]=nodenum;
      pfifo_w++;
      pfifo_w=pfifo_w&32767;
      assert(pfifo_w!=pfifo_r);
      */
    }
  }
}

static void osmroute_ways2fifo(int nodenum, int destnodenum)
{
  GList *way_list;
  double oldlen;
  struct osm_node *node=get_osm_node(nodenum);
  struct osm_node *destnode=destnodenum?get_osm_node(destnodenum):0;
  if (!node)
    return;
  osmroute_get_way_info(nodenum,&oldlen,NULL);
  if (destnode)
    oldlen-=point_dist(node->lon,node->lat,destnode->lon,destnode->lat);
  //printf("dist: %f\n",oldlen);
  for(way_list = node->way_list; way_list;way_list=g_list_next(way_list)) {
    struct osm_way *way=way_list->data;
    int i;
    int npos=0;
    if (!way->street)
      continue;
    if (!way->foot)
      continue;
    for(i=0;i<way->nr_nodes;i++) {
      if (way->nodes[i]==nodenum) {
	npos=i;
	break;
      }
    }
    if (way->nodes[npos]==nodenum) {
      double addlen=0;
      double oldlon,oldlat;
      oldlon=node->lon;
      oldlat=node->lat;
      for(i=npos+1;i<way->nr_nodes;i++) {
	struct osm_node *node2=get_osm_node(way->nodes[i]);
	if (!node2)
	  continue;
	addlen+=point_dist(oldlon,oldlat,node2->lon,node2->lat);
	osmroute_check_node(node2->head.id,oldlen+addlen+
			    ((destnode)?point_dist(node2->lon,node2->lat,
						   destnode->lon,destnode->lat):0),
			    way->nodes[i-1]);
	oldlon=node2->lon;
	oldlat=node2->lat;
      }
      oldlon=node->lon;
      oldlat=node->lat;
      addlen=0;
      for(i=npos-1;i>=0;i--) {
	struct osm_node *node2=get_osm_node(way->nodes[i]);
	if (!node2)
	  continue;
	addlen+=point_dist(oldlon,oldlat,node2->lon,node2->lat);
	osmroute_check_node(node2->head.id,addlen+oldlen+((destnode)?point_dist(node2->lon,node2->lat,destnode->lon,destnode->lat):0),way->nodes[i+1]);
	oldlon=node2->lon;
	oldlat=node2->lat;
      }
    }
  }
}

static void reset_distance_info(struct osm_file *osmf)
{
  GList *nodes;
  for(nodes=g_list_first(osmf->nodes);nodes;nodes=g_list_next(nodes)) {
    struct osm_node *node=(struct osm_node *)nodes->data;
    struct node_render_data *nrd;
    if (!node->user_data)
      init_node_render_data(node);
    nrd =(struct node_render_data *)node->user_data;
    nrd->totdist=DBL_MAX;
    nrd->prevnode=0;
  }
}

void osmroute_add_path(struct mapwin *mw,
		       struct osm_file *osmf, void (*path_to_lines)(double lon, double lat, void *data), int x, int y, void *data)
{
  int nnum=find_nearest_node(mw,osmf,x,y,1);
  if (nnum) {
    struct osm_node *node=get_osm_node(nnum);
    int nextnode=0;
    nextnode=nnum;
    if (!osmroute_get_way_info(nnum,NULL,&nextnode))
      return;
    if (!nextnode)
      return;
    if (!node)
      return;
    path_to_lines(node->lon,node->lat,data);
    while(osmroute_get_way_info(nnum,NULL,&nextnode)) {
      if (!nextnode)
	return;
      node=get_osm_node(nextnode);
      if (!node)
	return;
      path_to_lines(node->lon,node->lat,data);
      nnum=nextnode;
    }
  }
}

gint ptree_cmp(gconstpointer a,
	       gconstpointer b)
{
  struct node_render_data *nrda=(struct node_render_data *)a;
  struct node_render_data *nrdb=(struct node_render_data *)b;
  if (a==b)
    return 0;
  if (nrda->totdist==nrdb->totdist)
    return (a<b)?-1:1;
  return (nrda->totdist<nrdb->totdist)?-1:1;
}

gboolean ptree_trav(gpointer key,
		    gpointer val,
		    gpointer data)
{
  struct osm_node *node=(struct osm_node *)
    val;
  *((int *)data)=node->head.id;
  return TRUE;
}

int osmroute_start_calculate(struct mapwin *mw,
			     struct osm_file *osmf,
			     int nnum, int destx, int desty)
{
  struct timeval tv1;
  struct timeval tv2;
#ifndef _WIN32
  gettimeofday(&tv1,NULL);
#endif
  int nnumdest=0;
  if ((destx)&&(desty)) {
    nnumdest=find_nearest_node(mw,osmf,destx,desty,1);
  }
  if ((nnum)&&(nnum!=nnumdest)) {
    double l;
    osmroute_get_way_info(nnum,&l,NULL);
    if (l==0)
      return 1;
    reset_distance_info(osmf);
    ptree=g_tree_new(ptree_cmp);
#if 0
    memset(pfifo,0,sizeof(pfifo));
    pfifo_r=0;
    pfifo_w=0;
#endif
    osmroute_put_way_info(nnum,0,0);
    osmroute_ways2fifo(nnum,nnumdest);
    nnum=0;
#ifdef USE_GTK2
    g_tree_foreach(ptree,ptree_trav,&nnum);
#else
    g_tree_traverse(ptree,ptree_trav,G_IN_ORDER,&nnum);
#endif
    while((nnum)&&(nnum!=nnumdest)) {
      /*
      double mindist=DBL_MAX;
      int mpos=pfifo_r;
      int t;
      for(i=pfifo_r;i!=pfifo_w;i=(i+1)&32767) {
	double d;
	osmroute_get_way_info(nnum,&d,NULL);
	if (d<mindist) {
	  mindist=d;
	  mpos=i;
	}
	}
      t=pfifo[pfifo_r];
      pfifo[pfifo_r]=pfifo[mpos];
      pfifo[mpos]=t; */
      g_tree_remove(ptree,get_osm_node(nnum)->user_data);
      osmroute_ways2fifo(nnum,nnumdest);
      nnum=0;
#ifdef USE_GTK2
      g_tree_foreach(ptree,ptree_trav,&nnum);
#else
      g_tree_traverse(ptree,ptree_trav,G_IN_ORDER,&nnum);
#endif
    }
    g_tree_destroy(ptree);
#ifndef _WIN32
    gettimeofday(&tv2,NULL);
    printtimediff("calculating route in %d ms\n",&tv1,&tv2);
#endif
    return 1;
  }
  return 0;
}

static int find_nearest_node(struct mapwin *mw,
			     struct osm_file *osmf,
			     int x, int y, int only_with_way)
{
  int mindist=INT_MAX;
  int nnum=0;
  GList *lnodes;
  for(lnodes=osmf->nodes;lnodes;lnodes=g_list_next(lnodes)) {
    struct osm_node *node=(struct osm_node *)lnodes->data;
    struct node_render_data *nrd;
    if (only_with_way) {
      struct osm_way *way;
      GList *l;
      if (!node->nr_ways)
	continue;
      for(l=g_list_first(node->way_list);l;l=g_list_next(l)) {
	way=(struct osm_way *)l->data;
	if (way->street)
	  break;
      }
      if (!l)
	continue;
    }
    if (!node->user_data)
      init_node_render_data(node);
    nrd =(struct node_render_data *)node->user_data;
    
    if ((mw->page_x < nrd->x) && (mw->page_x+mw->page_width > nrd->x) &&
	(mw->page_y < nrd->y) && (mw->page_y+mw->page_height > nrd->y)) {
      int d=(nrd->x-x)*(nrd->x-x)+(nrd->y-y)*(nrd->y-y);
      if (d<mindist) {
	mindist=d;
	nnum=node->head.id;
      }
    }
  }
  return nnum;
}



static void draw_ways(struct mapwin *mw, struct osm_file *osmf,
		      GdkGC *osmgc, int set_color)
{
  GList *lnodes;
  GList *lway;
  struct osm_object *old_sel_obj = mw->osm_inf->selected_object;
  reset_processed(osmf); 
  lnodes = g_list_first(osmf->nodes);
  mw->osm_inf->min_distance=10;
  /*  if (mw->osm_inf->autoselect_center)
      mw->osm_inf->selected_way = NULL;*/
  for(lway=osmf->ways;lway;lway=g_list_next(lway)){
    draw_way(mw,lway->data,osmgc,set_color);
  }
  if (mw->osm_inf->display_nodes) {
    for(;lnodes;lnodes=g_list_next(lnodes)) {
      struct osm_node *node=(struct osm_node *)lnodes->data;
      struct node_render_data *nrd;
      if (!node->user_data)
	init_node_render_data(node);
      nrd =(struct node_render_data *)node->user_data;
      
      if ((mw->page_x < nrd->x) && (mw->page_x+mw->page_width > nrd->x) &&
	  (mw->page_y < nrd->y) && (mw->page_y+mw->page_height > nrd->y)) {
	if (set_color) {
	  gdk_gc_set_foreground(osmgc,&node_color);
	  if (node->nr_ways!=0) {
	    gdk_draw_rectangle(mw->map->window,osmgc,1,nrd->x-mw->page_x-node->nr_ways,
			       nrd->y-mw->page_y-node->nr_ways,2*node->nr_ways,2*node->nr_ways);
	  } else {
	    gdk_draw_line(mw->map->window,osmgc,nrd->x-mw->page_x-4,nrd->y-4-mw->page_y,nrd->x+4-mw->page_x,nrd->y+4-mw->page_y);
	    gdk_draw_line(mw->map->window,osmgc,nrd->x+4-mw->page_x,nrd->y-4-mw->page_y,nrd->x-4-mw->page_x,
			  nrd->y+4-mw->page_y);
	  }
	}
	/* draw node */
      }
    }
  }
  if ((set_color)&&(mw->osm_inf->selected_object)) {
    gdk_gc_set_foreground(osmgc,&black_color);
    gdk_gc_set_line_attributes(osmgc,5,
			       GDK_LINE_SOLID,
			       GDK_CAP_BUTT,
			       GDK_JOIN_BEVEL);
    if (mw->osm_inf->selected_object->type == WAY) {
      struct way_render_data *wrd=get_way_render_data((struct osm_way *)mw->osm_inf->selected_object);
      wrd->processed=0;
      draw_way(mw,(struct osm_way *)mw->osm_inf->selected_object,osmgc,0);
    } else {
      struct node_render_data *nrd = (struct node_render_data *)((struct osm_node *)mw->osm_inf->selected_object)->user_data;
      gdk_draw_line(mw->map->window,osmgc,nrd->x-mw->page_x-4,nrd->y-4-mw->page_y,nrd->x+4-mw->page_x,nrd->y+4-mw->page_y);
      gdk_draw_line(mw->map->window,osmgc,nrd->x+4-mw->page_x,nrd->y-4-mw->page_y,nrd->x-4-mw->page_x,
		    nrd->y+4-mw->page_y);
    }
    if (old_sel_obj != mw->osm_inf->selected_object) {
      old_sel_obj = mw->osm_inf->selected_object;
      if (old_sel_obj) {
	display_tags(mw->osm_inf,old_sel_obj);
      } 
    }
  }
}

void draw_osm(struct mapwin *mw, struct osm_file *osmf,GdkGC *osmgc)
{
  
  if (!mw->osm_inf->display_osm)
    return;
  if (mw->osm_inf->display_street_borders) {
    gdk_gc_set_line_attributes(osmgc,8,
			       GDK_LINE_SOLID,
			       GDK_CAP_BUTT,
			       GDK_JOIN_BEVEL);
    gdk_gc_set_foreground(osmgc,&black_color);
    draw_ways(mw,osmf,osmgc,0);
  }
  gdk_gc_set_line_attributes(osmgc,mw->osm_inf->display_street_borders?5:3,
			     GDK_LINE_SOLID,
			     GDK_CAP_BUTT,
			     GDK_JOIN_BEVEL);
  draw_ways(mw,osmf,osmgc,1);
  
}

static void select_tag_key(GtkWidget *w, gpointer user_data)
{
  struct osm_info *osminf = (struct osm_info *)user_data;
  char *k = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(osminf->tag_combo)->entry),0,-1);
  display_tag_value(osminf,k);
  free(k);
}

static void set_tag_cb(char *k, char *v,
		       void *data, void *user_data)
{
  struct mapwin *mw = (struct mapwin *)user_data;
  struct osm_object *obj = (struct osm_object *)data;
  set_osm_tag(obj,k,v);
  if (obj->type == WAY) {
    free(((struct osm_way *)obj)->user_data);
    ((struct osm_way *)obj)->user_data=NULL;
  }
  if (mw->osm_inf->selected_object)
    display_tags(mw->osm_inf,mw->osm_inf->selected_object);
}



static void osm_set_tag_cb(GtkWidget *w, gpointer user_data)
{
  struct mapwin *mw= (struct mapwin *)user_data;
  char *k = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(mw->osm_inf->tag_combo)->entry),0,-1);
  char *v = gtk_editable_get_chars(GTK_EDITABLE(mw->osm_inf->tag_value),0,-1);
  mw->osm_main_file->changed=1;
  if (strlen(k)&&strlen(v)&&mw->osm_inf->selected_object) {
    set_tag_cb(k,v,mw->osm_inf->selected_object,mw);
  } 
  free(k);
  free(v);
}

static struct mapwin *sigmw;

int osm_save_file(const char *fname, struct osm_file *osmf,
		  int only_changes)
{
  if (only_changes)
    
    return save_osmchange_file(fname,osmf,0);
  else
    return save_osm_file(fname,osmf);
}


static void mysigh(int bla)
{
 if ((sigmw->osm_main_file) && ( sigmw->osm_main_file->changed)) {
   save_osm_file("dump.osm",sigmw->osm_main_file);
exit(1);
 }
}

static void myexitf()
{
  mysigh(0);
}

void init_osm_draw(struct mapwin *mw)
{
  int i,j;
  GtkWidget *table;
  char *col1[]={"motorway","motorway_link","trunk","primary","secondary"};
  char *col2[]={"tertiary","unclassified","service","residential",NULL};
  char *col3[]={"track","cycleway","footway","pedestrian","steps"};
  char **cols[]={col1,col2,col3};
  GdkColormap *cmap=gdk_window_get_colormap(mw->map->window);
  color_hash = g_hash_table_new(g_str_hash, g_str_equal);
  for(i=0;i<sizeof(all_colors)/sizeof(all_colors[0]);i++) {
    gdk_color_parse(all_colors[i].color_name,&all_colors[i].color);
    gdk_color_alloc(cmap,&all_colors[i].color);
    g_hash_table_insert(color_hash,all_colors[i].class_name,
			&all_colors[i].color);
  }

  mw->osm_inf = calloc(1,sizeof(struct osm_info));
  check_item_set_state(mw, MENU_VIEW_OSM_DATA,1);
  mw->osm_inf->display_osm=1;
  check_item_set_state(mw, MENU_OSM_AUTO_SELECT,0);
  mw->osm_inf->display_nodes=0;
  check_item_set_state(mw, MENU_OSM_DISPLAY_NODES,0);
  mw->osm_inf->display_street_borders=0;
  check_item_set_state(mw, MENU_OSM_DISPLAY_STREET_BORDERS,0);
  mw->osm_inf->display_all_ways=0;
  check_item_set_state(mw, MENU_OSM_DISPLAY_ALL_WAYS,0);
  menu_item_set_state(mw,  MENU_VIEW_OSM_DATA, 0);
  menu_item_set_state(mw, MENU_OSM_AUTO_SELECT,0);
  menu_item_set_state(mw, MENU_OSM_DISPLAY_NODES,0);
  menu_item_set_state(mw, MENU_OSM_DISPLAY_STREET_BORDERS,0);
  menu_item_set_state(mw, MENU_OSM_DISPLAY_ALL_WAYS,0);
  menu_item_set_state(mw, MENU_OSM_DISPLAY_TAGS,0);
  gdk_color_black(cmap,&black_color);
  gdk_color_parse("red",&node_color);
  gdk_color_alloc(cmap,&node_color);
  mw->osm_inf->tag_combo = gtk_combo_new();
  gtk_combo_set_value_in_list(GTK_COMBO(mw->osm_inf->tag_combo),
			      FALSE,FALSE);
  gtk_signal_connect(GTK_OBJECT(GTK_COMBO(mw->osm_inf->tag_combo)->list),
		     "selection_changed",GTK_SIGNAL_FUNC(select_tag_key),
		     mw->osm_inf);
  mw->osm_inf->tag_value = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(mw->bottom_box),mw->osm_inf->tag_combo, FALSE,
		     FALSE,0);
  gtk_box_pack_start(GTK_BOX(mw->bottom_box),mw->osm_inf->tag_value, TRUE,
		     TRUE,0);
  mw->osm_inf->tag_set=gtk_button_new_with_label(_("set"));
  gtk_box_pack_start(GTK_BOX(mw->bottom_box),mw->osm_inf->tag_set,FALSE,FALSE,0);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->tag_set),"clicked",GTK_SIGNAL_FUNC(osm_set_tag_cb),mw);
#if 0
  gtk_widget_show(mw->osm_inf->tag_combo);
  gtk_widget_show(mw->osm_inf->tag_value);
#endif
  mw->osm_inf->hwypopup=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(mw->osm_inf->hwypopup),"highwayclass");
  gtk_signal_connect_object(GTK_OBJECT(mw->osm_inf->hwypopup),
			    "delete-event",GTK_SIGNAL_FUNC(gtk_widget_hide),
			    (void*)mw->osm_inf->hwypopup);
  table=gtk_table_new(5,3,TRUE);
  for(i=0;i<3;i++) {
    char **col=cols[i];
    for(j=0;j<5;j++) {
      if (col[j]) {
	GtkWidget *but=gtk_button_new_with_label(col[j]);
	gtk_table_attach(GTK_TABLE(table),but,i,i+1,j,j+1,
			 GTK_EXPAND|GTK_FILL,GTK_EXPAND|GTK_FILL,
			 0,0);
	gtk_object_set_data(GTK_OBJECT(but),"hwyclass",col[j]);
	gtk_signal_connect(GTK_OBJECT(but),"clicked",
			   GTK_SIGNAL_FUNC(start_way_menu_cb),mw);
      }
    }
  }
  gtk_container_add(GTK_CONTAINER(mw->osm_inf->hwypopup),table);
  gtk_widget_show_all(table);
  if (!osm_parse_presetfile(expand_home("~/.mumpot/tagpresets")))
    osm_parse_presetfile(MUMPOT_DATADIR "/tagpresets");
  sigmw=mw;
#ifndef _WIN32
  signal(SIGHUP,mysigh);
#endif
  atexit(myexitf);
}

static void clear_osm_cb(GtkWidget *w,
			 gpointer data)
{
  struct mapwin *mw=(struct mapwin *)data;
  if (mw->osm_main_file) {
    free_osm_gfx((struct mapwin *)data,mw->osm_main_file);
    mw->osm_main_file=NULL;
    gtk_widget_queue_draw_area(mw->map,0,0,
			       mw->page_width,
			       mw->page_height);
  }
}

void osm_clear_data(struct mapwin *mw)
{
  if (mw->osm_main_file) {
    if (mw->osm_main_file->changed)
      yes_no_dlg(_("The OSM data is modified.\nDo you really want to loose it"),
		 GTK_SIGNAL_FUNC(clear_osm_cb),NULL,mw);
    else 
      clear_osm_cb(NULL,mw);
	
  }
}



static void toggle_osm_view(gpointer callback_data, guint callback_action,
			    GtkWidget *w)
{
  struct mapwin *mw = (struct mapwin *)callback_data;
  mw->osm_inf->display_osm = GTK_CHECK_MENU_ITEM(w)->active;
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
}

static void toggle_select_center(gpointer callback_data, guint callback_action,
				 GtkWidget *w)
{
  struct mapwin *mw = (struct mapwin *)callback_data;
  mw->osm_inf->autoselect_center = GTK_CHECK_MENU_ITEM(w)->active;
}

static void toggle_display_nodes(gpointer callback_data, guint callback_action,
				 GtkWidget *w)
{
  struct mapwin *mw = (struct mapwin *)callback_data;
  mw->osm_inf->display_nodes = GTK_CHECK_MENU_ITEM(w)->active;
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
}

static void toggle_display_street_borders(gpointer callback_data, guint callback_action,
					  GtkWidget *w)
{
  struct mapwin *mw = (struct mapwin *)callback_data;
  mw->osm_inf->display_street_borders = GTK_CHECK_MENU_ITEM(w)->active;
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height); 
}

static void toggle_display_all_ways(gpointer callback_data, guint callback_action,
					  GtkWidget *w)
{
  struct mapwin *mw = (struct mapwin *)callback_data;
  mw->osm_inf->display_all_ways = GTK_CHECK_MENU_ITEM(w)->active;
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height); 
}

static void toggle_display_tags(gpointer callback_data, guint callback_action,
				GtkWidget *w)
{
  struct mapwin *mw = (struct mapwin *)callback_data;
  mw->osm_inf->display_tags = GTK_CHECK_MENU_ITEM(w)->active;
  if (mw->osm_inf->display_tags) {
    gtk_widget_show(mw->osm_inf->tag_combo);
    gtk_widget_show(mw->osm_inf->tag_value);
    gtk_widget_show(mw->osm_inf->tag_set);
  } else {
    gtk_widget_hide(mw->osm_inf->tag_combo);
    gtk_widget_hide(mw->osm_inf->tag_value);
    gtk_widget_hide(mw->osm_inf->tag_set);
  }
}


void recalc_node_coordinates(struct mapwin *mw, struct osm_file *osmf)
{
  GList *lnodes;
  for(lnodes=g_list_first(osmf->nodes);lnodes;lnodes=g_list_next(lnodes)) {
    struct osm_node *node = (struct osm_node *)lnodes->data;
    init_node_render_data(node);
   
  }
}

void free_osm_gfx(struct mapwin *mw, struct osm_file *osmf)
{
  
  GList *l;
  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    struct osm_way *way = (struct osm_way *)l->data;
    free(way->user_data);
  }
  for(l=g_list_first(osmf->nodes);l;l=g_list_next(l)) {
    struct osm_node *node = (struct osm_node *)l->data;
    if (node->user_data)
      free(node->user_data);
  }
  free_osm_file(osmf);
}

static void start_way_menu_cb(GtkWidget *w, gpointer data)
{
  struct mapwin *mw=(struct mapwin *)data;

  mw->osm_inf->newway_hwyclass=gtk_object_get_data(GTK_OBJECT(w),
						    "hwyclass");
  gtk_widget_hide(mw->osm_inf->hwypopup);
}

static void copy_tag_list(struct osm_object *obj, GList *tl)
{
  GList *l;
  for(l=g_list_first(tl);l;l=g_list_next(l)) {
    char *tag=(char *)l->data;
    int taglen=strlen(tag)+1;
    set_osm_tag(obj,tag,tag+taglen);
  }
}

static void start_way_cb(GtkWidget *w, gpointer data)
{
  struct mapwin *mw=(struct mapwin *)data;
  if (osm_waypresets) {
    if (mw->osm_inf->newway_tags) {
      free_tag_list(mw->osm_inf->newway_tags);
      mw->osm_inf->newway_tags=NULL;
    }
    osm_choose_tagpreset(osm_waypresets,NULL,(void *)&mw->osm_inf->newway_tags,NULL);
  } else {
    gtk_widget_show(mw->osm_inf->hwypopup);
  }
  mw->osm_inf->newwaypoints_start=g_list_last(*mw->gps_line_list);
  if (mw->osm_inf->newwaypoints_start) {
    set_way_state(mw->osm_inf,1);
  }
  mw->osm_inf->merge_first_node=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->liveeditb.automergebut));
}


static void merge_newway_node(struct osm_file *osmf,
			      struct osm_way *newway,
			      struct osm_node *mergenode,double maxdist)
{
  GList *l;
  int i;
  double mdist=maxdist;
  struct osm_way *mergeway=NULL;
  int nodeafter=0;
  double lon1,lon2,lat1,lat2;
  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    struct osm_way *way=(struct osm_way*)l->data;
    struct osm_node *node;
    if (way==newway)
      continue;
    if (!way->street)
      continue;
    node=get_osm_node(way->nodes[0]);
    if (!node)
      continue;
    lon2=node->lon;
    lat2=node->lat;
    for(i=1;i<way->nr_nodes;i++) {
      double dist;
      lon1=lon2;
      lat1=lat2;
      node=get_osm_node(way->nodes[i]);
      if (!node)
	break;
      lon2=node->lon;
      lat2=node->lat;
      dist=get_distance_r(lon1,lat1,lon2,lat2,mergenode->lon,mergenode->lat);
      if (dist < mdist) {
	mdist=dist;
	mergeway=way;
	nodeafter=i;
      }
    }
  }
  if (mergeway) {
    struct osm_node *n1;
    struct osm_node *n2;
    double d1,d2;
    n1=get_osm_node(mergeway->nodes[nodeafter-1]);
    n2=get_osm_node(mergeway->nodes[nodeafter]);
    d1=(n1->lon-mergenode->lon)*(n1->lon-mergenode->lon)+
      (n1->lat-mergenode->lat)*(n1->lat-mergenode->lat);
    d2=(n2->lon-mergenode->lon)*(n2->lon-mergenode->lon)+
      (n2->lat-mergenode->lat)*(n2->lat-mergenode->lat);
    if ((d1 <(maxdist*maxdist)) || (d2 <(maxdist*maxdist))) {
      mergenode->lon=n1->lon;
      mergenode->lat=n1->lat;
      if (d2 < d1) {
	mergenode->lon=n2->lon;
	mergenode->lat=n2->lat;
      }
    } else {
      move_node_between(n1->lon,n1->lat,n2->lon,n2->lat,mergenode);
      osm_set_node_coords(mergenode,mergenode->lon,mergenode->lat);
    }
    if ((n1->lon==mergenode->lon)&&(n1->lat==mergenode->lat)) {
      osm_merge_node(osmf,n1,mergenode);
    } else if ((n2->lon==mergenode->lon)&&(n2->lat==mergenode->lat)) {
      osm_merge_node(osmf,n2,mergenode);
    } else {
      osm_merge_into_way(mergeway,nodeafter,mergenode);
    }
  }
  
}

static void make_new_way(struct mapwin *mw)
{
  struct osm_node *nd;
  GList *l;

  GList *newwaypoints;

  struct osm_way *osmw;
  if (!mw->osm_inf->newwaypoints_start) {
    return;
  }
  l=g_list_last(*mw->gps_line_list);
  if ((l)&&(mw->osm_main_file)&&(l!=mw->osm_inf->newwaypoints_start)) {
    int n;
    newwaypoints=simplify_lines(mw->osm_inf->newwaypoints_start,l,0.3);
    newwaypoints=g_list_append(newwaypoints,l->data);
    n=g_list_length(newwaypoints);
    if (g_list_length(newwaypoints)==2) {
      GList *l2;
      GList *ltest;
      l2=g_list_copy(newwaypoints);
      l2=g_list_append(l2,g_list_first(newwaypoints)->data);
      ltest=simplify_lines(l2,g_list_last(l2),0.3);
      g_list_free(l2);
      n=g_list_length(ltest);
      g_list_free(ltest);
    }
    if (n>1) {
      GList *nds=NULL;
      mw->osm_main_file->changed=1;
      osmw=add_new_osm_way(mw->osm_main_file);
      if (mw->osm_inf->newway_hwyclass)
	set_osm_tag(&osmw->head,"highway",mw->osm_inf->newway_hwyclass);
      if (mw->osm_inf->newway_tags) {
	copy_tag_list(&osmw->head,mw->osm_inf->newway_tags);
      }
      for(l=g_list_first(newwaypoints);l;l=g_list_next(l)) {
	struct t_punkt32 *pt=(struct t_punkt32 *)l->data;
	nd=new_osm_node_from_point(mw->osm_main_file,
				   pt->longg/3600.0,pt->latt/3600.0);
	nds=g_list_append(nds,nd);
      }       
      add_nodes_to_way(osmw,nds);
      if (mw->osm_inf->merge_first_node) {
	merge_newway_node(mw->osm_main_file,osmw,get_osm_node(osmw->nodes[0]),
			  1.0/3600);
      }
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->liveeditb.automergebut))) {
	merge_newway_node(mw->osm_main_file,osmw,get_osm_node(osmw->nodes[osmw->nr_nodes-1]),1.0/3600.0);
      }
      g_list_free(nds);
    }
    g_list_free(newwaypoints);
  }
  
  mw->osm_inf->newwaypoints_start=NULL;
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
}

static void end_way_cb(GtkWidget *w, gpointer data)
{
  struct mapwin *mw=(struct mapwin *)data;
  make_new_way(mw);
  set_way_state(mw->osm_inf,0);
}

static void restart_way_cb(GtkWidget *w, gpointer data)
{
  struct mapwin *mw=(struct mapwin *)data;
  make_new_way(mw);
  mw->osm_inf->newwaypoints_start=g_list_last(*mw->gps_line_list);
  if (mw->osm_inf->newwaypoints_start) {
    mw->osm_inf->merge_first_node=gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->liveeditb.automergebut));
    set_way_state(mw->osm_inf,1);
  } else {
    set_way_state(mw->osm_inf,0);
  }
}

static void start_route_cb(GtkWidget *w,
			   gpointer user_data)
{
  struct osm_info *osm_inf=(struct osm_info *)user_data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.start_route))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.end_route),FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.set_destination),FALSE);
  }
  osm_inf->has_dest=0;
}

static void end_route_cb(GtkWidget *w, gpointer user_data)
{
  struct osm_info *osm_inf=(struct osm_info *)user_data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.end_route))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.start_route),FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.set_destination),FALSE);
  }

}

static void set_destination_cb(GtkWidget *w, gpointer user_data)
{
  struct osm_info *osm_inf=(struct osm_info *)user_data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.set_destination))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.start_route),FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->routeb.end_route),FALSE);
  }
  osm_inf->has_dest=0;

}

static int set_route_start(struct mapwin *mw, int x, int y)
{
  if (mw->osm_main_file) {
    int n = find_nearest_node(mw,mw->osm_main_file,x,y,1);
    if (n) {
      mw->osm_inf->route_start_node=n;
      
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.start_route),0);
      gtk_widget_set_sensitive(mw->osm_inf->routeb.end_route,1);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.end_route),1);
    }
  } 
  return 1;
}

static int set_route_end(struct mapwin *mw, int x, int y)
{
  GList *l=NULL;
  if (mw->osm_main_file) {
    osmroute_start_calculate(mw,mw->osm_main_file,
			     mw->osm_inf->route_start_node,
			     x,y);
    osmroute_add_path(mw,mw->osm_main_file,path_to_lines,x,y,&l);
    *mw->mark_line_list=g_list_concat(*mw->mark_line_list,l);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.end_route),0);
    gtk_widget_set_sensitive(mw->osm_inf->routeb.end_route,0);
  }
  return 1;
}

int osmroute_start_calculate_nodest(struct mapwin *mw,
				    struct osm_file *osmf,
				    int x, int y)
{
  int n = find_nearest_node(mw,mw->osm_main_file,x,y,1);
  return n?osmroute_start_calculate(mw,mw->osm_main_file,n,0,0):0;
}

static int set_destination(struct mapwin *mw, int x, int y)
{
  if (mw->osm_main_file) {
    int n = find_nearest_node(mw,mw->osm_main_file,x,y,1);
    if (n) {
      osmroute_start_calculate(mw,mw->osm_main_file,n,0,0);
      
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.set_destination),0);
      mw->osm_inf->has_dest=1;
    }
  }
  return 1;
}

int osm_center_handler(struct mapwin *mw, GdkGC *mygc, int x, int y)
{
  GList *l=NULL;
  if (mw->osm_inf->has_dest)  {
    osmroute_add_path(mw,mw->osm_main_file,path_to_lines,x,y,&l);
    draw_line_list(mw,mygc,l,NULL);
    free_line_list(l);
    return TRUE;
  }
  return FALSE;
  
}

static void get_way_dist(struct osm_way *way, int x, int y, int *md,
			 struct osm_way **minw, int *nodeafter)
{
  int x1,y1,x2,y2;
  int i;
  struct osm_node *node;
  struct node_render_data *nrd;
  if (!way->nodes)
    return;
  if (way->nr_nodes == 0) {
    return;
  }
  node=get_osm_node(way->nodes[0]);
  if (!node)
    return;
  nrd = (struct node_render_data *)node->user_data;
  if (!nrd)
    return;
  x2=nrd->x;
  y2=nrd->y;
  for(i=1;i<way->nr_nodes;i++) {
    int dist;
    x1=x2;
    y1=y2;
    node=get_osm_node(way->nodes[i]);
    if (!node)
      return;
    nrd = (struct node_render_data *)node->user_data;
    if (!nrd)
      return;
    x2=nrd->x;
    y2=nrd->y;
    dist = (int) get_distance_r(x1,y1,x2,y2,x,y);
    if (dist < *md) {
      *md=dist;
      *minw=way;
      *nodeafter=i;
    }
  }
}

static struct osm_object * find_nearest_object(struct osm_file *osmf,
					       int x, int y, struct osm_object *not_that,
					       int *nodeafter)
{

  GList *l;
  int md=10;
  int md_old=10;
  int nodeafter_old=0;
  struct osm_way *minw=NULL;
  struct osm_way *minw2=NULL;
  struct osm_node *minn=NULL;
  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    struct osm_way *way=(struct osm_way*)l->data;
    if (not_that != &(way->head))
      get_way_dist(way,x,y,&md,&minw,nodeafter);
    if (minw2!=minw) {
      int i;
      if ((not_that)&&(not_that->type == NODE)) {
	for(i=0;i<minw->nr_nodes;i++) {
	  if (minw->nodes[i]==not_that->id) {
	    minw=minw2;
	    *nodeafter=nodeafter_old;
	    md=md_old;
	    break;
	  }
	}
      }
      minw2=minw;
      md_old=md;
      nodeafter_old=*nodeafter;
    }
  }
  md=md+10*3/2;
  for(l=g_list_first(osmf->nodes);l;l=g_list_next(l)) {
    int dist;
    struct osm_node *node = (struct osm_node *)l->data;
    struct node_render_data *nrd = (struct node_render_data *) node->user_data;
    if (!nrd)
      continue;
    dist=abs(nrd->x-x);
    dist+=abs(nrd->y-y);
    if ((dist < md) && (not_that != &node->head)) {
      md=dist;
      minn=node;
    }
  }
  if (minn) {
    if (nodeafter)
      *nodeafter=0;
    return &minn->head;
  } else
    return &minw->head;
}

static void handle_edit_sel_click(struct mapwin *mw, int x, int y)
{
  int nodeafter=0;
  mw->osm_inf->selected_object = find_nearest_object(mw->osm_main_file,
						     x,y,NULL,&nodeafter);
  
  if (mw->osm_inf->selected_object) {
    display_tags(mw->osm_inf,mw->osm_inf->selected_object);
    if (mw->osm_inf->selected_object->type == NODE)
      mw->osm_inf->moving_node=1;
  } else {
    double longsec=0;
    double lattsec=0;
    if (((x-mw->page_x)<(mw->page_width/4)) ||
	((x-mw->page_x-mw->page_width)>(-mw->page_width/4)) ||
	((y-mw->page_y)<(mw->page_height/4)) ||
	((y-mw->page_y-mw->page_height)>(-mw->page_height/4))) {
      point2geosec(&longsec,&lattsec,x,y);
      center_map(mw,longsec,lattsec);
    }
  }
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
}

static void handle_edit_addway_click(struct mapwin *mw,
				     int x, int y)
{
  double lon,lat;
  int nodeafter=0;
  GList *l=NULL;
  struct osm_node *nd;
  struct osm_object *nearest_obj;
  struct osm_way *newway;
  mw->osm_main_file->changed=1;
  if (!mw->osm_inf->way_to_edit) {
    mw->osm_inf->way_to_edit=add_new_osm_way(mw->osm_main_file);
    if (osm_waypresets)
      osm_choose_tagpreset(osm_waypresets,set_tag_cb,
			   (void *)mw->osm_inf->way_to_edit,mw);
  }
  newway=mw->osm_inf->way_to_edit;
  nearest_obj=find_nearest_object(mw->osm_main_file,
				  x,y,NULL,&nodeafter);
  if ((nearest_obj)&&(nearest_obj->type==NODE)) {
    if ((newway->nr_nodes)&&(nearest_obj->id==newway->nodes[newway->nr_nodes-1])) {
      return;
    }
    l=g_list_append(l,nearest_obj);
  } else {
    point2geosec(&lon, &lat,x,y);
    lon=lon/3600.0;
    lat=lat/3600.0;
    nd=new_osm_node_from_point(mw->osm_main_file,
			       lon,lat);
    center_map(mw,lon*3600.0,lat*3600.0);
    l=g_list_append(l,nd);
    if (nodeafter) {
      struct osm_way *mergeway=(struct osm_way *)nearest_obj;
      struct osm_node *n1=get_osm_node(mergeway->nodes[nodeafter-1]);
      struct osm_node *n2=get_osm_node(mergeway->nodes[nodeafter]);  
      if (n1&&n2) {
	move_node_between(n1->lon,n1->lat,n2->lon,n2->lat,nd);
	osm_set_node_coords(nd,nd->lon,nd->lat);
	osm_merge_into_way(mergeway,nodeafter,nd);
      }
    }
  } 
 
  add_nodes_to_way(mw->osm_inf->way_to_edit,l);
  g_list_free(l);
  mw->osm_inf->selected_object=&mw->osm_inf->way_to_edit->head;
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
}

static void handle_node_move(struct mapwin *mw, int x, int y)
{
  double nx,ny;
  double lon,lat;
  struct osm_node *node;
  if (!(mw->osm_inf->selected_object))
    return;
  if (mw->osm_inf->selected_object->type!=NODE)
    return;
  node=(struct osm_node *)
    mw->osm_inf->selected_object;
  geosec2point(&nx,&ny,node->lon*3600.0,node->lat*3600.0);
  nx=x-mw->mouse_x+nx;
  ny=y-mw->mouse_y+ny;
  point2geosec(&lon,&lat,nx,ny);
  lon/=3600.0;
  lat/=3600.0;
  mw->osm_main_file->changed=1;
  osm_set_node_coords(node,lon,lat);
  init_node_render_data(node);
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
}

int osm_mouse_handler(struct mapwin *mw, int x, int y, int millitime, int state)
{
  
  int above_limit;
  if (mw->osm_inf->clicktime==0) {
    above_limit=1;
  } else {
    above_limit=((millitime-mw->osm_inf->clicktime)>1000);
  }
  if (!state) {
    if ((mw->osm_inf->moving_node)&&(mw->osm_inf->selected_object)) {
      mw->osm_inf->moving_node=0;
      if (above_limit) {
	handle_node_move(mw,x,y);
      }
      return TRUE; 
    } else {
      return FALSE;
    }
  }
  mw->osm_inf->moving_node=0;
  if (GTK_WIDGET_MAPPED(mw->osm_inf->routeb.start_route)&&gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.start_route))) {
    if (above_limit) {
      if (mw->osm_main_file)
	set_route_start(mw,x,y);
      mw->osm_inf->clicktime=millitime;
    }
    return TRUE;


  } else if (GTK_WIDGET_MAPPED(mw->osm_inf->routeb.end_route)&&gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.end_route))) {
    if (above_limit) {
      if (mw->osm_main_file) {
	set_route_end(mw,x,y);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.start_route),1);
	gtk_widget_queue_draw_area(mw->map,0,0,
				   mw->page_width,
				   mw->page_height);
      }
      mw->osm_inf->clicktime=millitime;
    }
    return TRUE;
  } else if (GTK_WIDGET_MAPPED(mw->osm_inf->routeb.set_destination)&&gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->routeb.set_destination))) {
    if (above_limit) {
      if (mw->osm_main_file)
	set_destination(mw,x,y);
      mw->osm_inf->clicktime=millitime;
    }
    return TRUE;
  } else if ((GTK_WIDGET_MAPPED(mw->osm_inf->liveeditb.startwaybut))&&(osm_nodepresets)) {
    if (above_limit) {
      if (mw->osm_main_file) {
	struct osm_node *nd;
	double lon=0;
	double lat=0;
	point2geosec(&lon,&lat,x,y);
	lon/=3600.0;
	lat/=3600.0;
	nd=new_osm_node_from_point(mw->osm_main_file,lon,lat);
	osm_choose_tagpreset(osm_nodepresets,NULL,&nd->head.tag_list,NULL);
	mw->osm_inf->clicktime=millitime;
      }
    }
    return TRUE;
  } else if ((GTK_WIDGET_MAPPED(mw->osm_inf->editb.selbut))&&
	     (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->editb.selbut)))) {
    if (mw->osm_main_file)
      handle_edit_sel_click(mw,x,y);
    mw->osm_inf->clicktime=millitime;
    
    return TRUE;
  } else if ((GTK_WIDGET_MAPPED(mw->osm_inf->editb.addwaybut))&&
	     (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(mw->osm_inf->editb.addwaybut)))) {
    if (above_limit) {
      if (mw->osm_main_file)
	handle_edit_addway_click(mw,x,y);
      mw->osm_inf->clicktime=millitime;
    }
    return TRUE;
  }
  return FALSE;
}

static void osmedit_addwaybut_cb(GtkWidget *w, gpointer data)
{
  struct osm_info *osm_inf=(struct osm_info *)data;
  osm_inf->way_to_edit=NULL;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(osm_inf->editb.addwaybut))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->editb.selbut),FALSE);
  }
}

static void osmedit_selbut_cb(GtkWidget *w, gpointer data)
{
  struct osm_info *osm_inf=(struct osm_info *)data;
  osm_inf->way_to_edit=NULL;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(osm_inf->editb.selbut))) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(osm_inf->editb.addwaybut),FALSE);
  }
}

static void osmedit_joinbut_cb(GtkWidget *w, gpointer data)
{
  struct mapwin *mw = (struct mapwin *)data;
  struct osm_object *nearest_obj;
  struct osm_node *node;
  int nodeafter=0;
  double x,y;
  mw->osm_inf->way_to_edit=NULL;
  if (mw->osm_inf->selected_object == NULL)
    return;
  if (mw->osm_inf->selected_object->type != NODE)
    return;
  node=(struct osm_node *)mw->osm_inf->selected_object;
  geosec2point(&x,&y,node->lon*3600.0,node->lat*3600.0);
  nearest_obj=find_nearest_object(mw->osm_main_file,
				  x,y,mw->osm_inf->selected_object,
				  &nodeafter);
  if (nearest_obj == NULL)
    return;
  mw->osm_main_file->changed=1;
  if (nodeafter) {
    struct osm_way *mergeway=(struct osm_way *)nearest_obj;
    struct osm_node *n1=get_osm_node(mergeway->nodes[nodeafter-1]);
    struct osm_node *n2=get_osm_node(mergeway->nodes[nodeafter]);  
    if (n1&&n2) {
      move_node_between(n1->lon,n1->lat,n2->lon,n2->lat,node);
      osm_set_node_coords(node,node->lon,node->lat);
      osm_merge_into_way(mergeway,nodeafter,node);
    }
  } else if (nearest_obj->type == NODE) {
    osm_merge_node(mw->osm_main_file,(struct osm_node *)nearest_obj,
		   node);
    mw->osm_inf->selected_object=nearest_obj;
  }
  
}

static void osmedit_splitbut_cb(GtkWidget *w, gpointer data)
{
  struct mapwin *mw = (struct mapwin *)data;
  struct osm_node *node;
  mw->osm_inf->way_to_edit=NULL;
  if (mw->osm_inf->selected_object == NULL)
    return;
  if (mw->osm_inf->selected_object->type != NODE)
    return;
  node=(struct osm_node *)mw->osm_inf->selected_object;
  mw->osm_main_file->changed=1;
  osm_split_ways_at_node(mw->osm_main_file,
			 node);
}

static void osmedit_delobjbut_cb(GtkWidget *w, gpointer data)
{
  struct mapwin *mw = (struct mapwin *)data;
  mw->osm_inf->way_to_edit=NULL;
  if (mw->osm_inf->selected_object == NULL)
    return;
  if (mw->osm_inf->selected_object->type == WAY) {
    osm_delete_way(mw->osm_main_file,
		   (struct osm_way *)mw->osm_inf->selected_object);
  } else {
    osm_delete_node(mw->osm_main_file,
		    (struct osm_node *)mw->osm_inf->selected_object);
  }
  mw->osm_inf->selected_object=NULL;
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
}

void append_osm_edit_line(struct mapwin *mw,GtkWidget *box)
{
  struct sidebar_mode *sm;
  GtkTooltips *tt = gtk_tooltips_new();
  mw->osm_inf->editbar=gtk_vbox_new(TRUE,0);
  mw->osm_inf->liveeditb.startwaybut=make_pixmap_button(mw,start_way);
  mw->osm_inf->liveeditb.endwaybut=make_pixmap_button(mw,end_way);
  mw->osm_inf->liveeditb.restartwaybut=make_pixmap_button(mw,restart_way);
  mw->osm_inf->liveeditb.automergebut=make_pixmap_toggle_button(mw,automerge);
  gtk_box_pack_start(GTK_BOX(box),mw->osm_inf->editbar,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->editbar),mw->osm_inf->liveeditb.startwaybut,TRUE,TRUE,0);
  gtk_widget_show(mw->osm_inf->liveeditb.startwaybut);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->liveeditb.startwaybut),"clicked",GTK_SIGNAL_FUNC(start_way_cb),mw);
  gtk_tooltips_set_tip(tt,mw->osm_inf->liveeditb.startwaybut,
		   _("start a new way"),NULL);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->editbar),mw->osm_inf->liveeditb.endwaybut,
		     TRUE,TRUE,0);
  gtk_widget_show(mw->osm_inf->liveeditb.endwaybut);
  gtk_widget_set_sensitive(mw->osm_inf->liveeditb.startwaybut,0);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->liveeditb.endwaybut),"clicked",GTK_SIGNAL_FUNC(end_way_cb),mw);
  gtk_widget_set_sensitive(mw->osm_inf->liveeditb.endwaybut,0);
  gtk_tooltips_set_tip(tt,mw->osm_inf->liveeditb.endwaybut,
		   _("finish a new way"),NULL);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->editbar),mw->osm_inf->liveeditb.restartwaybut,TRUE,TRUE,0);
  gtk_widget_show(mw->osm_inf->liveeditb.restartwaybut);
  gtk_tooltips_set_tip(tt,mw->osm_inf->liveeditb.restartwaybut,
		       _("finish a way and start a new one at the same place"),NULL);
  gtk_widget_set_sensitive(mw->osm_inf->liveeditb.restartwaybut,0);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->liveeditb.restartwaybut),"clicked",
		     GTK_SIGNAL_FUNC(restart_way_cb),mw);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->editbar),mw->osm_inf->liveeditb.automergebut,
		     TRUE,TRUE,0);
  gtk_widget_show(mw->osm_inf->liveeditb.automergebut);
  gtk_tooltips_set_tip(tt,mw->osm_inf->liveeditb.automergebut,
		       _("auto-merge the start/end of a way to an existing way"),NULL);
  mw->osm_inf->routebar=gtk_vbox_new(TRUE,0);
  mw->osm_inf->routeb.start_route=make_pixmap_toggle_button(mw,start_route);
  mw->osm_inf->routeb.end_route=make_pixmap_toggle_button(mw,end_route);
  mw->osm_inf->routeb.set_destination=make_pixmap_toggle_button(mw,routestartgps);
  gtk_box_pack_start(GTK_BOX(box),mw->osm_inf->routebar,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->routebar),mw->osm_inf->routeb.start_route,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->routebar),mw->osm_inf->routeb.end_route,TRUE,TRUE,0);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->routebar),mw->osm_inf->routeb.set_destination,TRUE,TRUE,0);
  gtk_widget_show(mw->osm_inf->routeb.start_route);
  gtk_tooltips_set_tip(tt,mw->osm_inf->routeb.start_route,_("set the start point for the route calculation"),NULL);
  gtk_widget_show(mw->osm_inf->routeb.end_route);
  gtk_tooltips_set_tip(tt,mw->osm_inf->routeb.end_route,_("set the destination point for the route calculation"),NULL);
  gtk_widget_show(mw->osm_inf->routeb.set_destination);
  gtk_tooltips_set_tip(tt,mw->osm_inf->routeb.set_destination,_("navigate to the given destination"),NULL);
  gtk_widget_set_sensitive(mw->osm_inf->routeb.end_route,FALSE);
  gtk_widget_set_sensitive(mw->osm_inf->routeb.start_route,FALSE);
  gtk_widget_set_sensitive(mw->osm_inf->routeb.set_destination,FALSE);
  mw->osm_inf->meditbar=gtk_vbox_new(TRUE,0);
  gtk_box_pack_start(GTK_BOX(box),mw->osm_inf->meditbar,FALSE,FALSE,0);
  mw->osm_inf->editb.selbut=make_pixmap_toggle_button(mw,selosmobj);
  gtk_tooltips_set_tip(tt,mw->osm_inf->editb.selbut,
		       _("select the object to edit"),NULL);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->meditbar),mw->osm_inf->editb.selbut,TRUE,TRUE,0);
  mw->osm_inf->editb.addwaybut=make_pixmap_toggle_button(mw,newosmway);
  gtk_tooltips_set_tip(tt,mw->osm_inf->editb.addwaybut,
		       _("add a new way"),NULL);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->meditbar),mw->osm_inf->editb.addwaybut,
		     TRUE,TRUE,0);
  mw->osm_inf->editb.delobjbut=make_pixmap_button(mw,delosmobj);
  gtk_tooltips_set_tip(tt,mw->osm_inf->editb.delobjbut,
		       _("delete the currently selected object"),NULL);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->meditbar),mw->osm_inf->editb.delobjbut,
		     TRUE,TRUE,0);
  mw->osm_inf->editb.joinbut=gtk_button_new_with_label(" J ");
  gtk_tooltips_set_tip(tt,mw->osm_inf->editb.joinbut,
		       _("join the selected node with the nearest way"),
		       NULL);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->meditbar),
		     mw->osm_inf->editb.joinbut,
		     TRUE,TRUE,0);
  mw->osm_inf->editb.splitbut=gtk_button_new_with_label(" S ");
  gtk_tooltips_set_tip(tt,mw->osm_inf->editb.splitbut,
		       _("split the ways going through the selected node"),
		       NULL);
  gtk_box_pack_start(GTK_BOX(mw->osm_inf->meditbar),
		     mw->osm_inf->editb.splitbut,
		     TRUE,TRUE,0);
  gtk_widget_show(mw->osm_inf->editb.delobjbut);
  gtk_widget_show(mw->osm_inf->editb.addwaybut);
  gtk_widget_show(mw->osm_inf->editb.selbut);
  gtk_widget_show(mw->osm_inf->editb.joinbut);
  gtk_widget_show(mw->osm_inf->editb.splitbut);
  gtk_widget_set_sensitive(mw->osm_inf->editb.delobjbut,0);
  gtk_widget_set_sensitive(mw->osm_inf->editb.selbut,0);
  gtk_widget_set_sensitive(mw->osm_inf->editb.addwaybut,0);
  gtk_widget_set_sensitive(mw->osm_inf->editb.joinbut,0);
  gtk_widget_set_sensitive(mw->osm_inf->editb.splitbut,0);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->editb.selbut),"clicked",
		     GTK_SIGNAL_FUNC(osmedit_selbut_cb),mw->osm_inf);
  
  gtk_signal_connect_object(GTK_OBJECT(mw->osm_inf->editb.selbut),"clicked",			    
			    GTK_SIGNAL_FUNC(gtk_widget_grab_focus),mw->map);

  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->editb.splitbut),"clicked",
		     GTK_SIGNAL_FUNC(osmedit_splitbut_cb),mw);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->editb.addwaybut),"clicked",
		     GTK_SIGNAL_FUNC(osmedit_addwaybut_cb),mw->osm_inf);
  gtk_signal_connect_object(GTK_OBJECT(mw->osm_inf->editb.addwaybut),"clicked",			    
			    GTK_SIGNAL_FUNC(gtk_widget_grab_focus),mw->map);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->editb.delobjbut),"clicked",
		     GTK_SIGNAL_FUNC(osmedit_delobjbut_cb),mw);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->editb.joinbut),"clicked",
		     GTK_SIGNAL_FUNC(osmedit_joinbut_cb),mw);
  
  sm=calloc(sizeof(struct sidebar_mode),1);
  sm->name=_("routing");
  sm->w=mw->osm_inf->routebar;
  mw->modelist=g_list_append(mw->modelist,sm);
  sm=calloc(sizeof(struct sidebar_mode),1);
  sm->name=_("live edit");
  sm->w=mw->osm_inf->editbar;
  mw->modelist=g_list_append(mw->modelist,sm);
  sm=calloc(sizeof(struct sidebar_mode),1);
  sm->name=_("editing");
  sm->w=mw->osm_inf->meditbar;
  mw->modelist=g_list_append(mw->modelist,sm);
  
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->routeb.start_route),"clicked",
		     GTK_SIGNAL_FUNC(start_route_cb),mw->osm_inf);
  gtk_signal_connect_object(GTK_OBJECT(mw->osm_inf->routeb.start_route),"clicked",
			    
			    GTK_SIGNAL_FUNC(gtk_widget_grab_focus),mw->map);

  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->routeb.end_route),"clicked",
		     GTK_SIGNAL_FUNC(end_route_cb),mw->osm_inf);
  gtk_signal_connect_object(GTK_OBJECT(mw->osm_inf->routeb.end_route),"clicked",
			    
			    GTK_SIGNAL_FUNC(gtk_widget_grab_focus),mw->map);
  gtk_signal_connect(GTK_OBJECT(mw->osm_inf->routeb.set_destination),"clicked",
		     GTK_SIGNAL_FUNC(set_destination_cb),mw->osm_inf);
  gtk_signal_connect_object(GTK_OBJECT(mw->osm_inf->routeb.set_destination),"clicked",
			    
			    GTK_SIGNAL_FUNC(gtk_widget_grab_focus),mw->map);
}




void load_osm_gfx(struct mapwin *mw, const char *name)
{
  struct osm_file *osmf=parse_osm_file(mw->osm_main_file,
				       name,0);
  if (!osmf)
    return;
  mw->osm_inf->way_to_edit=NULL;
  mw->osm_inf->selected_object=NULL;
 
  if (mw->osm_main_file)
    free_osm_file(osmf);
  else
    mw->osm_main_file=osmf;
  recalc_node_coordinates(mw,mw->osm_main_file);
  menu_item_set_state(mw,  MENU_VIEW_OSM_DATA, 1);
  menu_item_set_state(mw, MENU_OSM_AUTO_SELECT,1);
  menu_item_set_state(mw, MENU_OSM_DISPLAY_NODES,1);
  menu_item_set_state(mw, MENU_OSM_DISPLAY_STREET_BORDERS,1);
  menu_item_set_state(mw, MENU_OSM_DISPLAY_ALL_WAYS,1),
  menu_item_set_state(mw, MENU_OSM_DISPLAY_TAGS,1);
  gtk_widget_set_sensitive(mw->osm_inf->liveeditb.startwaybut,1);
  gtk_widget_set_sensitive(mw->osm_inf->routeb.start_route,1);
  gtk_widget_set_sensitive(mw->osm_inf->routeb.set_destination,1);
  gtk_widget_set_sensitive(mw->osm_inf->editb.delobjbut,1);
  gtk_widget_set_sensitive(mw->osm_inf->editb.selbut,1);
  gtk_widget_set_sensitive(mw->osm_inf->editb.addwaybut,1);
  gtk_widget_set_sensitive(mw->osm_inf->editb.joinbut,1);
  gtk_widget_set_sensitive(mw->osm_inf->editb.splitbut,1);

}

GtkItemFactoryEntry *get_osm_menu_items(int *n_osm_items)
{
  static GtkItemFactoryEntry osmitems[]={
    {MENU_VIEW_OSM_DATA_N,NULL,GTK_SIGNAL_FUNC(toggle_osm_view),0,
     "<CheckItem>"},
    {MENU_OSM_AUTO_SELECT_N,NULL,GTK_SIGNAL_FUNC(toggle_select_center),0,
     "<CheckItem>"},
    {MENU_OSM_DISPLAY_NODES_N,NULL,GTK_SIGNAL_FUNC(toggle_display_nodes),
     0,"<CheckItem>"},
    {MENU_OSM_DISPLAY_STREET_BORDERS_N, NULL, GTK_SIGNAL_FUNC(toggle_display_street_borders), 
     0,"<CheckItem>"},
    {MENU_OSM_DISPLAY_ALL_WAYS_N, NULL, GTK_SIGNAL_FUNC(toggle_display_all_ways),
     0,"<CheckItem>"},
    /*    {MENU_OSM_DISPLAY_EDIT_BAR_N, NULL, GTK_SIGNAL_FUNC(toggle_display_bar),0,"<RadioItem>"},
	  {MENU_OSM_DISPLAY_ROUTE_BAR_N, NULL, GTK_SIGNAL_FUNC(toggle_display_bar),1, MENU_OSM_DISPLAY_EDIT_BAR_N}, */
    {MENU_OSM_DISPLAY_TAGS_N, NULL, GTK_SIGNAL_FUNC(toggle_display_tags),
     0,"<CheckItem>"},
  };
  *n_osm_items=sizeof(osmitems)/sizeof(osmitems[0]);
  return osmitems;
}
