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
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <locale.h>
#ifdef HAVE_BZLIB_H
#include <bzlib.h>
#endif
#include "common.h"
#include "osm_parse.h"
#ifndef OSMAPI_VERSION
#define OSMAPI_VERSION "0.6"
#endif

static GMemChunk *node_chunk;
static GMemChunk *way_chunk;
#ifdef USE_GTREE
static GTree *all_nodes[1];
static GTree *all_ways[1];
#else
static struct osm_object ***all_ways[65536*4];
static struct osm_object ***all_nodes[65536*4];
#endif
int max_free_num=-1;
static int total_way_count;
static int total_node_count;
static int index_total1,index_total2;
static void osmway_initialize_flags(struct osm_way *way);
enum format_type {
  FORMAT_OSC,
  FORMAT_JOSM,
  FORMAT_JOSMDEL
};
#ifdef USE_GTREE

gint osmhead_cmp(gconstpointer a, gconstpointer b)
{
  if ((*(int *)a)==(*(int *)b))
    return 0;
  return ((*(int *)a)<(*(int *)b))?1:-1;
}

struct osm_object *get_obj_id(GTree **tree, int id)
{
  if (!*tree)
    *tree=g_tree_new(osmhead_cmp);
  return (struct osm_object *)g_tree_lookup(*tree,
					    &id);
}

void put_obj_id(GTree **tree, struct osm_object *obj,int id)
{
  if (!*tree)
    *tree=g_tree_new(osmhead_cmp);
  if (!obj)
    g_tree_remove(*tree,&id);
  g_tree_insert(*tree,&obj->id,obj);
}
#else

void put_obj_id(struct osm_object ****all_objects,
                struct osm_object *obj,int id)
{
  struct osm_object ***ind;
    if (!all_objects[(id>>16)&0xffff]) {
      all_objects[(id>>16)&0xffff]=calloc(sizeof(struct osm_object ***),256);
      index_total1+=256*(sizeof(void *));
    }
    ind=all_objects[(id>>16)&0xffff];
    if (!ind[(id>>8)&0xff]) {
      ind[(id>>8)&0xff]=calloc(sizeof(struct osm_object **),256);
      index_total2+=256*(sizeof(void *));
    }
    (ind[(id>>8)&0xff])[id&0xff]=obj;
    if (id<=max_free_num) {
      max_free_num=id-1;
    }
}
struct osm_object *get_obj_id(struct osm_object ****all_objects,
                              int id)
{
  struct osm_object ***ind;
    if (!all_objects[(id>>16)&0xffff])
      return NULL;
    ind=all_objects[(id>>16)&0xffff];
    if (!ind[(id>>8)&0xff])
      return NULL;
    return (ind[(id>>8)&0xff])[id&0xff];
}

#endif
#if 0
static void add_created_tag(struct osm_object *obj)
{
  char *ctag=malloc(sizeof("created_by\0" PACKAGE));
  memcpy(ctag,"created_by\0" PACKAGE,sizeof("created_by\0" PACKAGE));
  obj->tag_list=g_list_append(obj->tag_list,ctag);
}
#endif

xmlNodePtr next_named_node(xmlNodePtr node, char *name)
{
  while(node) {
    node=next_el_node(node);
    if(!node)
      break;
    if (!strcmp((char *)node->name,name))
      return node;
    node=node->next;
  }
  return node;
}

void set_osm_tag(struct osm_object *obj, char *key, char *v)
{
  GList *l=g_list_first(obj->tag_list);
  char *knew=malloc(strlen(key)+strlen(v)+2);
  strcpy(knew,key);
  strcpy(knew+strlen(key)+1,v);
  obj->modified=1;
  for(;l;l=g_list_next(l)) {
    char *korig=(char *)l->data;
    if (!strcmp(key,korig)) {
      l->data=knew;
      free(korig);
      break;
    }
  }
  if (!l) {
    obj->tag_list=g_list_append(obj->tag_list,knew);
  }
  if (obj->type==WAY) {
    osmway_initialize_flags((struct osm_way *)obj);
  }
}

void osm_split_way_at_node(struct osm_file *osmf,
			   struct osm_way *way,
			   struct osm_node *nd)
{
  int i;
  GList *l;
  struct osm_way *way2;
  for(i=1;i<way->nr_nodes;i++) {
    if (way->nodes[i]==nd->head.id)
      break;
  }
  if ((i==0)||(i==way->nr_nodes)||((i+1)==way->nr_nodes)) {
    return;
  }
  osmf->changed=1;
  way2=add_new_osm_way(osmf);
  way2->nr_nodes=way->nr_nodes-i;
  way2->nodes=malloc(sizeof(int)*(way2->nr_nodes));
  memcpy(way2->nodes,way->nodes+i,way2->nr_nodes*sizeof(int));
  way->nr_nodes=i+1;
  way->head.modified=1;
  nd->way_list=g_list_append(nd->way_list,way2);
  nd->nr_ways++;
  for(i=1;i<way2->nr_nodes;i++) {
    nd=get_osm_node(way2->nodes[i]);
    if (nd) {
      nd->way_list=g_list_remove(nd->way_list,way);
      nd->way_list=g_list_append(nd->way_list,way2);
    }
  }
  for(l=g_list_first(way->head.tag_list);l;l=g_list_next(l)) {
    char *t;
    char *t2;
    int len;
    t=(char *)l->data;
    len=strlen(t);
    len++;
    len+=strlen(t+len);
    len++;
    t2=malloc(len);
    memcpy(t2,t,len);
    way2->head.tag_list=g_list_append(way2->head.tag_list,t2);
  }
  osmway_initialize_flags(way2);
}

