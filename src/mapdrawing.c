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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <unistd.h>
#ifdef _WIN32
#include <winsock.h>
#else
#define closesocket close
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#include "png_io.h"
#include "geometry.h"
#include "mapconfig_data.h"
#include "gps.h"
#include "mapdrawing.h"
#include "strasse.h"

int tile_cache_size=4;
#define MAX_CACHE tile_cache_size

#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif
#ifndef M_PI_2
#define M_PI_2         1.57079632679489661923
#endif

static int cache_count;
static GList *cache_list;
static GHashTable *http_hash;
static void free_image_cache(char *fname);
struct http_fetch_buf {
  char *request;
  int fd;
  int outfd;
  int use_tempname;
  char *url;
  char buf[8192];
  int bufpos;
  char *filename;
  GIOChannel *ioc;
  void *data;
  void (*finish_cb)(const char *,const char *,void *);
  void (*fail_cb)(const char *,const char *,void *);
};
struct cache_entry {
  char *name;
#ifdef USE_IMLIB
  GdkPixmap *p;
#else
  struct pixmap_info *p;
#endif
  int count;
  time_t mtime;
};
/* compare the number of the image cache entry */
static gint compare_cache(gconstpointer a, gconstpointer b)
{
  const struct cache_entry *ce=a;
  const struct cache_entry *ce2=b;
  return (ce->count>ce2->count)?1:-1;
}

/* compare the filename of two cache entries */
static gint find_cache(gconstpointer a, gconstpointer b)
{
  const struct cache_entry *ce=a;
  const struct cache_entry *ce2=b;
  return strcmp(ce->name,ce2->name);
}

/* cleans up the http buf */
static void cleanup_http_buf(struct http_fetch_buf *hfb)
{
  g_hash_table_remove(http_hash,hfb->url);
  if (hfb->request)
    g_free(hfb->request);
  if (hfb->url)
    g_free(hfb->url);
  if (hfb->filename)
    g_free(hfb->filename);
  if (hfb->ioc)
    g_io_channel_unref(hfb->ioc);
  if (hfb->outfd>=0)
    close(hfb->outfd);
  if (hfb->fd>=0) {
    if (closesocket(hfb->fd)) {
      perror("close: ");
    }
  }
  g_free(hfb);
}

GtkWidget *make_pixmap_button(struct mapwin *mw,char **xpmdata)
{
  GdkBitmap *mask=NULL;
  GdkPixmap *gpm = gdk_pixmap_create_from_xpm_d(mw->map->window,
						&mask,NULL,xpmdata);
  GtkWidget *gtpm = gtk_pixmap_new(gpm,mask);
  GtkWidget *but=gtk_button_new();
  gtk_container_add(GTK_CONTAINER(but),gtpm);
  gtk_widget_show(gtpm);
  return but;
}

GtkWidget *make_pixmap_toggle_button(struct mapwin *mw,char **xpmdata)
{
  GdkBitmap *mask=NULL;
  GdkPixmap *gpm = gdk_pixmap_create_from_xpm_d(mw->map->window,
						&mask,NULL,xpmdata);
  GtkWidget *gtpm = gtk_pixmap_new(gpm,mask);
  GtkWidget *but=gtk_toggle_button_new();
  gtk_container_add(GTK_CONTAINER(but),gtpm);
  gtk_widget_show(gtpm);
  return but;
}




static int my_connectto(char *host,int port)
{
  int sock;
  struct hostent *ph;
  struct sockaddr_in destaddr;
  ph=gethostbyname(host);
  if (!ph) return -1;
  destaddr.sin_family=ph->h_addrtype;  /* Uebertragen der besorgten 
                                          Informationen */
  destaddr.sin_port=htons(port);   /* htons dreht die Bytes in die
                                    Netzwerkreihenfolge = host
                                    to network order */
  memcpy((char *)&destaddr.sin_addr,ph->h_addr,ph->h_length);   
  sock=socket(AF_INET,SOCK_STREAM,0);
  fcntl(sock,F_SETFL,fcntl(sock,F_GETFL)|O_NONBLOCK);

  if (connect(sock,(struct sockaddr *)&destaddr,sizeof(destaddr))<0) {
    if ((errno != EINPROGRESS) && (errno != EAGAIN)) {
      closesocket(sock);
      return -1;
    }
  }   
  return sock;
}

/* draw route lines */
void draw_line_list(struct mapwin *mw, GdkGC *mygc, GList *l)
{
  int n,i;
  int x1,y1,x2,y2;
  int is_single=0;
  struct t_punkt32 *p;

  n=g_list_length(l);
  if (n<2)
    return;
  l=g_list_first(l);
  p=(struct t_punkt32 *)l->data;
  l=g_list_next(l);
  x1=(p->x>>globalmap.zoomshift)-mw->page_x;
  y1=(p->y>>globalmap.zoomshift)-mw->page_y;
  for(i=1;i<n;i++) {
    p=(struct t_punkt32 *)l->data;
    x2=(p->x>>globalmap.zoomshift)-mw->page_x;
    y2=(p->y>>globalmap.zoomshift)-mw->page_y;
    if ((!p->single_point)&&(!p->start_new)&&(!is_single)&&(check_crossing(x1,y1,x2,y2,mw->page_width,mw->page_height))) {  
      gdk_draw_line(mw->map->window,mygc,x1,y1,x2,y2);
    } else if (p->single_point) {
      if ((x2 >= 0) && (x2 < mw->page_width) &&
          (y2 >= 0) && (y2 < mw->page_height))  {
        gdk_draw_line(mw->map->window,mygc,x2-10,y2,
	    	    x2+10,y2);
        gdk_draw_line(mw->map->window,mygc,x2,y2-10,
		    x2,y2+10);
      }

    }
    x1=x2;
    y1=y2;
    is_single=p->single_point;
    l=g_list_next(l);
  }
}


