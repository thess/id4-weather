/* linuxdefs.h
 *
 * Definitions for Linux port of webio demo.
 */

#ifndef _LINUXDEFS_H_
#define _LINUXDEFS_H_    1

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>

#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <malloc.h>
#include <time.h>
#include <sys/time.h>

#define socketclose(s) close(s)

void * memset( void *, int chr, size_t length);

#define stricmp(s,t)  strcasecmp(s,t)
#define strnicmp(s,t,n) strncasecmp(s,t,n)

#define WI_NOBLOCKSOCK(socket) fcntl(socket, F_SETFL, O_NONBLOCK)

#define closesocket(socket) close(socket)

#define cticks GetTickCount()

// Ticks / Sec (ms)
#define TPS 1000

#endif /* LINUXDEFS */
