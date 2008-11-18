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

#include <libxml/parser.h>
#include <string.h>
static int placedepth=-1;
static int depth=0;
static double lon;
static double lat;
static char *placename;
static char *placetype;
static char *is_in;
static void mystarthandler(void *ctx,
        const xmlChar *name,
        const xmlChar ** atts)
{
  depth++;
  if (!strcmp((char *)name,"node")) {
    int i;
    placedepth=depth;
    for(i=0;atts[i];i+=2) {
      if (!strcmp((char *)atts[i],"lon")) {
        if (atts[i+1]) {
            lon=atof((char *)atts[i+1]);
        }
      } else if (!strcmp((char *)atts[i],"lat")) {
        if (atts[i+1]) {
          lat=atof((char *)atts[i+1]);
        }
      }
    }
  }
  if (depth==placedepth+1) {
    if (!strcmp((char *)name,"tag")) {
    const char *k=NULL;
    const char *v=NULL;
    int i;
    for(i=0;atts[i];i+=2) {
      if (!strcmp((char *)atts[i],"k")) {
        k=(char *)atts[i+1];
      } else if (!strcmp((char *)atts[i],"v")) {
        v=(char *)atts[i+1];
      }
    }
    if (k&&v) {
    if (!strcmp(k,"place")) {
      placetype=strdup(v);
    } else if (!strcmp(k,"name")) {
      placename=strdup(v);
    } else if (!strcmp(k,"is_in")) {
      is_in=strdup(v);
    }
    }
    }
  }
}

static void myendhandler(void *ctx,
        const xmlChar *name)
{
    if (depth==placedepth) {
        if (placetype&&placename) {
           if (is_in) {
             int i;
             for(i=0;is_in[i];i++) {
               if (is_in[i]==',') {
                 is_in[i]=';';
               }
             }
           }
           printf("%s,%s,%d,%d\n",placename,
                   is_in?is_in:"",(int)(lon*3600.0),(int)(lat*3600.0));
        }
        if (is_in)
            free(is_in);
        is_in=NULL;
        if (placetype)
            free(placetype);
        placetype=NULL;
        if (placename)
            free(placename);
        placename=NULL;
        placedepth=-1;
    }
    depth--;
}


int main(int argc, char **argv)
{
  FILE *f;
  int l;
  static xmlSAXHandler myhandler={
    .initialized = 1,
    .startElement = mystarthandler,
    .endElement = myendhandler
  };
  xmlParserCtxtPtr ctxt;
  char buf[256];
  if (argc!=2) {
    fprintf(stderr,"Usage %s file.osm > places.txt\n", argv[0]);
    return 1;
  }
  f=fopen(argv[1],"rb");
  if (!f) {
    fprintf(stderr,"Cannot open %s\n",argv[1]);
    fprintf(stderr,"Usage %s file.osm > places.txt\n",argv[0]);
    return 1;
  }
  l = fread(buf, 1, sizeof(buf), f);
  if (l<0)
    return 1;
  ctxt = xmlCreatePushParserCtxt(&myhandler, NULL,
                                 buf,l,argv[1]);

  while((l=fread(buf,1,sizeof(buf),f))>0) {
    xmlParseChunk(ctxt,buf,l,0);
  }
  xmlParseChunk(ctxt, buf, 0 ,1);
  xmlFreeParserCtxt(ctxt);
  fclose(f);
  return 0;
}
