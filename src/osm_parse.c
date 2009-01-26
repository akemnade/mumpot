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
#include "osm_parse.h"
static GMemChunk *node_chunk;
static GMemChunk *way_chunk;
static struct osm_object ***all_objects[65536];
int max_free_num=-1;
static int total_way_count;
static int total_node_count;

static void osmway_initialize_flags(struct osm_way *way);

struct osm_object *get_obj_id(int id)
{
  struct osm_object ***ind;
    if (!all_objects[(id>>16)&0xffff])
      return NULL;
    ind=all_objects[(id>>16)&0xffff];
    if (!ind[(id>>8)&0xff])
      return NULL;
    return (ind[(id>>8)&0xff])[id&0xff];
}

static void add_created_tag(struct osm_object *obj)
{
#ifdef USE_DOM_PARSER
  xmlNodePtr nd=obj->xmlnode;
  xmlNodePtr node=xmlNewNode(NULL,(xmlChar *)"tag");
  xmlSetProp(node,(xmlChar *)"k",
	     (xmlChar *)"created_by");
  xmlSetProp(node,(xmlChar *)"v",
	     (xmlChar *)PACKAGE);
  xmlAddChild(nd,node);
#else
  char *ctag=malloc(sizeof("created_by\0" PACKAGE));
  memcpy(ctag,"created_by\0" PACKAGE,sizeof("created_by\0" PACKAGE));
  obj->tag_list=g_list_append(obj->tag_list,ctag);
#endif
}

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
#ifndef USE_DOM_PARSER
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
#else
  node=node->children;
  while(node) {
    node=next_el_node(node);
    if (!node)
      break;
    if (!strcmp((char *)node->name,"tag")) {
      char *k=(char *)xmlGetProp(node,(xmlChar *)"k");
      if (k) {
	if (!strcmp(k,key)) {
	  xmlFree(k);
	  break;
	}
	xmlFree(k);
      }
    }
    node=node->next;
  }
  if (!node) {
    node=xmlNewChild(obj->xmlnode,NULL,(xmlChar *)"tag",NULL);
    xmlSetProp(node,(xmlChar *)"k",(xmlChar *)key);
  }
  xmlSetProp(node,(xmlChar *)"v",(xmlChar *)v);
#endif
}

void put_obj_id(struct osm_object *obj,int id)
{
  struct osm_object ***ind;
    if (!all_objects[(id>>16)&0xffff]) {
      all_objects[(id>>16)&0xffff]=calloc(sizeof(struct osm_object ***),256);
    }
    ind=all_objects[(id>>16)&0xffff];
    if (!ind[(id>>8)&0xff]) {
      ind[(id>>8)&0xff]=calloc(sizeof(struct osm_object **),256);
    }
    (ind[(id>>8)&0xff])[id&0xff]=obj;
    if (id<=max_free_num) {
      max_free_num=id-1;
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
  put_obj_id(&way->head,id);
  return way;
}

struct osm_way *add_new_osm_way(struct osm_file *f)
{
#ifdef USE_DOM_PARSER
  char buf[80];
#endif
  struct osm_way *way=new_osm_way(max_free_num);
#ifdef USE_DOM_PARSER
  way->head.xmlnode=xmlNewNode(NULL,(xmlChar*)"way");
  snprintf(buf,sizeof(buf),"%d",way->head.id);
  xmlSetProp(way->head.xmlnode,(xmlChar *)"id",(xmlChar *)buf);
  xmlSetProp(way->head.xmlnode,(xmlChar *)"visible",(xmlChar*)"true");
  xmlAddChild(f->headnode,way->head.xmlnode);
#endif
  add_created_tag(&way->head);
  f->ways=g_list_append(f->ways,way);
  return way;
}

struct osm_node *new_osm_node_from_point(struct osm_file *f,
					 double lon, double lat)
{
#ifdef USE_DOM_PARSER
  char buf[80];
#endif
  struct osm_node *nd=new_osm_node(max_free_num);
  nd->lon=lon;
  nd->lat=lat;
  f->nodes=g_list_append(f->nodes,nd);
#ifdef USE_DOM_PARSER
  nd->head.xmlnode=xmlNewNode(NULL,(xmlChar *)"node");

  snprintf(buf,sizeof(buf),"%d",nd->head.id);
  xmlSetProp(nd->head.xmlnode,(xmlChar *)"id",(xmlChar *)buf);
  xmlSetProp(nd->head.xmlnode,(xmlChar *)"visible",(xmlChar *)"true");

  snprintf(buf,sizeof(buf),"%f",lon);
  xmlSetProp(nd->head.xmlnode,(xmlChar *)"lon",(xmlChar *)buf);
  snprintf(buf,sizeof(buf),"%f",lat);
  xmlSetProp(nd->head.xmlnode,(xmlChar *)"lat",(xmlChar *)buf);
  xmlAddChild(f->headnode,nd->head.xmlnode);
#endif
  add_created_tag(&nd->head);
  return nd;
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
    osmf->deleted_ways=(int *)realloc(osmf->deleted_ways,
				      osmf->deleted_way_count+sizeof(int)*16);
  }
  osmf->deleted_ways[osmf->deleted_way_count]=way->head.id;
  osmf->deleted_way_count++;
  
  free_osm_way(way->head.id);
  osmf->ways=g_list_remove(osmf->ways,way);
  osmf->changed=1;
}