void osm_split_ways_at_node(struct osm_file *osmf,
			    struct osm_node *nd)
{
  GList *l;
  for(l=g_list_first(nd->way_list);l;l=g_list_next(l)) {
    osm_split_way_at_node(osmf,(struct osm_way *)l->data,nd);
  }
}



struct osm_way *new_osm_way(int id)
{
  struct osm_way *way;
  if (!way_chunk) 
    way_chunk=g_mem_chunk_create(struct osm_way,1024,G_ALLOC_AND_FREE);
  way=g_chunk_new0(struct osm_way,way_chunk);
  total_way_count++;
  way->head.id=id;
  way->head.type=WAY;
  put_obj_id(all_ways,&way->head,id);
  return way;
}

struct osm_way *add_new_osm_way(struct osm_file *f)
{
  struct osm_way *way=new_osm_way(max_free_num);
  f->ways=g_list_append(f->ways,way);
  return way;
}

struct osm_node *new_osm_node_from_point(struct osm_file *f,
					 double lon, double lat)
{
  struct osm_node *nd=new_osm_node(max_free_num);
  nd->lon=lon;
  nd->lat=lat;
  f->nodes=g_list_append(f->nodes,nd);
  return nd;
}

void osm_way_update_id(struct osm_file *osmf,
		       int fromnum,int tonum,int version)
{
  struct osm_way *way=get_osm_way(fromnum);
  if (!way)
    return;
  way->head.id=tonum;
  way->head.version=version;
  way->head.modified=0;
  put_obj_id(all_ways,NULL,fromnum);
  put_obj_id(all_ways,&way->head,tonum);
}

void osm_node_update_id(struct osm_file *osmf,
			int fromnum,int tonum,int version)
{
  struct osm_node *nd=get_osm_node(fromnum);
  GList *l;
  if (!nd)
    return;
  nd->head.id=tonum;
  nd->head.version=version;
  nd->head.modified=0;
  put_obj_id(all_nodes,NULL,fromnum);
  put_obj_id(all_nodes,&nd->head,tonum);

  for(l=g_list_first(nd->way_list);l;l=g_list_next(l)) {
    int i;
    struct osm_way *way=(struct osm_way *)l->data;
    for(i=0;i<way->nr_nodes;i++) {
      if (way->nodes[i]==fromnum)
	way->nodes[i]=tonum;
    }
  }
}



static void remove_node_from_way(struct osm_way *way,
				 struct osm_node *node)
{
  int i,j;
  for(i=0,j=0;i<way->nr_nodes;i++) {
    if (way->nodes[i]==node->head.id) {
      node->way_list=g_list_remove(node->way_list,way);
      node->nr_ways--;
    } else {
      way->nodes[j]=way->nodes[i];
      j++;
    }
  }
  way->head.modified=1;
  way->nr_nodes=j;
}

void osm_delete_way(struct osm_file *osmf,
		    struct osm_way *way)
{
  struct osm_node *node;
  int i;
  for(i=0;i<way->nr_nodes;i++) {
    node=get_osm_node(way->nodes[i]);
    node->way_list=g_list_remove(node->way_list,way);
    node->nr_ways--;
    if (node->nr_ways==0) {
      osm_delete_node(osmf,node);
    }
  }
  if (0==(osmf->deleted_way_count&0xf)) {
    osmf->deleted_ways=(struct osm_way **)realloc(osmf->deleted_ways,
						 (16+osmf->deleted_way_count)*sizeof(struct osm_way *));
  }
  osmf->deleted_ways[osmf->deleted_way_count]=way;
  osmf->deleted_way_count++;
  way->head.modified=1;
  put_obj_id(all_ways,NULL,way->head.id);
  osmf->ways=g_list_remove(osmf->ways,way);
  osmf->changed=1;
}

void osm_delete_node(struct osm_file *osmf,
		     struct osm_node *node)
{
  GList *l;
  while(node->way_list) {
    struct osm_way *way=(struct osm_way *)node->way_list->data;
    remove_node_from_way(way,node);
    if (way->nr_nodes<2) {
      osm_delete_way(osmf,way);
    }
  }
  if ((osmf->deleted_node_count&0xf)==0) {
    osmf->deleted_nodes=(struct osm_node **)realloc(osmf->deleted_nodes,
						    (osmf->deleted_node_count+16)*sizeof(struct osm_node *));
  }
  osmf->deleted_nodes[osmf->deleted_node_count]=node;
  osmf->deleted_node_count++;
  node->head.modified=1;
  osmf->nodes=g_list_remove(osmf->nodes,node);
  put_obj_id(all_nodes,NULL,node->head.id);
  osmf->changed=1;
}

void add_nodes_to_way(struct osm_way *way, GList *l)
{
  int offset=way->nr_nodes;
  int la=g_list_length(l);
  int i;
  way->nr_nodes+=la;
  way->nodes=realloc(way->nodes,sizeof(int)*way->nr_nodes);
  for(i=offset;l;l=g_list_next(l),i++) {
    struct osm_node *nd=(struct osm_node *)l->data;
    way->nodes[i]=nd->head.id;
    nd->way_list=g_list_append(nd->way_list,way);
    nd->nr_ways++;
  }
}

