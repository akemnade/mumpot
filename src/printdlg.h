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

#ifndef K_PRINTDLG_H
#define K_PRINTDLG_H

struct print_job {
  char *tmpname;
  int fd;
  int start_page;
  int end_page;
  int page;
  GList *page_data;
  void *data;
};

struct printdlg_t;

struct printdlg_t *create_printdlg(int numpages, void (*cancel_cb)(void *), 
				   void (*ok_cb)(void *,struct print_job *), void *data);
void delete_print_job(struct print_job *pj);
#endif