static void tile_fetched(const char *url, const char *filename,
			 gpointer data)
{
  char *nfname=g_strdup(filename);
  char *dot;
  struct mapwin *mw=(struct mapwin *)data;
  dot=strrchr(nfname,'.');
  if (dot) {
    dot[0]=0;
  }
  free_image_cache(nfname);
  g_free(nfname);
  
  gtk_widget_queue_draw_area(mw->map,0,0,
			     mw->page_width,
			     mw->page_height);
  mapwin_draw(mw,mw->map->style->fg_gc[mw->map->state],globalmap.first,
	      mw->page_x,mw->page_y,0,0,mw->page_width,mw->page_height);

}

/* do http recv */
static gboolean do_http_recv(GIOChannel *source,
			     GIOCondition condition,
			     gpointer data)
{
  int l;
  struct http_fetch_buf *hfb=(struct http_fetch_buf *)data;
  l=recv(hfb->fd,hfb->buf+hfb->bufpos,sizeof(hfb->buf)-hfb->bufpos-1,0);
  if (l<=0) {
    if ((l==0)&&(hfb->outfd>=0)) {
      close(hfb->outfd);
      if (!hfb->use_tempname) {
	char *nfname=g_strdup_printf("%s.",hfb->filename);
	rename(nfname,hfb->filename);
	g_free(nfname);
      }
      hfb->finish_cb(hfb->url,hfb->filename,hfb->data);
      hfb->outfd=-1;
    }
    
    cleanup_http_buf(hfb);
    return FALSE;
  }
  if (hfb->outfd>=0) {
    write(hfb->outfd,hfb->buf,l);
  } else {
    char *datapos;
    int newbufpos=hfb->bufpos+l;
        
    if ((datapos=strstr(hfb->buf,"\r\n\r\n"))) {
      if (!strncmp(hfb->buf+9,"200 OK",6)) {
	if (hfb->use_tempname)
	  hfb->outfd=mkstemp(hfb->filename);
	else {
	  char *nfname=g_strdup_printf("%s.",hfb->filename);
	  hfb->outfd=open(nfname,O_WRONLY|O_CREAT|O_TRUNC,0666);
	  fprintf(stderr,"opened %s\n",nfname);
	  g_free(nfname);
	}
      } else {
	datapos[0]=0;
	fprintf(stderr,"%s\n",hfb->buf);
      }
      if (hfb->outfd<0) {
	if (hfb->fail_cb)
	  hfb->fail_cb(hfb->url,hfb->filename,hfb->data);
	cleanup_http_buf(hfb);
	return FALSE;
      }
      write(hfb->outfd,datapos+4,newbufpos-4-((int)(datapos-hfb->buf)));
      hfb->bufpos=0;
    } else {
      hfb->bufpos=newbufpos;
    }
  }
  return TRUE;
}

/* do http send */
static gboolean do_http_send(GIOChannel *source,
			     GIOCondition condition,
			     gpointer data)
{
  struct http_fetch_buf *hfb=(struct http_fetch_buf *)data;
  if ((send(hfb->fd,hfb->request,strlen(hfb->request),0))>0) {
    fprintf(stderr,"request sended %s\n",hfb->request);
    g_io_add_watch(source,G_IO_IN|G_IO_HUP,do_http_recv,data);
  } else {
    if (hfb->fail_cb)
      hfb->fail_cb(hfb->url,hfb->filename,hfb->data);
    cleanup_http_buf(hfb);
  }
  return FALSE;
}

static void create_path(const char *path)
{
  char *cpy=g_strdup(path);
  char *endpos;
  int endp;
  while((endpos=strrchr(cpy,'/'))) {
    endpos[0]=0;
    if (endpos==cpy) {
      endpos=NULL;
      break;
    }
    if (!mkdir(cpy,0777))
      break;
  }
  if (!endpos) {
    g_free(cpy);
    return;
  }
  endp=endpos-cpy;
  endp++;
  strcpy(cpy,path);
  endp+=strcspn(cpy+endp,"/");
  while(cpy[endp]) {
    cpy[endp]=0;
    if (mkdir(cpy,0777))
      break;
    cpy[endp]='/';
    endp++;
    endp+=strcspn(cpy+endp,"/");
    
  }
  g_free(cpy);
}

static int check_create_file(char *fullname)
{
  int fd=open(fullname,O_WRONLY|O_CREAT,0666);
  if (fd>=0) {
    close(fd);
    return 1;
  }
  create_path(fullname);
  fd=open(fullname,O_WRONLY|O_CREAT,0666);
  if (fd>=0) {
    close(fd);
    return 1;
  }
  return 0;
}