struct osm_way *get_osm_way(int id) 
{
    struct osm_object *oobj = get_obj_id(all_ways,id);
    if ((oobj) && (oobj->type == WAY)) {
      return (struct osm_way *)oobj;
    }
    return NULL;
}



struct osm_node *new_osm_node(int id)
{
  struct osm_node *node;
  if (!node_chunk)
    node_chunk = g_mem_chunk_create(struct osm_node, 1024, G_ALLOC_AND_FREE);
  node=g_chunk_new0(struct osm_node,node_chunk);
  node->head.id=id;
  node->head.type=NODE;
  total_node_count++;
  put_obj_id(all_nodes,&node->head,id);
  return node;
}

void free_osm_way(int id)
{
  struct osm_way *obj = get_osm_way(id);
  if (obj) {
    if (obj->nodes) {
      free(obj->nodes);
    }
    if (obj->head.tag_list)
      free_tag_list(obj->head.tag_list);
    g_mem_chunk_free(way_chunk,obj);
  }
  put_obj_id(all_ways,NULL,id);
}

void free_tag_list(GList *tl)
{
  GList *l;
  for(l=tl;l;l=g_list_next(l)) {
    free(l->data);
  }
  g_list_free(tl);
}



void free_osm_node(int id) 
{
  struct osm_node *obj = get_osm_node(id);
  if (obj) {
    if (obj->way_list)
      g_list_free(obj->way_list);
    free_tag_list(obj->head.tag_list);
    g_mem_chunk_free(node_chunk,obj);
  }
  put_obj_id(all_nodes,NULL,id);
}

void free_osm_file(struct osm_file *f)
{
  GList *l;
  if (!f->merged) {
    for(l=g_list_first(f->ways);l;l=g_list_next(l)) {
      free_osm_way(((struct osm_object *)l->data)->id);
    }
    
    for(l=g_list_first(f->nodes);l;l=g_list_next(l)) {
      free_osm_node(((struct osm_object *)l->data)->id);
    }
  }
  free(f);
  
}

struct osm_node *get_osm_node(int id)
{
    struct osm_object *oobj = get_obj_id(all_nodes,id);
    if ((oobj) && (oobj->type == NODE)) {
      return (struct osm_node *)oobj;
    }
    return NULL;
}

struct highway_perms {
  char *class;
  unsigned foot:1;
  unsigned bicycle:1;
  unsigned motorcar:1;
};


static void osmway_initialize_flags(struct osm_way *way)
{
  char *val;
  int i;
  const struct highway_perms hperms[]={
    {"motorway",0,0,1},
    {"motorway_link",0,0,1},
    {"trunk",1,1,1},
    {"trunk_link",1,1,1},
    {"primary",1,1,1},
    {"primary_link",1,1,1},
    {"secondary",1,1,1},
    {"tertiary",1,1,1},
    {"unclassified",1,1,1},
    {"track",1,1,1},
    {"pedestrian",1,0,0},
    {"cycleway",1,1,0},
    {"footway",1,0,0}
  };
  val=get_tag_value(&way->head,"highway");
  if (val) {
    way->street=1;
    way->foot=1;
    way->bicycle=1;
    way->motorcar=1;
    for(i=0;i<(sizeof(hperms)/sizeof(hperms[0]));i++) {
      if (!strcmp(hperms[i].class,val)) {
	way->foot=hperms[i].foot;
	way->bicycle=hperms[i].bicycle;
	way->motorcar=hperms[i].motorcar;
	break;
      }
    }
    val=get_tag_value(&way->head,"bicycle");
    if (val) {
      if (strcmp(val,"no")&&strcmp(val,"0")&&strcmp(val,"false"))
	way->bicycle=1;
    }
    val=get_tag_value(&way->head,"foot");
    if (val) {
      if (strcmp(val,"no")&&strcmp(val,"0")&&strcmp(val,"false"))
	way->foot=1;
    }
    val=get_tag_value(&way->head,"motorcar");
    if (val) {
      if (strcmp(val,"no")&&strcmp(val,"0")&&strcmp(val,"false"))
	way->motorcar=1;
    }
    val=get_tag_value(&way->head,"oneway");
    if (val)
      way->oneway=1;
  } 
}

static int build_references(GList *ways)
{
  GList *l;
  int ret=1;
  
  for(l=g_list_first(ways);l;l=g_list_next(l)) {
    int i;
    struct osm_way *way=(struct osm_way*)l->data;
    for(i=0;i<way->nr_nodes;i++)  {
      struct osm_node *node=get_osm_node(way->nodes[i]);
      if (!node)
	ret=0;
      else {
	/*  if (node->nr_ways==0) {
	osmf->nodes=g_list_prepend(osmf->nodes,node);
	} */
      node->way_list=g_list_append(node->way_list,way);
      node->nr_ways++;
      }
    }
    osmway_initialize_flags(way);
  }
  return ret;
}

