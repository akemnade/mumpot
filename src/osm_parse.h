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
#ifndef K_OSM_PARSE_H
#define K_OSM_PARSE_H
struct osm_object {
  GList *tag_list;
  char *timestamp;
  char *user;
  int id;
  int version;
  int modified:1;
  enum { NODE, RELATION, WAY } type:8;
};

extern int max_free_num;

struct osm_node {
   struct osm_object head;
  short nr_ways;
  /* GList *segment_list; */
   GList *way_list; 
  /* int nr_segments; */
  
  double lat;
  double lon;

  void *user_data;
};

struct osm_way {
  struct osm_object head;
  int *nodes;
  int nr_nodes;
  unsigned foot:1;
  unsigned bicycle:1;
  unsigned motorcar:1;
  unsigned oneway:1;
  unsigned street:1;
  void *user_data;
};

struct osm_file {
  xmlDocPtr xmldoc;
  xmlNodePtr headnode;
  GList *ways;
  GList *nodes;
  struct osm_way **deleted_ways;
  struct osm_node **deleted_nodes;
  int deleted_way_count;
  int deleted_node_count;
  int changed;
  int merged;
};

struct osm_object *get_obj_id(int id);
void put_obj_id(struct osm_object *obj,int id);
struct osm_way *new_osm_way(int id);
struct osm_way *get_osm_way(int id);
struct osm_node *new_osm_node(int id);
struct osm_node *get_osm_node(int id);
void free_osm_way(int id);
void free_osm_node(int id);
struct osm_file * parse_osm_file(struct osm_file *mergeto,
				 const char *fname, int all_ways);
void free_osm_file(struct osm_file *f);
xmlNodePtr next_el_node(xmlNodePtr node);
void free_tag_list(GList *tl);
char *get_tag_value(struct osm_object *obj, const char *key);
void set_osm_tag(struct osm_object *obj, char *k, char *v);
struct osm_way *add_new_osm_way(struct osm_file *f);
struct osm_node *new_osm_node_from_point(struct osm_file *f,
					 double lon, double lat);
void add_nodes_to_way(struct osm_way *way, GList *l);
void osm_merge_into_way(struct osm_way *mergeway, int pos,
			struct osm_node *node);
void osm_merge_node(struct osm_file *osmf,
		    struct osm_node *mergeto, 
		    struct osm_node *mergefrom);
void osm_delete_way(struct osm_file *osmf,
		    struct osm_way *way);
void osm_delete_node(struct osm_file *osmf,
		     struct osm_node *node);
void osm_set_node_coords(struct osm_node *node,double lon, double lat);
int save_osm_file(const char *fname, struct osm_file *osmf);
int save_osmchange_file(const char *fname, struct osm_file *osmf,
			int changeset);
char *save_osmchange_buf(struct osm_file *osmf, int changeset);
void osm_way_update_id(struct osm_file *osmf,
		       int fromnum,int tonum,int version);
void osm_node_update_id(struct osm_file *osmf,
			int fromnum,int tonum,int version);
void printtimediff(const char *format,const struct timeval *tvstart, const struct timeval *tvend);
void osm_split_way_at_node(struct osm_file *osmf,
			   struct osm_way *way,
			   struct osm_node *nd);
void osm_split_ways_at_node(struct osm_file *osmf,
			    struct osm_node *nd);

#endif
