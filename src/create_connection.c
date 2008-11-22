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
#ifndef _WIN32
#include <gtk/gtk.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <png.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/termios.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>
#include "myintl.h"

struct connection_dialog {
  GtkWidget *win;
  GtkWidget *tableser;
  GtkWidget *serfield;
  GtkWidget *baudratefield;
  GtkWidget *tablebt;
  GtkWidget *btaddrfield;
  GtkWidget *channelfield;
  GtkWidget *tableinet;
  GtkWidget *hostnamefield;
  GtkWidget *portfield;
  GtkWidget *chooser;
  GtkWidget *failedlabel;
  void *data;
  int current_mode;
  void (*conn_created)(struct connection_dialog *,int,void *);
  void (*dlg_canceled)(struct connection_dialog *,void *);
};

static int search_btname(char *name, bdaddr_t *result, int timeout) 
{
  char addr[80];
  inquiry_info *info=NULL;
  int i;
  int found=0;
  static int found_class=0;
  int dd=hci_open_dev(0);
  /* get a list of all available devices */
  int num_dev;
  num_dev=hci_inquiry(0,timeout*8/10,100,NULL,&info,found_class?0:IREQ_CACHE_FLUSH);
  if (num_dev<0){
    fprintf(stderr,_("searching devices failed\n"));
    sleep(1);
  }
  found_class=0;
  fprintf(stderr,_("found %d devices\n"),num_dev);
 
  for(i=0;i<num_dev;i++) {
    ba2str(&info[i].bdaddr,addr);
    fprintf(stderr,"%s\n",addr);
  }
  for(i=0;i<num_dev;i++){
    char buf[80];
    /* check major device class, the value is only valid
     * for PromiESD-Devices */
    /*    if (info[i].dev_class[1]==0x1f) */ {  
      found_class=1;
      ba2str(&info[i].bdaddr,addr);
      fprintf(stderr,"getting name for %s: ",addr);
      /* get the bluetooth name */
      if (hci_read_remote_name(dd,&info[i].bdaddr,sizeof(buf),buf,100000)<0) {
        fprintf(stderr,"failed\n");
      } else {
        fprintf(stderr,"%s: ",buf)
          /* compare the device name */;
        if (strcmp(name,buf)==0) {
          found=1;
          *result=info[i].bdaddr;
          fprintf(stderr,"found!\n");
          /* right device, done */
          break;
        } else {
          fprintf(stderr,"nothing interesting\n");
        }
      }
    } 
  }
  if (!found) {
    sleep(1);
  }
  hci_close_dev(dd);
  return found;
  }


/* create a rfcomm connection */
static int open_bluetooth(char *mac, int channel)
{
  bdaddr_t local;
  int fd;
  struct sockaddr_rc rcaddr;  
  fd=socket(PF_BLUETOOTH,SOCK_STREAM,BTPROTO_RFCOMM);
  if (fd<0) {
    perror("socket(): ");
    return -1;
  }
  hci_devba(0, &local);
  bacpy(&rcaddr.rc_bdaddr,&local);  
  rcaddr.rc_family=AF_BLUETOOTH;  
  rcaddr.rc_channel=0;
  if (bind(fd,(struct sockaddr *)&rcaddr,sizeof(rcaddr))) {
    perror("bind: ");
    close(fd);
    return -1;
  }
  rcaddr.rc_channel=channel;
  if (mac[0]==':') {
    /* try to find out the bluetooth addr */ 
    if (!search_btname(mac+1,&rcaddr.rc_bdaddr,10)) {
      fprintf(stderr,"cannot find %s\n",mac);
      close(fd);
      return -1;
    }
  } else {
    str2ba(mac,&rcaddr.rc_bdaddr);
  }
  if (connect(fd,(struct sockaddr *)&rcaddr,sizeof(rcaddr))) {
    perror("connect: ");
    close(fd);
    return -1;
  }
  return fd;
}


/* open serial device, set the baudrate and set mode to raw mode */

static int open_ser(char *fname, int bps) 
{
  struct termios mytermios;
  int b;
  int fd=open(fname,O_RDWR);
  if (fd<0) {
    fprintf(stderr,"Cannot open connection to %s\n",fname);
    return -1;
  }
  tcgetattr(fd, &mytermios);
  cfmakeraw(&mytermios);
  mytermios.c_cflag&=(~CRTSCTS);
  /* mytermios.c_cflag&=(~CLOCAL); */
  b=B115200;
  switch(bps) {
  case 9600: b=B9600; break;
  case 19200: b=B19200; break;
  case 38400: b=B38400; break;
  case 57600: b=B57600; break;
  case 115200: b=B115200; break;
  default: fprintf(stderr,"invalid rate: %d\n",bps);
  }
  cfsetospeed(&mytermios,b);
  cfsetispeed(&mytermios,B0);
  tcsetattr(fd,TCSANOW,&mytermios);
  tcflush(fd,TCIFLUSH);
  tcflush(fd,TCOFLUSH); 
  return fd;
}

