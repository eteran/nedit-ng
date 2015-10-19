static const char CVSID[] = "$Id: vmsUtils.c,v 1.6 2004/07/21 11:32:07 yooden Exp $";
/*******************************************************************************
*									       *
* vmsUtils.c - Utility routines for VMS systems.			       *
*									       *
*	       This file contains the following functions:		       *
*									       *
*	 StrDescToNul      - Convert VMS String Descriptor to C-like null-     *
*			       terminated string.  Returns the address of      *
*			       the malloc'd string.  (Call FreeNulStr() when   *
*			       done with the string.)			       *
*	 NulStrToDesc	   - Convert C-like null-terminated string to VMS      *
*			       String Descriptor.  Returns the address of      *
*			       the malloc'd descriptor, which should be free'd *
*			       when done by calling FreeStrDesc().  	       *
*	 NulStrWrtDesc	   - Convert C-like null-terminated string to VMS      *
*			       String Descriptor for writing into.  The C      *
*			       String should already be allocated and the      *
*			       length passed as the second parameter.  Returns *
*			       the address of the malloc'd descriptor, which   *
*			       should be free'd when done via FreeStrDesc().   *
*	 FreeNulStr	   - Frees null-terminated strings created by 	       *
*			       StrDescToNul(). 				       *
*	 FreeStrDesc 	   - Frees VMS String Descriptors created by 	       *
*			       NulStrToDesc() and NulStrWrtDesc(). 	       *
*	 ConvertVMSCommandLine	- Convert an argument vector representing a    *
*			       VMS-style command line to something Unix-like.  *
*			       Limitations: no abbreviations, some syntax      *
*			       information is lost so some errors will yield   *
*			       strange results.				       * 
*									       *
*	 rint   	   - Returns the integer (represented as a double      *
*			       precision number) nearest its double argument.  *
*									       *
*	 VMSFileScan   	   - Calls LIB$FILE_SCAN for filenames on VMS systems  *
*									       *
*	 VMSFileScanDone   - Ends LIB$FILE_SCAN context & frees memory used    *
*                                                                              *
*        ProcAlive 	   - See if a process (identified by pID) is still     *
*			       alive on VMS.				       *
*									       *
* Copyright (C) 1999 Mark Edel						       *
*									       *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.							       *
* 									       *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA		                       *
*									       *
* Nirvana Text Editor	    						       *
* February 22, 1993							       *
*									       *
* Written by Joy Kyriakopulos						       *
*									       *
*******************************************************************************/


