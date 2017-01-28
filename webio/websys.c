/* websys.c
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

#include <ctype.h>

#include "websys.h"     /* port dependant system files */
#include "webio.h"

/* This file contains the routines which change from OS to OS
 * These are:
 *
 * WI_NOBLOCKSOCK(socktype sock) - set a socket to non-blocking mode
 *
 */

#if defined(WIN32) || defined(LWIP_NET)
int
WI_NOBLOCKSOCK(long sock)
{
    int   err;
    int   option = TRUE;

    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&option, sizeof(option));

    err = ioctlsocket((int)sock, FIONBIO, (u_long *)&option);
    return(err);
}
#endif

#if defined(WIN32) || defined(LINUX)

/* format: "Mon, 26 Feb 2007 01:43:54 GMT" */

static char datebuf[36];

extern char *sDayOfWeek[];
extern char *sMonName[];
extern struct tm tmLocalTime;

char *
wi_getdate(wi_sess * sess, struct tm *tmtime)
{
    time_t	timeval;

    USE_ARG(sess);

    timeval = time(NULL);
    if (tmtime == &tmLocalTime)
    {
        // Get local time
        tmLocalTime = *(localtime(&timeval));
    }
    else
    {
        // GMT time
        tmtime = gmtime(&timeval);
    }

    sprintf(datebuf, "%s, %u %s %u %02u:%02u:%02u",
            sDayOfWeek[tmtime->tm_wday],
            tmtime->tm_mday,
            sMonName[tmtime->tm_mon],
            tmtime->tm_year + 1900, /* Windows year is based on 1900 */
            tmtime->tm_hour,
            tmtime->tm_min,
            tmtime->tm_sec);

    return datebuf;
}

#endif

#if defined(WIN32)

portSHORT ks10GetTODClock(void)
{
    time_t	timenow;

    timenow = time(NULL);
    tmLocalTime = *(localtime(&timenow));

    return (60 * tmLocalTime.tm_hour) + tmLocalTime.tm_min;
}

#endif /* WIN32 */

#ifdef LINUX

u_long GetTickCount(void)
{
    struct timespec ts;

    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0;   //error
    }

    // Retrun count in us
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

#endif // LINUX

#ifdef _FREERTOS

#include "FreeRTOS.h"

extern portCHAR *xShellGetTime(struct tm * tmtime);

char *
wi_getdate(wi_sess * sess, struct tm * tmtime)
{
    portUNUSED_ARG(sess);

    return xShellGetTime(tmtime);
}

int stricmp(const char *s1, const char *s2)
{
    unsigned char c1, c2;

    if (s1 == s2)
        return 0;

    do
    {
        c1 = tolower (*s1++);
        c2 = tolower (*s2++);

        if (c1 == '\0')
            break;
    }
    while (c1 == c2);

    return c1 - c2;
}

int strnicmp (const char *s1, const char *s2, size_t n)
{
    unsigned char c1, c2;

    if (s1 == s2 || n == 0)
        return 0;

    do
    {
        c1 = tolower(*s1++);
        c2 = tolower(*s2++);

        if ((--n == 0) || (c1 == '\0'))
            break;
    }
    while (c1 == c2);

    return c1 - c2;
}

//
// Socket abstraction routines for iChip
//

#endif
