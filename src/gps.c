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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/xmlwriter.h>
#include <locale.h>
#include <zlib.h>
#include <time.h>
#include "common.h"
#include "gps.h"

struct gpsfile {
  int fd;
  int bufpos;
  int first;
  char buf[256];
  int use_zlib;
  z_stream zstream;
  char xmlbuf[256];
  char gzbuf[1024];
  int xmlbufpos;
  int contreading;
  void (*handling_procedure)(struct gpsfile *,void (*)(struct nmea_pointinfo *,void *), void *data);
  struct nmea_pointinfo curpoint;
  int last_fix;
  xmlParserCtxtPtr ctxt;
  void *gpsproc_data;
  void (*gpsproc)(struct nmea_pointinfo *, void *);
  
};

double to_seconds(char *str)
{
  int pointindex = strcspn(str,".");
  double mins, degs;
  if (pointindex <2)
    return 0;
  mins = atof(str+pointindex-2);
  str[pointindex-2]=0;
  degs = atof(str);
  return degs+mins/60.0;
  
}

int save_gpx(const char *fname,GList *save_list)
{
  time_t t=time(NULL);
  struct tm tm;
  GList *l;
  xmlTextWriterPtr writer;
  writer=xmlNewTextWriterFilename(fname,0);
  if (!writer)
    return 0;
  xmlTextWriterStartDocument(writer,NULL,"UTF-8",NULL);
  xmlTextWriterStartElement(writer,(xmlChar *)"gpx");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"version",(xmlChar *)"1.0");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"creator",(xmlChar *)PACKAGE " " VERSION);
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"xmlns:xsi",(xmlChar *)"http://www.w3.org/2001/XMLSchema-instance");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"xmlns",
			      (xmlChar *)"http://www.topografix.com/GPX/1/0");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"xsi:schemaLocation",(xmlChar *)"http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd");
  tm=*gmtime(&t);
  xmlTextWriterWriteFormatElement(writer,(xmlChar *)"time","%04d-%02d-%02dT%02d:%02d:%02d",
			    tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
			    tm.tm_hour,tm.tm_min,tm.tm_sec);
  xmlTextWriterStartElement(writer,(xmlChar *)"trk");
  xmlTextWriterStartElement(writer,(xmlChar *)"trkseg");
  for(l=g_list_first(save_list);l;l=g_list_next(l)) {
    struct t_punkt32 *p=(struct t_punkt32 *)l->data;
    xmlTextWriterStartElement(writer,(xmlChar *)"trkpt");
    xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"lat","%f",
				      p->latt);
    xmlTextWriterWriteFormatAttribute(writer,(xmlChar *)"lon","%f",
				      p->longg);
    if (p->time) {
      t=p->time;
      tm=*gmtime(&t);
      xmlTextWriterWriteFormatElement(writer,(xmlChar*)"time",
				"%04d-%02d-%02dT%02d:%02d:%02d",
				tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,
				tm.tm_hour,tm.tm_min,tm.tm_sec);
    }
    if (p->speed>0)
      xmlTextWriterWriteFormatElement(writer,(xmlChar *)"speed","%f",p->speed*1.852/3.6);
    xmlTextWriterEndElement(writer); /* /trkpt */
    
  }
  xmlTextWriterEndElement(writer); /*trkseg */
  xmlTextWriterEndElement(writer); /* trk */
  xmlTextWriterEndElement(writer); /* gpx */
  xmlTextWriterEndDocument(writer);
  xmlFreeTextWriter(writer);
  return 1;
}

void save_nmea(FILE *f,GList *save_list)
{
  int i;
  GList *l;
  time_t t;
  struct tm *tm;
  for(i=0,l=g_list_first(save_list);l;l=g_list_next(l),i++)
  {
  struct t_punkt32 *p=(struct t_punkt32 *)l->data;
  int lattdeg, longdeg;
  double lattmin, longmin;
  lattdeg = p->latt;
  longdeg = p->longg;
  longmin = ((double)longdeg);
  longmin = p->longg - longmin;
  longmin *= 60.0;
  lattmin = ((double)lattdeg);
  lattmin = p->latt - lattmin;
  lattmin *= 60.0;
  t=p->time;
  tm=gmtime(&t);
  if (tm->tm_year > 100)
    tm->tm_year-=100;
  /*
  fprintf(f,"%.1f %.1f\n",p->latt,p->longg);
  */
  fprintf(f,"$GPRMC,%02d%02d%02d.0,A,%02d%07.4f,%c,%03d%07.4f,%c,%f,,%02d%02d%02d,0.4,E,S\n",
          tm->tm_hour,tm->tm_min,tm->tm_sec,
          lattdeg,lattmin,'N',longdeg,longmin,'E',p->speed,
	  tm->tm_mday,tm->tm_mon+1,tm->tm_year);
  }
}
 
