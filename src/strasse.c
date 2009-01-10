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
#include <unistd.h>
#ifdef _WIN32
#include <winsock.h>
#else
#include <netinet/in.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include "strasse.h"
#define MAP_TILE_WIDTH 1518
#define MAP_TILE_HEIGHT 1032
#define MY_ABS(x) ((x)>0?(x):(-(x)))

struct strfile_listentry {
  struct strasse **str_list;
  struct kreuzung **kr_list;
  int *kr_last_str;
  int *kr_len;
  int str_count;
  int kr_count;
};
#define ALL_STR_COUNT 65536
static struct strfile_listentry str_list_all[ALL_STR_COUNT];
static GHashTable *file_hash;

int write_strasse(int fd, struct strasse *str)
{
  char buf[64];
  int i,j;
  short *bufs=(short *)buf;
  int *bufi=(int *)buf;
  bufi[0]=htonl(str->id);
  bufi[1]=htonl(str->anfang_krid);
  bufi[2]=htonl(str->end_krid);
  bufs[6]=htons(str->len);
  bufs[7]=htons(str->anz);
  bufs[8]=htons(str->anfang.x);
  bufs[9]=htons(str->anfang.y);
  bufs[10]=htons(str->ende.x);
  bufs[11]=htons(str->ende.y);
  buf[24]=str->r;
  buf[25]=str->g;
  buf[26]=str->b;
  if (write(fd,buf,27)!=27)
    return -1;
  for(i=0,j=0;i<str->anz;i++,j++) {
    if (j==sizeof(buf)/4) {
      if (write(fd,buf,sizeof(buf))!=sizeof(buf))
	return -1;
      j=0;
    }
    bufs[2*j]=htons(str->punkte[i].x);
    bufs[2*j+1]=htons(str->punkte[i].y);
  }
  if ((j)&&(write(fd,buf,j*4)==j*4))
    return -1;
  return 0;
}

struct strasse *read_strasse(int fd)
{
  int i;
  int n;
  char buf[64];
  short *bufs=(short *)buf;
  int *bufi=(int *)buf;
  struct strasse *str;
  if (read(fd,buf,27)!=27)
    return NULL;
  n=ntohs(bufs[6]);
  str=malloc(sizeof(struct strasse)+n*sizeof(struct punkt));
  str->id=ntohl(bufi[0]);
  str->anfang_krid=ntohl(bufi[1]);
  str->end_krid=ntohl(bufi[2]);
  str->len=ntohs(bufs[6]);
  str->anz=ntohs(bufs[7]);
  str->anfang.x=ntohs(bufs[8]);
  str->anfang.y=ntohs(bufs[9]);
  str->ende.x=ntohs(bufs[10]);
  str->ende.y=ntohs(bufs[11]);
  str->r=buf[24];
  str->g=buf[25];
  str->b=buf[26];
  if (str->anz<0)
    return NULL;
  str->punkte=(struct punkt *)(&str[1]);
  read(fd,str->punkte,sizeof(struct punkt)*str->anz);
  for(i=0;i<str->anz;i++) {
    str->punkte[i].x=ntohs(str->punkte[i].x);
    str->punkte[i].y=ntohs(str->punkte[i].y);
  }
  return str;
}

void read_str_file(char *name,char *key)
{
  int entry_num;
  struct kreuzung **kr_list_new;
  struct strasse **str_list_new;
  char *key_dup;
  short l;
  int i;
  int fd;
  if (!name)
    return;
  if (!file_hash) {
    file_hash=g_hash_table_new(g_str_hash,g_str_equal);
  }
  if (g_hash_table_lookup(file_hash,key))
    return;
  fd=open(name,O_RDONLY);
  if (fd<0)
    return;
  key_dup=strdup(key);
  g_hash_table_insert(file_hash,key_dup,key_dup);
  if (read(fd,&l,2)!=2)
    return;
  l=ntohs(l);
  if (l==0)
    return;
  str_list_new=malloc(l*sizeof(struct strassen *));
  for(i=0;i<l;i++) {
    if ((str_list_new[i]=read_strasse(fd))==NULL) {
      l=i;
      break;
    }
  }
  if (l==0) {
    free(str_list_new);
    close(fd);
    return;
  }
  entry_num=str_list_new[0]->id>>16;
  str_list_all[entry_num].str_list=str_list_new;
  str_list_all[entry_num].str_count=l;
  if (read(fd,&l,2)==2) {
    l=ntohs(l);
    kr_list_new=malloc(l*sizeof(struct kreuzung *));
    for(i=0;i<l;i++) {
      if ((kr_list_new[i]=read_kreuzung(fd))==NULL) {
	l=i;
	break;
      }
    }
    str_list_all[entry_num].kr_list=kr_list_new;
    str_list_all[entry_num].kr_count=l;
  }
  close(fd);
}

struct strasse *get_str_id(int strid)
{
  if (str_list_all[strid>>16].str_list) {
    return str_list_all[strid>>16].str_list[strid&65535];
  }
  return NULL;
}

struct kreuzung *get_kr_id(int krid) 
{
  if (str_list_all[krid>>16].kr_list) {
    return str_list_all[krid>>16].kr_list[krid&65535];
  }
  return NULL;
}

void reset_way_info()
{
  int i;
  for(i=0;i<65536;i++) {
    if (str_list_all[i].kr_last_str)
      free(str_list_all[i].kr_last_str);
    str_list_all[i].kr_last_str=NULL;
    str_list_all[i].kr_len=NULL;
  }
}

