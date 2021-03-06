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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <gtk/gtk.h>
#include <png.h>

#include "png_io.h"

GtkWidget *scrwin;
GtkWidget *koord_label;
GtkWidget *mainwin;
GtkWidget *dr_area;
GtkAdjustment *zoom_adj;
struct pixmap_info *my_pinfo;
int allow_redraw=1;
int zoom_fac=1;
int zoomout_fac=1;
void draw_pinfo(GdkWindow *w, GdkGC *gc,struct pixmap_info *p_info,
                int srcx,int srcy, int destx, int desty,
                int width, int height)
{
  if (p_info->num_palette) {	    
  guchar *bitbuf=p_info->row_pointers[srcy]+srcx;
  GdkRgbCmap* cmap=gdk_rgb_cmap_new(p_info->gdk_palette,
                                    p_info->num_palette);
  if ((srcx>my_pinfo->width)||(srcy>my_pinfo->height))
    return;
  if (srcx+width>my_pinfo->width) 
    width=my_pinfo->width-srcx;
  if (srcy+height>my_pinfo->height)
    height=my_pinfo->height-srcy;
  gdk_draw_indexed_image(w,gc,destx,desty,width,height,
                         GDK_RGB_DITHER_NONE,
                         bitbuf,p_info->row_len,
                         cmap);
  gdk_rgb_cmap_free(cmap);
  } else {
     guchar *bitbuf=p_info->row_pointers[srcy]+3*srcx;
     gdk_draw_rgb_image(w,gc,destx,desty,width,height,
                        GDK_RGB_DITHER_NONE,bitbuf,
                       p_info->row_len);

  }
}

void draw_pinfo_zoomed(GdkWindow *w, GdkGC *gc,struct pixmap_info *p_info,
		       int srcx,int srcy, int destx, int desty,
		       int width, int height)
{
  int x,y,xn;
  int i;
  guchar *row_old;
  guchar *row_new;
  guchar *bitbuf_new; 

  GdkRgbCmap* cmap;
  width+=destx%zoom_fac;
  height+=desty%zoom_fac;
  destx-=(destx%zoom_fac);
  desty-=(desty%zoom_fac);
  width=(zoomout_fac*(width+zoom_fac-1))/zoom_fac;
  height=(zoomout_fac*(height+zoom_fac-1))/zoom_fac;
  if (srcx+width>my_pinfo->width) {
    width=my_pinfo->width-srcx; 
  }
  if (width<0) 
    return;
  if (srcy+height>my_pinfo->height) {
    height=my_pinfo->height-srcy;
  }
  if (height<0)
    return;
  bitbuf_new=malloc(width*height*zoom_fac*zoom_fac*p_info->bit_depth/8);
  row_new=bitbuf_new;
  for(y=0;y<height;y++) {
    row_old=my_pinfo->row_pointers[srcy+y]+srcx*p_info->bit_depth/8;
    if ((y%zoomout_fac))
      continue;
    if (p_info->bit_depth==8) { 
      if (zoomout_fac<=1) {
        for(x=0,xn=0;x<width;x++) {
          guchar b;
          b=row_old[x];
          for(i=0;i<zoom_fac;i++,xn++) {
       	    row_new[xn]=b;
          }
        }
      } else {
        for(x=0,xn=0,i=1;x<width;x++,i++) {
          if (i==zoomout_fac) {
            i=0;
          }
          if (0==i) {
            row_new[xn]=row_old[x];
            xn++;
          }
        }
      }
    } else if (p_info->bit_depth==24) {
      int rowlen=width*3;
      if (zoomout_fac<=1) {
        for(x=0,xn=0;x<rowlen;x+=3) {
          guchar b[3];
          b[0]=row_old[x];
          b[1]=row_old[x+1];
          b[2]=row_old[x+2];
          for(i=0;i<zoom_fac;i++,xn+=3) {
            row_new[xn]=b[0];
            row_new[xn+1]=b[1];
            row_new[xn+2]=b[2];
          }
        } 
      } else {
        for(x=0,xn=0,i=1;x<rowlen;x+=3,i++) {
          if (i==zoomout_fac) {
            i=0;
          }
          if (0==i) {
            row_new[xn]=row_old[x];
            row_new[xn+1]=row_old[x+1];
            row_new[xn+2]=row_old[x+2];
            xn+=3;
          }
        }
      }
    }
    row_new+=width*zoom_fac/zoomout_fac*p_info->bit_depth/8;
    for(i=1;i<zoom_fac;i++,row_new+=width*zoom_fac*p_info->bit_depth/8) {
      memcpy(row_new,row_new-width*zoom_fac*p_info->bit_depth/8,width*zoom_fac*p_info->bit_depth/8);
    }
  } 
  if (p_info->num_palette) {
  cmap=gdk_rgb_cmap_new(p_info->gdk_palette,
				    p_info->num_palette);
  gdk_draw_indexed_image(w,gc,destx,desty,width*zoom_fac/zoomout_fac,height*zoom_fac/zoomout_fac,
                         GDK_RGB_DITHER_NONE,
                         bitbuf_new,width*zoom_fac/zoomout_fac,
                         cmap);
  gdk_rgb_cmap_free(cmap);
  } else {
    gdk_draw_rgb_image(w,gc,destx,desty,width*zoom_fac/zoomout_fac,height*zoom_fac/zoomout_fac,
                       GDK_RGB_DITHER_NONE,bitbuf_new,
                       width*zoom_fac/zoomout_fac*p_info->bit_depth/8);
  }
  free(bitbuf_new);
}