/* initiate a http request for a tile */
static void get_http_tile(struct mapwin *mw,
			  const char *url, const char *filename)
{
  char *fullname=g_strdup_printf("%s.png",filename);
  if (!check_create_file(fullname)) {
    g_free(fullname);
    return;
  }
  get_http_file(url,fullname,tile_fetched,NULL,mw);
  g_free(fullname);
}

/* initiate a http request */
void get_http_file(const char *url,const char *filename,
			  void (*finish_cb)(const char *,const char*,void *),
		   void (*fail_cb)(const char *,const char *,void *),void *data)
{
  int i;
  struct http_fetch_buf *hfb;
  char hostname[512];
  const char *hostn;
  const char *slash;
  int sock;
  int port;
  if (!http_hash) {
    http_hash = g_hash_table_new(g_str_hash,g_str_equal);
  }
  if (g_hash_table_lookup(http_hash,url)) {
    return;
  }

  hostn=url+sizeof("http://")-1;
  for(i=0;(i<511)&&(hostn[i]!=':')&&(hostn[i]!='/');i++)
    hostname[i]=hostn[i];
  hostname[i]=0;
  hostn=hostn+i;
  slash=strchr(hostn,'/');
  if (!slash) 
    slash="/"; 
  if (hostn[0]==':') {
    port=atoi(hostn+1);
  } else {
    port=80;
  }
  sock=my_connectto(hostname,port);
  if (sock<0) {
    if (fail_cb)
      fail_cb(url,filename,data);
    return;
  }
  hfb=g_new0(struct http_fetch_buf,1);
  hfb->outfd=-1;
  hfb->request=g_strdup_printf("%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s %s\r\n\r\n",
			       "GET",slash,hostname,PACKAGE,VERSION);
  hfb->url=g_strdup(url);
  hfb->fd=sock;
  hfb->finish_cb=finish_cb;
  hfb->fail_cb=fail_cb;
  hfb->data=data;
  if (filename) {
    hfb->filename=g_strdup(filename);
    hfb->use_tempname=0;
  } else {
    hfb->filename=g_strdup("/tmp/mp.XXXXXX");
    hfb->use_tempname=1;
  }
  g_hash_table_insert(http_hash,hfb->url,hfb);
#ifdef _WIN32
  hfb->ioc=g_io_channel_win32_new_stream_socket(hfb->fd);
#else
  hfb->ioc=g_io_channel_unix_new(hfb->fd);
#endif
  g_io_add_watch(hfb->ioc,
		 G_IO_OUT|G_IO_HUP,
		 do_http_send,hfb);
  
}


/* draw a part of a pixmap info to the window */
void draw_pinfo(GdkWindow *w, GdkGC *gc,struct pixmap_info *p_info,
		int srcx,int srcy, int destx, int desty,
		int width, int height)
{
  guchar *bitbuf=p_info->row_pointers[srcy]+srcx*p_info->bit_depth/8;
  if (p_info->num_palette) {
    GdkRgbCmap* cmap=gdk_rgb_cmap_new(p_info->gdk_palette,
				      p_info->num_palette);
    
    /* printf("drawing %x:(%d,%d) to (%d,%d) w:%d h=%d\n",(int)p_info,
       srcx,srcy,destx,desty,width,height); */
    gdk_draw_indexed_image(w,gc,destx,desty,width,height,
			   GDK_RGB_DITHER_NONE,
			   bitbuf,p_info->row_len,
			   cmap);
    gdk_rgb_cmap_free(cmap);
  } else {
    gdk_draw_rgb_image(w,gc,destx,desty,width,height,
		       GDK_RGB_DITHER_NONE,bitbuf,
		       p_info->row_len);
	     
  }
  
}

void path_to_lines(double lon, double lat, void *data)
{ 
  struct t_punkt32 *p_new;
  GList **l=data;
  p_new=geosec2pointstruct(lon*3600.0,lat*3600.0);
  *l=g_list_prepend(*l,p_new);
}

struct t_punkt32 *geosec2pointstruct(double long_sec, double latt_sec)
{
  struct t_punkt32 *p_new=calloc(1,sizeof(struct t_punkt32));
  double x,y;
  geosec2point(&x,&y,long_sec,latt_sec);
  x*=(1<<globalmap.zoomshift);
  y*=(1<<globalmap.zoomshift);
  p_new->longg=long_sec;
  p_new->latt=latt_sec;
  p_new->x=(int)x;
  p_new->y=(int)y;
  p_new->time=NULL;
  p_new->speed=0;
  return p_new;
}

/* convert coordinates from geosec to x and y (UTM projektion) */
void geosec2point(double *xr, double *yr,double long_sec, double latt_sec)
{
  double x,y;
  double utmr,utmh;
  x=long_sec;
  y=latt_sec;
  //printf("latt_sec:%.3f long_sec: %.3f\n",latt_sec,long_sec);
  x=x/(3600.0*180.0)*M_PI;
  y=y/(3600.0*180.0)*M_PI;
  if (globalmap.is_utm) {
    x=x-(9.0*M_PI/180.0);
    //printf("latt sec rad:%.6f long rad: %.6f\n",y,x);
    utmh=M_PI_2-atan(cos(x)/tan(y));
    utmr=asin(sin(x)*cos(y));
    //printf("utm right rad: %.6f utm high rad: %.6f\n",utmr,utmh);
    
  } else {
    utmr=x;
    utmh=log(tan(y)+1.0/cos(y));
  }
  utmr-=globalmap.xoffset;
  utmh-=globalmap.yoffset;
  x=utmr/M_PI;
  y=utmh/M_PI;
  x=x*180.0*3600.0;
  y=y*180.0*3600.0;
  y=-y*globalmap.yfactor;
  x=x*globalmap.xfactor;
  *xr=x;
  *yr=y;
}