int gps_writeback(struct gpsfile *gpsf, void *data, int len)
{
   return write(gpsf->fd, data, len);
}


static int my_split(char *bigstr, char **needle, char *delim, int maxsplit)
{
  int i;
  for(i=0;i<maxsplit;i++) {
    int n;
    needle[i]=bigstr;
    n=strcspn(bigstr,delim);
    if (bigstr[n]==0) {
      i++;
      break;
    }
    bigstr[n]=0;
    bigstr+=n;
    bigstr++;
  }
  return i;
}

static void gps_to_line(struct nmea_pointinfo *nmea,void  *data)
{
  GList **mll=data;
  {
    struct t_punkt32 *p_new;
    p_new=calloc(1,sizeof(struct t_punkt32));
    p_new->longg=nmea->lon;
    p_new->latt=nmea->lat;
    p_new->time=nmea->time;
    p_new->speed=nmea->speed;
    p_new->single_point=nmea->single_point;
    p_new->start_new=nmea->start_new;
    *mll=g_list_append(*mll,p_new);
  }
}

void load_gps_line_noproj(const char *fname, GList **mll)
{
  struct gpsfile *gpsf;
  int fd=open(fname,O_RDONLY);
  if (fd<0)
    return;
  gpsf=open_gps_file(fd);
  while(0<proc_gps_input(gpsf,gps_to_line,mll)); 
  close_gps_file(gpsf,1);
}

static int proc_gps_decompress(struct gpsfile *gpsf,
				void (*gpsproc)(struct nmea_pointinfo *,void *), void *data)
{
  while(gpsf->zstream.avail_in>0) {
    gpsf->zstream.next_out=(unsigned char *)gpsf->buf+gpsf->bufpos;
    gpsf->zstream.avail_out=sizeof(gpsf->buf)-gpsf->bufpos;
    if (Z_OK != inflate(&gpsf->zstream,Z_SYNC_FLUSH))
      return 0;
    gpsf->bufpos=sizeof(gpsf->buf)-gpsf->zstream.avail_out;
    gpsf->handling_procedure(gpsf,gpsproc,data);
  }
  return 1;
}

int proc_gps_input(struct gpsfile *gpsf,
                   void (*gpsproc)(struct nmea_pointinfo *,void *), void *data)
{
  int l;
  if (gpsf->use_zlib>0) {
    l=read(gpsf->fd,gpsf->gzbuf,sizeof(gpsf->gzbuf));
    if (l>0) {
      gpsf->zstream.avail_in=l;
      gpsf->zstream.next_in=(unsigned char *)gpsf->gzbuf;
      if (!proc_gps_decompress(gpsf,gpsproc,data)) {
	perror("decompress error");
	return -1;
      }
      return l;
    }
  } else {
    l=read(gpsf->fd,gpsf->buf+gpsf->bufpos,sizeof(gpsf->buf)-gpsf->bufpos);
  }
  if (l<=0) {
    perror("proc_gps_input: ");
    return l;
  }
  gpsf->bufpos+=l;
  if (gpsf->use_zlib==0) {
    if (gpsf->bufpos>2) {
      if ((gpsf->buf[0]!=0x1f)||(gpsf->buf[1]!='\x8b')) {
	gpsf->use_zlib=-1;
      } else {
	memcpy(gpsf->gzbuf,gpsf->buf,gpsf->bufpos);
	gpsf->zstream.avail_in=gpsf->bufpos;
	gpsf->zstream.next_in=(unsigned char *)gpsf->gzbuf;
	gpsf->zstream.avail_out=sizeof(gpsf->buf);
	gpsf->zstream.next_out=(unsigned char *)gpsf->buf;
	gpsf->bufpos=0;
	if (Z_OK==inflateInit2(&gpsf->zstream,31)) {
	  gpsf->use_zlib=1;
	  if (!proc_gps_decompress(gpsf,gpsproc,data))
	    l=0;
	  return l;
	}
      }
    }
  }
  gpsf->handling_procedure(gpsf,gpsproc,data);
  return l;
}