char *get_tag_value(struct osm_object *obj,const char *key)
{
  GList *l=obj->tag_list;
  if (l) {
    for(l=g_list_first(l);l;l=g_list_next(l)) {
      if (!strcmp(key,(char *)l->data)) {
	char *k=(char *)l->data;
	return k+strlen(k)+1;
      }
    }
  }
  
  return NULL;
}

xmlNodePtr next_el_node(xmlNodePtr node)
{
  while((node)&&(node->type==XML_TEXT_NODE))
    node=node->next;
  return node;
}

void osm_merge_node(struct osm_file *osmf,
		    struct osm_node *mergeto,
		    struct osm_node *mergefrom)
{
  GList *l;
  int i;
  if (mergeto==mergefrom) {
    fprintf(stderr,"merging same nodes: %d\n",mergeto->head.id);
    return;
  }
  for(l=g_list_first(mergefrom->way_list);l;l=g_list_next(l)) {
    struct osm_way *way = (struct osm_way *)l->data;
    way->head.modified=1;
    for(i=0;i<way->nr_nodes;i++) {
      if (way->nodes[i]==mergefrom->head.id) {
	way->nodes[i]=mergeto->head.id;
      }
    }
  }
  mergeto->way_list = g_list_concat(mergeto->way_list,
				    mergefrom->way_list);
  mergefrom->way_list=NULL;
  free_osm_node(mergefrom->head.id);
  osmf->nodes=g_list_remove(osmf->nodes,mergefrom);
}

void osm_set_node_coords(struct osm_node *node,double lon, double lat)
{
  node->lon=lon;
  node->lat=lat;
  node->head.modified=1;
}

void osm_merge_into_way(struct osm_way *mergeway, int pos,
			struct osm_node *node)
{
  mergeway->nodes=realloc(mergeway->nodes,(mergeway->nr_nodes+1)*sizeof(int));
  memmove(mergeway->nodes+pos+1,mergeway->nodes+pos,(mergeway->nr_nodes-pos)*(sizeof(int)));
  mergeway->nodes[pos]=node->head.id;
  mergeway->nr_nodes++;
  mergeway->head.modified=1;
  node->way_list=g_list_append(node->way_list,mergeway);
  node->nr_ways++;
}

void printtimediff(const char *format,const struct timeval *tvstart, const struct timeval *tvend)
{
    int tvdiff=tvend->tv_sec-tvstart->tv_sec;
    struct timeval tve=*tvend;
    tvdiff*=1000;
    if (tve.tv_usec<tvstart->tv_usec)  {
      
      tve.tv_usec+=1000000;
      tvdiff-=1000;
    }
    tvdiff+=((tve.tv_usec-tvstart->tv_usec)/1000);
    fprintf(stderr,format,tvdiff);
}


static int osm_write_tags(xmlTextWriterPtr writer,
			  struct osm_object *obj)
{
  GList *l;
  for(l=g_list_first(obj->tag_list);l;l=g_list_next(l)) {
    char *k=(char *)l->data;
    char *v=k+strlen(k)+1;
    xmlTextWriterStartElement(writer,(xmlChar*)"tag");
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"k",(xmlChar *)k);
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"v",(xmlChar *)v);
    xmlTextWriterEndElement(writer);
  }
  return 1;
}

static int osm_write_node_xml(xmlTextWriterPtr writer,
			      struct osm_node *node,
			      enum format_type fmt, int changeset)
{
  if ((!node->head.modified)&&(node->head.id>0)&&(fmt == FORMAT_OSC))
    return 1;
  xmlTextWriterStartElement(writer,(xmlChar *)"node");
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"id","%d",node->head.id);
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"lon","%.7f",node->lon);
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"lat","%.7f",node->lat);
  if (node->head.version)
    xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"version","%d",node->head.version);
  if (fmt == FORMAT_JOSM) {
    if ((node->head.modified)&&(node->head.id>0)) {
      xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"modify");
    }
  } else if (fmt == FORMAT_OSC) {
    if (changeset)
      xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"changeset","%d",
					changeset);
  } else if (fmt == FORMAT_JOSMDEL) {
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"delete");
  }
  if (node->head.user)
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"user",(xmlChar *)node->head.user);
  if (node->head.timestamp) {
    struct tm tm;
    tm=*gmtime(&node->head.timestamp);
    xmlTextWriterWriteFormatElement(writer,(xmlChar *)"timestamp","%04d-%02d-%02dT%02d:%02d:%02d",
				    tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
				    tm.tm_hour,tm.tm_min,tm.tm_sec);
  }
  osm_write_tags(writer,&node->head);
  xmlTextWriterEndElement(writer);
  return 1;
}

