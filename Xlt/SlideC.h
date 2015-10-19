/**
 *
 * $Id: SlideC.h,v 1.1 2003/12/23 08:34:36 tksoh Exp $
 *
 * Copyright (C) 1996 Free Software Foundation, Inc.
 * Copyright © 1999-2001 by the LessTif developers.
 *
 * This file is part of the GNU LessTif Extension Library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **/

#ifndef _SLIDEC_H
#define _SLIDEC_H

#include <X11/Intrinsic.h>

#ifndef XltIsSlideContext
#define XltIsSlideContext(w) XtIsSubclass(w,xltSlideContextClass)
#endif
#ifndef XltNslideFinishCallback
#define XltNslideFinishCallback (String)"slideFinishCallback"
#endif
#ifndef XltCSlideFinishCallback
#define XltCSlideFinishCallback (String)"SlideFinishCallback"
#endif
#ifndef XltNslideMotionCallback
#define XltNslideMotionCallback (String)"slideMotionCallback"
#endif
#ifndef XltCSlideMotionCallback
#define XltCSlideMotionCallback (String)"SlideMotionCallback"
#endif
#ifndef XltNslideWidget
#define XltNslideWidget (String)"slideWidget"
#endif
#ifndef XltCSlideWidget
#define XltCSlideWidget (String)"SlideWidget"
#endif
#ifndef XltNslideInterval
#define XltNslideInterval (String)"slideInterval"
#endif
#ifndef XltCSlideInterval
#define XltCSlideInterval (String)"SlideInterval"
#endif
#ifndef XltNslideDestWidth
#define XltNslideDestWidth (String)"slideDestWidth"
#endif
#ifndef XltCSlideDestWidth
#define XltCSlideDestWidth (String)"SlideDestWidth"
#endif
#ifndef XltNslideDestHeight
#define XltNslideDestHeight (String)"slideDestHeight"
#endif
#ifndef XltCSlideDestHeight
#define XltCSlideDestHeight (String)"SlideDestHeight"
#endif
#ifndef XltNslideDestX
#define XltNslideDestX (String)"slideDestX"
#endif
#ifndef XltCSlideDestX
#define XltCSlideDestX (String)"SlideDestX"
#endif
#ifndef XltNslideDestY
#define XltNslideDestY (String)"slideDestY"
#endif
#ifndef XltCSlideDestY
#define XltCSlideDestY (String)"SlideDestY"
#endif

extern WidgetClass xltSlideContextWidgetClass;

typedef struct _XltSlideContextRec *XltSlideContextWidget;
typedef struct _XltSlideContextClassRec *XmSlideContextWidgetClass;

typedef struct _XltSlideStruct {
	Widget w;
	XtWidgetGeometry dest;
	unsigned long interval;
	XtIntervalId id;
} XltSlideStruct, *XltSlidePtr;

void XltSlide(XltSlidePtr slide_info);

#endif
