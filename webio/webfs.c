/* webfs.c
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

#include "websys.h"     /* port dependant system files */
#include "webio.h"
#include "webfs.h"


/* This file contins webio file access routines. The external "wi_f" entry
 * points have the same semantics as C buffered file IO (fopen, etc). These
 * determine which of the actual file systems should be called and make
 * the call.
 */

#ifdef WI_STDFILES
// Internal routine for *nix fs
static int   fs_fgetsize(void * fd);
/* Voiding these pointers is ugly, but so are the Linux declarations. */
wi_filesys sysfs = {
   (void*)fopen,
   (void*)fread,
   (void*)fwrite,
   (void*)fclose,
   (void*)unlink,
   (void*)fs_fgetsize,
   NULL, NULL
};
#endif   /* WI_STDFILES */

#ifdef WI_EFSFAT
wi_filesys effs = {
   effs_open,
   effs_read,
   NULL,
   effs_close,
   NULL,
   effs_getsize,
   NULL, NULL
};
#endif   /* WI_EFSFAT */

#ifdef WI_EMBFILES
wi_filesys emfs = {
   em_fopen,
   em_fread,
   NULL,
   em_fclose,
   NULL,
   em_fgetsize,
   NULL, NULL
};
#endif   /* WI_EMBFILES */

/* Table ofthe supported file systems */
wi_filesys * wi_filesystems[] =    /* list of file systems */
{
#ifdef WI_EMBFILES
   &emfs,
#endif
#ifdef WI_EFSFAT
   &effs,
#endif
#ifdef WI_STDFILES
   &sysfs,
#endif
   NULL     /* reserved for runtime entry */
};


wi_file *      wi_allfiles;   /* list of all open files */

/* wi_fopen()
 *
 * webio top level file open routine. This is just wrapper for the lower
 * level routine - either the Embedded FS, and the host system's native FS.
 *
 * Returns: 0 if OK else negative WIE_ error code.
 *
 */

int
wi_fopen(wi_sess * sess, char * name, char * mode)
{
   wi_filesys *   fsys;    /* File system which has the file */
   wi_file *      newfile; /* transient file structrure */
   void *         fd;      /* descriptior from fs */
   u_long         i;

   /* Loop through the FS list, trying an open on each */
   for(i = 0; i < sizeof(wi_filesystems)/sizeof(wi_filesys*); i++)
   {
      fsys = wi_filesystems[i];
      if(fsys == NULL)
         continue;
      fd = fsys->wfs_fopen(name, mode);
      if(fd)
      {
         /* Got an open - create a wi_file & fill it in. */
         newfile = wi_newfile(fsys, sess, fd);
         if(!newfile)
         {
            fsys->wfs_fclose(fd);
            return WIE_MEMORY;
         }

         return 0;
      }
   }
   return WIE_NOFILE;
}


int
wi_fread(char * buf, unsigned size1, unsigned size2, WI_FILE * fd)
{
   int   bytes;
   bytes = fd->wf_routines->wfs_fread(buf, size1, size2, fd->wf_fd);
   return bytes;
}

int
wi_fwrite(char * buf, unsigned size1, unsigned size2, WI_FILE * fd)
{
   int   bytes = 0;

   if (fd->wf_routines->wfs_fwrite)
   {
       bytes = fd->wf_routines->wfs_fwrite(buf, size1, size2, fd->wf_fd);
   }

   return bytes;
}

int
wi_fclose(WI_FILE * fd)
{
   int   error;

   /* close file at lower level, get an error code */
   error = fd->wf_routines->wfs_fclose(fd->wf_fd);

   /* Delete our intermediate layer struct for this file. */
   wi_delfile(fd);

   return error;    /* return error from lower layer delete */
}

int
wi_fremove(char * name)
{
	wi_filesys *fsys;
	u_long	i;
	int		res = 0;

	/* Loop through the FS list, trying an remove on each */
	for(i = 0; i < sizeof(wi_filesystems)/sizeof(wi_filesys*); i++)
	{
		fsys = wi_filesystems[i];
		if(!fsys)
			continue;

		if (fsys->wfs_fremove)
		{
			res = fsys->wfs_fremove(name);
		}
	}

	return res;
}

int
wi_fgetsize(WI_FILE * fd)
{
	return (fd->wf_routines->wfs_fgetsize(fd->wf_fd));
}

#ifdef WI_STDFILES
static int
fs_fgetsize(void *fd)
{
	int   current, filelen;

	current = ftell((FILE *)fd);
	fseek((FILE *)fd, 0, SEEK_END);
	filelen = ftell((FILE *)fd);
	fseek((FILE *)fd, current, SEEK_SET);

	return filelen;
}
#endif


/***************** Optional embedded FS starts here *****************/
#ifdef USE_EMFILES

#include "wsfdata.h"

/* Set up master list of embedded files. If "efslist[]" is an unresolved
 * external when you link then you have neglected to provide the data
 * for the embedded files. The normal way ofdoing this is to use the
 * HTML compiler to produce one or more C files containing the data.
 * this process will define and generate efslist[] for you.
 */
em_file const * emfiles = &efslist[0];

/* transient list of em_ files which are currently open */
EOFILE * em_openlist;

/* em_verify()
 *
 * Make sure a passed fd is really an EOFILE.
 *
 * Returns 0 if it is, or WIE_BADFILE if not.
 */
int
em_verify(EOFILE * fd)
{
   EOFILE *    eofile;

   /* verify file pointer is valid */
   for(eofile = em_openlist; eofile;eofile = eofile->eo_next)
   {
      if(eofile == fd)
         break;
   }
   if(!eofile)
      return WIE_BADFILE;

   return 0;
}