void osm_delete_node(struct osm_file *osmf,
		     struct osm_node *node)
{
  GList *l;
  for(l=g_list_first(node->way_list);l;l=g_list_next(l)) {
    struct osm_way *way=(struct osm_way *)l->data;
    remove_node_from_way(way,node);
    if (way->nr_nodes<2) {
      osm_delete_way(osmf,way);
    }
  }
  if ((osmf->deleted_node_count&0xf)==0) {
    osmf->deleted_nodes=(int *)realloc(osmf->deleted_nodes,
				       osmf->deleted_node_count+sizeof(int)*16);
  }
  osmf->deleted_nodes[osmf->deleted_node_count]=node->head.id;
  osmf->deleted_node_count++;

  osmf->nodes=g_list_remove(osmf->nodes,node);
  free_osm_node(node->head.id);
  osmf->changed=1;
}

void add_nodes_to_way(struct osm_way *way, GList *l)
{
  int offset=way->nr_nodes;
#ifdef USE_DOM_PARSER
  char buf[80];
#endif
  int la=g_list_length(l);
  int i;
  way->nr_nodes+=la;
  way->nodes=realloc(way->nodes,sizeof(struct osm_node *)*way->nr_nodes);
  for(i=offset;l;l=g_list_next(l),i++) {
    struct osm_node *nd=(struct osm_node *)l->data;
    way->nodes[i]=nd->head.id;
    nd->way_list=g_list_append(nd->way_list,way);
    nd->nr_ways++;
#ifdef USE_DOM_PARSER
    {
      snprintf(buf,sizeof(buf),"%d",nd->head.id);
      xmlNodePtr node=xmlNewNode(NULL,(xmlChar *)"nd"); 
      xmlSetProp(node,(xmlChar *)"ref",(xmlChar*)buf);
      xmlAddChild(way->head.xmlnode,node);
    }
#endif
  }
}

struct osm_way *get_osm_way(int id) 
{
    struct osm_object *oobj = get_obj_id(id);
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
  put_obj_id(&node->head,id);
  return node;
}

void free_osm_way(int id)
{
  struct osm_way *obj = get_osm_way(id);
  if (obj) {
    if (obj->nodes) {
      free(obj->nodes);
    }
    g_mem_chunk_free(way_chunk,obj);
  }
  put_obj_id(NULL,id);
}

void free_osm_node(int id) 
{
  struct osm_node *obj = get_osm_node(id);
  if (obj) {
    if (obj->way_list)
      g_list_free(obj->way_list);
    g_mem_chunk_free(node_chunk,obj);
  }
  put_obj_id(NULL,id);
}

void free_osm_file(struct osm_file *f)
{
  GList *l;
  for(l=g_list_first(f->ways);l;l=g_list_next(l)) {
    free_osm_way(((struct osm_object *)l->data)->id);
  }

  for(l=g_list_first(f->nodes);l;l=g_list_next(l)) {
    free_osm_node(((struct osm_object *)l->data)->id);
  }
  xmlFreeDoc(f->xmldoc);
  free(f);
  
}

