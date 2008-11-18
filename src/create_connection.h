/********************************************************************** 
 mumpot - Copyright (C) 2008 - Andreas Kemnade
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2, or (at your option)
 any later version.
             
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied
 warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
***********************************************************************/

#ifndef K_CREATE_CONNECTION_H
#define K_CREATE_CONNECTION_H
struct connection_dialog;
void show_connection_dialog(struct connection_dialog *cdlg);
void delete_connection_dialog(struct connection_dialog *cdlg);
struct connection_dialog *create_connection_dialog(void (*conn_created)(struct connection_dialog *,int,void*),
						   void (*dlg_canceled)(struct connection_dialog *,void*),
						   void *data);

#endif
