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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <glib.h>
#include "myintl.h"
#include "osm_tagpresets.h"

extern void tagselparse(FILE *f);
int tagpreset_lineno=1;
YYSTYPE tagsellval;

int osm_parse_presetfile(char *fname)
{
  FILE *f;
  setlocale(LC_NUMERIC,"C");
  f=fopen(fname,"r");
  if (!f)
    return 0;
  yyrestart(f);
  tagselparse(f);
  return 1;
}

static char *delbracket(char *str)
{   
  char *b;
  if (str[0]=='_') {
    char *bb;
    b=strdup(str+3);
    b[strlen(b+2)]=0;
    bb=strdup(_(b));
    free(b); 
    b=bb;
  } else {
    b=strdup(str+1);
    b[strlen(b+1)]=0;
  }
  return b;
}

%}
%%
\"[^\"]*\"     tagsellval.str=delbracket(yytext); return T_STRING;
_\(\"[^\"]*\"\)     tagsellval.str=delbracket(yytext); return T_STRING;
pos            return T_POS;
gfx            return T_IMG;
settag         return T_SETTAG;
settagedit     return T_SETTAGEDIT;
append         return T_APPEND;
nodepreset           return T_NODEPRESET;
waypreset      return T_WAYPRESET;
checkbox       return T_CHECKBOX;
yes            return T_YES;
no             return T_NO;

[ \t]+ /* ignore */
#.* /* ignore*/
\n   tagpreset_lineno++;
-?[[:digit:]]+          tagsellval.num=atoi(yytext); return T_NUM;
.    return (int)(yytext[0]&0xff);