int open_tcp(char *host, int port)
{
  struct hostent *ph=gethostbyname(host);
  int sock;
  struct sockaddr_in addr;
  addr.sin_family=AF_INET;
  addr.sin_port=htons(port);
  if (!ph) {
    return -1;
  }
  sock=socket(AF_INET,SOCK_STREAM,0);
  memcpy((char *) &addr.sin_addr,ph->h_addr,ph->h_length); 
  if (sock<0) {
    return -1;
  } 
  if (connect(sock,(struct sockaddr *)&addr,sizeof(addr))) {
    close(sock);
    return -1;
  }
  return sock;
  
}


static void conn_ok(GtkWidget *w, gpointer data)
{
  int fd = -1;
  struct connection_dialog *cdlg = (struct connection_dialog *)data;
  printf("OK pressed\n");
  if (cdlg->conn_created) {
  if (cdlg->current_mode == 0) {
    char *host = gtk_editable_get_chars(GTK_EDITABLE(cdlg->btaddrfield),0,-1);
    char *channel = gtk_editable_get_chars(GTK_EDITABLE(cdlg->channelfield),0,-1);
    fd = open_bluetooth(host,atoi(channel));
    g_free(host);
    g_free(channel);
  } else if (cdlg->current_mode == 1) {
    char *host = gtk_editable_get_chars(GTK_EDITABLE(cdlg->hostnamefield),0,-1);
    char *port = gtk_editable_get_chars(GTK_EDITABLE(cdlg->portfield),0,-1);
    fd=open_tcp(host,atoi(port));
    g_free(host);
    g_free(port);
  } else {
    char *dev = gtk_editable_get_chars(GTK_EDITABLE(cdlg->serfield),0,-1);
    char *baudrate = gtk_editable_get_chars(GTK_EDITABLE(cdlg->baudratefield),0,-1);
    fd=open_ser(dev,atoi(baudrate));
  }
  }
  if (!(fd < 0)) {
    gtk_widget_hide(cdlg->win);
    cdlg->conn_created(cdlg,fd,cdlg->data);
  } else {
    gtk_widget_show(cdlg->failedlabel);
  }
}

static void conn_cancel(GtkWidget *w, gpointer data)
{
  struct connection_dialog *cdlg = (struct connection_dialog *)data;
  printf("cancel pressed\n");
  if (cdlg->dlg_canceled)
    cdlg->dlg_canceled(cdlg,cdlg->data);
  gtk_widget_hide(cdlg->win);
}

void show_connection_dialog(struct connection_dialog *cdlg)
{
  gtk_widget_hide(cdlg->failedlabel);
  gtk_widget_show(cdlg->win);
}

void delete_connection_dialog(struct connection_dialog *cdlg)
{
  gtk_widget_destroy(cdlg->win);
  free(cdlg);
}

static void change_mode(GtkWidget *w, gpointer data)
{
  struct connection_dialog *cdlg = (struct connection_dialog *)data;
  char *mode=gtk_editable_get_chars(GTK_EDITABLE(w),0,-1);
  gtk_widget_hide(cdlg->tableser);
  gtk_widget_hide(cdlg->tablebt);
  gtk_widget_hide(cdlg->tableinet);
  if (0==strcmp(mode,"Bluetooth")) {
    cdlg->current_mode = 0;
    gtk_widget_show(cdlg->tablebt);
  }  else if (0==strcmp(mode,_("TCP/IP"))) {
    cdlg->current_mode = 1;
    gtk_widget_show(cdlg->tableinet);
  } else {
    cdlg->current_mode = 2;
    gtk_widget_show(cdlg->tableser);
  }
}