static void mystarthandler(void *ctx,
			   const xmlChar *name,
			   const xmlChar ** atts)
{
  struct gpsfile *gpsf=(struct gpsfile *)ctx;
  if ((!strcmp((char *)name,"wpt"))||(!strcmp((char *)name,"trkpt"))) {
    int i;
    for(i=0;atts[i];i+=2) {
      if (!strcmp((char *)atts[i],"lon")) {
        if (atts[i+1]) {
	  gpsf->curpoint.lon=atof((char *)atts[i+1]);
        }
      } else if (!strcmp((char *)atts[i],"lat")) {
        if (atts[i+1]) {
          gpsf->curpoint.lat=atof((char *)atts[i+1]);
        }
      }
    }
    gpsf->curpoint.speed=0;
  }
  if ((!strcmp((char *)name,"speed")) ||
      (!strcmp((char *)name,"fix")) ||
      (!strcmp((char *)name,"course")) ||
      (!strcmp((char *)name,"time"))) {
    gpsf->contreading=1;
    gpsf->xmlbufpos=0;
  }
}

static void myendhandler(void *ctx,
			 const xmlChar *name)
{
  struct gpsfile *gpsf=(struct gpsfile *)ctx;
  if (!strcmp((char *)name,"trkpt")) {
    gpsf->curpoint.single_point=0;
    if (gpsf->first)
      gpsf->curpoint.start_new=1;
    gpsf->gpsproc(&gpsf->curpoint,gpsf->gpsproc_data);
    gpsf->first=0;
    gpsf->curpoint.start_new=0;
  } else if (!strcmp((char *)name,"wpt")) {
    gpsf->curpoint.single_point=1;
    gpsf->gpsproc(&gpsf->curpoint,gpsf->gpsproc_data);
  } else if (!strcmp((char *)name,"speed")) {
    gpsf->xmlbuf[gpsf->xmlbufpos]=0;
    gpsf->curpoint.speed=atof(gpsf->xmlbuf)*3.6/1.852;
    gpsf->contreading=0;
    gpsf->xmlbufpos=0;
  } else if (!strcmp((char *)name,"time")) {
    gpsf->xmlbuf[gpsf->xmlbufpos]=0;
    gpsf->curpoint.time=parse_xml_time(gpsf->xmlbuf);
    gpsf->contreading=0;
    gpsf->xmlbufpos=0;
  } else if (!strcmp((char *)name,"course")) {
    gpsf->xmlbuf[gpsf->xmlbufpos]=0;
    gpsf->curpoint.heading=atof(gpsf->xmlbuf);
    gpsf->contreading=0;
    gpsf->xmlbufpos=0;
  } else if (!strcmp((char *)name,"fix")) {
    gpsf->xmlbuf[gpsf->xmlbufpos]=0;
    gpsf->curpoint.state=strcmp("3d",gpsf->xmlbuf)?'V':'A';
    gpsf->contreading=0;
    gpsf->xmlbufpos=0;
  } else if (!strcmp((char *)name,"trkseg")) {
    gpsf->curpoint.start_new=1;
  }
}

static void mycharacters(void *ctx,
		       const xmlChar *ch,
		       int len)
{
  struct gpsfile *gpsf = (struct gpsfile *)ctx;
  if (!gpsf->contreading)
    return;
  if ((len+gpsf->xmlbufpos)>=sizeof(gpsf->xmlbuf)) {
    len=sizeof(gpsf->xmlbuf)-gpsf->xmlbufpos-1;
  }
  memcpy(gpsf->xmlbuf+gpsf->xmlbufpos,ch,len);
  gpsf->xmlbufpos+=len;
}

