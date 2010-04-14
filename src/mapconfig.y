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
%name-prefix="kconfy"
%{
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include "mapconfig.h"
#include "mapconfig_data.h"
#define YYDEBUG 1
#define YYERROR_VERBOSE 1
extern char *current_file;
extern int current_lineno;
double parsed_lattitude;
double parsed_longitude;
extern int parse_init_state;
extern int parsing_started;
struct t_globalmap globalmap;
void yyerror(const char *str)
{ 
  fprintf(stderr,"%s: %d: error: %s\n",current_file,current_lineno,str);
  exit(1);
}

int kconfwrap()
{       
  return 1;
}

int yylex()
{
  int ret;
  if (!parsing_started) {
    ret=parse_init_state;
    parsing_started = 1;
  } else {
    ret=kconflex();
  }
  return ret;
}

char *expand_home(char *h)
{
#ifdef _WIN32
    char *home=NULL;
    char buf[MAX_PATH];
    if (S_OK == SHGetFolderPath(NULL,CSIDL_APPDATA,
                      NULL,0,buf)) {
       home=buf;
    }
#else
    char *home=getenv("HOME");
#endif
    if ((home)&&(strncmp(h,"~/",2)==0)) {
      char *newstr=malloc(strlen(home)+strlen(h));
      strcpy(newstr,home);
      strcat(newstr,"/");
      strcat(newstr,h+2);
      return newstr;
    } else {
      return h;
    }

}

%}

%union
{
  char *str;
  int num;
  double realnum;
  struct t_map *map;
}

%token <str> T_STRING
%token <num> T_NUM
%token <realnum> T_REALNUM
%token T_PLACEFILE T_STARTPLACE T_FULLWIDTH T_FULLHEIGHT T_INITIALZOOM T_MAP T_ORIGIN T_XFACTOR T_YFACTOR T_TILEWIDTH T_TILEHEIGHT T_FILEPATTERN T_NEWLINE PARSE_MAPCONFIG PARSE_COORDS T_MERCATOR T_UTM T_PROJECTION T_URL
%type <realnum> lattitude longitude degree degmin degsec realnum

%%


mainp:  PARSE_MAPCONFIG { globalmap.is_utm=1; globalmap.zoomfactor=1; } configfile 
       | PARSE_COORDS coords
;

coords: lattitude longitude {
  parsed_longitude = $2;
  parsed_lattitude = $1;
};

/*§ The configfile consists of lines in the following format */
configfile: | configfile configline ;

/*§ one line can have the following contents: */
configline: /* nothing */
     T_NEWLINE |
     configexp T_NEWLINE
     ;

configexp:
   /*§ load the given place file list */
    T_PLACEFILE T_STRING {
      char *exph=expand_home($2);
      globalmap.placefilelist=g_list_append(globalmap.placefilelist,exph);
      if ($2 != exph)
	free($2);
    }
   /*§ specify the start place by lattitude/longitude */
   | T_STARTPLACE lattitude longitude {
     globalmap.startlatt=$2;
     globalmap.startlong=$3;
   }
   /*§ specify origin (lattitude/longitude)
     § of the coordinate system */
   | T_ORIGIN lattitude longitude {
     globalmap.zerolatt=$2;
     globalmap.zerolong=$3;
   }
   /*§ set the start place by a name of a place */
   | T_STARTPLACE T_STRING {
     globalmap.startplace=$2;
   }
   /*§ width of the coordinate system in pixels */
   | T_FULLWIDTH T_NUM {
     globalmap.fullwidth=$2;
   }
   /*§ height of the coordinate system in pixels */
   | T_FULLHEIGHT T_NUM {
     globalmap.fullheight=$2;
   }
   /*§ initial zoomfactor */
   | T_INITIALZOOM T_NUM {
     globalmap.zoomfactor=$2;
   }
   /*§ global xfactor */
   | T_XFACTOR realnum {
     globalmap.orig_xfactor=$2;
   }
   /*§ global yfactor */
   | T_YFACTOR realnum {
     globalmap.orig_yfactor=$2;
   }
   | T_PROJECTION projtype 
   
   /*§ definition of a map */
   | T_MAP map 
   ;
   /*§ definition of a map */
map: T_STRING { $<map>$=map_new($1); } T_NEWLINE '{' maplines '}' 
   /*§ or without a newline */
   | T_STRING { $<map>$=map_new($1); } '{' maplines '}' 
   ;
   
   /*§ a map definition can consist of lines */
maplines: | maplines T_NEWLINE mapline
;
   /*§ in the following form */
mapline: /* nothing */
   /*§ a reference point with geographical coordinates
     § and with x/y coordinates  relative to the origin specified above */
   | T_ORIGIN lattitude longitude T_NUM T_NUM {
     $<map>-3->reflatt=$2;
     $<map>-3->reflong=$3;
     $<map>-3->refx=$4;
     $<map>-3->refy=$5;
   }
   /*§ x scaling factor relative to the global coordinate system */
   | T_XFACTOR realnum {
     $<map>-3->xfactor=$2;
   }
   | T_URL T_STRING {
     $<map>-3->url=$2;
   }
   /*§ y scaling factor relative to the global coordinate system */
   | T_YFACTOR realnum {
     $<map>-3->yfactor=$2;
   }
   /*§ tile width (width of a single file) */
   | T_TILEWIDTH T_NUM {
     $<map>-3->tilewidth=$2;
   }
   /*§ tile height (height of a single file) */
   | T_TILEHEIGHT T_NUM {
     $<map>-3->tileheight=$2;
   }
   /*§ file pattern: %X and %Y are replaced
     § with the x and y number of the tile, 
     § both must appear
     § printf modifiers are allowed (0, - and 0-9) */
   | T_FILEPATTERN T_STRING {
     char *exph=expand_home($2);
     $<map>-3->filepattern=exph;
     if (exph!=$2)
       free($2);
   }
   ; 


projtype: T_UTM {globalmap.is_utm=1;} | T_MERCATOR {globalmap.is_utm=0;}
realnum: T_NUM {
        $$=(double)$1;
	}
       | T_REALNUM {
        $$=(double)$1;
       }
       ;

/*§ a longitude can be defined as
  § a degree followed  by E for east */
longitude: degree 'E' { $$=$1; }
/*§ or by W for west */
	 | degree 'W' { $$=-$1; }
         ;
	 
/*§ a lattitude can be defined as
  § a degree followed by N for north */
lattitude:  degree 'N' { $$=$1; }
/*§ or by S for south */
	 | degree 'S' { $$=-$1; }
         ;
	 
/*§ a degree can contain the degree with 
  § minutes separated by the ° character*/
degree: realnum { $$=$1; }  
        | realnum '°' degmin { $$=$1+$3; } 
        | realnum '°' { $$=$1; }
/*§ or  minutes only */
        | degmin { $$=$1; }
/*§ or  seconds only */
	| degsec { $$=$1; }
	;
	
/*§ minutes can be minutes with secods  */
degmin:  realnum '\'' degsec { $$=$1/60.0+$3; }
/*§ or only minutes followed by the ' character */
        | realnum '\'' { $$=$1/60.0; }
	;
/*§ seconds followed by '' */
degsec: realnum '\'' '\'' { $$=$1/3600.0; }
        ;
