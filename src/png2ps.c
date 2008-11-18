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

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <png.h>
#include "png_io.h"

static char hexcode[512]="000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

static char *Ascii85Tuple(unsigned char *data)
{
  static char
    tuple[6];

  register long
    i,
    x;

  unsigned int
                code;

  unsigned long
                quantum;

  code=(((data[0] << 8) | data[1]) << 16) | (data[2] << 8) | data[3];
  if (code == 0L)
    {
      tuple[0]='z';
      tuple[1]='\0';
      return(tuple);
    } 
  quantum=85L*85L*85L*85L;
  for (i=0; i < 4; i++)
  {
    x=code/quantum;
    code-=quantum*x;
    tuple[i]='!'+x;
    quantum/=85L;
    
  }
  tuple[4]='!'+(code % 85L);
  tuple[5]='\0';
  return(tuple);
}

static void ascii85write(char *row_buf, int len,char *buf, int fd)
{
  int j,l;
  char tmp[4];
  char *q;
  for(j=0,l=0;j+4<len;j+=4) {
    q=Ascii85Tuple((unsigned char *)row_buf+j);
    strncpy(buf+l,q,5);
    l+=strlen(q);
    
  }
  memset(tmp,0,4);
  memcpy(tmp,row_buf+j,len-j);
  q=Ascii85Tuple((unsigned char *)tmp);
  if (q[0]=='z') {
    memcpy(buf+l,"!!!!!",5);
    l+=5;
  } else { 
    strncpy(buf+l,q,5);
    l+=strlen(q);
  }
  buf[l]='~';
  l++;
  buf[l]='>';
  l++;
  buf[l]='\n';
  l++;
  write(fd,buf,l);
}

void colordump_ps(int fd,struct pixmap_info *pinfo, int outformat)
{
  char *buf;
  unsigned char *row_buf;
  int i,j,k;
  unsigned char red_pal[256];
  unsigned char green_pal[255];
  unsigned char blue_pal[255];
  unsigned char *pal[]={red_pal,green_pal,blue_pal};
  row_buf=malloc(pinfo->width*4);
  buf=malloc(65536);
  /*  snprintf(buf,65535,"1 dict begin /rstr %lu string def /gstr %lu string def /bstr %lu string def\n%lu %lu 8 [%lu 0 0 %lu neg 0 %lu]\n"
      "{currentfile pstr readhexstring pop} image\n",1,pinfo->width,pinfo->height,pinfo->width,pinfo->height,pinfo->height); */
  if (outformat) {
    snprintf(buf,65535,"1 dict begin /rstr %lu string def /gstr %lu string def /bstr %lu string def\n%lu %lu 8 [1 0 0 -1 0 0]\n"
	     "{currentfile /ASCII85Decode filter rstr readstring pop}\n"
	     "{currentfile /ASCII85Decode filter gstr readstring pop}\n"
	     "{currentfile /ASCII85Decode filter bstr readstring pop} true 3 colorimage\n",
	     pinfo->width,pinfo->width,pinfo->width,pinfo->width,pinfo->height); 
  } else {
    snprintf(buf,65535,"1 dict begin /rstr %lu string def /gstr %lu string def /bstr %lu string def\n%lu %lu 8 [1 0 0 -1 0 0]\n"
	     "{currentfile rstr readhexstring pop}\n"
	     "{currentfile gstr readhexstring pop}\n"
	     "{currentfile bstr readhexstring pop} true 3 colorimage\n",
	     pinfo->width,pinfo->width,pinfo->width,pinfo->width,pinfo->height); 
  }
  write(fd,buf,strlen(buf));
  for(i=0;i<pinfo->num_palette;i++) {
    red_pal[i]=pinfo->gdk_palette[i]>>16;
    green_pal[i]=(pinfo->gdk_palette[i]>>8)&255;
    blue_pal[i]=pinfo->gdk_palette[i]&255;
  }
  buf[pinfo->width*2]='\n';
  for(i=0;i<pinfo->height;i++) { 
    unsigned char *p=pinfo->row_pointers[i];
    for(k=0;k<3;k++) {
      if (pinfo->num_palette) {
	unsigned char *cur_pal=pal[k];
	for(j=0;j<pinfo->width;j++) {
	  row_buf[j]=cur_pal[p[j]];
	}
      } else {
	for(j=0;j<pinfo->width;j++) {
	  row_buf[j]=p[j*pinfo->bit_depth/8+k];
	}
      }
      if (outformat&1) {
	ascii85write((char *)row_buf,pinfo->width,buf,fd);
      } else {
	for(j=0;j<pinfo->width;j++) {
	  ((short *)buf)[j]=((short *)hexcode)[row_buf[j]];
	}
	write(fd,buf,pinfo->width*2+1);
      }
    }
  }
  write(fd,"end\n",4);
  free(buf);
  free(row_buf);
}  

#ifdef STANDALONE
int main(int argc, char **argv)
{
  char buf[256];
  char *extro="grestore showpage\n%%EOF";
  if (argc==3) {
    
    struct pixmap_info *pinfo=load_gfxfile(argv[1]);
    if (!pinfo)
      return 1;
    snprintf(buf,sizeof(buf),"%%!PS-Adobe-2.0 EPSF-2.0\n%%%%BoundingBox: 0 0 %lu %lu\ngsave 0 %lu translate\n",pinfo->width,pinfo->height,pinfo->height);
    write(1,buf,strlen(buf));
    colordump_ps(1,pinfo,atoi(argv[2]));
    write(1,extro,strlen(extro));
    return 0;
  }
}
#endif