static void proc_gps_gpx(struct gpsfile *gpsf,
			 void (*gpsproc)(struct nmea_pointinfo *,void *),
			 void *data)
{
  static xmlSAXHandler myhandler={
    .initialized = 1,
    .startElement = mystarthandler,
    .endElement = myendhandler,
    .characters = mycharacters
  };

  gpsf->gpsproc=gpsproc;
  gpsf->gpsproc_data=data;
  if (!gpsf->ctxt) {
    
    gpsf->ctxt = xmlCreatePushParserCtxt(&myhandler, gpsf,
					 gpsf->buf,gpsf->bufpos,
					 "gpxfile");
  } else {
    xmlParseChunk(gpsf->ctxt,gpsf->buf,gpsf->bufpos,0);
  }
  gpsf->bufpos=0;
}

static void proc_gps_nmea(struct gpsfile *gpsf,
			  void (*gpsproc)(struct nmea_pointinfo *,void *),
			  void *data)
{
  char *endp;
  if (!strncmp(gpsf->buf,"<?xml",5)) {
    gpsf->handling_procedure=proc_gps_gpx;
    proc_gps_gpx(gpsf,gpsproc,data);
    return;
  }
  while ((endp=memchr(gpsf->buf,'\n',gpsf->bufpos))) {
    int readlen;
    *endp=0;
    /* printf("gps line: %s ENDLINE\n",gpsf->buf); */
    if (strstr(gpsf->buf,"<?xml")) {
      *endp='\n';
      gpsf->handling_procedure=proc_gps_gpx;
      proc_gps_gpx(gpsf,gpsproc,data);
      return;
    }
    if (strncmp(gpsf->buf,"$GPRMC",6)==0) {
      char *fields[13];
      int numfields=my_split(gpsf->buf,fields,",",13);
      if (((numfields == 13)||(numfields == 12))&&(strlen(fields[3])>3)) {
	struct tm tm;
        time_t t;
	gpsf->curpoint.lat=to_seconds(fields[3]);
        gpsf->curpoint.lon=to_seconds(fields[5]);
        if ((fields[4])[0] == 'S')
          gpsf->curpoint.lat=-gpsf->curpoint.lat;
        if ((fields[6])[0] == 'W')
          gpsf->curpoint.lon=-gpsf->curpoint.lon;
	memset(&tm,0,sizeof(tm));
	sscanf(fields[1],"%02d%02d%02d",&tm.tm_hour,&tm.tm_min,&tm.tm_sec);
	sscanf(fields[9],"%02d%02d%02d",&tm.tm_mday,&tm.tm_mon,&tm.tm_year);
	tm.tm_mon--;
	if (tm.tm_year<70)
	  tm.tm_year+=100;
	t=gmmktime(&tm);  /* t=tm-tz */
        gpsf->curpoint.time=t;
	gpsf->curpoint.start_new=gpsf->first?1:0;
        if ((gpsf->curpoint.time-gpsf->last_fix)>20) {
          gpsf->curpoint.start_new=1; 
        }
        gpsf->last_fix=t;
        gpsf->curpoint.speed=atof(fields[7]);
	if (strlen(fields[8]))
	  gpsf->curpoint.heading=atof(fields[8]);
        else
	  gpsf->curpoint.heading=INVALID_HEADING;
	gpsf->curpoint.state=(numfields==13)?((fields[12])[0]):'?';
        gpsproc(&gpsf->curpoint,data);
	gpsf->first=0;
      }
    } else if (!strncmp(gpsf->buf,"$GPGGA",6)) {
      char *fields[15];
      int numfields=my_split(gpsf->buf,fields,",",15);
      if (numfields>8)
	gpsf->curpoint.hdop=10.0*atof(fields[8]);
    }
    endp++;
    readlen=endp-gpsf->buf;
    if (readlen != gpsf->bufpos)
      memmove(gpsf->buf,endp,gpsf->bufpos-readlen);
    gpsf->bufpos-=readlen;
  }
 
}


struct gpsfile *open_gps_file(int fd)
{
  struct gpsfile *gpsf=calloc(sizeof(struct gpsfile),1);
  setlocale(LC_NUMERIC,"C");
  gpsf->fd=fd;
  gpsf->first=1;
  gpsf->handling_procedure=proc_gps_nmea;
  gpsf->curpoint.hdop=990;
  return gpsf;
}

void close_gps_file(struct gpsfile *gpsf,int closefd)
{
  if (gpsf->ctxt) {
    xmlParseChunk(gpsf->ctxt,gpsf->buf,0,1);
    xmlFreeParserCtxt(gpsf->ctxt);
  }
  if (closefd)
    close(gpsf->fd);
  free(gpsf);
}
