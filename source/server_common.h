/*******************************************************************************
*                                                                              *
* server_common.h -- Nirvana Editor common server stuff                        *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* November, 1995                                                               *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#ifndef SERVER_COMMON_H_
#define SERVER_COMMON_H_


// TODO(eteran): add the concept of user/server uniqueness similar to the following
// comment
/*
 * Create the server property atoms for the server with serverName.
 * Atom names are generated as follows:
 *
 * NEDIT_SERVER_EXISTS_<host_name>_<user>_<server_name>
 * NEDIT_SERVER_REQUEST_<host_name>_<user>_<server_name>
 *
 * <server_name> is the name that can be set by the user to allow
 * for multiple servers to run on the same display. <server_name>
 * defaults to "" if not supplied by the user.
 *
 * <user> is the user name of the current user.
 */

#define SERVICE_NAME "com.nedit"

#endif
