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
#include <curl/curl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#include "png_io.h"
#include "gui_common.h"
#include "geometry.h"
#include "mapconfig_data.h"
#include "gps.h"
#include "mapdrawing.h"
#include "strasse.h"

int tile_cache_size=64;
#define MAX_CACHE tile_cache_size

#ifndef M_PI
#define M_PI           3.14159265358979323846  /* pi */
#endif
#ifndef M_PI_2
#define M_PI_2         1.57079632679489661923
#endif

#define MAX_HTTP_REQUESTS 2

static int cache_count;
static GList *cache_list;
/* contains the urls which are actually downloaded to avoid
   downloading the same url multiple times at the same time */
static GHashTable *http_hash;
/* contains the image files which are to fetch,
   the files are only removed on successful downloads from
   the hash to avoid rerequesting failed tiles again and
   again so that the tile server does not get overloaded */
static GHashTable *tile_files_to_fetch;
static GList *tile_fetch_queue;
static GList *request_list;
static int running_requests=0;
static void free_image_cache(char *fname);
static CURLM *curlm;
int tile_request_mode=1;
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
#ifndef SIMPLE_SOCK_FETCH
  CURL *curl;
#endif
  void (*finish_cb)(const char *,const char *,void *);
  void (*fail_cb)(const char *,const char *,void *);
  int (*size_check)(const char *,void *,int);
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

static void check_request_queue();
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
  if ((hfb->fd>=0) || hfb->curl) {
    running_requests--;
  }
  if (hfb->fd>=0) {
    if (closesocket(hfb->fd)) {
      perror("close: ");
    }
  }
  if (hfb->curl)
    curl_easy_cleanup(hfb->curl);
  g_free(hfb);
  check_request_queue();
}

/* center the view to a place */
void center_map(struct mapwin *mw,double longsec, double lattsec)
{

  double x,y;
  geosec2point(&x,&y,(double)longsec,(double)lattsec);
  x=x-mw->page_width/2;
  y=y-mw->page_height/2;
  x=floor(x);
  y=floor(y);
  if ((x != GTK_ADJUSTMENT(mw->hadj)->value) ||
      (y != GTK_ADJUSTMENT(mw->vadj)->value)) {
    GTK_ADJUSTMENT(mw->hadj)->value=x;
    GTK_ADJUSTMENT(mw->vadj)->value=y;
    gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->hadj));
    gtk_adjustment_value_changed(GTK_ADJUSTMENT(mw->vadj));
  }
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
  struct sockaddr_in *stored_addr;
  static GHashTable *dnshash;
  if (!dnshash)
    dnshash=g_hash_table_new(g_str_hash,g_str_equal);
  stored_addr=g_hash_table_lookup(dnshash,host);
  if (stored_addr) {
    destaddr=*stored_addr;
  } else {
    ph=gethostbyname(host);
    if (!ph) return -1;
    destaddr.sin_family=ph->h_addrtype; 
    memcpy((char *)&destaddr.sin_addr,ph->h_addr,ph->h_length);
    stored_addr=malloc(sizeof(struct sockaddr_in));
    *stored_addr=destaddr;
    g_hash_table_insert(dnshash,strdup(host),stored_addr);
  }
  destaddr.sin_port=htons(port); 
  sock=socket(AF_INET,SOCK_STREAM,0);
#ifndef _WIN32
  fcntl(sock,F_SETFL,fcntl(sock,F_GETFL)|O_NONBLOCK);
#else
  {
    u_long m = 1;
    ioctlsocket(sock,FIONBIO,&m);
  }
#endif
  if (connect(sock,(struct sockaddr *)&destaddr,sizeof(destaddr))<0) {
#ifdef _WIN32
    int err = WSAGetLastError();
    if ((err != WSAEINPROGRESS) && (err != WSAEWOULDBLOCK))
#else
    if ((errno != EINPROGRESS) && (errno != EAGAIN)) 
#endif
    {
      closesocket(sock);
      return -1;
    }
  }   
  return sock;
}

/* draw route lines */
void draw_line_list(struct mapwin *mw, GdkGC *mygc, GList *l, GdkColor *color256 )
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
      if (color256) {
        int speedval=p->speed*10.0;
        /* int speedval=p->hdop*10.0;  */
        if (speedval < 0)
          speedval=0;
        if (speedval >255)
          speedval=255;
        /*printf("%d\n",speedval); */
        gdk_gc_set_foreground(mygc,color256+speedval);
      } 
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

