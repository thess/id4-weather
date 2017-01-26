/* websys.h
 *
 * Part of the Webio Open Source lightweight web server.
 *
 * Copyright (c) 2007 by John Bartas
 * All rights reserved.
 *
 * Use license: Modified from standard BSD license.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation, advertising
 * materials, Web server pages, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by John Bartas. The name "John Bartas" may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#ifndef _WEBSYS_H_
#define _WEBSYS_H_    1

/* This file contains definitions intended for modification during porting */

/*********** Optional webio features - comment out to remove ***************/

#define WI_EMBFILES 1       /* Use embedded FS */
#if defined(_FREERTOS)
#define WI_EFSFAT	1		/* Use ChaN FatFs */
#else
#define WI_STDFILES	1		/* Use system "fopen" files */
#endif

#define WI_THREAD   1     /* Drive webio with a thread rather than polling */


/*********** Webio sizes and limits ***************/

#ifndef _FREERTOS
#define WI_RXBUFSIZE    1536  /* rxbuf[] total size */
#define WI_TXBUFSIZE    1400  /* txbuf[] section size */
#define WI_MAXURLSIZE   512   /* URL buffer size */
#define WI_FSBUFSIZE    4096  /* file read buffer size */
#else
#define WI_RXBUFSIZE    1024  /* rxbuf[] total size */
#define WI_TXBUFSIZE    1400   /* txbuf[] section size */
#define WI_FSBUFSIZE    2048   /* file read buffer size */
#endif

#define WI_PERSISTTMO   300   /* persistent connection timeout */

/*********** OS portability ***************/

#ifdef BUSTER

#include "../buster/busterport.h"

#else   /* not BUSTER */

#ifndef MEMCPY
#define MEMCPY(dest,src,size)    memcpy(dest,src,size)
#endif

#endif  /* BUSTER */


#ifdef LINUX
#include "linuxdefs.h"
#endif	/* LINUX */
#ifdef _WIN32
#include "windowsdefs.h"
#endif /* _WIN32 */

//
// Definitions for FreeRTOS in GNU GCC environment
//
#ifdef _FREERTOS
#include "FreeRTOSdefs.h"

#else	/* _FREERTOS */

/* Define TPS (Ticks Per Second). If this is contained an another project with
 * TPS defined (eg Buster) then use the external definition.
 */
#ifndef TPS
#define TPS 50
#endif /* no TPS */

extern u_long cticks;

/* Map Webio heap routine to system's */
#define WI_MALLOC(size)     malloc(size)
#define WI_FREE(size)       free(size)

#endif	/* _FREERTOS */


/*********** Network portability ***************/


#ifdef BUSTER
extern   int      WI_NOBLOCKSOCK(long sock);
#endif  /* BUSTER or not */

typedef long socktype;


/*********** File system mapping ***************/

#define  USE_EMFILES 1
#define  USE_SYSFILES 1
#include <stdio.h>

/*********** debug support **************/

#ifndef BUSTER
extern   void     ws_dtrap(void);
#define  dtrap()  ws_dtrap()
#define  dprintf  printf

#include <stdarg.h>

#ifndef USE_ARG
#define USE_ARG(c) (c=c)
#endif  /* USE_ARG */
#endif  /* BUSTER */

void panic(char * msg);

extern u_long wi_totalblocks;

#endif   /* _WEBSYS_H_ */