static int osm_write_way_xml(xmlTextWriterPtr writer,
			     struct osm_way *way,enum format_type fmt, int changeset)
{
  int i;
  if ((!way->head.modified)&&(way->head.id>0)&&(fmt == FORMAT_OSC))
    return 1;
  xmlTextWriterStartElement(writer,(xmlChar *)"way");
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"id","%d",way->head.id);
  if (way->head.user)
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"user",(xmlChar *)way->head.user);
  if (way->head.timestamp) {
    struct tm tm;
    tm=*gmtime(&way->head.timestamp);
    xmlTextWriterWriteFormatElement(writer,(xmlChar *)"timestamp","%04d-%02d-%02dT%02d:%02d:%02d",
				    tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
				    tm.tm_hour,tm.tm_min,tm.tm_sec);
  }
  if (way->head.version) {
    xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"version",
				      "%d",way->head.version);
  }
  if (fmt == FORMAT_JOSM) {
    if ((way->head.modified)&&(way->head.id>0)) {
      xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"modify");
    }
  } else if (fmt == FORMAT_OSC) {
    if (changeset)
      xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"changeset","%d",
					changeset);
  } else if (fmt == FORMAT_JOSMDEL) {
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"delete");
  }
  for(i=0;i<way->nr_nodes;i++) {
    xmlTextWriterStartElement(writer,(xmlChar *)"nd");
    xmlTextWriterWriteFormatAttribute(writer,(xmlChar*)"ref","%d",way->nodes[i]);
    xmlTextWriterEndElement(writer);
  }
  osm_write_tags(writer,&way->head);
  xmlTextWriterEndElement(writer);
  return 1;
}

static int save_osmchange_xmlwriter(xmlTextWriterPtr writer, struct osm_file *osmf,
			     int changeset)
{
  GList *l;
  int i;
  xmlTextWriterStartDocument(writer,NULL,"UTF-8",NULL);
  xmlTextWriterStartElement(writer,(xmlChar *)"osmChange");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"version",(xmlChar *)OSMAPI_VERSION);
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"generator",(xmlChar *)PACKAGE " " VERSION);
  
  xmlTextWriterStartElement(writer,(xmlChar *)"create");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"version",(xmlChar *)OSMAPI_VERSION);
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"generator",(xmlChar *)PACKAGE " " VERSION);
  
  for(l=g_list_first(osmf->nodes);l;l=g_list_next(l)) {
    if (((struct osm_object *)l->data)->id<0) {
      osm_write_node_xml(writer,(struct osm_node *)l->data,FORMAT_OSC,changeset);
    }
  }
  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    if (((struct osm_object *)l->data)->id<0) {
      osm_write_way_xml(writer,(struct osm_way *)l->data,FORMAT_OSC,changeset);
    }
  }
  xmlTextWriterEndElement(writer);
  
  xmlTextWriterStartElement(writer,(xmlChar *)"modify");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"version",(xmlChar *)OSMAPI_VERSION);
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"generator",(xmlChar *)PACKAGE " " VERSION);
  for(l=g_list_first(osmf->nodes);l;l=g_list_next(l)) {
    if (((struct osm_object *)l->data)->id>0) {
      osm_write_node_xml(writer,(struct osm_node *)l->data,FORMAT_OSC,changeset);
    }
  }
  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    if (((struct osm_object *)l->data)->id>0) {
      osm_write_way_xml(writer,(struct osm_way *)l->data,FORMAT_OSC,changeset);
    }
  }
  xmlTextWriterEndElement(writer);
  
  xmlTextWriterStartElement(writer,(xmlChar *)"delete");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"version",(xmlChar *)OSMAPI_VERSION);
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"generator",(xmlChar *)PACKAGE " " VERSION);
  
  for(i=0;i<osmf->deleted_way_count;i++) {
    if (osmf->deleted_ways[i]->head.id>0)
      osm_write_way_xml(writer,osmf->deleted_ways[i],FORMAT_OSC,changeset);
  }

  for(i=0;i<osmf->deleted_node_count;i++) {
    if (osmf->deleted_nodes[i]->head.id>0)
      osm_write_node_xml(writer,osmf->deleted_nodes[i],
			 FORMAT_OSC,changeset);
  }
  

  xmlTextWriterEndElement(writer);
  xmlTextWriterEndElement(writer);
  
  xmlTextWriterEndDocument(writer);

  xmlFreeTextWriter(writer);
  /* osmf->changed=0; */
  return 1;
}

int save_osmchange_file(const char *fname, struct osm_file *osmf,
			int changeset)
{
  xmlTextWriterPtr writer;
  writer=xmlNewTextWriterFilename(fname,0);
  if (!writer) 
    return 0;
  save_osmchange_xmlwriter(writer,osmf,changeset);
  return 1;
}

char *save_osmchange_buf(struct osm_file *osmf, int changeset)
{
  char *dst;
  xmlBufferPtr xmlbuf=xmlBufferCreate();
  xmlTextWriterPtr writer=xmlNewTextWriterMemory(xmlbuf,0);
  if (!writer)
    return NULL;
  save_osmchange_xmlwriter(writer,osmf,changeset);
  dst=malloc(xmlbuf->use+1);
  memcpy(dst,xmlbuf->content,xmlbuf->use);
  dst[xmlbuf->use]=0;
  xmlBufferFree(xmlbuf);
  return dst;
}



