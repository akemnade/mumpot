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
#include <sys/time.h>
#include <curl/curl.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlwriter.h>
#include <glib.h>
#include "myintl.h"
#include "osm_parse.h"
#include "osm_upload.h"

struct osm_upload_data {
  struct osm_file *osmf;
  char url[512];
  char *inbuf;
  int inbuflen;
  char *userpw;
  CURLM *curlm;
  CURL *curl;
  xmlParserCtxtPtr diffparser;
  int ioc_source;
  int flagschanged;
  GIOChannel *ioc;
  char *finishedmsg;
  int socket;
  long result;
  int changeset;
  char *apibase;
  int bufpos;
  int buflen;
  char *buf;
  void (*msg_callback)(void *,char *,int);
  void *msg_cb_data;
  enum {REQ_CSET, UPLOAD, CLOSE_CSET } upload_state;
};

static void cleanup_upload(struct osm_upload_data *oud);

static size_t mycurl_writebuf(void *ptr, size_t size,
			      size_t nmemb, void *data)
{
  int ret=size*nmemb;
  struct osm_upload_data *oud=(struct osm_upload_data *)data;
  if (!oud->inbuf)
    oud->inbuf=malloc(ret+1);
  else
    oud->inbuf=realloc(oud->inbuf,oud->inbuflen+ret+1);
  memcpy(oud->inbuf+oud->inbuflen,ptr,ret);
  oud->inbuflen+=ret;
  oud->inbuf[oud->inbuflen]=0;
  return ret;
}



static void close_cset(struct osm_upload_data *oud)
{
  int running=0;
  CURLMcode ret;
  snprintf(oud->url,sizeof(oud->url),"%s/changeset/%d/close",oud->apibase,
	   oud->changeset);
  curl_multi_remove_handle(oud->curlm,oud->curl);
  curl_easy_setopt(oud->curl, CURLOPT_URL,
		   oud->url);
  curl_easy_setopt(oud->curl, CURLOPT_INFILESIZE,
		   0L);
  curl_easy_setopt(oud->curl, CURLOPT_WRITEFUNCTION, mycurl_writebuf);
  curl_easy_setopt(oud->curl,CURLOPT_PUT,1L);
  curl_easy_setopt(oud->curl,CURLOPT_UPLOAD,1L);
  curl_easy_setopt(oud->curl,CURLOPT_POST,0L);
  oud->upload_state=CLOSE_CSET;
  curl_multi_add_handle(oud->curlm,oud->curl);
  if (oud->msg_callback) {
    char *buf=g_strdup_printf(_("closing changeset %d"),oud->changeset);
    oud->msg_callback(oud->msg_cb_data,buf,0);
    free(buf);
  }
  while(CURLM_CALL_MULTI_PERFORM==(ret=curl_multi_socket_all(oud->curlm,&running)));
  if (!running) {
    if (oud->msg_callback)
      oud->msg_callback(oud->msg_cb_data,oud->finishedmsg?oud->finishedmsg:_("uploading changes failed"),1);
    cleanup_upload(oud);
  }
}

static void handle_upload_finished(struct osm_upload_data *oud)
{
  oud->bufpos=0;
  free(oud->buf);
  oud->buf=NULL;
  oud->buflen=0;
  if (oud->result==200) {
    oud->finishedmsg=strdup(_("uploading successful"));
    if (oud->msg_callback)
      oud->msg_callback(oud->msg_cb_data,_("uploading changeset finished"),0);
  } else {
    oud->finishedmsg=g_strdup_printf("uploading changeset failed %d:\n%s",(int)oud->result,(oud->inbuf)?oud->inbuf:"");
    if (oud->inbuf)
      free(oud->inbuf);
    if (oud->msg_callback)
      oud->msg_callback(oud->msg_cb_data,_("uploading changeset failed"),0);
  } 
  
  oud->inbuflen=0;
  oud->inbuf=NULL;
  if (oud->diffparser) {
    xmlParseChunk(oud->diffparser,NULL,0,1);
    xmlFreeParserCtxt(oud->diffparser);
  }
  close_cset(oud);
}

static void diffparse_endhandler(void *ctx,
				 const xmlChar *name)
{
  
}

