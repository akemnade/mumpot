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
%option prefix="tagsel"
%option nounput
%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "osm_tagpresets.h"
#include "osm_tagpresetfile_lex.l"


int tagpreset_lineno;
YYTYPE tagsellval;

struct osmtagpresets *osm_parse_presetfile(char *fname)
{
  FILE *f;
  char buf[256];
  setlocale(LC_NUMERIC,"C");
  
}
