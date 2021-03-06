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
#ifndef K_OSM_TAGPRESETS_GUI_H
#define K_OSM_TAGPRESETS_GUI_H


void osm_choose_tagpreset(struct osm_preset_menu_sect *sect,
			  void (*set_tag)(char *,char *,void *,void *),
			  void *taglist,void *user_data);

#endif