/* convert coordinates to geosec */
void point2geosec(double *longr, double *lattr, double x, double y)
{
  double utmh, utmr;
  double latt,longg;
  x=x/globalmap.xfactor;
  /* y=y-1228259; */
  y=-y/globalmap.yfactor;
  y=y/(180*3600);
  /* x=x-9331.3392; */
  x=x/(180*3600);
  utmr=x*M_PI;
  utmh=y*M_PI;
  utmr+=globalmap.xoffset;
  utmh+=globalmap.yoffset;
  /**   Napiers rule
   *    cot(utmh)=cos(long)/tan(latt)
        sin(utmr)=sin(long)*cos(latt) */
  /**   pieces of a spheric triangle 
	\alpha=long
	a=utmr
	b=utmh
	c=latt 
        \alpha=a b    
	Napiers rule
        \cot\alpha=\cot a\cdot \sin b
        long=arccot(\cot utmr \cdot \sin utmh)
        \cos c  =   \cos a   \cos b 
        latt=arccos(cos(utmr)*cos(utmh)) */

  //printf("back: utmh: %.6f utmr: %.6f\n",utmh, utmr);
  if (globalmap.is_utm) {
    latt=asin(cos(utmr)*sin(utmh));
    longg=atan(tan(utmr)/cos(utmh));
  } else {
    longg=utmr;
    latt=atan(sinh(utmh));
  }
  //printf("back: latt rad: %.6f utmr: %.6f\n",latt, longg);
  longg/=M_PI;
  latt/=M_PI;
  longg*=(3600*180);
  latt*=(3600*180);
  if (globalmap.is_utm)
    longg+=3600*9;
  *longr=longg;
  *lattr=latt;
}


static void free_image_cache(char *fname)
{
  struct cache_entry ce_search;
  struct cache_entry *ce;
  GList *cached;
  ce_search.name=fname;
  cached=g_list_find_custom(cache_list,
			    &ce_search,find_cache);
  if (cached) {
    ce=(struct cache_entry *)cached->data;
    free_pinfo(ce->p);
    g_free(ce->name);
    g_free(ce);
    cache_list=g_list_remove_link(cache_list,cached);
    g_list_free(cached);
  }
}

/* load a map file or get it from the cache */
static struct pixmap_info *load_image_mtime(char *name,time_t *mtime  /*GdkWindow *win*/)
{
  GList *cached;
  struct cache_entry ce_search;
  struct cache_entry *ce;
  struct stat st;
  char filename[512];
#ifdef USE_IMLIB
  GdkPixmap *p;
  GdkImlibImage *im;
#else
  struct pixmap_info *p;
#endif

  cache_count++;
  ce_search.name=name;
  p=NULL;
 
  cached=g_list_find_custom(cache_list,
			    &ce_search,find_cache);
  if (cached)
    {
      ce=(struct cache_entry *)cached->data;
      if (ce) {
	ce->count=cache_count;
	if (mtime)
	  *mtime=ce->mtime;
	return ce->p;
      }
    }
  if (g_list_length(cache_list)>=MAX_CACHE)
    {
      GList *rem;
      cache_list=g_list_sort(cache_list,compare_cache);
      ce=(struct cache_entry *)g_list_nth_data(cache_list,0);
      if (ce->p)
#ifdef USE_IMLIB
	gdk_imlib_free_pixmap(ce->p);
#else
      free_pinfo(ce->p);
#endif
      printf("entferne %s\n",ce->name);
      g_free(ce->name);
      g_free(ce);
      rem=g_list_first(cache_list);
      cache_list=g_list_remove_link(cache_list,rem);
      g_list_free(rem);
      
    }
  
    snprintf(filename,sizeof(filename),"%s.str", name);
    read_str_file(filename,name);
    snprintf(filename,sizeof(filename),"%s.png", name);
#ifndef USE_IMLIB
    p=load_gfxfile(filename);
    if (!p) {
      if (!stat(filename,&st)) {
	if (mtime)
	  *mtime=st.st_mtime;
	return NULL;
      }
      snprintf(filename,sizeof(filename),"%s.jpg", name);
      p=load_gfxfile(filename);
    }
#endif
#ifdef USE_IMLIB
    im=gdk_imlib_load_image(filename);
    if (im)
      break;
    snprintf(filename,sizeof(filename),"%s/%s.bmp",kartenpfad[i],
	     name);
    im=gdk_imlib_load_image(filename);
    if (im)
      break;
#endif
#ifdef USE_IMLIB
  if (!im)
    return NULL;
  w=im->rgb_width; h=im->rgb_height;
  if (!gdk_imlib_render(im,w,h)) {
    gdk_imlib_destroy_image(im);
    return NULL;
  }
  p=gdk_imlib_move_image(im);
  gdk_imlib_destroy_image(im);
#endif
  if (p) {
    struct stat st;
    ce=g_malloc(sizeof(struct cache_entry));
    ce->p=p;
    ce->name=g_strdup(name);
    ce->count=cache_count;
    cache_list=g_list_append(cache_list,ce);
    stat(filename,&st);
    ce->mtime=st.st_mtime;
    if (mtime)
	*mtime=ce->mtime;
  } else {
    if (!stat(filename,&st)) {
      if (mtime)
	*mtime=st.st_mtime;
      return NULL;
    }
      
  }
  