void put_way_info(int krid, int len, int last_str) 
{
  struct strfile_listentry *le;
  le=&str_list_all[krid>>16];
  krid&=65535;
  if (!le->kr_last_str) {
    if (le->kr_count<=0)
      return;
    le->kr_last_str=malloc(sizeof(int)*2*le->kr_count);
    memset(le->kr_last_str,-1,sizeof(int)*2*le->kr_count);
    le->kr_len=le->kr_last_str+le->kr_count;
  }
  le->kr_last_str[krid]=last_str;
  le->kr_len[krid]=len;
}

void get_way_info(int krid, int *len, int *last_str)
{
  struct strfile_listentry *le;
  le=&str_list_all[krid>>16];
  krid&=65535;
  if (!le->kr_last_str) {
    *len=-1;
    *last_str=-1;
  } else {
    *last_str=le->kr_last_str[krid];
    *len=le->kr_len[krid];
  }
}

int write_kreuzung(int fd, struct kreuzung *k)
{
  char buf[64];
  unsigned char tmp;
  int i;
  /* short *bufs=(short *)buf; */
  int *bufi=(int *)buf;
  bufi[0]=htonl(k->id);
  buf[4]=k->n;
  tmp=0;
  for(i=0;i<k->n;i++) {
    if (k->r[i])
      tmp|=(1<<i);
  }
  buf[5]=tmp;
  for(i=0;i<k->n;i++)
    bufi[i+2]=htonl(k->abzweigungen[i]);
  if (write(fd,buf,k->n*4+8)!=(k->n*4+8))
    return -1;
  return 0;
}

struct kreuzung *read_kreuzung(int fd)
{
  struct kreuzung *k;
  char buf[64];
  int i;
  /*  short *bufs=(short *)buf; */
  int *bufi=(int *)buf;
  k=malloc(sizeof(struct kreuzung));
  read(fd,buf,8);
  k->n=buf[4];
  k->id=ntohl(bufi[0]);
  read(fd,buf+8,k->n*4);
  for(i=0;i<k->n;i++) {
    k->r[i]=(buf[5]&(1<<i))?1:0;
    k->abzweigungen[i]=ntohl(bufi[i+2]);
  }
  return k;
}


static int abst_str(struct strasse *str, int x, int y, int *is_anfang)
{
  int d1,d2;
  int x0,y0;
  x0=str->anfang.x;
  y0=str->anfang.y;
  x0-=x;
  y0-=y;
  x0=MY_ABS(x0);
  y0=MY_ABS(y0);
  if (x0>20000)
    return 1<<30;
  if (y0>20000)
    return 1<<30;
  d1=x0*x0+y0*y0;
  x0=str->ende.x;
  y0=str->ende.y;
  x0-=x;
  y0-=y;
  x0=MY_ABS(x0);
  y0=MY_ABS(y0);
  d2=x0*x0+y0*y0;
  if (d2<d1) {
    *is_anfang=0;
    return d2;
  } else {
    *is_anfang=1;
    return d1;
  }
}

struct strasse *suche_naechste_str(int *x_ret, int *y_ret, int *is_anfang) 
{
  int i;
  int mindest,t;
  int strid_base;
  int str_count;
  int x=*x_ret;
  int y=*y_ret;
  struct strasse *beststr;
  struct strasse **str_list;
  strid_base=((x/MAP_TILE_WIDTH)<<8)+y/MAP_TILE_HEIGHT;
  if (strid_base > ALL_STR_COUNT)
      return NULL;
  str_list=str_list_all[strid_base].str_list;
  str_count=str_list_all[strid_base].str_count;
  mindest=1<<30;
  beststr=NULL;
  x=x%MAP_TILE_WIDTH;
  y=y%MAP_TILE_HEIGHT;
  for(i=0;i<str_count;i++) {
    int is_anf=0; /* silence gcc */
    t=abst_str(str_list[i],x,y,&is_anf);
    if (t<mindest) {
      beststr=str_list[i];
      mindest=t;
      *is_anfang=is_anf;
    }
  }
  if (beststr==NULL)
    return NULL;
  if (*is_anfang) {
    x=beststr->anfang.x;
    y=beststr->anfang.y;
  } else {
    x=beststr->ende.x;
    y=beststr->ende.y;
  }
  x=x+(MAP_TILE_WIDTH*(beststr->id>>24));
  y=y+(MAP_TILE_HEIGHT*((beststr->id>>16)&255));
  *x_ret=x;
  *y_ret=y;
  return beststr;
}

void str_to_mark_list(void (*add_pkt_to_list)(GList **,int,int),
		      GList **l,struct strasse *str,int is_anfang)
{
  int i;
  int xoffset,yoffset;
  xoffset=(str->id>>24)&255;
  yoffset=(str->id>>16)&255;
  xoffset*=MAP_TILE_WIDTH;
  yoffset*=MAP_TILE_HEIGHT;
  if (is_anfang) {
    add_pkt_to_list(l,xoffset+str->anfang.x,yoffset+str->anfang.y);
    for(i=0;i<str->anz;i++)
      add_pkt_to_list(l,xoffset+str->punkte[i].x,
		      yoffset+str->punkte[i].y);
    add_pkt_to_list(l,xoffset+str->ende.x,
		    yoffset+str->ende.y);
  } else {
    add_pkt_to_list(l,xoffset+str->ende.x,
		    yoffset+str->ende.y);
    for(i=str->anz-1;i>=0;i--)
      add_pkt_to_list(l,xoffset+str->punkte[i].x,yoffset+str->punkte[i].y);
    add_pkt_to_list(l,xoffset+str->anfang.x,yoffset+str->anfang.y);
  }
}
