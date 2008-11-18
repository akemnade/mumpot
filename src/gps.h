struct nmea_pointinfo {
  double longsec;
  double lattsec;
  double speed;
  double heading;
  char *time;
  char *date;
  char state;
  int hdop;  /* 10 times the hdop */
  unsigned start_new: 1;
  unsigned single_point: 1;
};

struct t_punkt32 {
  int x,y;
  float speed;
  int hdop;
  double longg;
  double latt;
  char *time;
  unsigned start_new:1;
  unsigned single_point:1;
};

struct gpsfile;

void  save_nmea(FILE *f,GList *save_list);
void load_gps_line_noproj(const char *fname, GList **mll);
struct gpsfile *open_gps_file(int fd);
void close_gps_file(struct gpsfile *gpsf,int closefd);
int proc_gps_input(struct gpsfile *gpsf,
                   void (*gpsproc)(struct nmea_pointinfo *,void *), void *data);