static void diffparse_starthandler(void *ctx,
				   const xmlChar *name,
				   const xmlChar **atts)
{
  struct osm_upload_data *oud=(struct osm_upload_data *)ctx;
  int i;
  int fromnum=0;
  int tonum=0;
  int version=0;
  if (strcmp((char *)name,"way")&&strcmp((char*)name,"node"))
    return;
  for(i=0;atts[i];i+=2) {
    if (!strcmp((char *)atts[i],"old_id")) {
      fromnum=atoi((char *)atts[i+1]);
    } else if (!strcmp((char *)atts[i],"new_id")) {
      tonum=atoi((char *)atts[i+1]);
    } else if (!strcmp((char *)atts[i],"new_version")) {
      version=atoi((char *)atts[i+1]);
    }
  }
  if (fromnum&&tonum&&version) {
    printf("%s %d -> %d version: %d\n",(char *)name,
	   fromnum,tonum,version);
    if (!strcmp((char *)name,"node")) {
      osm_node_update_id(oud->osmf,fromnum,tonum,version);
    } else if (!strcmp((char *)name,"way")) {
      osm_way_update_id(oud->osmf,fromnum,tonum,version);
    }
  }
}

static size_t process_diff(void *ptr, ssize_t size, ssize_t nmemb,
			   void *data)
{
  struct osm_upload_data *oud=(struct osm_upload_data *)
    data;
  int len=size*nmemb;
  if (!oud->result) {
    curl_easy_getinfo(oud->curl,CURLINFO_RESPONSE_CODE,
		      &oud->result);
    fprintf(stderr,"HTTP result %d\n",(int)oud->result);
  }
  if (oud->result==200) {
    if (!oud->diffparser) {
      static xmlSAXHandler myhandler={
	.initialized = 1,
	.startElement = diffparse_starthandler,
	.endElement = diffparse_endhandler
      };
      oud->diffparser=xmlCreatePushParserCtxt(&myhandler,
					      oud,ptr,len,"diff");
    } else {
      xmlParseChunk(oud->diffparser,ptr,len,0);
    }
  } else {
    return mycurl_writebuf(ptr,size,nmemb,data);
  }
  return len;
}

static void start_upload(struct osm_upload_data *oud)
{
  CURLMcode ret;
  int running=0;
  curl_multi_remove_handle(oud->curlm,oud->curl);
  oud->buf=save_osmchange_buf(oud->osmf,oud->changeset);
  oud->bufpos=0;
  oud->buflen=strlen(oud->buf);
  snprintf(oud->url,sizeof(oud->url),"%s/changeset/%d/upload",
	   oud->apibase,oud->changeset);
  curl_easy_setopt(oud->curl, CURLOPT_URL,
		   oud->url);
  curl_easy_setopt(oud->curl,CURLOPT_PUT,0L);
  curl_easy_setopt(oud->curl,CURLOPT_UPLOAD,0L);
  curl_easy_setopt(oud->curl,CURLOPT_POST,1L);
  curl_easy_setopt(oud->curl,CURLOPT_INFILESIZE,oud->buflen);
  curl_easy_setopt(oud->curl,CURLOPT_POSTFIELDSIZE,oud->buflen);
  curl_easy_setopt(oud->curl,CURLOPT_WRITEFUNCTION,process_diff);
  oud->upload_state=UPLOAD;
  curl_multi_add_handle(oud->curlm,oud->curl);
  while(CURLM_CALL_MULTI_PERFORM==(ret=curl_multi_socket_all(oud->curlm,&running)));
  if (!running) {
    if (oud->msg_callback)
      oud->msg_callback(oud->msg_cb_data,oud->finishedmsg?oud->finishedmsg:_("uploading changes failed"),1);
    cleanup_upload(oud);
  }
}