static gboolean mouse_move(GtkWidget *w, GdkEventMotion *event, 
			   gpointer user_data)
{
  char buf[80];
  snprintf(buf,sizeof(buf),"x: %-4d y: %-4d %d/%dx",(int)event->x/zoom_fac*zoomout_fac,
	   (int)event->y/zoom_fac*zoomout_fac,zoom_fac,zoomout_fac);
  gtk_label_set_text(GTK_LABEL(koord_label),buf);
  return TRUE;
}

static int expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  GdkGC *mygc;
  if (!allow_redraw)
    return TRUE;
  gdk_window_clear_area (widget->window,
                         event->area.x, event->area.y,
                         event->area.width, event->area.height);
  mygc=widget->style->fg_gc[widget->state];
  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state],
                             &event->area);
  if ((zoom_fac==1)&&(zoomout_fac==1)) {
    draw_pinfo(widget->window,mygc,my_pinfo,event->area.x,event->area.y,
	       event->area.x,event->area.y,
	       event->area.width,event->area.height);
  } else {
    draw_pinfo_zoomed(widget->window,mygc,my_pinfo,event->area.x*zoomout_fac/zoom_fac,
	       event->area.y*zoomout_fac/zoom_fac,
	       event->area.x,event->area.y,
	       event->area.width,event->area.height);
  }
  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state],
                             NULL);

  return TRUE;
}

static void set_zoom(int mx, int my) {
  GtkAdjustment *hadj;
  GtkAdjustment *vadj;
  allow_redraw=0;

  hadj=gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrwin));
  vadj=gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrwin));
  gtk_widget_set_usize(dr_area,
		       my_pinfo->width/zoomout_fac*zoom_fac,
		       my_pinfo->height/zoomout_fac*zoom_fac);
  if ((mx>=0)&&(my>=0)) {
    gtk_adjustment_set_value(hadj,mx*zoom_fac/zoomout_fac-hadj->page_size/2);
    gtk_adjustment_set_value(vadj,my*zoom_fac/zoomout_fac-vadj->page_size/2);
  }
  allow_redraw=1;
  gtk_widget_queue_draw(dr_area);
}

gboolean mouse_click(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  int mx;
  int my;
  mx=event->x*zoomout_fac/zoom_fac;
  my=event->y*zoomout_fac/zoom_fac;
  if (event->button==1) {
    if (zoomout_fac>1)
      zoomout_fac--;
    else
      zoom_fac++;
  }
  if ((event->button==3)) {
    if (zoom_fac>1)
      zoom_fac--;
    else
      zoomout_fac++;
  }
  set_zoom(mx,my);
  return TRUE;
}


int main(int argc, char **argv)
{
  int events;
  GtkWidget *box;
  gtk_init(&argc,&argv);
  gdk_rgb_init();
  if (argc<2) {
    printf("Usage: %s pngfile\n",argv[0]);
    return 1;
  }
  my_pinfo=load_gfxfile(argv[1]);
  if (!my_pinfo) {
    printf("Cannot open %s\n",argv[1]);
    return 1;
  }
  mainwin=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_usize(mainwin,200,200);
  box=gtk_vbox_new(FALSE,0);
  gtk_container_add(GTK_CONTAINER(mainwin),box);
  scrwin=gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				GTK_POLICY_AUTOMATIC,
				GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(box),scrwin,TRUE,TRUE,0);
  koord_label=gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(box),koord_label,FALSE,FALSE,0);
  dr_area=gtk_drawing_area_new();
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrwin),
					dr_area);
  gtk_widget_set_usize(dr_area,my_pinfo->width,my_pinfo->height);
  gtk_widget_show(dr_area);
  gtk_signal_connect(GTK_OBJECT(mainwin),"destroy",
		     GTK_SIGNAL_FUNC(gtk_main_quit),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(dr_area),"expose_event",
                     GTK_SIGNAL_FUNC(expose_event),NULL);
  gtk_widget_realize(dr_area);
  events=(int)gdk_window_get_events(dr_area->window);
  gdk_window_set_events(dr_area->window,
			(GdkEventMask )(events+GDK_BUTTON_PRESS_MASK
					+GDK_BUTTON_RELEASE_MASK
					+GDK_POINTER_MOTION_MASK));
  gtk_signal_connect(GTK_OBJECT(dr_area),"motion_notify_event",
		     GTK_SIGNAL_FUNC(mouse_move),NULL);
  gtk_signal_connect(GTK_OBJECT(dr_area),"button_press_event",
		     GTK_SIGNAL_FUNC(mouse_click),NULL);
  gtk_widget_show_all(mainwin);
  {
    int w,h;
    gdk_window_get_size(mainwin->window,&w,&h);
    w=(my_pinfo->width+w-1)/w; 
    h=(my_pinfo->height+h-1)/h;
    if (w>h)
      h=w;
    zoomout_fac=h;
    set_zoom(-1,-1);
  }
  gtk_main();
  return 0;
}
