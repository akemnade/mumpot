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
#ifndef K_OSM_TAGPRESETS_DATA_H
#define K_OSM_TAGPRESETS_DATA_H
struct osm_preset_menu_sect {
  GList *tags;
  GList *items;
};
struct osm_presetitem {
  char *name;
  char *img;
  char *preset;
  char *tagname;
  char *yesstr;
  char *nostr;
  enum {
    BUTTON,TEXT,CHECKBOX
  } type;
  int x;
  int y;
  struct osm_preset_menu_sect *menu;
};
extern struct osm_preset_menu_sect *osm_waypresets;
extern struct osm_preset_menu_sect *osm_nodepresets;
int osm_parse_presetfile(char *fname);
int tagsellex();
#endif