int save_osm_file(const char *fname, struct osm_file *osmf)
{
  xmlTextWriterPtr writer;
  GList *l;
  int i;
  writer=xmlNewTextWriterFilename(fname,0);
  if (!writer) 
    return 0;
  xmlTextWriterStartDocument(writer,NULL,"UTF-8",NULL);
  xmlTextWriterStartElement(writer,(xmlChar *)"osm");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"version",(xmlChar *)OSMAPI_VERSION);
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"generator",(xmlChar *)PACKAGE " " VERSION);

  for(l=g_list_first(osmf->nodes);l;l=g_list_next(l)) {
    osm_write_node_xml(writer,(struct osm_node *)l->data,FORMAT_JOSM,0);
  }

  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    osm_write_way_xml(writer,(struct osm_way *)l->data,FORMAT_JOSM,0);
  }
  
  for(i=0;i<osmf->deleted_way_count;i++) {
    osm_write_way_xml(writer,osmf->deleted_ways[i],FORMAT_JOSMDEL,0);
  }

  for(i=0;i<osmf->deleted_node_count;i++) {
    osm_write_node_xml(writer,osmf->deleted_nodes[i],FORMAT_JOSMDEL,0);
  }
  

  xmlTextWriterEndElement(writer);

  xmlTextWriterEndDocument(writer);

  xmlFreeTextWriter(writer);
  osmf->changed=0;
  return 1;
}

struct osm_parser_ctxt {
  struct osm_file *osmf;
  struct osm_node *node;
  struct osm_way *way;
  GList *revisit_ways;
  int have_osm;
  int error;
  int way_nodes[65536];
  int way_node_count;
};

static void osmparse_endhandler(void *ctx,
				 const xmlChar *name)
{
  struct osm_parser_ctxt *octxt=(struct osm_parser_ctxt *)ctx;
  if (!strcmp((char *)name,"way")) {
    if (octxt->way) {
      int deleted=0;
      octxt->way->nodes=malloc(sizeof(int)*octxt->way_node_count);
      octxt->way->nr_nodes=octxt->way_node_count;
      memcpy(octxt->way->nodes,octxt->way_nodes,sizeof(int)*octxt->way_node_count);
      if ((octxt->osmf->deleted_way_count) &&
	  (octxt->osmf->deleted_ways[octxt->osmf->deleted_way_count-1]==octxt->way))
	deleted=1;

      octxt->way=NULL;
      octxt->way_node_count=0;
    }
  } else if (!strcmp((char *)name,"node")) {
    if (octxt->node) {
      int deleted=0;
      if ((octxt->osmf->deleted_node_count) &&
	  (octxt->osmf->deleted_nodes[octxt->osmf->deleted_node_count-1]==octxt->node))
	deleted=1;
      octxt->node=NULL;
    }
  }
}
				 
static void remove_way_from_nodes(struct osm_way *w)
{
  int i;
  for(i=0;i<w->nr_nodes;i++) {
    struct osm_node *nd=get_osm_node(w->nodes[i]);
    if (nd) {
      nd->way_list=g_list_remove(nd->way_list,w);
      nd->nr_ways--;
    }
  }
}