WI_FILE *
em_fopen(char * name, char * mode)
{
   em_file const *   emf;
   EOFILE *    eofile;
   char *      cmpname = name;

   /* Search the efs a name matching the one passed. */
   for(emf = emfiles; emf; emf = emf->em_next)
   {
      if(emf->em_name[0] != *cmpname)  /* fast test of first char */
         continue;

      if(strcmp(emf->em_name, cmpname) == 0)
         break;
   }
   if(!emf)             /* If file not in list, return NULL */
      return NULL;

   if( *mode != 'r' )   /* All files are RO,otherwise return NULL */
      return NULL;

   /* We're going to open file. Allocate the transient control structure */
   eofile = (EOFILE *)wi_alloc(sizeof(EOFILE), 'IFOE');
   if(!eofile)
      return NULL;
   eofile->eo_emfile = emf;
   eofile->eo_position = 0;

   /* Add new open struct to open files list */
   eofile->eo_next = em_openlist;
   em_openlist = eofile;

   return ( (WI_FILE*)eofile);
}

int
em_fread(char * buf, unsigned size1, unsigned size2, void * fd)
{
   unsigned    datalen;    /* length of data to move */
   EOFILE *    eofile;
   em_file const *   emf;
   int         error;

   eofile = (EOFILE *)fd;
   error = em_verify(eofile);
   if(error)
      return error;

   emf = eofile->eo_emfile;

   /* SSI and forms should not make it this far down the call chain */
   if(emf->em_flags & (EMF_SSI|EMF_FORM))
   {
      dtrap();
      return 0;
   }

   /* handle server push */
   if(emf->em_flags & EMF_PUSH)
   {
      dprintf("Server push call...\n");
      //dtrap(); /* later */
   }

   datalen = size1 * size2;
   if(datalen > (emf->em_size - eofile->eo_position))
      datalen =  emf->em_size - eofile->eo_position;

   /* Check for position at End of File - EOF */
   if(datalen == 0)
      return 0;

   memcpy(buf, &emf->em_data[eofile->eo_position], datalen);
   eofile->eo_position += datalen;

   return datalen;
}

int
em_fclose(void * voidfd)
{
   EOFILE *    passedfd;
   EOFILE *    tmpfd;
   EOFILE *    last;

   passedfd = (EOFILE *)voidfd;

   /* verify file pointer is valid */
   last = NULL;
   for(tmpfd = em_openlist; tmpfd; tmpfd = tmpfd->eo_next)
   {
      if(tmpfd == passedfd)  /* If we found it, unlink */
      {
         if(last)
            last->eo_next = passedfd->eo_next;
         else
            em_openlist = passedfd->eo_next;
         break;
      }
      last = tmpfd;
   }

   if(tmpfd == NULL)       /* fd not in list? */
      return WIE_BADFILE;

   wi_free(passedfd, 'IFOE');
   return 0;
}

int em_fgetsize(void * fd)
{
	EOFILE *	emf;
	int			error;

   emf = (EOFILE *)fd;
   error = em_verify(emf);
   if(error)
      return error;

	return emf->eo_emfile->em_size;
}

int
em_push(void * fd, wi_sess * sess)
{
   int         error;
   EOFILE *    emf;
   PUSH_ROUTINE * pushfunc;

   emf = (EOFILE *)fd;
   if(emf->eo_emfile->em_routine == NULL)
	   return WIE_BADFILE;


   /* call embedded files embedded function */
   dtrap();
   pushfunc = emf->eo_emfile->em_routine;
   error = pushfunc(sess, emf);

   return error;
}


#endif  /* USE_EMFILES */

/***************** Optional ChaN FatFS support starts here *****************/
#ifdef WI_EFSFAT

WI_FILE *
effs_open(char * name, char * mode)
{
	FIL *efsfile;
	FRESULT	res;

	// All files are RO,otherwise return NULL
	if(*mode != 'r')
		return NULL;

	// Allocate FIL struct
	efsfile = (FIL *)wi_alloc(sizeof(FIL), ' LIF');
	if(efsfile == NULL)
		return NULL;

#ifdef WI_EFSFAT
	int		tmo = MMC_MOUNT_TIMEOUT;

	// Wait for MMC available
	while (--tmo >= 0)
	{
		if (f_mount(0, &hFS) == FR_OK)
			break;
		vTaskDelay(1000 / portTICK_RATE_MS);
	}

	// Check for timeout
	if (tmo < 0)
		return NULL;
#endif

	res = f_open(efsfile, name, FA_READ);
	if (res != FR_OK)
    {
#ifdef WI_EFSFAT
		f_mount(0, NULL);
#endif
        wi_free(efsfile, ' LIF');
		return NULL;
    }

	return (WI_FILE *)efsfile;
}

int effs_read(char * buf, unsigned size1, unsigned size2, void * fd)
{
	FRESULT			res;
	unsigned int	bytesread;

	res = f_read((FIL *)fd, buf, size1 * size2, &bytesread);
	if (res != FR_OK)
		return 0;

	return bytesread;
}

int effs_close(void * fd)
{
	f_close((FIL *)fd);
#ifdef WI_EFSFAT
	f_mount(0, NULL);
#endif
	wi_free(fd, ' LIF');

	return 0;
}

int effs_getsize(void * fd)
{
	return f_size((FIL *)fd);
}

#endif /* USE_EFSFAT */
