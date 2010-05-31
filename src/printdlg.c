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
#ifdef _WIN32
#include <windows.h>
#endif
#ifdef HAVE_PAPER_H
#include <paper.h>
#endif
#include "myintl.h"
#include "printdlg.h"
#define DEFAULTPRINTER _("Default printer")
static GtkWidget *psizewin;
static GtkWidget *psizeselect;
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
  void (*ok_cb)(void *,struct print_job *);
  void (*cancel_cb)(void *);
  void *data;
  
  /*  GtkWidget *singlepage; */
};

struct t_mypapersizes {
  int width;
  int height;
  char *name;
  int idx;
};

#ifndef HAVE_PAPER_H
static struct t_mypapersizes mypp[]= {
  {841,1189,"a0",0},
  {594,841,"a1",0},
  {420,594,"a2",0},
  {297,420,"a3",0},
  {210,297,"a4",0},
  {148,210,"a5",0},
  {105,148,"a6",0},
  {74,105,"a7",0},
};

static struct t_mypapersizes *mypapersizes[]={mypp,mypp+1,
					      mypp+2,mypp+3,
					      mypp+4,mypp+5,
					      mypp+6,mypp+7,
					      NULL};

static struct t_mypapersizes *selected_papersize=mypp+4;
static void init_papersizes()
{
}
#else
static struct t_mypapersizes **mypapersizes;
static struct t_mypapersizes *selected_papersize;

static void init_papersizes()
{
  int n,i;
  const struct paper *p;
  const char *syspaper;
  paperinit();
  for(p=paperfirst(),n=0;p;p=papernext(p)) {
    n++;
  }
  syspaper=systempapername(); 
  selected_papersize=NULL;
  mypapersizes=(struct t_mypapersizes **)
    calloc(sizeof(void *),n+1);
  for(i=0,p=paperfirst();(i<n)&&p;p=papernext(p),i++) {
    mypapersizes[i]=g_new0(struct t_mypapersizes,1);
    mypapersizes[i]->width=paperpswidth(p)/72.0*25.4;
    mypapersizes[i]->height=paperpsheight(p)/72.0*25.4;
    mypapersizes[i]->name=strdup(papername(p));
    if (strcmp(papername(p),syspaper)==0)  {
      selected_papersize=mypapersizes[i];
    }
  }
  paperdone();
}
#endif
char *get_paper_name()
{
  if (!selected_papersize)
    init_papersizes();
  return selected_papersize->name;
}

int get_paper_width()
{
  if (!selected_papersize)
    init_papersizes();
  return selected_papersize->width;
}

int get_paper_height()
{
  if (!selected_papersize)
    init_papersizes();
  return selected_papersize->height;
}

static void psize_ok_cb(GtkWidget *w, gpointer data)
{
  GList *l=GTK_CLIST(psizeselect)->selection;
  if (l) {
    int item=(int)l->data;
    selected_papersize=(struct t_mypapersizes *)
      gtk_clist_get_row_data(GTK_CLIST(psizeselect),item);
    gtk_widget_hide(psizewin);
  }
}

void select_pagesize()
{
  if (!psizewin) {
    GtkWidget *but;
    GtkWidget *scr;
    int i;
    init_papersizes();
    psizewin=gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(psizewin),
			 _("Select page size"));
    but=gtk_button_new_with_label(_("OK"));
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(psizewin)->action_area),
		       but,TRUE,TRUE,0);
    gtk_signal_connect(GTK_OBJECT(but),"clicked",
		       GTK_SIGNAL_FUNC(psize_ok_cb),psizewin);
    but=gtk_button_new_with_label(_("Cancel"));
    gtk_signal_connect_object(GTK_OBJECT(but),"clicked",
			      GTK_SIGNAL_FUNC(gtk_widget_hide),GTK_OBJECT(psizewin));
    gtk_signal_connect_object(GTK_OBJECT(psizewin),"delete-event",
			      GTK_SIGNAL_FUNC(gtk_widget_hide),GTK_OBJECT(psizewin));
    gtk_box_pack_end(GTK_BOX(GTK_DIALOG(psizewin)->action_area),
		     but,TRUE,TRUE,0);
    psizeselect=gtk_clist_new(1);
    scr=gtk_scrolled_window_new(NULL,NULL);
    gtk_container_add(GTK_CONTAINER(scr),psizeselect);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(psizewin)->vbox),
		       scr,TRUE,TRUE,0);
    
    for(i=0;mypapersizes[i];i++) {
      mypapersizes[i]->idx=gtk_clist_append(GTK_CLIST(psizeselect),
					    &mypapersizes[i]->name);
      gtk_clist_set_row_data(GTK_CLIST(psizeselect),mypapersizes[i]->idx,
			     mypapersizes[i]);
    }
    gtk_clist_set_selection_mode(GTK_CLIST(psizeselect),
				 GTK_SELECTION_SINGLE);
    if (selected_papersize)
      gtk_clist_select_row(GTK_CLIST(psizeselect),selected_papersize->idx,0);
  }
  gtk_widget_show_all(psizewin);
}

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

void delete_print_job(struct print_job *pj)
{
  if (pj->tmpname) {
#ifdef _WIN32
    ShellExecute(NULL,"print",pj->tmpname,NULL,NULL,SW_SHOWNORMAL);
    printf("printed\n");
#endif
    free(pj->tmpname);
  }
  if (pj->fd>=0)
    close(pj->fd);
  free(pj);
}


static void printdlg_ok(GtkWidget *w, gpointer data)
{
  int fd=-1;
#ifdef _WIN32
  int tmpfileused=0;
  char lptmp[256];
#endif
  struct printdlg_t *pd = (struct printdlg_t *)data;
  struct print_job *pj;
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
#else
    memset(lptmp,0,sizeof(lptmp));
    tmp=tempnam(NULL,"karte");
    strcpy(lptmp,tmp);
    strcat(lptmp,".ps");
    printf("creating %s\n",lptmp);
    fd=open(lptmp,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0666);
    if (fd>=0) {
      tmpfileused=1;
    } else {
      perror("open");
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
    pj = g_new0(struct print_job,1);
    pj->fd=fd;
    pj->data=pd->data;
    pj->start_page=start_page;
    pj->end_page=end_page;
#ifdef _WIN32
    if (tmpfileused)
      pj->tmpname=strdup(lptmp);
#endif    
    pd->ok_cb(pd->data, pj);

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
				   void (*ok_cb)(void *,struct print_job *),void *data)
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
