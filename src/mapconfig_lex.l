/********************************************************************** 
 mumpot - Copyright (C) 2008 - Andreas Kemnade
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
             
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
***********************************************************************/
%option prefix="kconf"
%option nounput
%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "mapconfig.h"
char *current_file;
int current_lineno=1;
YYSTYPE yylval;
extern double parsed_lattitude;
extern double parsed_longitude;
int parse_init_state;
int parsing_started;
extern int kconfyparse();
int parse_mapconfig(char *fname) 
{
  FILE *f;
  char buf[256];
  snprintf(buf,sizeof(buf),"%s",setlocale(LC_ALL,NULL));
  setlocale(LC_ALL,"C");
  f=fopen(fname,"r");
  current_file=fname;
  if (!f)
    return 0;
  parse_init_state = PARSE_MAPCONFIG;
  parsing_started = 0;

  yyrestart(f);
  kconfyparse();
  setlocale(LC_ALL,"");
  return 1;
  	  
}

int parse_coords(char *coordstr, double *lattitude, double *longitude)
{
  YY_BUFFER_STATE bs;
  char buf[256];
  snprintf(buf,sizeof(buf),"%s",setlocale(LC_ALL,NULL));
  setlocale(LC_ALL,"C");
  parse_init_state = PARSE_COORDS;
  parsing_started=0;
  bs=yy_scan_string(coordstr);
  kconfyparse();
  yy_delete_buffer(bs);
  *lattitude=parsed_lattitude;
  *longitude=parsed_longitude;
  setlocale(LC_ALL,"");
  return 1;
}


static char *delquote(char *str)
{   
  char *b;
  b=strdup(str+1);
  b[strlen(b+1)]=0;
  return b;
}
%}			  
%%
\"[^\"]*\"     yylval.str=delquote(yytext); return T_STRING;
placefile      return T_PLACEFILE;
startplace     return T_STARTPLACE;
fullwidth      return T_FULLWIDTH;
fullheight     return T_FULLHEIGHT;
map            return T_MAP;
origin         return T_ORIGIN;
xfactor        return T_XFACTOR;
yfactor        return T_YFACTOR;
tilewidth      return T_TILEWIDTH;
tileheight     return T_TILEHEIGHT;
filepattern    return T_FILEPATTERN;
projection     return T_PROJECTION;
initialzoom    return T_INITIALZOOM;
utm            return T_UTM;
url            return T_URL;
mercator       return T_MERCATOR;
\n   current_lineno++; return T_NEWLINE;
[ \t]+ /* ignore */
#.* /* ignore*/
-?[[:digit:]]+\.[[:digit:]]+  yylval.realnum=atof(yytext); return T_REALNUM;
-?[[:digit:]]+          yylval.num=atoi(yytext); return T_NUM;
.    return (int)(yytext[0]&0xff);

%%

