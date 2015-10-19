/**
 *
 * $Id: BubbleButton.h,v 1.2 2003/12/25 06:55:07 tksoh Exp $
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
#ifndef _BUBBLEBUTTON_H
#define _BUBBLEBUTTON_H

#include <X11/IntrinsicP.h>

#ifndef XltNfadeRate
#define XltNfadeRate (String)"fadeRate"
#endif
#ifndef XltCFadeRate
#define XltCFadeRate (String)"FadeRate"
#endif
#ifndef XltNdelay
#define XltNdelay (String)"delay"
#endif
#ifndef XltCDelay
#define XltCDelay (String)"Delay"
#endif
#define XltNbubbleString (String)"bubbleString"
#define XltCBubbleString (String)"BubbleString"
#define XltNshowBubble (String)"showBubble"
#define XltCShowBubble (String)"ShowBubble"

#ifndef XltNmouseOverPixmap
#define XltNmouseOverPixmap (String)"mouseOverPixmap"
#define XltCMouseOverPixmap (String)"MouseOverPixmap"
#endif
#ifndef XltNmouseOverString
#define XltNmouseOverString (String)"mouseOverString"
#define XltCMouseOverString (String)"MouseOverString"
#endif
#ifndef XltNbubbleDuration
#define XltNbubbleDuration (String)"bubbleDuration"
#endif
#ifndef XltCBubbleDuration
#define XltCBubbleDuration (String)"BubbleDuration"
#endif
#ifndef XltNslidingBubble
#define XltNslidingBubble (String)"slidingBubble"
#endif
#ifndef XltCslidingBubble
#define XltCslidingBubble (String)"SlidingBubble"
#endif
#ifndef XltNautoParkBubble
#define XltNautoParkBubble (String)"autoParkBubble"
#endif
#ifndef XltCautoParkBubble
#define XltCautoParkBubble (String)"AutoParkBubble"
#endif

extern WidgetClass xrwsBubbleButtonWidgetClass;

typedef struct _XltBubbleButtonRec *XltBubbleButtonWidget;
typedef struct _XltBubbleButtonClassRec *XltBubbleButtonWidgetClass;
#if 0
typedef struct {
	int reason;
	char *data;
	int len;
} XltHostCallbackStruct, _XltHostCallbackStruct;
#endif


#define XltIsBubbleButton(w) XtIsSubclass((w), xrwsBubbleButtonWidgetClass)

extern Widget XltCreateBubbleButton(Widget parent,
			     char *name,
			     Arg *arglist,
			     Cardinal argCount);
#endif
