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

%token T_POS T_WAYPRESET T_NODEPRESET T_SETTAG
%token <str> T_STRING
%token <num> T_NUM
%type <list> settags items
%type <str> settag 
%type <menu> menu
%type <item> item
%%


presetfile: /* nothing */
     | presetfile
     presetexp 
     ;

presetexp:
T_WAYPRESET menu { osm_waypresets=$2; } |
T_NODEPRESET menu { osm_nodepresets=$2; }
     ;

menu: '{' settags '}' {
  struct osm_preset_menu_sect *m=g_new0(struct osm_preset_menu_sect,1);
  m->tags=$2;
  $$=m;
}
  |
  '{' items  '}' {
    struct osm_preset_menu_sect *m=g_new0(struct osm_preset_menu_sect,1);
    m->items=$2;
    $$=m;
  }
;

settags: { $$=NULL;} | settags settag { 
  $$=g_list_append($1,$2); 
  } 
;

settag: T_SETTAG T_STRING T_STRING {
  char *tstr=calloc(1,strlen($2)+strlen($3)+2);
  strcpy(tstr,$2);
  strcpy(tstr+strlen($2)+1,$3);
  $$=tstr;
}
;

items: {$$=NULL;} | items item { 
  $$=g_list_append($1,$2); 
  }
;

item: T_POS T_NUM T_NUM T_STRING menu {
  struct osm_presetitem *pitem=g_new0(struct osm_presetitem,1);
  pitem->x=$2;
  pitem->y=$3;
  pitem->name=$4;
  pitem->menu=$5;
  $$=pitem;
};