static void osmparse_starthandler(void *ctx,
        const xmlChar *name,
        const xmlChar ** atts)
{
  int i;
  struct osm_parser_ctxt *octxt=(struct osm_parser_ctxt *)ctx;
  if (!octxt->have_osm) {
    if (!strcmp("osm",(char *)name)) {
      octxt->have_osm=1;
    }
  } else if ((!octxt->node)&&(!octxt->way)) {
    if (!strcmp("node",(char *)name)) {
      int modified=0;
      int deleted=0;
      char*lon=NULL;
      char*lat=NULL;
      char*timestamp=NULL;
      char*user=NULL;
      int id=0;
      int version=0;
      for(i=0;atts[i];i+=2) {
	if (!strcmp((char *)atts[i],"lon")) {
	  if (atts[i+1]) {
            lon=(char *)atts[i+1];
	  }
	} else if (!strcmp((char *)atts[i],"lat")) {
	  if (atts[i+1]) {
	    lat=(char *)atts[i+1];
	  }
	} else if (!strcmp((char *)atts[i],"id")) {
	  id=atoi((char *)atts[i+1]);
	} else if (!strcmp((char *)atts[i],"user")) {
	  user=(char *)atts[i+1];
	} else if (!strcmp((char *)atts[i],"timestamp")) {
	  timestamp=(char *)atts[i+1];
	} else if (!strcmp((char *)atts[i],"action")) {
	  if (!strcmp((char *)atts[i+1],"modify")) {
	    modified=1;
	  } else if (!strcmp((char *)atts[i+1],"delete")) {
	    deleted=1;
	  }
	} else if (!strcmp((char *)atts[i],"version")) {
	  version=atoi((char *)atts[i+1]);
	}
      }
      if (id&&lat&&lon) {
	struct osm_node *nd;
	nd=get_osm_node(id);
	if (!nd) {
	  octxt->node = new_osm_node(id);
	  if (!deleted)
	    octxt->osmf->nodes=g_list_prepend(octxt->osmf->nodes,octxt->node);
	} else if ((!nd->head.modified)||(version>nd->head.version)) {
	  octxt->node = nd;
	  free_tag_list(nd->head.tag_list);
	  nd->head.tag_list=NULL;
	} else
	  return;
	octxt->node->head.modified=modified;
	octxt->node->lon=atof(lon);
	octxt->node->lat=atof(lat);
	octxt->node->head.version=version;
	if (user)
	  octxt->node->head.user=strdup(user);
	if (timestamp)
	  octxt->node->head.timestamp=parse_xml_time(timestamp);
	if (deleted) {
	  struct osm_file *osmf=octxt->osmf;
	  if ((osmf->deleted_node_count&0xf)==0) {
	    osmf->deleted_nodes=(struct osm_node **)realloc(osmf->deleted_nodes,
							    (osmf->deleted_node_count+16)*sizeof(struct osm_node *));
	  }
	  osmf->deleted_nodes[osmf->deleted_node_count]=octxt->node;
	  octxt->node->head.modified=1;
	  osmf->deleted_node_count++;
	  put_obj_id(all_nodes,NULL,id);
	} 
      }
    } else if (!strcmp("way",(char *)name)) {
      int modified=0;
      int deleted=0;
      int version=0;
      int id=0;
      char *user=NULL;
      char *timestamp=NULL;
      for(i=0;atts[i];i+=2) {
	if (!strcmp((char *)atts[i],"id")) {
	  id=atoi((char *)atts[i+1]);
	} else if (!strcmp((char *)atts[i],"user")) {
	  user=(char *)atts[i+1];
	} else if (!strcmp((char *)atts[i],"timestamp")) {
	  timestamp=(char *)atts[i+1];
	} else if (!strcmp((char *)atts[i],"action")) {
	  if (!strcmp((char *)atts[i+1],"modify")) {
	    modified=1;
	  } else if (!strcmp((char *)atts[i+1],"delete")) {
	    deleted=1;
	  }
	} else if (!strcmp((char *)atts[i],"version")) {
	  version=atoi((char *)atts[i+1]);
	}
	
      }
      if (id) {
	struct osm_way *way;
	way=get_osm_way(id);
	if (!way) {
	  octxt->way = new_osm_way(id);
	  if (!deleted)
	    octxt->osmf->ways=g_list_prepend(octxt->osmf->ways,octxt->way); 
	} else if ((!way->head.modified)||(version>way->head.version)) {
	  octxt->way = way;
	  /* remove references to this way in the corresponding
	     nodes */
	  remove_way_from_nodes(octxt->way);
	  free(way->nodes);
	  way->nodes = NULL;
	  way->nr_nodes = 0;
	  /* rebuild the reference list later on */
	  octxt->revisit_ways=g_list_append(octxt->revisit_ways,way);
	  free_tag_list(octxt->way->head. tag_list);
	  octxt->way->head.tag_list = NULL;
	} else
	  return;
	if (deleted) {
	  struct osm_file *osmf=octxt->osmf;
	  if (0==(osmf->deleted_way_count&0xf)) {
	    osmf->deleted_ways=(struct osm_way  **)realloc(osmf->deleted_ways,
							   (osmf->deleted_way_count+16)*sizeof(struct osm_way *));
	  }
	  put_obj_id(all_ways,NULL,octxt->way->head.id);
	  osmf->deleted_ways[osmf->deleted_way_count]=octxt->way;
	  octxt->way->head.modified=1;
	  osmf->deleted_way_count++;
	}
      }
      if (octxt->way) {
	if (user)
	  octxt->way->head.user=strdup(user);
	if (timestamp)
	  octxt->way->head.timestamp=parse_xml_time(timestamp);
	if (modified)
	  octxt->way->head.modified=modified;
	octxt->way->head.version=version;
      }
    }
  } else if (!strcmp("tag",(char *)name)) {
    GList **l;
    char *k=NULL;
    char *v=NULL;
    if ((octxt->way)||(octxt->node)) {
      if (octxt->way)
	l=&octxt->way->head.tag_list;
      else
	l=&octxt->node->head.tag_list;
      for(i=0;atts[i];i+=2) {
	if (!strcmp((char *)atts[i],"k")) {
	  k=(char *)atts[i+1];
	} else {
	  v=(char *)atts[i+1];
	}
      }
      if (k&&v) {
	char *t=malloc(strlen(k)+strlen(v)+2);
	strcpy(t,k);
	strcpy(t+strlen(k)+1,v);
	*l=g_list_append(*l,t);
      }
    }
  } else if ((octxt->way)&&(!strcmp("nd",(char *)name))) {
    for(i=0;atts[i];i+=2) {
      if (!strcmp((char *)atts[i],"ref")) {
	octxt->way_nodes[octxt->way_node_count]=atoi((char *)atts[i+1]);
	octxt->way_node_count++;
      }
    }
  }
}

static void  head_stats(struct osm_object *obj, int *tssize,int *usersize, int *taglistsize,
			int *tagsize)
{
  GList *l;
  if (obj->user)
    (*usersize)+=strlen(obj->user)+1;
  (*taglistsize)+=(sizeof(GList)*g_list_length(obj->tag_list));
  for(l=g_list_first(obj->tag_list);l;l=g_list_next(l)) {
    char *t=l->data;
    char *v=t+strlen(t)+1;
    (*tagsize)+=(strlen(t)+strlen(v)+2);
  }
}

