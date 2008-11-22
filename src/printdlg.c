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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include "myintl.h"
#define DEFAULTPRINTER _("Default printer")

struct printdlg_t {
  GtkWidget *win;
  GtkWidget *startnum;
  GtkWidget *endnum;
  GtkWidget *printsel;
  GtkWidget *printercheck;
  GtkWidget *filenamecheck;
  GtkWidget *commandcheck;
  GtkWidget *command;
  GtkWidget *filename;
  GtkWidget *pagerange;
  GtkWidget *allpages;
  int num_pages;
  void (*ok_cb)(void *,int,int,int);
  void (*cancel_cb)(void *);
  void *data;
  
  /*  GtkWidget *singlepage; */
};

static GList *get_printer_list()
{
  GList *l = g_list_append(NULL, g_strdup(DEFAULTPRINTER));
  char buf[256];
  FILE *f = popen("lpstat -a","r");
  while(fgets(buf, sizeof(buf), f)) {
    buf[strcspn(buf," \t")]=0;
    l=g_list_append(l,g_strdup(buf));
  }
  pclose(f);
  return l;
}


static void free_printer_list_item(gpointer data, gpointer bla)
{
  g_free(data);
}

static void printdlg_ok(GtkWidget *w, gpointer data)
{
  int fd=-1;
  struct printdlg_t *pd = (struct printdlg_t *)data;
  int start_page;
  int end_page;
  char *tmp;
  tmp=gtk_editable_get_chars(GTK_EDITABLE(pd->startnum),0,-1);
  start_page=atoi(tmp);
  g_free(tmp);
  tmp=gtk_editable_get_chars(GTK_EDITABLE(pd->endnum),0,-1);
  end_page=atoi(tmp);
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pd->allpages))) {
    start_page=1;
    end_page=pd->num_pages;
  }
  if ((start_page>pd->num_pages) ||
      (start_page<1) ||
      (end_page >pd->num_pages) ||
      (end_page<start_page)) 
    return;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pd->filenamecheck))) {
    tmp=gtk_editable_get_chars(GTK_EDITABLE(pd->filename),0,-1);
    fd=open(tmp,O_CREAT|O_TRUNC|O_WRONLY,0666);
    
  } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pd->printercheck))) {
#ifndef _WIN32
    char printdest[256];
    int lppipe[2];
    int pid;
    tmp=gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(pd->printsel)->entry),0,-1);
    printdest[0]=0;
    if (strcmp(tmp,DEFAULTPRINTER)) {
      snprintf(printdest,sizeof(printdest),"-d%s",tmp);
    }
    free(tmp);
    pipe(lppipe);
    pid=fork();
    if (pid<0) {
      close(lppipe[0]);
      close(lppipe[1]);
    } else {
      if (pid==0) {
	char *args[4];
	args[0]="lp";
	args[1]="-s";
	args[3]=NULL;
	if (printdest[0]) {
	  args[2]=printdest;
	} else {
	  args[2]=NULL;
	}
	dup2(lppipe[0],0);
	close(lppipe[1]);
	execvp("lp",args);
	_exit(127);
      }
      close(lppipe[0]);
      fd=lppipe[1];
    } 
#endif
  } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pd->commandcheck))) {
#ifndef _WIN32
    int lppipe[2];
    int pid;
    tmp=gtk_editable_get_chars(GTK_EDITABLE(pd->command),0,-1);
    pipe(lppipe);
    pid=fork();
    if (pid==0) {
      char *args[]={"sh","-c",NULL,NULL};
      args[2]=tmp;
      dup2(lppipe[0],0);
      close(lppipe[1]);
      execvp("lp",args);
      _exit(127);
    }
    close(lppipe[0]);
    fd=lppipe[1];
#endif
  }
  if (fd>0) {
    pd->ok_cb(pd->data, start_page, end_page, fd);
    close(fd);
    gtk_widget_destroy(pd->win);
    free(pd);
  }
  
}