  return p;  
}

struct pixmap_info *load_image(char *name)
{
  return load_image_mtime(name,NULL);
}

/* blit a piece of one pinfo to another */
static void blt_pinfo(struct pixmap_info *src, struct pixmap_info *dst,
		      unsigned char *color_conv,int xsrc, int ysrc, 
		      int xdest, int ydest, int width, int height)
{
  int x_itr;
  int y_itr;
  png_bytep src_row;
  png_bytep dest_row;
  if ((src->bit_depth!=dst->bit_depth)&&(src->bit_depth!=8)&&(dst->bit_depth!=24))
    return;
  for(y_itr=0;y_itr<height;y_itr++) {
    if (src->bit_depth==8) {
      src_row=src->row_pointers[ysrc+y_itr]+xsrc;
      if (dst->bit_depth==24) {
	dest_row=dst->row_pointers[ydest+y_itr]+3*xdest;
	for(x_itr=0;x_itr<width;x_itr++) {
	  dest_row[x_itr*3]=src->gdk_palette[src_row[x_itr]]>>16;
	  dest_row[x_itr*3+1]=(src->gdk_palette[src_row[x_itr]]>>8)&255;
	  dest_row[x_itr*3+2]=src->gdk_palette[src_row[x_itr]]&255;
	}
      } else {
	dest_row=dst->row_pointers[ydest+y_itr]+xdest;
	for(x_itr=0;x_itr<width;x_itr++) {
	  dest_row[x_itr]=color_conv[src_row[x_itr]];
	}
      }
    } else {
      src_row=src->row_pointers[ysrc+y_itr]+xsrc*src->bit_depth/8;
      dest_row=dst->row_pointers[ydest+y_itr]+xdest*src->bit_depth/8;
      memcpy(dest_row,src_row,width*src->bit_depth/8);
    }
  }
}

/* create the filename from the mapfilename pattern 
 */
static int get_mapfilename(char *dest, int destlen,
			   struct t_map *map, char *pattern,  int x, int y)
{
  char *buf;
  int i,j;
  int ret;
  int val[3];
  int vpos=0;
  buf=strdup(pattern);
  for(i=0;buf[i];i++) {
    if (buf[i]=='%') {
      if (vpos==3) {
	vpos++;
	break;
      }
      i++;
      j=strspn(buf+i,"0123456789-");
      if (buf[i+j]=='X') {
	val[vpos]=x;
	vpos++;
	buf[i+j]='d';
      } else if (buf[i+j]=='Y') {
	val[vpos]=y;
	vpos++;
	buf[i+j]='d';
      } else if (buf[i+j]=='Z') {
	val[vpos]=globalmap.zoomfactor;
	vpos++;
	buf[i+j]='d';
      }

    }
  }
  ret=((vpos==3)||(vpos==2));
  if (ret) {
    if (vpos==2) {
      ret=snprintf(dest,destlen,buf,val[0],val[1]);
    } else {
      ret=snprintf(dest,destlen,buf,val[0],val[1],val[2]);
    }
  } else {
    fprintf(stderr,"format error in pattern: %s\n",map->filepattern);
  }
  free(buf);
  return ret;
}




static void gps_to_line(struct nmea_pointinfo *nmea,void  *data)
{
  GList **mll=data;
  struct t_punkt32 *p_new;
  p_new=geosec2pointstruct(nmea->longsec,nmea->lattsec);
  p_new->time=g_strdup(nmea->time);
  p_new->speed=nmea->speed;
  p_new->hdop=nmea->hdop;
  p_new->single_point=nmea->single_point;
  p_new->start_new=nmea->start_new;
  *mll=g_list_append(*mll,p_new);
  
}

void load_gps_line(const char *fname, GList **mll)
{
  struct gpsfile *gpsf;
  int fd=open(fname,O_RDONLY);
  if (fd<0)
    return;
  gpsf=open_gps_file(fd);
  while(0<proc_gps_input(gpsf,gps_to_line,mll)); 
  close_gps_file(gpsf,1);
}