static void mem_stats(struct osm_file *osmf)
{
  GList *l;
  int total=0;
  int tssize=0;
  int usersize=0;
  int tagsize=0;
  int taglistsize=0;
  int nwaylistsize=0;
  int ncount=g_list_length(osmf->nodes);
  int wcount=g_list_length(osmf->ways);
  total+=ncount*(sizeof(GList)+sizeof(struct osm_node));
  total+=wcount*(sizeof(GList)+sizeof(struct osm_way));
  printf("%-50s: %7d\n","nodes",ncount);
  printf("%-50s: %7d\n","node structs",(sizeof(GList)+sizeof(struct osm_node))*ncount);
  for(l=g_list_first(osmf->nodes);l;l=g_list_next(l)) {
    struct osm_node *nd=(struct osm_node *)l->data;
    head_stats(&nd->head,&tssize,&usersize,&taglistsize,
	       &tagsize);
    nwaylistsize+=nd->nr_ways*sizeof(GList);
  }
  printf("%-50s: %7d\n","node way list size",nwaylistsize);
  printf("%-50s: %7d\n","username size",usersize);
  printf("%-50s: %7d\n","taglist size",taglistsize);
  printf("%-50s: %7d\n","tags size",tagsize);
  total+=nwaylistsize+tssize+usersize+taglistsize+tagsize;

  nwaylistsize=tssize=usersize=taglistsize=tagsize=0;
  printf("%-50s: %7d\n","ways",wcount);
  printf("%-50s: %7d\n","way structs",(sizeof(GList)+sizeof(struct osm_way))*wcount);
  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    struct osm_way *way=(struct osm_way *)l->data;
    head_stats(&way->head,&tssize,&usersize,&taglistsize,
	       &tagsize);
    nwaylistsize+=way->nr_nodes*sizeof(int);
  }
  printf("%-50s: %7d\n","way node arrays size",nwaylistsize);
  printf("%-50s: %7d\n","username size",usersize);
  printf("%-50s: %7d\n","taglist size",taglistsize);
  printf("%-50s: %7d\n","tags size",tagsize);
  printf("%-50s: %7d\n","id->pointer index1",index_total1);
  printf("%-50s: %7d\n","id->pointer index1",index_total2);
  total+=nwaylistsize+tssize+usersize+taglistsize+tagsize+index_total1+index_total2;
  printf("\n%-50s: %7d\n","TOTAL",total);  
}

struct osm_file * parse_osm_file(struct osm_file *mergeto,
				 const char *fname, int all_ways)
{
  struct timeval tv1;
  struct timeval tv2;
  struct timeval tv3;
  char *suf;
  FILE *f=fopen(fname,"rb");
  struct osm_file *osmf=NULL;
  struct osm_parser_ctxt *octxt=NULL;
  int l;
  static xmlSAXHandler myhandler={
    .initialized = 1,
    .startElement = osmparse_starthandler,
    .endElement = osmparse_endhandler
  };
  xmlParserCtxtPtr ctxt;
  char buf[1024];
  
  if (!f) {
    fprintf(stderr,"Cannot open %s\n",fname);
    return NULL;
  }
#ifndef _WIN32
  gettimeofday(&tv1,NULL);
#endif
  setlocale(LC_NUMERIC,"C");
#ifdef HAVE_BZLIB_H
  suf=strrchr(fname,'.');
  if ((suf)&&(!strcmp(suf,".bz2"))) {
    int bzerror=BZ_OK;
    BZFILE *bzf;
    bzf=BZ2_bzReadOpen(&bzerror,f,0,0,NULL,0);
    if (bzerror==BZ_OK) {
      l=BZ2_bzRead(&bzerror,
		   bzf,buf,sizeof(buf));
      if (bzerror==BZ_OK) {
	osmf=calloc(1,sizeof(struct osm_file));
	octxt=calloc(1,sizeof(struct osm_parser_ctxt));
	octxt->osmf=osmf;
	ctxt = xmlCreatePushParserCtxt(&myhandler,octxt,
				       buf,l,fname);
	while(((l=BZ2_bzRead(&bzerror,bzf,buf,sizeof(buf)))>0)&&(bzerror==BZ_OK)) {
	  xmlParseChunk(ctxt,buf,l,0);
	}
	xmlParseChunk(ctxt,buf,0,1);
	xmlFreeParserCtxt(ctxt);
      }
    }
    BZ2_bzReadClose(&bzerror,bzf);
   
  } else 
#endif
    {
      l = fread(buf, 1, sizeof(buf), f);
      if (l<0) {
	fclose(f);
	return NULL;
      }
      osmf=calloc(1,sizeof(struct osm_file));
      octxt=calloc(1,sizeof(struct osm_parser_ctxt));
      octxt->osmf=osmf;
      ctxt = xmlCreatePushParserCtxt(&myhandler,octxt,
				     buf,l,fname);
      
      while((l=fread(buf,1,sizeof(buf),f))>0) {
	xmlParseChunk(ctxt,buf,l,0);
      }
      xmlParseChunk(ctxt, buf, 0 ,1);
      xmlFreeParserCtxt(ctxt);
    }
  fclose(f);
#ifndef _WIN32
  gettimeofday(&tv2,NULL);
  printtimediff("SAX parsing xml: %d ms\n",&tv1,&tv2);
#endif
  if (!build_references(osmf->ways)) {
    fprintf(stderr,"warning,: inconsistencies detected\n");
  }
#ifndef _WIN32
  gettimeofday(&tv3,NULL);
  printtimediff("build references: %d ms\n",&tv2,&tv3);
#endif
  if (octxt) {
    if (octxt->revisit_ways)
      build_references(octxt->revisit_ways);
    if (mergeto) {
      mergeto->ways=g_list_concat(mergeto->ways,osmf->ways);
      mergeto->nodes=g_list_concat(mergeto->nodes,osmf->nodes);
      osmf->merged=1;
    }
    free(octxt);
  }
  mem_stats(mergeto?mergeto:osmf);
  return osmf;
}
