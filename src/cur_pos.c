#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>

#include "gps.h"

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

static void got_position(struct nmea_pointinfo *nmea,
                             void *data)
{
  int longi=nmea->longsec;
  int latti=nmea->lattsec;
  int *readable=(int *)data;
  if (*readable) {
  printf("%0d\260%02d'%02d.%d''N %0d\260%02d'%02d.%d''E",
           latti/3600,
           (latti/60)%60,
           latti%60,
           ((int)(nmea->lattsec*10.0))%10,
           longi/3600,
           (longi/60)%60,
           longi%60,
           ((int)(nmea->longsec*10.0))%10);
 } else {
  printf("%0da%02db%02d.%dbbN %0da%02db%02d.%dbbE",
           latti/3600,
           (latti/60)%60,
           latti%60,
           ((int)(nmea->lattsec*10.0))%10,
           longi/3600,
           (longi/60)%60,
           longi%60,
           ((int)(nmea->longsec*10.0))%10);
 } 
 exit(0);
   
}

int main(int argc, char **argv)
{
  int fd=open_tcp("localhost",2947);
  int readable=(argc>1)?1:0;
  struct gpsfile *gpsf=open_gps_file(fd);
  while(0<proc_gps_input(gpsf,got_position,&readable));
  exit(1); 
}