static void close_printdlg(GtkWidget *w, gpointer data)
{
  struct printdlg_t *pd = (struct printdlg_t *)data;
  gtk_widget_destroy(pd->win);
  if (pd->cancel_cb)
    pd->cancel_cb(pd->data);
  free(pd);
  
}

struct printdlg_t *create_printdlg(int numpages, void (*cancel_cb)(void *), 
				   void (*ok_cb)(void *,int,int,int), void *data)
{
  GtkWidget *but;
  GtkWidget *label;
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GList *plist;
  struct printdlg_t *pd = (struct printdlg_t *)malloc(sizeof(struct printdlg_t));
  pd->ok_cb=ok_cb;
  pd->cancel_cb=cancel_cb;
  pd->data=data;
  pd->num_pages=numpages;
  pd->win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(pd->win),_("Print"));
  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_add(GTK_CONTAINER(pd->win),vbox);
  hbox=gtk_hbox_new(FALSE,0);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  pd->printercheck=gtk_radio_button_new_with_label(NULL, _("to printer: "));
  gtk_box_pack_start(GTK_BOX(hbox),pd->printercheck,TRUE,TRUE,0);
  pd->printsel=gtk_combo_new();
  gtk_box_pack_start(GTK_BOX(hbox),pd->printsel,FALSE,FALSE,0);
  gtk_combo_set_value_in_list(GTK_COMBO(pd->printsel), TRUE, FALSE);
  plist=get_printer_list();
  gtk_combo_set_popdown_strings(GTK_COMBO(pd->printsel), plist);
  g_list_foreach(plist,free_printer_list_item,NULL);
  g_list_free(plist);
  hbox=gtk_hbox_new(FALSE,0);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  pd->filenamecheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->printercheck),_("Print to file: "));
  gtk_box_pack_start(GTK_BOX(hbox),pd->filenamecheck,FALSE,FALSE,0);
  pd->filename=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox),pd->filename,TRUE,TRUE,0);
  hbox=gtk_hbox_new(FALSE,0);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  pd->commandcheck=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->printercheck),_("Print with the following command: "));
  gtk_box_pack_start(GTK_BOX(hbox),pd->commandcheck,FALSE,FALSE,0);
  pd->command=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox),pd->command,TRUE,TRUE,0);
  frame=gtk_frame_new(_("Print range"));
  gtk_box_pack_start(GTK_BOX(vbox),frame,FALSE,FALSE,0);
  hbox=gtk_hbox_new(TRUE,0);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  but=gtk_button_new_with_label(_("OK"));
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(printdlg_ok),pd);
  gtk_box_pack_start(GTK_BOX(hbox), but, TRUE, TRUE, 0);
  but=gtk_button_new_with_label(_("Cancel"));
  gtk_box_pack_end(GTK_BOX(hbox), but, TRUE, TRUE, 0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(close_printdlg),pd);
  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_add(GTK_CONTAINER(frame),vbox);
  pd->allpages=gtk_radio_button_new_with_label(NULL, _("all pages"));
  gtk_box_pack_start(GTK_BOX(vbox),pd->allpages,FALSE,FALSE,0);
  hbox=gtk_hbox_new(FALSE,0);
  gtk_box_pack_start(GTK_BOX(vbox),hbox, FALSE,FALSE,0);
  pd->pagerange=gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(pd->allpages), _("Page from: "));
  gtk_box_pack_start(GTK_BOX(hbox),pd->pagerange,TRUE,TRUE,0);
  pd->startnum=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox),pd->startnum,FALSE,FALSE,0);
  label=gtk_label_new(_("to: "));
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  pd->endnum=gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(hbox),pd->endnum,FALSE,FALSE,0);
  gtk_signal_connect(GTK_OBJECT(pd->win),"delete_event",
		     GTK_SIGNAL_FUNC(close_printdlg),pd);
  gtk_widget_show_all(pd->win);
  return pd;
}
