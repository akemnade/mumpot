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

#ifndef K_STRASSE_H
#define K_STRASSE_H

struct punkt {
  short int x;
  short int y;
};
struct strasse {
  int id;
  short int len;
  struct punkt *punkte;
  int anz;
  struct punkt anfang;
  struct punkt ende;
  int anfang_krid;
  int end_krid;
  unsigned char r,g,b;
};

struct kreuzung {
  int id;
  char n;
  int abzweigungen[8];
  /* end=1 start=0 */
  char r[8];
};

struct strasse *read_strasse(int fd);
int write_strasse(int fd, struct strasse *str);
struct kreuzung *read_kreuzung(int fd);
int write_kreuzung(int fd, struct kreuzung *k);
void read_str_file(char *name,char *key);
struct strasse *get_str_id(int strid);
struct kreuzung *get_kr_id(int krid);
void reset_way_info();
void put_way_info(int krid, int len, int last_str);
void get_way_info(int krid, int *len, int *last_str);
void str_to_mark_list(void (*)(GList **,int,int),
		      GList **l,struct strasse *str,int is_anfang);
struct strasse *suche_naechste_str(int *x, int *y, int *is_anfang);

#endif