struct osm_node *get_osm_node(int id)
{
    struct osm_object *oobj = get_obj_id(id);
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

static int build_references(struct osm_file *osmf)
{
  GList *l;
  int ret=1;
  
  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
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


static char *get_tag_value_xml(xmlNodePtr node,const char *key)
{
  if (!node)
    return NULL;
  node=node->children;
  while(node) {
    node=next_el_node(node);
    if (!node)
      return 0;
    if (!strcmp((char *)node->name,"tag")) {
      char *k=(char *)xmlGetProp(node,(xmlChar *)"k");
      if (k) {
	if (!strcmp(k,key)) {
	  return (char *)xmlGetProp(node,(xmlChar *)"v");
	}
	xmlFree(k);
      }
    }
    node=node->next;
  }
  return 0;
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
  if (obj->xmlnode) {
    return get_tag_value_xml(obj->xmlnode,key);
  }
  return NULL;
}

xmlNodePtr next_el_node(xmlNodePtr node)
{
  while((node)&&(node->type==XML_TEXT_NODE))
    node=node->next;
  return node;
}

#ifdef USE_DOM_PARSER
static int is_street(xmlNodePtr node)
{
  char *r = get_tag_value_xml(node,"highway");
  if (r) {
    xmlFree(r);
    return 1;
  }
  return 0;
}
#endif

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
#ifdef USE_DOM_PARSER
    xmlNodePtr cnode = way->head.xmlnode;
    cnode=way->head.xmlnode->children;
#endif
    for(i=0;i<way->nr_nodes;i++) {
#ifdef USE_DOM_PARSER
      char b[20];
      cnode=next_named_node(cnode,"nd");
#endif
      if (way->nodes[i]==mergefrom->head.id) {
	way->nodes[i]=mergeto->head.id;
#ifdef USE_DOM_PARSER
	snprintf(b,sizeof(b),"%d",mergeto->head.id);
	if (cnode) {
	  xmlSetProp(cnode,(xmlChar *)"ref",(xmlChar *)b);
	  if (way->head.id > 0)
	    xmlSetProp(way->head.xmlnode,(xmlChar *)"action",(xmlChar *)"modify");
	}
#endif
      }
#ifdef USE_DOM_PARSER
      cnode=cnode->next;
      #endif
    }
  }
  mergeto->way_list = g_list_concat(mergeto->way_list,
					mergefrom->way_list);
#ifdef USE_DOM_PARSER
  xmlUnlinkNode(mergefrom->head.xmlnode);
  xmlFreeNode(mergefrom->head.xmlnode);
  mergefrom->head.xmlnode=NULL;
#endif
  mergefrom->way_list=NULL;
  free_osm_node(mergefrom->head.id);
  osmf->nodes=g_list_remove(osmf->nodes,mergefrom);
}

void osm_set_node_coords(struct osm_node *node,double lon, double lat)
{
#ifdef USE_DOM_PARSER
  char buf[80];
#endif
  node->lon=lon;
  node->lat=lat;
  node->head.modified=1;
#ifdef USE_DOM_PARSER
  snprintf(buf,sizeof(buf),"%f",node->lon);
  xmlSetProp(node->head.xmlnode,(xmlChar *)"lon",(xmlChar *)buf);
  snprintf(buf,sizeof(buf),"%f",node->lat);
  xmlSetProp(node->head.xmlnode,(xmlChar *)"lat",(xmlChar *)buf);
  if (node->head.id>0) {
    xmlSetProp(node->head.xmlnode,(xmlChar *)"action",(xmlChar *)"modify");
  }
#endif
}

