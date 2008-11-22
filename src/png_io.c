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

#include <png.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <jerror.h>
#include "png_io.h"

static struct pixmap_info *load_jpg_file(const char *filename);


/* free a pixmap info */

void free_pinfo(struct pixmap_info *p_info)
{
  free(p_info->row_pointers[0]);
  free(p_info->row_pointers);
  free(p_info);
}

/* save a png file from the pixmap info struct */


void save_pinfo(const char *filename,struct pixmap_info *pinfo)
{
  int i;
  FILE *fh; 
  png_color palette[256];
  png_structp png_ptr;
  png_infop info_ptr;
  /* convert the palette */
  if (pinfo->num_palette) {
    for (i=0;i<pinfo->num_palette;i++)
      {
	palette[i].red=pinfo->gdk_palette[i]>>16;
	palette[i].green=(pinfo->gdk_palette[i]>>8)&255;
	palette[i].blue=pinfo->gdk_palette[i]&255;
      }
  }
  fh=fopen(filename,"wb");
  if (!fh)
    return;
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                   NULL,NULL,NULL);
  if (!png_ptr)
    {
      fclose(fh);
      return;
    }
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    {
      fclose(fh);
      png_destroy_write_struct(&png_ptr,
			       (png_infopp)NULL);
      return;
    }
  if (setjmp(png_ptr->jmpbuf))
    {
      /* Free all of the memory associated with the png_ptr and info_ptr */
      png_destroy_write_struct(&png_ptr, &info_ptr);
      fclose(fh);
      /* If we get here, we had a problem reading the file */
      return ;
   }     
  /* intialize headers */
  png_init_io(png_ptr, fh);
  png_set_IHDR(png_ptr, info_ptr, pinfo->width, pinfo->height,
	       pinfo->color_type==PNG_COLOR_TYPE_RGB?
                   pinfo->bit_depth/3:pinfo->bit_depth,
	       pinfo->color_type, pinfo->interlace_type,
	       PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
  if (pinfo->num_palette)
    png_set_PLTE(png_ptr, info_ptr,palette,
		 pinfo->num_palette);
  png_write_info(png_ptr, info_ptr);
  png_set_packing(png_ptr);
  /* write the image */
  png_write_image(png_ptr, pinfo->row_pointers);
  png_write_end(png_ptr, info_ptr);
  png_destroy_write_struct(&png_ptr, &info_ptr);
  fclose(fh);


}

/* load a png file and store it in a pixmap_info struct */
struct pixmap_info *load_gfxfile(const char *filename)
{
  struct pixmap_info *pi_ret;
  struct pixmap_info pinfo;
  FILE *fh;

  png_structp png_ptr;
  png_infop info_ptr;

  int row;
  png_colorp palette;
 
  int num_channels;
     
  unsigned char header[8];
  pinfo.row_pointers=NULL;
  fh=fopen(filename,"rb");
  if (!fh) 
    return NULL;
  fread(header,1,8,fh);
  /* file type check */
  if (png_sig_cmp(header, 0,8))
    {
      fclose(fh);
      return load_jpg_file(filename);    
    }
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                   NULL,NULL,NULL);
  if (png_ptr == NULL)
    {
      fclose(fh);
      return NULL;
    }  
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL)
    {
      fclose(fh);
      png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
     
      return NULL;
    }
  if (setjmp(png_ptr->jmpbuf)) {
    /* Free all of the memory associated with the png_ptr and info_ptr */
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    fclose(fh);
    if (pinfo.row_pointers) {
      if (pinfo.row_pointers[0])
	free(pinfo.row_pointers[0]);
      free(pinfo.row_pointers);
    }
    /* If we get here, we had a problem reading the file */
    return NULL;
  }     
/* read png headers */
  png_init_io(png_ptr,fh);   
  png_set_sig_bytes(png_ptr, 8);   
  png_read_info(png_ptr, info_ptr);  
  png_get_IHDR(png_ptr, info_ptr, &pinfo.width, 
               &pinfo.height, &pinfo.bit_depth, 
               &pinfo.color_type,
               &pinfo.interlace_type, NULL, NULL);
  pinfo.num_palette=0;
  if (pinfo.color_type == PNG_COLOR_TYPE_PALETTE)
    {
      int i;
      /* we want 8 bit per pixel */
      if (pinfo.bit_depth < 8)
        png_set_packing(png_ptr);
      /* read and convert palette */
      png_get_PLTE(png_ptr, info_ptr, &palette,
                   &pinfo.num_palette);
      for (i=0;i<pinfo.num_palette;i++)
        {
          pinfo.gdk_palette[i]=(palette[i].red<<16)+
            (palette[i].green<<8)+palette[i].blue;
	  /* fprintf(stderr,"color %2d: %6x %4x\n",i,pinfo.gdk_palette[i],
		 (pinfo.gdk_palette[i]>>19<<11)+
		 (((pinfo.gdk_palette[i]>>10)&63)<<5)+
		 ((pinfo.gdk_palette[i]>>3)&31)); */
        }
    }
  num_channels=png_get_channels(png_ptr,info_ptr);
