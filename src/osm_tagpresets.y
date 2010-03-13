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

%name-prefix="tagsel"
%{
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "osm_tagpresets.h"
#include "osm_tagpresets_data.h"
#define YYDEBUG 1
#define YYERROR_VERBOSE 1
struct osm_preset_menu_sect *osm_waypresets=NULL;
struct osm_preset_menu_sect *osm_nodepresets=NULL;
extern int tagpreset_lineno;
static GHashTable *preset_menu_hash;

static void tagselerror(const char *str)
{
  fprintf(stderr,"presetfile %d: error: %s\n",tagpreset_lineno,str);
  exit(1);
}

int tagselwrap()
{
  return 1;
}

%}

%union
{
  char *str;
  int num;
  struct osm_preset_menu_sect *menu;
  struct osm_presetitem *item;
  GList *list;
}

%token T_POS T_PRESET T_WAYPRESET T_NODEPRESET T_SETTAG T_IMG T_SETTAGEDIT T_APPEND T_YES T_NO T_CHECKBOX
%token <str> T_STRING
%token <num> T_NUM
%type <str> settag title img preset yesstr nostr
%type <menu> menu menuorvar
%type <item> item
%%


presetfile: /* nothing */
     | presetfile
     presetexp 
     ;

presetexp:
T_WAYPRESET menu { osm_waypresets=$2; } |
T_NODEPRESET menu { osm_nodepresets=$2; } |
T_STRING '=' menu { 
  if (!preset_menu_hash)
    preset_menu_hash=g_hash_table_new(g_str_hash,g_str_equal);
  g_hash_table_insert(preset_menu_hash,$1,$3);
} ;

menu: '{' {
  struct osm_preset_menu_sect *m=g_new0(struct osm_preset_menu_sect,1);
  $<menu>$=m;
} menulines  '}' { 
  $$=$<menu>2; }  
;

menulines: | menulines settag {
  $<menu>0->tags=g_list_append($<menu>0->tags,$2);
}
| menulines item {
  $<menu>0->items=g_list_append($<menu>0->items,$2);
}
;


settag: T_SETTAG T_STRING T_STRING {
  char *tstr=calloc(1,strlen($2)+strlen($3)+2);
  strcpy(tstr,$2);
  strcpy(tstr+strlen($2)+1,$3);
  $$=tstr;
}
;

menuorvar: menu { $$=$1; } |
T_APPEND T_STRING menu {
  struct osm_preset_menu_sect *m=$3;
  struct osm_preset_menu_sect *mappend=(preset_menu_hash)?
    g_hash_table_lookup(preset_menu_hash,$2):NULL;
  if (!mappend) {
    fprintf(stderr,"no menu found with the name %s\n",$2);
  } else {
    m->tags=g_list_concat(m->tags,g_list_copy(mappend->tags));
    m->items=g_list_concat(m->items,g_list_copy(mappend->items));
  }
  $$=m;
};

item: T_POS T_NUM T_NUM title img menuorvar {
  struct osm_presetitem *pitem=g_new0(struct osm_presetitem,1);
  pitem->x=$2;
  pitem->y=$3;
  pitem->name=$4;
  pitem->img=$5;
  pitem->menu=$6;
  pitem->type=BUTTON;
  $$=pitem;
} | T_SETTAGEDIT T_STRING img T_STRING preset {
  struct osm_presetitem *pitem=g_new0(struct osm_presetitem,1);
  pitem->x=-1;
  pitem->y=-1;
  pitem->name=$2;
  pitem->img=$3;
  pitem->tagname=$4;
  pitem->preset=$5;
  pitem->menu=NULL;
  pitem->type=TEXT;
  $$=pitem;
} | T_CHECKBOX T_STRING T_STRING yesstr nostr {
  struct osm_presetitem *pitem=g_new0(struct osm_presetitem,1);
  pitem->x=-1;
  pitem->y=-1;
  pitem->tagname=$3;
  pitem->name=$2;
  pitem->type=CHECKBOX;
  pitem->yesstr=$4;
  pitem->nostr=$5;
  $$=pitem;
};

yesstr: { $$=NULL; } | T_YES T_STRING { $$=$2; };
nostr: { $$=NULL; } | T_NO T_STRING { $$=$2; };

img:   { $$=NULL; } | T_IMG T_STRING { $$=$2; };
preset:{ $$=NULL; } | T_PRESET T_STRING { $$=$2; };
title: { $$=NULL; } | T_STRING { $$=$1; };