void osm_merge_into_way(struct osm_way *mergeway, int pos,
			struct osm_node *node)
{
#ifdef USE_DOM_PARSER
  xmlNodePtr xmlnode=mergeway->head.xmlnode;
  xmlNodePtr nn;
  char b[80];
  int i;
#endif
  mergeway->nodes=realloc(mergeway->nodes,(mergeway->nr_nodes+1)*sizeof(int));
  memmove(mergeway->nodes+pos+1,mergeway->nodes+pos,(mergeway->nr_nodes-pos)*(sizeof(int)));
  mergeway->nodes[pos]=node->head.id;
  mergeway->nr_nodes++;
  mergeway->head.modified=1;
  node->way_list=g_list_append(node->way_list,mergeway);
  node->nr_ways++;
#ifdef USE_DOM_PARSER
  if (xmlnode)
    xmlnode=xmlnode->children;
  for(i=0;i<pos;i++) {
    xmlnode=next_named_node(xmlnode,"nd");
    xmlnode=xmlnode->next;
  }
  nn=xmlNewNode(NULL,(xmlChar *)"nd");
  snprintf(b,sizeof(b),"%d",node->head.id);
  xmlSetProp(nn,(xmlChar *)"ref",(xmlChar *)b);
  xmlAddPrevSibling(xmlnode,nn);
  if (mergeway->head.id>0) {
    xmlSetProp(mergeway->head.xmlnode,(xmlChar *)"action",(xmlChar *)"modify");
  }
#endif
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

#ifdef USE_DOM_PARSER

int save_osm_file(const char *fname, struct osm_file *osmf)
{
  return xmlSaveFile(fname,osmf->xmldoc);
}

struct osm_file * parse_osm_file(const char *fname, int all_ways)
{
    xmlNodePtr node;
    xmlNodePtr docroot;
    xmlDocPtr doc;
    struct osm_file *osmf;
    xmlLineNumbersDefault(1);
    struct timeval tv1;
    struct timeval tv2;
    struct timeval tv3;
    struct timeval tv4;
    gettimeofday(&tv1,NULL);
    doc = xmlParseFile(fname);
    if (doc == NULL)
        return NULL;
    gettimeofday(&tv2,NULL);
    printtimediff("parse xml: %d ms\n",&tv1,&tv2);
    docroot=xmlDocGetRootElement(doc);
    if (!docroot)
      return NULL;
    osmf = calloc(1,sizeof(struct osm_file));
    osmf->xmldoc=doc;
    node=doc->children;
    while(node) {
      node=next_el_node(node);
      if (!node)
        break;
      if (!strcmp((char *)node->name,"osm")) {
	osmf->headnode=node;
        node=node->children;
      } else if (!strcmp((char *)node->name,"way")) {
        int nodecount=0;
        struct osm_way *way;
	char *n;	
        n = (char *)xmlGetProp(node,(xmlChar *)"id");
        if ((n)&&(all_ways||is_street(node))) {
	  int i;
          xmlNodePtr nodes=node->children;
          while(nodes) {
            nodes=next_el_node(nodes);
	    if (!nodes)
	      break;
            if (!strcmp((char *)nodes->name,"nd"))
              nodecount++;
	    nodes=nodes->next;
          }
          way=new_osm_way(atoi(n));
          way->head.xmlnode=node;
          way->nr_nodes=nodecount;
	  way->nodes = malloc(sizeof(int)*nodecount);
	  nodes=node->children;
	  for(i=0;nodes&&(nodes=next_el_node(nodes));nodes=nodes->next) {
	    char *prop;
	    prop=(char *)xmlGetProp(nodes,(xmlChar *)"ref");
	    if (prop) {
	      way->nodes[i]=atoi(prop);
	      i++;
	      xmlFree(prop);
	    }
	  }
	  way->nr_nodes=i;
	  
	  osmf->ways=g_list_prepend(osmf->ways,way); 
        }
	if (n)
	  xmlFree(n);
      } else if (!strcmp((char *)node->name,"node")) {
	struct osm_node *osmnode;
	char *n=(char *)xmlGetProp(node,(xmlChar *)"id");
	if (n) {
	  char *lon=(char *)xmlGetProp(node,(xmlChar *)"lon");
	  char *latt=(char *)xmlGetProp(node,(xmlChar *)"lat");
	  if (lon&&latt) {
	    osmnode = new_osm_node(atoi(n));
	    osmnode->lon=atof(lon);
	    osmnode->lat=atof(latt);
	    osmnode->head.xmlnode=node;
	  }
	  if (lon)
	    xmlFree(lon);
	  if (latt)
	    xmlFree(latt);
	  xmlFree(n);
	}
      }
      node=node->next;
    }
    gettimeofday(&tv3,NULL);
    printtimediff("iterate doctree: %d ms\n",&tv2,&tv3);
    if (!build_references(osmf)) {
      fprintf(stderr,"warning,: inconsistencies detected\n");
    }
    gettimeofday(&tv4,NULL);
    printtimediff("parse xml: %d ms\n",&tv1,&tv2);
    printtimediff("iterate doctree: %d ms\n",&tv2,&tv3);
    printtimediff("buld references: %d ms\n",&tv3,&tv4);
    return osmf;
}
#else

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
			      struct osm_node *node)
{
  xmlTextWriterStartElement(writer,(xmlChar *)"node");
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"id","%d",node->head.id);
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"lon","%.7f",node->lon);
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"lat","%.7f",node->lat);
  if ((node->head.modified)&&(node->head.id>0)) {
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"modify");
  }
  if (node->head.user)
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"user",(xmlChar *)node->head.user);
  if (node->head.timestamp)
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"timestamp",(xmlChar *)node->head.timestamp);
  osm_write_tags(writer,&node->head);
  xmlTextWriterEndElement(writer);
  return 1;
}