static int handle_req_cset(struct osm_upload_data *oud)
{
  long result=0;
  curl_easy_getinfo(oud->curl,CURLINFO_RESPONSE_CODE,
                    &result);
  if (result!=200) {
    char *buf=g_strdup_printf(_("Error creating changeset: %d\n%s"),
                              (int)result,(oud->inbuflen)?oud->inbuf:"");  
    if (oud->msg_callback)
      oud->msg_callback(oud->msg_cb_data,buf,1);
    return 0; 
  }
  if (oud->inbuflen) {
    oud->changeset=atoi(oud->inbuf);
    oud->bufpos=0;
    free(oud->buf);
    oud->buf=NULL;
    oud->buflen=0;
    oud->inbuflen=0;
    free(oud->inbuf);
    oud->inbuf=NULL;
    if (oud->changeset) {
      if (oud->msg_callback) {
        char buf[80];
        snprintf(buf,sizeof(buf),_("created changeset %d"),oud->changeset);
        oud->msg_callback(oud->msg_cb_data,buf,0);
      }
      fprintf(stderr,"got changeset %d\n",oud->changeset);
      start_upload(oud);
      return 1;
    } 
  }
  if (oud->msg_callback) {
    char *buf=g_strdup_printf(_("got strange result:\n%s"),oud->inbuflen?oud->inbuf:"");
    oud->msg_callback(oud->msg_cb_data,buf,1);
    g_free(buf);
  }
  return 0;
}

static void cleanup_upload(struct osm_upload_data *oud)
{
  if (oud->buf)
    free(oud->buf);
  if (oud->inbuf)
    free(oud->inbuf);
  if (oud->userpw)
    free(oud->userpw);
  curl_multi_cleanup(oud->curlm);
  curl_easy_cleanup(oud->curl);
}

static int my_curl_iocb(GIOChannel *source,
			 GIOCondition cond,
			 gpointer data)
{
  int bm=0;
  int handles=0;
  struct osm_upload_data *oud=(struct osm_upload_data *)data;
  fprintf(stderr,"gio callback\n");
  if (!cond)
    return FALSE;
  if (cond&G_IO_IN)
    bm|=CURL_CSELECT_IN;
  if (cond&G_IO_OUT)
    bm|=CURL_CSELECT_OUT;
  while(CURLM_CALL_MULTI_PERFORM==curl_multi_socket_action(oud->curlm,oud->socket,bm,
							   &handles));
  if (!handles) {
    fprintf(stderr,"transfer completed\n");
    switch(oud->upload_state) {
    case REQ_CSET:
      if (!handle_req_cset(oud))
        cleanup_upload(oud);
      break;
    case UPLOAD:
      handle_upload_finished(oud);
      break;
    case CLOSE_CSET:
      if (oud->msg_callback) {
        oud->msg_callback(oud->msg_cb_data,oud->finishedmsg?oud->finishedmsg:_("uploading changes failed"),1);
      }
      if (oud->finishedmsg)
        free(oud->finishedmsg);
      if (oud->inbuf)
	fprintf(stderr,"close cset ret: %s\n",oud->inbuf);
      fprintf(stderr,"close cset no ret\n");
      cleanup_upload(oud);
      break;
    }
    return FALSE;
  }
  return TRUE;
}

static int my_curl_sockfunc(CURL *easy,
			     curl_socket_t s,
			     int action,
			     void *userp,
			     void *socketp)
{
  struct osm_upload_data *oud =(struct osm_upload_data *)
    userp;
  GIOCondition cond=0;
  if (!oud->ioc) {
#ifdef _WIN32
    oud->ioc=g_io_channel_win32_new_stream_socket(s);
#else
    oud->ioc=g_io_channel_unix_new(s);
#endif
  }
  if (oud->ioc_source) {
    g_source_remove(oud->ioc_source);
  }
  fprintf(stderr,"socket callback\n");
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
  }
  oud->socket=s;
  if (cond) {
    oud->ioc_source=g_io_add_watch(oud->ioc,
				   cond, my_curl_iocb,
				   oud);
  }  
  return 0;
}

static size_t mycurl_readbuf(void *ptr, size_t size,
			      size_t nmemb, void *data)
{
  struct osm_upload_data *oud=(struct osm_upload_data *)data;
  size_t ret=size*nmemb;
  /* fprintf(stderr,"write xml to net: %d\n",ret); */
  if (!oud->buf)
    return 0;
  if (ret > (oud->buflen-oud->bufpos))
    ret=oud->buflen-oud->bufpos;
  memcpy(ptr,oud->buf+oud->bufpos,ret);

  /*  fprintf(stderr,"write xml to net: ");
  fwrite(oud->buf+oud->bufpos,ret,1,stderr);
  fprintf(stderr,"\n"); */
  oud->bufpos+=ret;
  return ret;
}

