/* webmain.c
 *
 * Copyright (c) 2014 by Ted Hess
 * Kitschensync - RPi daemon for Heathkit ID4001

 *
 * Portions used from the Webio Open Source lightweight web server.
 * Copyright (c) 2007 by John Bartas
 * All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include <stdio.h>
#include <string.h>

#include "webio/websys.h"
#include "webio/webio.h"
#include "webio/webfs.h"
#include "wsfdata.h"

#if defined(_FREERTOS)
#include "Board.h"
#include "shell.h"
#endif

#if !defined(portUNUSED_ARG)
#define portUNUSED_ARG(x) ((void)x)
#endif

/* Temp authentication info */
static const char *test_name = "guest";
static const char *test_passwd = "tourist";

int   wfs_auth(void *fd, char *name, char *password);

void *xWebStart(void *args)
{
	int error;

	dprintf("Webio server starting...\n");

thread_restart:
	error = wi_init();
	if(error < 0)
	{
		dprintf("wi_init error %d\n", error);
		return NULL;
	}

	/* Install our port-local authentication routine */
	emfs.wfs_fauth = wfs_auth;

	error = wi_thread();   /* blocks here until killed */
	if(error < 0)
	{
		// Check for restart needed
		if (error == WIE_RESTART)
		{
			dprintf("Webio server restarting\n");
			goto thread_restart;
		}

		dprintf("wi_thread returned error %d\n", error);
	}

	dprintf("...Webio exit(%d)\n", error);

	return NULL;
}

/* Return true if user gets access to the embedded file, 0 if not. */

int wfs_auth(void *fd, char *name, char *password)
{

   /* See if this file requires authentication. */
   EOFILE *eofile;
   em_file const *emf;

   eofile = (EOFILE *)fd;
   emf = eofile->eo_emfile;

   if(emf->em_flags & EMF_AUTH)
   {
      if(strcmp(name, test_name) != 0)
         return 0;
      if(strcmp(password, test_passwd) != 0)
         return 0;
   }
   return 1;
}


void
ws_dtrap(void)
{
   printf("dtrap - need breakpoint\n");
}

void
panic(char * msg)
{
   printf("panic: %s\n", msg);
   dtrap();
pstop:
	goto pstop;
}

#if 0
/* status_ssi()
 *
 * Sample SII routine to dump memory statistics into an output html file.
 *
 */

extern u_long   wi_blocks;
extern u_long   wi_bytes;
extern u_long   wi_maxbytes;
extern u_long   wi_totalblocks;


int
status_ssi(wi_sess * sess, EOFILE * eofile)
{
	portUNUSED_ARG(eofile);

	/* print memory stats to the session's TX buffers */
   wi_printf(sess, "Current blocks: %d <br>", wi_blocks );
   wi_printf(sess, "Current bytes: %d <br>", wi_bytes );
   wi_printf(sess, "Total blocks: %d <br>", wi_totalblocks );
   wi_printf(sess, "Max. bytes: %d <br>", wi_maxbytes );

   return 0;      /* OK return code */
}
#endif