static int osm_write_way_xml(xmlTextWriterPtr writer,
			     struct osm_way *way)
{
  int i;
  xmlTextWriterStartElement(writer,(xmlChar *)"way");
  xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"id","%d",way->head.id);
  if (way->head.user)
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"user",(xmlChar *)way->head.user);
  if (way->head.timestamp)
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"timestamp",(xmlChar *)way->head.timestamp);
  if ((way->head.modified)&&(way->head.id>0)) {
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"modify");
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
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"version",(xmlChar *)"0.5");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"generator",(xmlChar *)PACKAGE " " VERSION);

  for(l=g_list_first(osmf->nodes);l;l=g_list_next(l)) {
    osm_write_node_xml(writer,(struct osm_node *)l->data);
  }

  for(l=g_list_first(osmf->ways);l;l=g_list_next(l)) {
    osm_write_way_xml(writer,(struct osm_way *)l->data);
  }
  
  for(i=0;i<osmf->deleted_way_count;i++) {
    xmlTextWriterStartElement(writer,(xmlChar *)"way");
    xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"id","%d",
				      osmf->deleted_ways[i]);
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"delete");
    xmlTextWriterEndElement(writer);
  }

  for(i=0;i<osmf->deleted_node_count;i++) {
    xmlTextWriterStartElement(writer,(xmlChar *)"node");
    xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"id","%d",
				      osmf->deleted_nodes[i]);
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"lat",(xmlChar *)"0");
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"lon",(xmlChar *)"0");
    xmlTextWriterWriteAttribute(writer,(xmlChar *)"action",(xmlChar *)"delete");
    xmlTextWriterEndElement(writer);
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
      octxt->way->nodes=malloc(sizeof(int)*octxt->way_node_count);
      octxt->way->nr_nodes=octxt->way_node_count;
      memcpy(octxt->way->nodes,octxt->way_nodes,sizeof(int)*octxt->way_node_count);
      octxt->osmf->ways=g_list_prepend(octxt->osmf->ways,octxt->way); 
      octxt->way=NULL;
      octxt->way_node_count=0;
    }
  } else if (!strcmp((char *)name,"node")) {
    octxt->osmf->nodes=g_list_prepend(octxt->osmf->nodes,octxt->node);
    octxt->node=NULL;
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
      char*lon=NULL;
      char*lat=NULL;
      char*timestamp=NULL;
      char*user=NULL;
      int id=0;
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
	}
      }
      if (id&&lat&&lon) {
	octxt->node = new_osm_node(id);
	octxt->node->lon=atof(lon);
	octxt->node->lat=atof(lat);
	if (user)
	  octxt->node->head.user=strdup(user);
	if (timestamp)
	  octxt->node->head.timestamp=strdup(timestamp);
      }
    } else if (!strcmp("way",(char *)name)) {
      int id=0;
      char *user=NULL;
      char *timestamp=NULL;
      for(i=0;atts[i];i+=2) {
	if (!strcmp((char *)atts[i],"id")) {
	  id=atoi((char *)atts[i+1]);
	  octxt->way = new_osm_way(id);
	} else if (!strcmp((char *)atts[i],"user")) {
	  user=(char *)atts[i+1];
	} else if (!strcmp((char *)atts[i],"timestamp")) {
	  timestamp=(char *)atts[i+1];
	}
      }
      if (id) {
	octxt->way = new_osm_way(id);
	if (user)
	  octxt->way->head.user=strdup(user);
	if (timestamp)
	  octxt->way->head.timestamp=strdup(timestamp);
      }
    }
  } else if (!strcmp("tag",(char *)name)) {
    GList **l;
    char *k=NULL;
    char *v=NULL;
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
  } else if ((octxt->way)&&(!strcmp("nd",(char *)name))) {
    for(i=0;atts[i];i+=2) {
      if (!strcmp((char *)atts[i],"ref")) {
	octxt->way_nodes[octxt->way_node_count]=atoi((char *)atts[i+1]);
	octxt->way_node_count++;
      }
    }
  }
}

struct osm_file * parse_osm_file(const char *fname, int all_ways)
{
  struct timeval tv1;
  struct timeval tv2;
  struct timeval tv3;
  char *suf;
  FILE *f=fopen(fname,"rb");
  struct osm_file *osmf=NULL;
  struct osm_parser_ctxt *octxt;
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
	free(octxt);
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
      free(octxt);
    }
  fclose(f);
#ifndef _WIN32
  gettimeofday(&tv2,NULL);
  printtimediff("SAX parsing xml: %d ms\n",&tv1,&tv2);
#endif
  if (!build_references(osmf)) {
    fprintf(stderr,"warning,: inconsistencies detected\n");
  }
#ifndef _WIN32
  gettimeofday(&tv3,NULL);
  printtimediff("build references: %d ms\n",&tv2,&tv3);
#endif
  return osmf;
}
#endif