static int mystrequal(gconstpointer a, gconstpointer b)
{
  return strcmp((char *)a,(char *)b);
}

static void tile_failed(const char *url, const char *filename,
			gpointer data)
{
  GList *l=g_list_find_custom(tile_fetch_queue,url,mystrequal);
  if (l) {
    free(l->data);
    tile_fetch_queue=g_list_remove_link(tile_fetch_queue,l);
    g_list_free(l);
  }
}

static void tile_fetched(const char *url, const char *filename,
			 gpointer data)
{
  char *nfname=g_strdup(filename);
  char *dot;
  GList *l=g_list_find_custom(tile_fetch_queue,url,mystrequal);
  struct mapwin *mw=(struct mapwin *)data;
  dot=strrchr(nfname,'.');
  if (dot) {
    dot[0]=0;
  }
  free_image_cache(nfname);
  g_free(nfname);
  if (mw) {
    gtk_widget_queue_draw_area(mw->map,0,0,
			       mw->page_width,
			       mw->page_height);
    mapwin_draw(mw,mw->map->style->fg_gc[mw->map->state],globalmap.first,
		mw->page_x,mw->page_y,0,0,mw->page_width,mw->page_height);
  }
  if (l) {
    free(l->data);
    tile_fetch_queue=g_list_remove_link(tile_fetch_queue,l);
    g_list_free(l);
  }
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
        struct stat st;
	char *nfname=g_strdup_printf("%s.",hfb->filename);
        if ((!stat(nfname,&st))&&
           ((!hfb->size_check)||(hfb->size_check(hfb->filename,hfb->data,st.st_size)))) { 
          if (!rename(nfname,hfb->filename)) {
            g_hash_table_remove(tile_files_to_fetch,hfb->filename);
          }
        } else {
          unlink(nfname);
        }
	g_free(nfname);
      }
      if (hfb->finish_cb)
	hfb->finish_cb(hfb->url,hfb->filename,hfb->data);
      hfb->outfd=-1;
    } else {
      if (hfb->fail_cb)
	hfb->fail_cb(hfb->url,hfb->filename,hfb->data);
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
	if (hfb->use_tempname) {
#ifdef _WIN32
	  mktemp(hfb->filename);
	  hfb->outfd=open(hfb->filename,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
	  printf("opened: %s\n",hfb->filename);
#else
	  hfb->outfd=mkstemp(hfb->filename);
#endif
	} else {
	  char *nfname=g_strdup_printf("%s.",hfb->filename);
#ifdef _WIN32
	  hfb->outfd=open(nfname,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
#else
	  hfb->outfd=open(nfname,O_WRONLY|O_CREAT|O_TRUNC,0666);
#endif
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
    g_io_add_watch(source,G_IO_IN|G_IO_HUP,do_http_recv,data);
  } else {
    if (hfb->fail_cb)
      hfb->fail_cb(hfb->url,hfb->filename,hfb->data);
    cleanup_http_buf(hfb);
  }
  return FALSE;
}

static int check_create_file(char *fullname)
{
  int fd;
  fd=open(fullname,O_WRONLY,0666);
  if (fd>=0) {
    close(fd);
    return 1;
  }
  fd=open(fullname,O_WRONLY|O_CREAT,0666);
  if (fd>0) {
    close(fd);
    unlink(fullname);
    return 1;
  }
  create_path(fullname);
  fd=open(fullname,O_WRONLY,0666);
  if (fd>=0) {
    close(fd);
    return 1;
  }
  fd=open(fullname,O_WRONLY|O_CREAT,0666);
  if (fd>0) {
    close(fd);
    unlink(fullname);
    return 1;
  }
  return 0;
}

/* ignore tiles if they are getting smaller or
   have zero size */
static int tile_size_check(const char *filename, void *data,
                           int len)
{
  struct stat st;
  if (len==0)
    return 0;
  if (stat(filename,&st))
    return 1;
  if (st.st_size> (len*3)) {
    return 0;
  }
  return 1;
}


int tile_requests_processed()
{
  return tile_fetch_queue==NULL;
}
/* initiate a http request for a tile */
static void get_http_tile(struct mapwin *mw,
			  const char *url, const char *filename,
			  int do_queue)
{
  char *fullname=g_strdup_printf("%s.png",filename);
  char *fn2;
  char *urldup;
  if (!tile_files_to_fetch)
    tile_files_to_fetch=g_hash_table_new(g_str_hash,g_str_equal);
  if (g_hash_table_lookup(tile_files_to_fetch,fullname)) {
    g_free(fullname);
    return;
  }
  fn2=strdup(fullname); 
  g_hash_table_insert(tile_files_to_fetch,fn2,fn2); 
  if (!check_create_file(fullname)) {
    g_free(fullname);
    return;
  }
  urldup=strdup(url);
  tile_fetch_queue=g_list_append(tile_fetch_queue,urldup);
  if (!get_http_file(url,fullname,tile_fetched,tile_failed,tile_size_check,mw)) {
    if (do_queue) {
      tile_fetch_queue=g_list_remove(tile_fetch_queue,urldup);
    }
  }
  g_free(fullname);
}

/* do http recv */
static ssize_t  maptile_writefunc(void *ptr, size_t size,
				  size_t nmemb, void *data)
{
  ssize_t l = size * nmemb;
  struct http_fetch_buf *hfb=(struct http_fetch_buf *)data;
  if (hfb->outfd < 0) {
    if (! memcmp(ptr,"<!", 2)) {
      fprintf(stderr," junk received: \n");
      fwrite(ptr, size, nmemb, stderr);
      return l;
    }
    if (hfb->use_tempname) {
#ifdef _WIN32
      mktemp(hfb->filename);
      hfb->outfd=open(hfb->filename,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
      printf("opened: %s\n",hfb->filename);
#else
      hfb->outfd=mkstemp(hfb->filename);
#endif
    } else {
      char *nfname=g_strdup_printf("%s.",hfb->filename);
#ifdef _WIN32
      hfb->outfd=open(nfname,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);
#else
      hfb->outfd=open(nfname,O_WRONLY|O_CREAT|O_TRUNC,0666);
#endif
      fprintf(stderr,"opened %s\n",nfname);
      g_free(nfname);
    }
  }
  if (hfb->outfd >= 0) {
    write(hfb->outfd,ptr,l);
  }
  return l;
}

static gboolean maptile_failed(gpointer data)
{
  struct http_fetch_buf *hfb = (struct http_fetch_buf *) data;
  if (hfb->fail_cb)
    hfb->fail_cb(hfb->url,hfb->filename,hfb->data);
  cleanup_http_buf(hfb);
  return FALSE;
}

static gboolean maptile_finished(gpointer data)
{
  struct http_fetch_buf *hfb = (struct http_fetch_buf *) data;
  if (hfb->finish_cb)
    hfb->finish_cb(hfb->url,hfb->filename,hfb->data);
  cleanup_http_buf(hfb);
  return FALSE;
}

static gboolean maptile_check_msg(gpointer key, gpointer value, gpointer user_data)
{
  CURLMsg *msg = (CURLMsg *)user_data;
  struct http_fetch_buf *hfb = (struct http_fetch_buf *) value;
  if (hfb->curl == msg->easy_handle) {
    if (hfb->outfd > 0) {
      close(hfb->outfd);
      hfb->outfd = -1;
    }
    curl_multi_remove_handle(curlm, hfb->curl);
    if (msg->data.result == CURLE_OK) {
      if (!hfb->use_tempname) {
	struct stat st;
	char *nfname=g_strdup_printf("%s.",hfb->filename);
	if ((!stat(nfname,&st))&&
	    ((!hfb->size_check) || (hfb->size_check(hfb->filename,hfb->data,st.st_size)))) { 
	  if (!rename(nfname,hfb->filename)) {
	    g_hash_table_remove(tile_files_to_fetch,hfb->filename);
	  }
	} else {
	  unlink(nfname);
	}
	g_free(nfname);
      }
      g_idle_add(maptile_finished, hfb);
     
    } else {
      g_idle_add(maptile_failed, hfb);
    }
    return TRUE;
  }
  return FALSE;
}

static gboolean maptile_check_msgs(void *data)
{
  int msgs_left = 0;
  CURLMsg *msg; /* for picking up messages with the transfer status */

  while((msg = curl_multi_info_read(curlm, &msgs_left))) {
    if (msg->msg == CURLMSG_DONE) {
      g_hash_table_foreach_remove(http_hash, maptile_check_msg, msg);
    }
  }
  return FALSE;
}


static int maptile_iofunc(GIOChannel *source,
			  GIOCondition cond,
			  gpointer data)
{
  int handles;
  int bm;
  if (!cond)
    return FALSE;
  if (cond&G_IO_IN)
    bm|=CURL_CSELECT_IN;
  if (cond&G_IO_OUT)
    bm|=CURL_CSELECT_OUT;
  while(CURLM_CALL_MULTI_PERFORM==curl_multi_socket_action(curlm,
							   g_io_channel_unix_get_fd(source),
							   bm,
                                                           &handles));
  g_idle_add(maptile_check_msgs, NULL);
  return TRUE;

}

static int maptile_sockfunc(CURL *easy,
			    curl_socket_t s,
			    int action,
			    void *userp,
			    void *socketp)
{
  struct {
    GIOChannel *ioc;
    int src;
  } *sockdata;
  if (socketp) {
    sockdata = socketp;
  } else {
    sockdata = calloc(1, sizeof(*sockdata));
    curl_multi_assign(curlm, s, (void *)sockdata);
  }
  if (sockdata->ioc == NULL) {
#ifdef _WIN32
    sockdata->ioc = g_io_channel_win32_new_stream_socket(s);
#else
    sockdata->ioc = g_io_channel_unix_new(s);
#endif
  }
  GIOCondition cond = 0;
  g_source_remove_by_user_data(sockdata);
  switch(action) {
  case CURL_POLL_IN:
    cond |= G_IO_IN;
    break;
  case CURL_POLL_OUT:
    cond |= G_IO_OUT;
    break;
  case CURL_POLL_INOUT:
    cond |= (G_IO_IN | G_IO_OUT);
    break;
  case CURL_POLL_REMOVE:
    if (sockdata->ioc) {
      g_io_channel_unref(sockdata->ioc);
      sockdata->ioc = NULL;
      free(sockdata);
      curl_multi_assign(curlm, s, (void *)NULL);
    }
    return 0;
  }

  if (cond) {
    sockdata->src = g_io_add_watch(sockdata->ioc, cond, maptile_iofunc, sockdata);
  }
  
  return 0;
}

static int curl_ping(void *data)
{
  int running = 0;
  while(CURLM_CALL_MULTI_PERFORM==(curl_multi_socket_all(curlm,&running)));
  return running != 0;	
}

static void start_http_request(struct http_fetch_buf *hfb)
{
  int running = 0;
  fprintf(stderr, "fetching %s\n", hfb->url);
#ifdef SIMPLE_SOCK_FETCH

  int i;
  char hostname[512];
  const char *hostn;
  const char *slash;
  int sock;
  int port;
  hostn=hfb->url+sizeof("http://")-1;
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
    if (hfb->fail_cb)
      hfb->fail_cb(hfb->url,hfb->filename,hfb->data);
    cleanup_http_buf(hfb);
    return;
  }
#else
  if (!curlm) {
    curlm = curl_multi_init();
    curl_multi_setopt(curlm, CURLMOPT_SOCKETFUNCTION,
		      maptile_sockfunc);
  }
#endif
  running_requests++;
#ifdef SIMPLE_SOCK_FETCH
  hfb->fd=sock;
  hfb->request=g_strdup_printf("%s %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s %s\r\n\r\n",
			       "GET",slash,hostname,PACKAGE,VERSION);
#ifdef _WIN32
  hfb->ioc=g_io_channel_win32_new_stream_socket(hfb->fd);
#else
  hfb->ioc=g_io_channel_unix_new(hfb->fd);
#endif
  g_io_add_watch(hfb->ioc,
		 G_IO_OUT|G_IO_HUP,
		 do_http_send,hfb);
#else
  hfb->curl = curl_easy_init();
  curl_easy_setopt(hfb->curl, CURLOPT_URL, hfb->url);
  curl_easy_setopt(hfb->curl, CURLOPT_USERAGENT, "tileloader");
  curl_easy_setopt(hfb->curl,CURLOPT_NOSIGNAL,1);
  curl_easy_setopt(hfb->curl, CURLOPT_WRITEDATA, hfb);
  curl_easy_setopt(hfb->curl, CURLOPT_WRITEFUNCTION, maptile_writefunc);
  curl_multi_add_handle(curlm, hfb->curl);
  while(CURLM_CALL_MULTI_PERFORM==(curl_multi_socket_all(curlm,&running)));
  g_timeout_add(2000, curl_ping, NULL);
#endif
}

static void check_request_queue()
{
  while ((running_requests < MAX_HTTP_REQUESTS)&&(request_list)) {
    struct http_fetch_buf *hfb=(struct http_fetch_buf *)
      g_list_first(request_list)->data;
    request_list=g_list_remove_link(request_list,g_list_first(request_list));
    start_http_request(hfb);
  }
}


/* initiate a http request */
int get_http_file(const char *url,const char *filename,
		  void (*finish_cb)(const char *,const char*,void *),
		  void (*fail_cb)(const char *,const char *,void *),
		  int (*size_check)(const char *, void *, int),void *data)
{
  struct http_fetch_buf *hfb;
  
  if (!http_hash) {
    http_hash = g_hash_table_new(g_str_hash,g_str_equal);
  }
  if (g_hash_table_lookup(http_hash,url)) {
    return 0;
  }

 
  hfb=g_new0(struct http_fetch_buf,1);
  hfb->outfd=-1;

  hfb->url=g_strdup(url);
  hfb->fd=-1;
  hfb->finish_cb=finish_cb;
  hfb->fail_cb=fail_cb;
  hfb->size_check=size_check;
  hfb->data=data;
  if (filename) {
    hfb->filename=g_strdup(filename);
    hfb->use_tempname=0;
  } else {
    hfb->filename=g_strdup_printf("%s/mp.XXXXXX", g_get_tmp_dir());
    hfb->use_tempname=1;
  }
  g_hash_table_insert(http_hash,hfb->url,hfb);
  request_list=g_list_append(request_list,hfb);
  check_request_queue();
  return 1;
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
  p_new=geosec2pointstruct(lon,lat);
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
  p_new->time=0;
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
  x=x/(180.0)*M_PI;
  y=y/(180.0)*M_PI;
  if (globalmap.proj) {
    PJ_COORD c;
    c.lp.lam = x;
    c.lp.phi = y;
    c = proj_trans(globalmap.proj, PJ_FWD, c);
    x = c.xy.x;
    y = c.xy.y;
  } else {
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
    x=x*180.0;
    y=y*180.0;
  }
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
  if (globalmap.proj) {
    PJ_COORD c;
    c.xy.x = x;
    c.xy.y = y;
    c = proj_trans(globalmap.proj, PJ_INV, c);
    longg = c.lp.lam;
    latt = c.lp.phi;
    longg/=M_PI;
    latt/=M_PI;
    longg*=(180);
    latt*=(180);
  } else {
    y=y/(180);
    /* x=x-9331.3392; */
    x=x/(180);
    x*=M_PI;
    y*=M_PI;
	
    utmr=x;
    utmh=y;
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
    longg*=(180);
    latt*=(180);
    if (globalmap.is_utm)
      longg+=9;
  }
  *longr=longg;
  *lattr=latt;
}

/* callback for freeing a line struct */
static void free_line(gpointer data,gpointer user_data)
{
  free(data);
}


void free_line_list(GList *l)
{
  g_list_foreach(l,free_line,NULL);
  g_list_free(l);
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
      /* printf("entferne %s\n",ce->name); */
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
  p_new=geosec2pointstruct(nmea->lon,nmea->lat);
  p_new->time=nmea->time;
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
	      if (((tile_request_mode>1)&&(now-mtime>tile_request_mode))||
		  ((tile_request_mode==1)&&(!src)&&(!mtime))) {
		char url[512];
		if (get_mapfilename(url,sizeof(url),
				    map, map->url, x_page, y_page)) {
		  get_http_tile(mw,url,filename,0);
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
/*	  gtk_label_set_text(GTK_LABEL(mw->dlabel),filename); 
*/
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
	  time_t mtime;
	  time_t now=time(NULL);
	  int x_page=src_x/map->tilewidth;
	  int xoffset=src_x%map->tilewidth;
	  if (xoffset<0) {
	    x_page--;
	    
	    xoffset+=map->tilewidth;
	    
	  }
	  mtime=0;
	  snprintf(filename,sizeof(filename),
		   "karte%03d/%03d",y_page,x_page);
	  if (get_mapfilename(filename,sizeof(filename),
			      map,map->filepattern,x_page,y_page)) {

	    if ((src=load_image_mtime(filename,&mtime))) {
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
	    if (map->url) {
	      if (((tile_request_mode>1)&&(now-mtime>tile_request_mode))||
                  ((tile_request_mode==1)&&(!src)&&(!mtime))) {
                char url[512];
		if (get_mapfilename(url,sizeof(url),
                                    map, map->url, x_page, y_page)) {
                  get_http_tile(NULL,url,filename,1);
		}
	      }
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
  struct t_map *map;
  /*printf("zero latt_sec: %.3f zero long_sec: %.3f\n",globalmap.zerolatt,
	 globalmap.zerolong); */
/*  printf("zero latt_sec rad: %.6f zero long_sec rad: %.6f\n",y,x); */
  if (globalmap.proj) {
    globalmap.xoffset=0;
    globalmap.yoffset=0;
  } 
  else if (globalmap.is_utm) {
    double x,y;
    x=(globalmap.zerolong-9.0)/(180)*M_PI;
    y=globalmap.zerolatt/(180)*M_PI;
    globalmap.xoffset=asin(sin(x)*cos(y));
    globalmap.yoffset=M_PI_2-atan(cos(x)/tan(y));
    /*  printf("xoffset: %.6f, yoffset: %.6f\n",globalmap.xoffset, globalmap.yoffset); */
  } else {
    double x,y;
    x=globalmap.zerolong/(180)*M_PI;
    y=globalmap.zerolatt/(180)*M_PI;
    globalmap.xoffset=x;
    globalmap.yoffset=log(tan(y)+1/cos(y));
  }
  globalmap.xfactor=globalmap.orig_xfactor*(1<<(globalmap.zoomfactor-1))*3600;
  globalmap.yfactor=globalmap.orig_yfactor*(1<<(globalmap.zoomfactor-1))*3600;
  globalmap.zoomshift=0;
  /* globalmap.zoomfactor=1; */
  globalmap.zoomable=1;
  map=globalmap.first;
  while(map) {
    double x,y;
    if (!strstr(map->filepattern,"%Z")) {
     globalmap.zoomable=0;
    globalmap.xfactor=globalmap.orig_xfactor;
    globalmap.yfactor=globalmap.orig_yfactor;
    }
    geosec2point(&x,&y,map->reflong,map->reflatt);
    map->xoffset=map->refx-(int)x;
    map->yoffset=map->refy-(int)y;
    map=map->next;
  }
  if (globalmap.zoomable)
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
  int n,i;
  int x1,y1,x2,y2;
  int is_single=0;
  GList *l;
  struct t_punkt32 *p;
  write(fd,"1 0 1 setrgbcolor 3 setlinewidth\n",strlen("1 0 1 setrgbcolor 3 setlinewidth\n"));
  l=g_list_first(mark_line_list);
  n=g_list_length(l);
  if (n<2)
    return;
  l=g_list_first(l);
  p=(struct t_punkt32 *)l->data;
  l=g_list_next(l);
  x1=(p->x>>globalmap.zoomshift);
  y1=(p->y>>globalmap.zoomshift);
  for(i=1;i<n;i++) {
    p=(struct t_punkt32 *)l->data;
    x2=(p->x>>globalmap.zoomshift);
    y2=(p->y>>globalmap.zoomshift);
    if ((!p->single_point)&&(!p->start_new)&&(!is_single)) {  
      draw_line_ps(fd,mx,my,w,h,x1,y1,x2,y2);
    } else if (p->single_point) {
      draw_line_ps(fd,mx,my,w,h,x2-10,y2,x2+10,y2);
      draw_line_ps(fd,mx,my,w,h,x2,y2-10,x2,y2+10);
    }
    x1=x2;
    y1=y2;
    is_single=p->single_point;
    l=g_list_next(l);
  }

}