/* draw the mapwin into the backing store pixmap */ 
int mapwin_draw(struct mapwin *mw,
		 GdkGC *mygc,struct t_map *map,
		 int sx,int sy,int dx,int dy,int width,int height)
{
#ifndef WIN32
  struct timeval tv1;
  struct timeval tv2;
#endif
  struct pixmap_info *src;
  int tcount=0;
  int dest_x,dest_y,src_x,src_y;
  dest_y=dy;
  if (!map)
    return 0;
  src_y=sy+map->yoffset;
#ifndef WIN32
  gettimeofday(&tv1,NULL);
#endif
  src=NULL;
  while(dest_y<dy+height)
    {
      int y_page=src_y/map->tileheight;
      int yoffset=src_y%map->tileheight;
      if (yoffset<0) {
	y_page--;
	yoffset+=map->tileheight;
      }
      dest_x=dx;
      src_x=sx+map->xoffset;
      while(dest_x<dx+width)
	{
	  char filename[512];
	  int x_page=src_x/map->tilewidth;
	  int xoffset=src_x%map->tilewidth;
	  if (xoffset<0) {
	    x_page--;
	    
	    xoffset+=map->tilewidth;
	    
	  }
	  src=NULL;
	  if (get_mapfilename(filename, sizeof(filename),
			      map, map->filepattern, x_page, y_page))
	  {
	    time_t mtime=0;
	    time_t now=time(NULL);
#ifndef WIN32
	    
	    gettimeofday(&tv1,NULL);
#endif
	    mtime=0;
	    src=load_image_mtime(filename,&mtime);
	    if (src) {
	      /* printf("load_image: %x=%s %d\n",(int)src,filename,mtime); */
	      tcount++;
	    }
	    if (map->url) {
	      if (((mw->request_mode>1)&&(now-mtime>mw->request_mode))||
		  ((mw->request_mode==1)&&(!src)&&(!mtime))) {
		char url[512];
#if 0
		char cmd[2048];
#endif
		if (get_mapfilename(url,sizeof(url),
				    map, map->url, x_page, y_page)) {
		  get_http_tile(mw,url,filename);
#if 0
		  /* HACK */
		  snprintf(cmd,sizeof(cmd),"mkdir -p $(dirname %s) || true ; [ ! -f %s.lck ] && ( touch %s.lck ; wget '%s' -O %s.png ; rm %s.lck ) & ",filename,filename,filename,url,filename,filename);
		  system(cmd); 
#ifdef _WIN32
		  Sleep(200);
#else
		  usleep(200000);
#endif
#endif
		}
	      }
	    }
#ifndef WIN32
	    gettimeofday(&tv2,NULL);
	    if (src)
	      {
		int tvdiff;
		tvdiff=tv2.tv_sec-tv1.tv_sec;
		tvdiff=tvdiff*1000;
		tvdiff=(tv2.tv_usec-tv1.tv_usec)/1000+tvdiff;
		/*		printf("Zeit (load_image): %4d ms\n",tvdiff); */
	      }
#endif
	  }
	  gtk_label_set_text(GTK_LABEL(mw->dlabel),filename);
	  if (src) {
#ifdef USE_IMLIB
	    gdk_draw_pixmap(mw->map->window,mygc,src,
			    xoffset,yoffset,
			    dest_x,dest_y,
			    MIN(mw->page_width-dest_x,map->tilewidth-xoffset),
			    MIN(mw->page_height-dest_y,
				MAP_TILE_HEIGHT-yoffset));
#else
	    draw_pinfo(mw->map_store,mygc,src,
		       xoffset,yoffset,
		       dest_x,dest_y,
		       MIN(width+dx-dest_x,map->tilewidth-xoffset),
		       MIN(height+dy-dest_y,
			   map->tileheight-yoffset));
#endif
	  } else {
	    if (map->next) {
	      tcount+=mapwin_draw(mw,mygc,map->next,src_x-map->xoffset,
			  src_y-map->yoffset,dest_x,dest_y,
			  MIN(width+dx-dest_x,map->tilewidth-xoffset),
			  MIN(height+dy-dest_y,
			      map->tileheight-yoffset));  
	    } else {
	      gdk_draw_rectangle(mw->map_store,
				 mw->map->style->bg_gc[mw->map->state],
				 TRUE,dest_x,dest_y,
				 MIN(width+dx-dest_x,
				     map->tilewidth-xoffset),
				 MIN(height+dy-dest_y,
				     map->tileheight-yoffset));
	    }
	  }
	  src_x=src_x+map->tilewidth-xoffset;
	  dest_x=dest_x+map->tilewidth-xoffset;
	}
      src_y=src_y+map->tileheight-yoffset;
      dest_y=dest_y+map->tileheight-yoffset;
    }
  gdk_draw_pixmap(mw->map->window,mygc,mw->map_store,dx,dy,dx,dy,width,
		  height);
 
#ifndef WIN32
  gettimeofday(&tv2,NULL);
  if (src)
    {
      int tvdiff;
      tvdiff=tv2.tv_sec-tv1.tv_sec;
      tvdiff=tvdiff*1000;
      tvdiff=(tv2.tv_usec-tv1.tv_usec)/1000+tvdiff;
      /* printf("Zeit: %4d ms\n",tvdiff); */
    }
#endif
  if (tcount>=tile_cache_size) {
    tile_cache_size=tcount+1;
  }
  return tcount;
}


/* fill in bit depth and the row pointers */

static void init_pinfo_bitdata_parm(struct pixmap_info *pinfo,
				    png_byte bit_depth,
				    png_byte color_type)
{
  int i;
  pinfo->bit_depth=bit_depth;
  pinfo->interlace_type=PNG_INTERLACE_NONE;
  pinfo->color_type=color_type;
  pinfo->row_len=pinfo->width*pinfo->bit_depth/8;
  if (pinfo->row_len&3) {
    pinfo->row_len=pinfo->row_len+4-(pinfo->row_len&3);
  }
  pinfo->row_pointers[0]=malloc(pinfo->row_len*pinfo->height);
  for(i=1;i<pinfo->height;i++) {
    pinfo->row_pointers[i]=pinfo->row_pointers[i-1]+pinfo->row_len;
  }
}