#if 0
  if (num_channels<4)
    png_set_bgr(png_ptr);
#endif
  if (pinfo.color_type == PNG_COLOR_TYPE_GRAY) {
    int i;
    if (  pinfo.bit_depth < 8)
      png_set_expand(png_ptr);       
    pinfo.num_palette=256;
    for(i=0;i<256;i++) {
      pinfo.gdk_palette[i]=(i<<16)+(i<<8)+i;
    }
  }
  /* tell libpng about our format wishes */
  png_read_update_info(png_ptr, info_ptr);
  if (pinfo.color_type==PNG_COLOR_TYPE_RGB)
    pinfo.bit_depth=24;
  else
    pinfo.bit_depth=8;
  /* alloc the bitmap memory */
  pinfo.row_pointers=malloc(sizeof(png_bytep)*pinfo.height);
  pinfo.row_len=(png_get_rowbytes(png_ptr,info_ptr)+3)&(~3);
  pinfo.row_pointers[0]=malloc(pinfo.height*pinfo.row_len);
  for (row = 1; row < pinfo.height; row++)
    {
      pinfo.row_pointers[row] = pinfo.row_pointers[row-1]+pinfo.row_len;
    }  
  /* read the whole image */
  png_read_image(png_ptr, pinfo.row_pointers); 
  png_read_end(png_ptr, info_ptr);  
  png_destroy_read_struct(&png_ptr, &info_ptr,
			  NULL);
  fclose(fh);
  pi_ret=(struct pixmap_info *)malloc(sizeof(struct pixmap_info));
  memcpy(pi_ret,&pinfo,sizeof(struct pixmap_info));
  return pi_ret;
  
}

static void print_jpeg_error(j_common_ptr jpeg_info, int level)
{
  char message[JMSG_LENGTH_MAX];
  (jpeg_info->err->format_message)(jpeg_info,message);
  fprintf(stderr,"jpeg error: %s\n",message);
  
}


struct my_jpg_err_mgr {
  struct jpeg_error_mgr mgr;
  jmp_buf jmpbuf;
};

static void jpeg_errorhandler(j_common_ptr jpeg_info)
{
  print_jpeg_error(jpeg_info,0);
  longjmp(((struct my_jpg_err_mgr *)jpeg_info->err)->jmpbuf,1);
  /* exit(1); */
}


static struct pixmap_info *load_jpg_file(const char *filename)
{
  struct jpeg_decompress_struct
	                jpeg_info;
  struct my_jpg_err_mgr
                  jpeg_error;
  int i,row;
  struct pixmap_info *pi_ret;
  struct pixmap_info pinfo;
  FILE *f=fopen(filename,"rb");
  if (!f) {
    return NULL;
  }
  jpeg_info.err=jpeg_std_error(&jpeg_error.mgr);
  /* jpeg_info.err->emit_message=(void (*)(j_common_ptr,int)) print_jpeg_error; */
  jpeg_info.err->error_exit=(void (*)(j_common_ptr)) jpeg_errorhandler;
  if (setjmp(jpeg_error.jmpbuf)) {
    jpeg_destroy_decompress(&jpeg_info);
    fclose(f);
    return NULL;
  }
  jpeg_create_decompress(&jpeg_info);
  jpeg_stdio_src(&jpeg_info,f);
  jpeg_read_header(&jpeg_info,1);
  jpeg_start_decompress(&jpeg_info);
  pinfo.num_palette=0;
  if (jpeg_info.output_components==1) {
        pinfo.num_palette=256;
    for(i=0;i<256;i++) {
      pinfo.gdk_palette[i]=(i<<16)+(i<<8)+i;
    }
    pinfo.bit_depth=8;
  } else {
    pinfo.bit_depth=24;
  }
 fprintf(stderr,"reading %s\n",filename);
  pinfo.color_type=PNG_COLOR_TYPE_RGB;
  pinfo.interlace_type=0;
  pinfo.height=jpeg_info.output_height;
  pinfo.width=jpeg_info.output_width;
  pinfo.row_len=jpeg_info.output_width*jpeg_info.output_components;
  pinfo.row_pointers=malloc(sizeof(png_bytep)*pinfo.height);
  pinfo.row_pointers[0]=malloc(pinfo.height*pinfo.row_len);
  for (row = 1; row < pinfo.height; row++)
    { 
      pinfo.row_pointers[row] = pinfo.row_pointers[row-1]+pinfo.row_len;
    }
  while(jpeg_info.output_scanline <pinfo.height) {
  jpeg_read_scanlines(&jpeg_info,pinfo.row_pointers+jpeg_info.output_scanline,pinfo.height-jpeg_info.output_scanline);
  }
  (void) jpeg_finish_decompress(&jpeg_info);
  jpeg_destroy_decompress(&jpeg_info);
  fclose(f);
  pi_ret=(struct pixmap_info *)malloc(sizeof(struct pixmap_info));
  memcpy(pi_ret,&pinfo,sizeof(struct pixmap_info));
  return pi_ret;

}
