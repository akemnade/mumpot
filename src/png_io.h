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

#ifndef K_PNG_IO_H
#define K_PNG_IO_H
/* pixmap info struct */
struct pixmap_info {
  unsigned char **row_pointers; /* bitmap data pointers, one pointer per row */
  unsigned char **row_mask_pointers;
  int num_palette; /* number of colors */
  unsigned long width;  
  unsigned long height;
  int row_len;        /* length of a row in bytes */
  int row_mask_len;
  unsigned int gdk_palette[256];  
  int bit_depth;
  int color_type;
  int interlace_type;
};
void save_pinfo(const char *filename,struct pixmap_info *pinfo);
void free_pinfo(struct pixmap_info *p_info);

struct pixmap_info *load_gfxfile(const char *filename);
/* read and write a pixel in a pixmap_info */
#define MY_GET_PIXEL(p,x,y) (((p)->row_pointers[y])[(x)])
#define MY_PUT_PIXEL(p,x,y,c) ((p)->row_pointers[y])[x]=(c)

#endif