static void init_pinfo_bitdata(struct pixmap_info *pinfo, 
			       struct pixmap_info *template)
{
  init_pinfo_bitdata_parm(pinfo,template->bit_depth,
			  template->color_type);
}

/* fill the pinfo (with and height are initialized) with map data
 * the given point is the upper left corner */
static void draw2pinfo_real(struct pixmap_info *pinfo,struct t_map *map,
		       int sx, int sy, int dx,int dy,int width, int height )
{
  struct pixmap_info *src;
  int dest_x,dest_y,src_x,src_y;
  dest_y=dy;
  src_y=sy+map->yoffset;
  src=NULL;
  while(dest_y<dy+height)
    {
      int y_page=src_y/map->tileheight;
      int yoffset=src_y%map->tileheight;
      if (yoffset<0) {
	y_page--;
	yoffset+=map->tileheight;
      }
      dest_x=dx;
      src_x=sx+map->xoffset;
      while(dest_x<pinfo->width)
	{
	  char filename[512];
	  int x_page=src_x/map->tilewidth;
	  int xoffset=src_x%map->tilewidth;
	  if (xoffset<0) {
	    x_page--;
	    
	    xoffset+=map->tilewidth;
	    
	  }
	  snprintf(filename,sizeof(filename),
		   "karte%03d/%03d",y_page,x_page);
	  if (get_mapfilename(filename,sizeof(filename),
			      map,map->filepattern,x_page,y_page)&&
	      ((src=load_image(filename)))) {
	    int i,j;
	    unsigned char  color_conv[256];
	    if (pinfo->row_len==0) {
	      init_pinfo_bitdata(pinfo,src);
	    }
	    for(i=0;(pinfo->bit_depth<=8)&&(i<src->num_palette);i++) {
	      int found=0;
	      for(j=0;j<pinfo->num_palette;j++) {
		if (src->gdk_palette[i]==pinfo->gdk_palette[j]) {
		  color_conv[i]=j;
		  found=1;
		  break;
		}
	      }
	      if (!found) {
		if (pinfo->num_palette<256) {
		  color_conv[i]=pinfo->num_palette;
		  pinfo->gdk_palette[pinfo->num_palette]=src->gdk_palette[i];
		  pinfo->num_palette++;
		} else {
		  break;
		}
	      }
	    }
	    if (((i==0)||(i!=src->num_palette))&&(pinfo->bit_depth<=8)) {
	      free(pinfo->row_pointers[0]);
	      init_pinfo_bitdata_parm(pinfo,24,PNG_COLOR_TYPE_RGB);
	      pinfo->num_palette=0;
	      draw2pinfo_real(pinfo,map,sx,sy,dx,dy,width,height);
	      return;
	    }
	    for(;i<256;i++)
	      color_conv[i]=0;
	    blt_pinfo(src,pinfo,color_conv,xoffset,yoffset,dest_x,dest_y,
		      MIN(width+dx-dest_x,map->tilewidth-xoffset),
		      MIN(height+dy-dest_y,
			  map->tileheight-yoffset));
	  } else {
	    if (map->next) {
	      draw2pinfo_real(pinfo,map->next,src_x-map->xoffset,
			      src_y-map->yoffset,dest_x,dest_y,
			      MIN(width+dx-dest_x,map->tilewidth-xoffset),
			      MIN(height+dy-dest_y,
				  map->tileheight-yoffset));
	      
	    } else {
	      /*  clear */
	    }
	  }	
	  src_x=src_x+map->tilewidth-xoffset;
	  dest_x=dest_x+map->tilewidth-xoffset;
	}
      src_y=src_y+map->tileheight-yoffset;
      dest_y=dest_y+map->tileheight-yoffset;
    }
}

/* wrapper to extract the width and height from the struct */
void draw2pinfo(struct pixmap_info *pinfo, struct t_map *map,
		       int x, int y)
{
  draw2pinfo_real(pinfo, map, x,y,0,0,pinfo->width,pinfo->height);
}



/* create a pinfo rectangle */
struct pixmap_info * get_map_rectangle(int x, int y, int w, int h)
{
  struct pixmap_info *pinfo=g_new0(struct pixmap_info,1);
  pinfo->width=w;
  pinfo->height=h;
  pinfo->num_palette=0;
  pinfo->row_len=0;
  pinfo->row_pointers=malloc(sizeof(png_bytep)*pinfo->height);
  
  draw2pinfo(pinfo,globalmap.first,x,y);
  return pinfo;
}

