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
#include <locale.h>
#include "gps.h"

struct gpsfile {
  int fd;
  int bufpos;
  int first;
  char buf[256];
  char xmlbuf[256];
  int xmlbufpos;
  int contreading;
  void (*handling_procedure)(struct gpsfile *,void (*)(struct nmea_pointinfo *,void *), void *data);
  struct nmea_pointinfo curpoint;
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
  return degs * 3600.0+mins*60.0;
  
}
void  save_nmea(FILE *f,GList *save_list)
{
  int i;
  GList *l;
  for(i=0,l=g_list_first(save_list);l;l=g_list_next(l),i++)
  {
  char *ptime;
  char tbuf[40];
  struct t_punkt32 *p=(struct t_punkt32 *)l->data;
  int lattdeg, longdeg;
  double lattmin, longmin;
  lattdeg = p->latt/3600;
  longdeg = p->longg/3600;
  longmin = ((double)longdeg)*3600.0;
  longmin = p->longg - longmin;
  longmin /= 60.0;
  lattmin = ((double)lattdeg)*3600.0;
  lattmin = p->latt - lattmin;
  lattmin /= 60.0;
  ptime=p->time;
  if (!ptime) {
      snprintf(tbuf,sizeof(tbuf),"%02d%02d%02d.0",i/3600,(i/60)%60,i%60);
      ptime=tbuf;
  }
  /*
  fprintf(f,"%.1f %.1f\n",p->latt,p->longg);
  */
  fprintf(f,"$GPRMC,%s,A,%02d%07.4f,%c,%03d%07.4f,%c,%f,,170507,0.4,E,S\n",
          ptime,
          lattdeg,lattmin,'N',longdeg,longmin,'E',p->speed);
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
    p_new->longg=nmea->longsec;
    p_new->latt=nmea->lattsec;
    p_new->time=g_strdup(nmea->time);
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


int proc_gps_input(struct gpsfile *gpsf,
                   void (*gpsproc)(struct nmea_pointinfo *,void *), void *data)
{
  int l=read(gpsf->fd,gpsf->buf+gpsf->bufpos,sizeof(gpsf->buf)-gpsf->bufpos);
  if (l<=0) {
    perror("proc_gps_input: ");
    return l;
  }
  gpsf->bufpos+=l;
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
	  gpsf->curpoint.longsec=3600.0*atof((char *)atts[i+1]);
        }
      } else if (!strcmp((char *)atts[i],"lat")) {
        if (atts[i+1]) {
          gpsf->curpoint.lattsec=3600.0*atof((char *)atts[i+1]);
        }
      }
    }
    gpsf->curpoint.speed=0;
  }
  if ((!strcmp((char *)name,"speed")) ||
      (!strcmp((char *)name,"fix")) ||
      (!strcmp((char *)name,"course"))) {
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
  while ((endp=memchr(gpsf->buf,'\n',gpsf->bufpos))) {
    int readlen;
    *endp=0;
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
        gpsf->curpoint.lattsec=to_seconds(fields[3]);
        gpsf->curpoint.longsec=to_seconds(fields[5]);
        if ((fields[4])[0] == 'S')
          gpsf->curpoint.lattsec=-gpsf->curpoint.lattsec;
        if ((fields[6])[0] == 'W')
          gpsf->curpoint.longsec=-gpsf->curpoint.longsec;
        gpsf->curpoint.time=fields[1];
        gpsf->curpoint.date=fields[9];
        gpsf->curpoint.speed=atof(fields[7]);
        gpsf->curpoint.heading=atof(fields[8]);
        gpsf->curpoint.state=(numfields==13)?((fields[12])[0]):'?';
	gpsf->curpoint.start_new=gpsf->first?1:0;
        gpsproc(&gpsf->curpoint,data);
	gpsf->first=0;
      }
    } else if (!strncmp(gpsf->buf,"$GPGGA",6)) {
      char *fields[15];
      int numfields=my_split(gpsf->buf,fields,",",15);
      if (numfields>=8)
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
