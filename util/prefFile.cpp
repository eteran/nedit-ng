/*******************************************************************************
*                                                                              *
* prefFile.c -- Nirvana utilities for providing application preferences files  *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* June 3, 1993                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "prefFile.h"



/*******************
Implementation Note:
Q: Why aren't you using the Xt type conversion services?
A: 1) To create a save file, you also need to convert values back to text form,
and there are no converters for that direction.  2) XtGetApplicationResources
can only be used on the resource database created by the X toolkit at
initialization time, and there is no way to intervene in the creation of
that database or store new resources in it reliably after it is created.
3) The alternative, XtConvertAndStore is not adequately documented.  The
toolkit mauual does not explain why it overwrites its input value structure.
4) XtGetApplicationResources and XtConvertAndStore do not work well together
because they use different storage strategies for certain data types.
*******************/