/* calculate auxillary values fsor the coordinate transformation*/
void calc_mapoffsets()
{
  double x,y;
  struct t_map *map;
  /*printf("zero latt_sec: %.3f zero long_sec: %.3f\n",globalmap.zerolatt,
	 globalmap.zerolong); */
  x=(globalmap.zerolong-3600.0*9.0)/(3600*180)*M_PI;
  y=globalmap.zerolatt/(3600*180)*M_PI;
/*  printf("zero latt_sec rad: %.6f zero long_sec rad: %.6f\n",y,x); */
  if (globalmap.is_utm) {
    x=(globalmap.zerolong-3600.0*9.0)/(3600*180)*M_PI;
    y=globalmap.zerolatt/(3600*180)*M_PI;
    globalmap.xoffset=asin(sin(x)*cos(y));
    globalmap.yoffset=M_PI_2-atan(cos(x)/tan(y));
    /*  printf("xoffset: %.6f, yoffset: %.6f\n",globalmap.xoffset, globalmap.yoffset); */
  } else {
    x=globalmap.zerolong/(3600*180)*M_PI;
    y=globalmap.zerolatt/(3600*180)*M_PI;
    globalmap.xoffset=x;
    globalmap.yoffset=log(tan(y)+1/cos(y));
  }
  globalmap.xfactor=globalmap.orig_xfactor*(1<<(globalmap.zoomfactor-1));
  globalmap.yfactor=globalmap.orig_yfactor*(1<<(globalmap.zoomfactor-1));
  globalmap.zoomshift=0;
  /* globalmap.zoomfactor=1; */
  globalmap.zoomable=1;
  map=globalmap.first;
  while(map) {
    double x,y;
    geosec2point(&x,&y,map->reflong,map->reflatt);
    map->xoffset=map->refx-(int)x;
    map->yoffset=map->refy-(int)y;
    if (!strstr(map->filepattern,"%Z"))
 
     globalmap.zoomable=0;
    map=map->next;
  }
  {
    int maxdim=globalmap.fullwidth;
    if (globalmap.fullheight>maxdim)
      maxdim=globalmap.fullheight;
    for(globalmap.zoomshift=31;maxdim;globalmap.zoomshift--) {
      maxdim/=2;
    }
    globalmap.zoomshift-=globalmap.zoomfactor;
    if (globalmap.zoomshift<0)
      globalmap.zoomshift=0;
  }
}

/* create and append a map struct to the maplist in globalmap */
struct t_map *map_new(char *name)
{
  struct t_map *m=g_new0(struct t_map,1);
  if (globalmap.last) {
    globalmap.last->next=m;
  } else {
    globalmap.first=m;
  }
  globalmap.last=m;
  m->name=name;
  m->url=NULL;
  return m;
}



/* output a ps command to draw a line. coordinates are 
 * converted to the coordinates required in the ps file */
static void draw_line_ps(int fd, int mx, int my, int w, int h,
			 double x1, double y1, double x2, double y2)
{
  char buf[256];
  /* double s=(y2-y1)/(x2-x1); */
  /* get_mark_xywh(rc,&mx,&my,&w,&h,&dim); */
  if (x1<mx) {
    if (x2<mx)
      return;
    y1=y1+(mx-x1)*(y2-y1)/(x2-x1);
    x1=mx;
  }
  if (x1>(mx+w)) {
    if (x2>(mx+w)) {
      return;
    }
    y1=y1-(x1-mx-w)*(y2-y1)/(x2-x1);
    x1=mx+w;
  }
  if (x2<mx) {
    y2=y2+(mx-x2)*(y2-y1)/(x2-x1);
    x2=mx;
  }
  if (x2>(mx+w)) {
    y2=y2+(x2-w-mx)*(y2-y1)/(x2-x1);
    x2=mx+w;
  }
  if (y1<my) {
    if (y2<my)
      return;
    x1=x1+(my-y1)*(x2-x1)/(y2-y1);
    y1=my;
  }
  if (y1>(my+h)) {
    if (y2>(my+h))
      return;
    x1=x1-(y1-my-h)*(x2-x1)/(y2-y1);
    y1=my+h;
  }
  if (y2<my) {
    x2=x2+(my-y2)*(x2-x1)/(y2-y1);
    y2=my;
  }
  if (y2>(my+h)) {
    x2=x2+(y2-h-my)*(x2-x1)/(y2-y1);
    y2=my+h;
  }
  if ((y1<my)||(y2<my)||(y2>(my+h))||(y1>(my+h)))
    return;
  snprintf(buf,sizeof(buf),"%d %d moveto %d %d lineto stroke\n",
          (int)x1-mx,(int)y1-my,
          (int)x2-mx,(int)y2-my);
  write(fd,buf,strlen(buf));
}



/* generate ps commands to draw a route */
void draw_marks_to_ps(GList *mark_line_list, int mx, int my,
		      int w, int h, int fd)
{
  GList *l;
  int i,len;
  int x1,x2,y1,y2;
  len=g_list_length(mark_line_list);
  write(fd,"1 0 1 setrgbcolor 3 setlinewidth\n",strlen("1 0 1 setrgbcolor 3 setlinewidth\n"));
  l=g_list_first(mark_line_list);
  x1=((struct t_punkt32 *)(l->data))->x>>globalmap.zoomshift;
  y1=((struct t_punkt32 *)(l->data))->y>>globalmap.zoomshift;
  for(i=0;i<len;i++){
    x2=((struct t_punkt32 *)(l->data))->x>>globalmap.zoomshift;
    y2=((struct t_punkt32 *)(l->data))->y>>globalmap.zoomshift;
    draw_line_ps(fd,mx,my,w,h,x1,y1,x2,y2);
    x1=x2;
    y1=y2;
    l=g_list_next(l);
  }
 
}