static char *create_cset_buf(char *csetmsg)
{
  char *dst;
  xmlBufferPtr xmlbuf=xmlBufferCreate();
  xmlTextWriterPtr writer=xmlNewTextWriterMemory(xmlbuf,0);
  if (!writer)
    return NULL;
  xmlTextWriterStartDocument(writer,NULL,"UTF-8",NULL);
  xmlTextWriterStartElement(writer,(xmlChar *)"osm");
  xmlTextWriterStartElement(writer,(xmlChar *)"changeset");
  
  xmlTextWriterStartElement(writer,(xmlChar *)"tag");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"k",
			      (xmlChar *)"created_by");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"v",
			      (xmlChar *)PACKAGE " " VERSION);
  xmlTextWriterEndElement(writer);

  xmlTextWriterStartElement(writer,(xmlChar *)"tag");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"k",
			      (xmlChar *)"comment");
  xmlTextWriterWriteAttribute(writer,(xmlChar *)"v",
			      (xmlChar *)csetmsg);
  xmlTextWriterEndElement(writer);

  xmlTextWriterEndElement(writer);
  xmlTextWriterEndElement(writer);

  xmlTextWriterEndDocument(writer);
  xmlFreeTextWriter(writer);
  dst=malloc(xmlbuf->use+1);
  memcpy(dst,xmlbuf->content,xmlbuf->use);
  dst[xmlbuf->use]=0;
  xmlBufferFree(xmlbuf);
  return dst;
}

void start_osm_upload(char *csetmsg, char *user, char *pw,
		      struct osm_file *osmf,
                      void (*msg_callback)(void *,char *,int), void *msg_data)
{
  static int initialized;
  int running=0;
  CURLMcode ret;
  struct osm_upload_data *oud=calloc(1,sizeof(struct osm_upload_data));
  if (!initialized) {
    curl_global_init(CURL_GLOBAL_ALL);
    initialized=1;
  }
  oud->osmf=osmf;
  oud->msg_callback=msg_callback; 
  oud->msg_cb_data=msg_data;
  if ((user)&&(pw))
    oud->userpw=g_strdup_printf("%s:%s",user,pw);
  else if (getenv("OSMACCOUNT"))
    oud->userpw=strdup(getenv("OSMACCOUNT"));
  oud->curlm=curl_multi_init();
  oud->curl=curl_easy_init();
  curl_multi_setopt(oud->curlm,CURLMOPT_SOCKETDATA,
		    oud);
  curl_multi_setopt(oud->curlm,CURLMOPT_SOCKETFUNCTION,
		    my_curl_sockfunc);
  oud->buf=create_cset_buf(csetmsg);
  oud->buflen=strlen(oud->buf);
  oud->apibase=getenv("OSMAPIURL");
  if (!oud->apibase)
    oud->apibase="http://www.openstreetmap.org/api/0.6";
  snprintf(oud->url,sizeof(oud->url),"%s/changeset/create",oud->apibase);
  curl_easy_setopt(oud->curl, CURLOPT_URL,
		   oud->url);
  curl_easy_setopt(oud->curl, CURLOPT_READFUNCTION, mycurl_readbuf);
  curl_easy_setopt(oud->curl, CURLOPT_PUT, 1L);
  curl_easy_setopt(oud->curl, CURLOPT_UPLOAD, 1L);
  if (oud->userpw)
    curl_easy_setopt(oud->curl, CURLOPT_USERPWD, oud->userpw);
  curl_easy_setopt(oud->curl, CURLOPT_READDATA, oud);
  curl_easy_setopt(oud->curl, CURLOPT_WRITEDATA, oud);
  curl_easy_setopt(oud->curl, CURLOPT_WRITEFUNCTION, mycurl_writebuf);
  curl_easy_setopt(oud->curl, CURLOPT_INFILESIZE,
		   (long)oud->buflen);
  
  curl_multi_add_handle(oud->curlm, oud->curl);
  oud->upload_state=REQ_CSET;
  if (oud->msg_callback)
    oud->msg_callback(oud->msg_cb_data,"requesting changeset",0);
  while(CURLM_CALL_MULTI_PERFORM==(ret=curl_multi_socket_all(oud->curlm,&running)));
  if (!running) {
    if (oud->msg_callback)
      oud->msg_callback(oud->msg_cb_data,oud->finishedmsg?oud->finishedmsg:_("uploading changes failed"),1);
    cleanup_upload(oud);
  }
}