struct connection_dialog *create_connection_dialog(void (*conn_created)(struct connection_dialog *,int,void*),
				      void (*dlg_canceled)(struct connection_dialog *,void*),
				      void *data)
{
  struct connection_dialog *cdlg = malloc(sizeof(struct connection_dialog));
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *but;
  GList *l;
  cdlg->data=data;
  cdlg->conn_created=conn_created;
  cdlg->dlg_canceled=dlg_canceled;
  cdlg->win=gtk_window_new(GTK_WINDOW_TOPLEVEL);
  vbox=gtk_vbox_new(FALSE,0);
  gtk_container_add(GTK_CONTAINER(cdlg->win),vbox);
  cdlg->chooser=gtk_combo_new();
  gtk_combo_set_value_in_list(GTK_COMBO(cdlg->chooser),TRUE,FALSE);
  l=NULL;
  l=g_list_append(l,_("Bluetooth"));
  l=g_list_append(l,_("TCP/IP"));
  l=g_list_append(l,_("Serial"));
  label=gtk_label_new(_("Type of connection:"));
  hbox=gtk_hbox_new(FALSE,0);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(hbox),cdlg->chooser,FALSE,FALSE,0);
  gtk_combo_set_popdown_strings(GTK_COMBO(cdlg->chooser),l);
  g_list_free(l);
  cdlg->tableser=gtk_table_new(2,2,FALSE);
  gtk_box_pack_start(GTK_BOX(vbox),cdlg->tableser,FALSE,FALSE,0);
  label = gtk_label_new(_("Device:"));
  gtk_table_attach(GTK_TABLE(cdlg->tableser),label,0,1,0,1,
		   GTK_FILL,GTK_FILL,0,0);
  cdlg->serfield=gtk_entry_new();
  gtk_table_attach(GTK_TABLE(cdlg->tableser),cdlg->serfield,1,2,0,1,
		   GTK_FILL|GTK_EXPAND,GTK_FILL,0,0);
  label = gtk_label_new(_("baud rate:"));
  gtk_table_attach(GTK_TABLE(cdlg->tableser),label,0,1,1,2,
		   GTK_FILL,GTK_FILL,0,0);
  cdlg->baudratefield = gtk_entry_new();
  gtk_table_attach(GTK_TABLE(cdlg->tableser),cdlg->baudratefield,1,2,1,2,
		   GTK_FILL|GTK_EXPAND,GTK_FILL,0,0);
  
  cdlg->tablebt=gtk_table_new(2,2,FALSE);
  gtk_box_pack_start(GTK_BOX(vbox),cdlg->tablebt,FALSE,FALSE,0);
  label = gtk_label_new(_("Bluetooth address:"));
  gtk_table_attach(GTK_TABLE(cdlg->tablebt),label,0,1,0,1,
		   GTK_FILL,GTK_FILL,0,0);
  cdlg->btaddrfield=gtk_entry_new();
  gtk_table_attach(GTK_TABLE(cdlg->tablebt),cdlg->btaddrfield,1,2,0,1,
		   GTK_FILL|GTK_EXPAND,GTK_FILL,0,0);
  label = gtk_label_new(_("Channel:"));
  gtk_table_attach(GTK_TABLE(cdlg->tablebt),label,0,1,1,2,
		   GTK_FILL,GTK_FILL,0,0);
  cdlg->channelfield = gtk_entry_new();
  gtk_table_attach(GTK_TABLE(cdlg->tablebt),cdlg->channelfield,1,2,1,2,
		   GTK_FILL|GTK_EXPAND,GTK_FILL,0,0);
  
  cdlg->tableinet=gtk_table_new(2,2,FALSE);
  gtk_box_pack_start(GTK_BOX(vbox),cdlg->tableinet,FALSE,FALSE,0);
  label = gtk_label_new(_("IP/Hostname:"));
  gtk_table_attach(GTK_TABLE(cdlg->tableinet),label,0,1,0,1,
		   GTK_FILL,GTK_FILL,0,0);
  cdlg->hostnamefield=gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(cdlg->hostnamefield),"localhost");
  gtk_table_attach(GTK_TABLE(cdlg->tableinet),cdlg->hostnamefield,1,2,0,1,
		   GTK_FILL|GTK_EXPAND,GTK_FILL,0,0);
  label = gtk_label_new(_("Port:"));
  gtk_table_attach(GTK_TABLE(cdlg->tableinet),label,0,1,1,2,
		   GTK_FILL,GTK_FILL,0,0);
  cdlg->portfield = gtk_entry_new();
  gtk_table_attach(GTK_TABLE(cdlg->tableinet),cdlg->portfield,1,2,1,2,
		   GTK_FILL|GTK_EXPAND,GTK_FILL,0,0);
  gtk_entry_set_text(GTK_ENTRY(cdlg->portfield),"2947");
  hbox=gtk_hbox_new(TRUE,0);
  gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,FALSE,0);
  but=gtk_button_new_with_label(_("OK"));
  gtk_box_pack_start(GTK_BOX(hbox),but,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(conn_ok),cdlg);
  but=gtk_button_new_with_label(_("Cancel"));
  gtk_box_pack_start(GTK_BOX(hbox),but,TRUE,TRUE,0);
  gtk_signal_connect(GTK_OBJECT(but),"clicked",
		     GTK_SIGNAL_FUNC(conn_cancel),cdlg);
  gtk_signal_connect(GTK_OBJECT(cdlg->win),"delete-event",
		     GTK_SIGNAL_FUNC(conn_cancel),cdlg);
  cdlg->failedlabel=gtk_label_new(_("Creating GPS connection failed"));
  gtk_box_pack_start(GTK_BOX(vbox),cdlg->failedlabel,FALSE,FALSE,0);
  gtk_widget_show_all(vbox);
  gtk_signal_connect(GTK_OBJECT(GTK_COMBO(cdlg->chooser)->entry),"changed",
		     GTK_SIGNAL_FUNC(change_mode),cdlg);
  change_mode(GTK_COMBO(cdlg->chooser)->entry,cdlg);
  return cdlg;
}

#endif
