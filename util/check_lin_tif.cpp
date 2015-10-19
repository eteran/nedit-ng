/*******************************************************************************
*                                                                              *
* verifyMotif:  A small test program to detect bad versions of LessTif and     *
* Open Motif.  Written on linux but should be portable.                        *
*                                                                              *
* Copyright (C) 2003 Nathaniel Gray                                            *
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
* July 28, 1992                                                                *
*                                                                              *
* Written by Nathaniel Gray                                                    *
*                                                                              *
* Modifications by:                                                            *
* Scott Tringali                                                               *
*                                                                              *
*******************************************************************************/

#include <Xm/Xm.h>
#include "motif.h"
#include <stdio.h>
#include <stdlib.h>

/* Print out a message listing the known-good versions */

static void showGoodVersions(void) 
{
    fputs("\nNEdit is known to work with Motif versions:\n", stderr);
    fputs(GetMotifStableVersions(), stderr);
    
    fputs(
        "OpenMotif is available from:\n"
        "\thttp://www.opengroup.org/openmotif/\n"
        "\thttp://www.ist.co.uk/DOWNLOADS/motif_download.html\n"
        "\nAlso, unless you need a customized NEdit you should STRONGLY\n"
        "consider downloading a pre-built binary from http://www.nedit.org,\n"
        "since these are the most stable versions.\n", stderr);
}

/*
** Main test driver.  Check the stability, and allow overrides if needed. 
*/
int main(int argc, const char *argv[])
{
    enum MotifStability stability = GetMotifStability();

    if (stability == MotifKnownGood)
        return EXIT_SUCCESS;

    if (stability == MotifKnownBad)
    {
        fprintf(stderr,
                "ERROR:  Bad Motif Version:\n\t%s\n", 
                XmVERSION_STRING);

        fprintf(stderr, 
            "\nThis version of Motif is known to be broken and is\n"
            "thus unsupported by the NEdit developers.  It will probably\n"
            "cause NEdit to crash frequently.  Check these pages for a more\n"
            "detailed description of the problems with this version:\n"
            "\thttp://www.motifdeveloper.com/tips/tip22.html\n"
            "\thttp://www.motifdeveloper.com/tips/Motif22Review.pdf\n");

#ifdef BUILD_BROKEN_NEDIT
        {
            char buf[2];
            fprintf(stderr,
                "\n========================== WARNING ===========================\n"
                "You have chosen to build NEdit with a known-bad version of Motif,\n"
                "risking instability and probable data loss.  You are very brave!\n"
                "Please do not report bugs to the NEdit developers unless you can\n"
                "reproduce them with a known-good NEdit binary downloaded from:\n"
                "\thttp://www.nedit.org\n"
                "\nHIT ENTER TO CONTINUE\n");
            fgets(buf, 2, stdin);
            return EXIT_SUCCESS;
        }
#else
        showGoodVersions();
        
        fprintf(stderr,
            "\nIf you really want to build a known-bad version of NEdit you\n"
            "can override this sanity check by adding -DBUILD_BROKEN_NEDIT\n"
            "to the CFLAGS variable in your platform's Makefile (e.g.\n"    
            "makefiles/Makefile.linux)\n");
#endif
    }

    if (stability == MotifUnknown)
    {
        /* This version is neither known-good nor known-bad */
        fprintf(stderr, 
                "ERROR:  Untested Motif Version:\n\t%s\n",
                XmVERSION_STRING);

        fprintf(stderr, 
            "You are attempting to build NEdit with a version of Motif that\n"
            "has not been verified to work well with NEdit.  This could be fine,\n"
            "but it could also lead to crashes and instability.  Historically, \n"
            "older versions of Motif have quite often been more stable\n"
            "than newer versions when used with NEdit, so don't assume newer\n"
            "is better.\n");

#ifdef BUILD_UNTESTED_NEDIT
        {
            char buf[2];
            fprintf(stderr,
                "\n========================== WARNING ===========================\n"
                "You have chosen to build NEdit with an untested version of Motif.\n"
                "Please report your success or failure with this version to:\n"
                "\tdevelop@nedit.org\n"
                "\nHIT ENTER TO CONTINUE\n");
            fgets(buf, 2, stdin);
            return EXIT_SUCCESS;
        }
#else
        showGoodVersions();

        fprintf(stderr,
            "\nIf you really want to build an untested version of NEdit you\n"
            "can override this sanity check by adding -DBUILD_UNTESTED_NEDIT\n"
            "to the CFLAGS variable in your platform's Makefile (e.g.\n"    
            "makefiles/Makefile.linux)\n");
#endif
    }

    return EXIT_FAILURE;
}
