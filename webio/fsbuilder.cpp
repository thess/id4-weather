/* fsbuild.c
 *
 * Part of the Webio Open Source lightweight web server.
 *
 * Copyright (c) 2007-2008 by John Bartas
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>

#ifdef __GNUC__
#define stricmp strcasecmp
#endif /* LINUX */


/**
 * This file contains a standalone application to open multiple
 * web content files and convert them into "C" language data
 * arrays. These arrays can be compiled into a webio file server
 * for use as an Embedded File System (EFS). The EFS files may
 * also contain a pointer to a C function intead of data - The
 * webio server will call these functions when the file in
 * invoked, and the functions will then provide dynamic content.
 */



/* option values for each input file */

#define  NAMELENGTH  32    // Max length of file and variable names

#define  TAGSIZE     256   // Max length of HTML tag args

/* input file (must be specified) */
char  listfilename[NAMELENGTH];

// Default output files
char  def_datafile[NAMELENGTH] = {"wsfdata.c"};
char  def_hdrfile[NAMELENGTH] = {"wsfdata.h"};
char  def_codefile[NAMELENGTH] = {"wsfcode.c"};

// filedata->opmask values
#define  OPT_AUTH    0x0001
#define  OPT_CACHE   0x0002
#define  OPT_SSI     0x0004
#define  OPT_CEXP    0x0008
#define  OPT_PUSH    0x0010
#define  OPT_FORM    0x0020
#define  OPT_DOXYGEN 0x0040
#define  OPT_THREAD  0x0080     // Added Sept-08, requires updated server code
#define  OPT_NOEFS   0x0100     // Added Sept-08 - parse only, no EFS entry


// Default option mask
long  def_mask = 0;


int      infiles = 0;   // total number of input files

/* all purpose buffer for file reading */
unsigned char  readbuf[4096];


/* Max. number of output files (all types) allowed */
#define  OUTPUTFILES 25

#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   1
#endif


#define  NON_BINARY_FILE  (OPT_SSI|OPT_PUSH|OPT_CEXP|OPT_FORM)

void
dtrap()
{
   printf("dtrap");
}

void
app_exit(int code)
{
   if(code < 0)
   {
      dtrap();
   }
   exit(-1);
}

int
argcpy(char * dest, char * src, int maxlength )
{
   int   length = 0;

   if(src == NULL)
	   return  -1;
   if(dest == NULL)
	   return -1;
   if(maxlength < 1)
	   return -1;

   while(*src > ' ')
   {
      if(length++ > maxlength)
         return -1;

      *dest++ = *src++;
   }
   *dest = 0;     // Null terminate copied string
   return 0;
}

int
argcmp(char * dest, char * src, int maxlength )
{
   int   length = 0;

   while(*src > ' ')
   {
      if(length++ > maxlength)
         return -1;

      if(*dest++ != *src++)
         return -1;
   }
   return 0;
}

/* tagcmp(char * tag1, char * tag2 )
 *
 * Compare two html tag or anme values. The tags are considered terminated if
 * the parser encounters ">", null, quote, or space.
 */

int
tagcmp(char * tag1, char * tag2)
{
   while(*tag1 > ' ')
   {
      // if tag ended return true
      if((*tag1 == '>') ||
         (*tag1 == '\'') ||
         (*tag1 == '"'))
         return TRUE;

      // case insensitive compare - exit if not same
      if((*tag1++ | 0x20)  != (*tag2++ | 0x20))
         return FALSE;
   }
   // tag ended, return true
   return TRUE;
}

char *
argnext(char * arg)
{
   if(arg == NULL)
      return NULL;
   if(*arg <= ' ')
      return NULL;
   while(*arg > ' ')
      arg++;
   while((*arg == ' ') ||(*arg == '\t'))
      arg++;
   if(*arg < ' ')
      return NULL;
   else
      return arg;
}

int
argterm(char * arg)
{
   while(*arg > ' ')
      arg++;
   *arg = 0;
   return 0;
}


/* get_tagparm(char * parm, char * html)
 *
 * Extract a tag parameter from passed html string. parm should be
 * the name of the parameter,such as "name" or "color".
 *
 * The html should contain the parm and a value, e.g. "color=red". The
 * value may be in single or double quotes.
 *
 * parm is not case-sensitive, however the returned value has case preserved.
 *
 * Returns C string name in new static buffer if OK
 * Returns NULL if parm is not found in html, or the parameter value is
 *    not properly terminated.
 */

char *
get_tagparm(char * parm, char * html, char * dest)
{
   int      i;
   char *   cp;
   bool     startquote;

   *dest = 0;  // clear return value - assume failure

   // First loop serches for parm in html,
   for( cp = html; *cp >= ' '; cp++)
   {
      if(*cp == '>')    // end of tag, parm not found
         return NULL;
      if((*cp == *parm) && tagcmp(parm, cp))
         break;
   }
   if(*cp <= ' ')       // end of html, parm not found
      return NULL;

   cp += strlen(parm);  // advance pointer over parm test

   if(*cp++ != '=')     // require "=" sign after parm name
      return NULL;

   // Allow a quote after the equals sign
   if((*cp == '\'') || (*cp == '"'))
   {
	   startquote = TRUE;
         cp++;
		 }
   else
	   startquote = FALSE;

   // copy parameter value into buffer
   for(i = 0; i < TAGSIZE; i++)
   {
      if(((*cp == ' ') && (startquote==FALSE)) ||
         (*cp == '\'') ||
         (*cp == '"'))
      {
         break;
      }
      *dest++ = *cp++;
   }

   // check buffer size, null-terminate if OK
   if(i >= (TAGSIZE - 1))
      return NULL;
   else
      *dest = 0;

   return dest;
}


/**
 * Filter characters which are not C-friendly from a passed string. This allows
 * a-z, A-Z, 0-9 and underscores to stay. Anything else gets replaced with
 * underscore.
 *
 * \param
 * outname - buffer to receive converted string
 *
 * \param
 * maxoutput - size of buffer to receive converted string
 *
 * \param
 * inname - input string (must be null terminated)
 *
 * \returns 0 if all chars replaced, -1 if ran out of output buffer
 */

int
filterchars(char * outname, int maxoutput, char * inname)
{
	int inchars = 0;

	while(*inname && (++inchars < maxoutput))
    {
        if(((*inname >= '0') && (*inname <= '9')) ||
			((*inname >= 'A') && (*inname <= 'Z')) ||
			((*inname >= 'a') &&(*inname <= 'z')))
		{
            *outname++ = *inname++;
		} else {
            *outname++ = '_';
            inname++;
		}
	}
    *outname = 0;
    if(inchars == 0)
		return -1;
    else
		return 0;
}


/* The options set per-file. One of these structures is included in every
 * file object. These are also used to define a single static structure
 * to build the options for each file while the file's line is read from
 * the input list.
 */

struct option_set
{
   int   opmask;                 /* options from command line */
   char  datafile[NAMELENGTH];   /* name of output C file w/content */
   char  codefile[NAMELENGTH];   /* name of C file for SSI & form code */
   char  hdrfile[NAMELENGTH];    /* name of output header file */
   char  routine[NAMELENGTH];    /* name of SSI or CGI routine */
};

/* The scratch single static structure used for building option sets */
struct option_set optionTmp;


/* filedata  - one of these is made for each input file */
class filedata
{
public:
   filedata * next;

   char     filename[NAMELENGTH];   /* name of file */
   char     caname[NAMELENGTH];     /* name of C array */
   long     casize;                 /* bytes in C array */
   struct   option_set opset;
   long     flags;                  /* FD_ flags, NOT arg options */
   int      filenumber;             /* 1 - through infiles */

   char *   ccode;                  /* code for C expression, if any */

   filedata(char * filename, int mask);
   ~filedata();
};

#define  FD_HASFORM     0x01
#define  FD_HASSSI      0x02


filedata * fdlist;
filedata * fdtail;

filedata::filedata(char * newfilename, int defmask)
{
   memset(this, 0, sizeof(filedata));

   /* add new item to linked list at end */
   if(fdtail)
   {
      fdtail->next = this;
      fdtail = this;
   }
   else
   {
      fdlist = fdtail = this;
   }

   argcpy(filename, newfilename, NAMELENGTH);
   opset.opmask = defmask;
   filenumber = ++infiles;
   return;
}

filedata::~filedata()
{
   filedata * tmp;
   filedata * last = NULL;

   /* removed deleteing item from list */
   for(tmp = fdlist; tmp; tmp = tmp->next)
   {
      if(tmp == this)
      {
         if(last)
            last->next = tmp->next;
         else
            fdlist = tmp->next;

         /* maintain list tail pointer */
         if(fdtail == this)
            fdtail = last;
      }
      last = tmp;
   }
}

/* ssifile - one of these is made for each ssi found */

class ssifile
{
public:
   ssifile * next;
   char     ssifilename[NAMELENGTH];   /* name of file */
   filedata * infile;

   ssifile(char * ssiname, filedata * );
};

ssifile * ssifiles;

ssifile::ssifile(char * ssiname, filedata * file)
{
   strncpy(ssifilename, ssiname, NAMELENGTH);
   infile = file;
   next = ssifiles;
   ssifiles = this;
   return;
}

class control;

/* form - object containing info for a form that has been
 * read from an HTML file. This is used to construct sample C code
 * to process the form at runtime.
 */

enum     form_method { UNDEFINED = 0, GET, POST };

class form
{
public:
   form *   next;
   char     formname[NAMELENGTH];
   filedata * file;
   control * controls;
   int      ctlcount;
   int      flags;

   enum     form_method method;
   char     action[NAMELENGTH];

   form(char * name);
};

#define FF_CCODED    0x01     // C code has been generated

form * formlist;

form::form(char * name)
{
    memset(this, 0 , sizeof(form));
    strncpy(formname, name, sizeof(formname));
    next = formlist;
    formlist = this;
}

class control
{
public:
    control * next;

    char ctltype[TAGSIZE];  // type from html
    char hname[TAGSIZE];    // name from html
    char cname[TAGSIZE];    // name converted to C variable
    char value[TAGSIZE];    // default value in html

    control(form * form, char * html);
};

control::control(form * form, char * html)
{
    memset(this, 0, sizeof(this));
    if(get_tagparm("type", html, ctltype) == NULL)
    {
        printf("form has no name\n");
        app_exit(-1);
    }
    get_tagparm("name", html, hname);
    get_tagparm("value", html, value);

	// Conver HTML name to a C variable name
    filterchars(cname, TAGSIZE, hname);

    // attach new control to passed form
    this->next = form->controls;
    form->controls = this;

    return;
}


/* Option details for orthogonal processing. The saem structure
 * supports command line options and per-file options from the
 * line of the input file list.
 */

struct option
{
   char     opt_char;      // char to invoke option
   char *   opt_help;      // help text
   int      opt_flags;     // OF_CMDLINE or not
   int      (*opt_setopt)(struct option *, char * parm);
   void *   target;        //parameter for setop
   long     opt_maskbit;   // flag to effect (if any)
};

/* bit values for opt_flags bitmask */
#define OF_CMDLINE   1     // option is allowed on command line


/* Generic option processing routines. These all accept:
 * 1) The structure for the option to be processed,
 * 2) pointer to the remaining args in the input line
 *
 * They all return the number of args from the input line processed. This
 * will be 0 if no additional args are used.
 */

int
opt_setbool(struct option * opt, char * parm)
{
   long *  maskp;

   /* Set the option's mask bit in the pased mask */
   maskp = (long*)(opt->target);
   *maskp |= opt->opt_maskbit;

   return 0;
}


int
opt_setstring(struct option * opt, char * parm)
{
   int      error;

   if(!parm)
   {
      dtrap();
      printf("error: arg '%c' requires a parameter\n", opt->opt_char);
      app_exit(-1);
   }
   error = argcpy( (char*)opt->target, parm, NAMELENGTH);
   if(error)
   {
      printf("command line arg '%s' too long\n", parm);
      app_exit(-1);
   }

   return 1;
}

/* opt_makefunc()
 *
 * Make an SSI, CGI, or server-side push 'code' function.
 *
 * "char * parm" is the args line, which should start with the name
 * for C routine to map to the the SSI, PUSH or FORM file.
 *
 *
 * returns 0 if OK.
 */

int
opt_makefunc(struct option * opt, char * parm)
{
   struct option_set * option = (struct option_set *)opt->target;

   option->opmask |= opt->opt_maskbit;

   if(parm == NULL)
   {
	   printf("Missing required arp for option '%c'\n", opt->opt_char);
	   return -1;
   }
   /* Just save routine name, output pass will generate code. */
   argcpy(option->routine, parm, NAMELENGTH);

   return 1;   /* used one term of parm string */
}



/* maketoken()
 *
 * convert passed string into a unique token for switch/case statement
 */

char tokenbuf[NAMELENGTH + 9];


enum caseparms { UPPERCASE, LOWERCASE, NOCASE};

char *
maketoken(char * name, unsigned number, enum caseparms caseparm)
{
   int   length = 0;

   while (*name && (length < NAMELENGTH))
   {
      if((*name >= 'a') && (*name <= 'z'))
      {
         if(caseparm == UPPERCASE)
            tokenbuf[length++] = (*name) & ~0x20;
         else
            tokenbuf[length++] = *name;
      }
      else if((*name >= 'A') && (*name <= 'Z'))
      {
         if(caseparm == LOWERCASE)
            tokenbuf[length++] = (*name) | 0x20;
         else
            tokenbuf[length++] = *name;
      }
      else if((*name >= '0') && (*name <= '9'))
         tokenbuf[length++] = *name;
      else
         tokenbuf[length++] = '_';

      name++;
   }
   if(number != 0)
      sprintf(&tokenbuf[length], "%u", number);
   else
      tokenbuf[length] = 0;

   return tokenbuf;
}

int
opt_setcexp(struct option * opt, char * parm)
{
   char *   ctype;
   char *   code;
   char *   codebuf;    /* buffer to store generated code */
   char *   cp;
   filedata *  file;
   struct option_set * option = (struct option_set *)opt->target;

   /* first arg should be one of the C types */
   ctype = parm;
   code = argnext(parm);
   argterm(code);
   file = fdtail;

   codebuf = (char*)calloc(1, strlen(parm) + 100);
   if(!codebuf)
   {
      printf("opt_setcexp: Memory exhausted\n");
      app_exit(-1);
   }

   cp = codebuf;
   cp += sprintf(cp, "   case %s:\n", maketoken(file->filename, file->filenumber, UPPERCASE) );

   if(strncmp(ctype, "u_long", 6) == 0)
   {
      sprintf(cp, "      e = wi_putlong(sess, (u_long)(%s));", code);
   }
   else if(strncmp(ctype, "int", 3) == 0)
   {
      sprintf(cp, "      e = wi_putlong(sess, (int)(%s));", code);
   }
   else if(strncmp(ctype, "char*", 3) == 0)
   {
      sprintf(cp, "      e = wi_putstring(sess, (char*)(%s));", code);
   }
   else if (strncmp(ctype, "time_t", 6) == 0)
   {
	   if (strcmp(code, "NULL") == 0)
			sprintf(cp, "      e = wi_putstring(sess, wi_getdate(sess, NULL);");
	   else
			sprintf(cp, "      e = wi_putstring(sess, wi_getdate(sess, (struct tm*)&%s));", code);
   }
   else
   {
      printf("Unhandled C type in expression: %s\n", parm);
      app_exit(-1);
   }

   file->ccode = codebuf;  /* Save for output pass */
   option->opmask |= opt->opt_maskbit;

   return 2;
}



struct option options[] =
{
   {  'a', "require authentication", OF_CMDLINE,
      opt_setbool, &def_mask, OPT_AUTH },
   {  'c', "enable cache control for all files", OF_CMDLINE,
      opt_setbool, &def_mask, OPT_CACHE },
   {  'd', "doxygen compatible comments", OF_CMDLINE,
      opt_setbool, &def_mask, OPT_DOXYGEN },
   {   'o', "<outfile> send C data arrays to named file", OF_CMDLINE,
      opt_setstring, &def_datafile, 0 },
   {   'h', "<outfile> send C headers to named file", OF_CMDLINE,
      opt_setstring, &def_hdrfile, 0 },
   {   'g', "<filename> file for C code to handle SSI or form data", OF_CMDLINE,
      opt_setstring, &optionTmp.codefile, 0 },
   {   'a', "require authentication", 0,
      opt_setbool, &optionTmp.opmask, OPT_AUTH },
   {   'c', "enable cache control", 0,
      opt_setbool, &optionTmp.opmask, OPT_CACHE },
   {   'o', "<outfile> send C data array output to named file", 0,
      opt_setstring, &optionTmp.codefile, 0 },
   {   's', "<funcname> data comes from named function (generated)", 0,
      opt_makefunc, &optionTmp, OPT_SSI },
   {   'f', "<funcname> file maps to C code form handler", 0,
      opt_makefunc, &optionTmp, OPT_FORM  },
   {   'p', "<funcname> file is server push, generate function", 0,
      opt_makefunc, &optionTmp, OPT_PUSH },
   {   'g', "<filename> file for C code to handle SSI or form data", 0,
      opt_setstring, &optionTmp.codefile, 0 },
   {   'e', "<exp> file maps to a \"C\" expression", 0,
      opt_setcexp, &optionTmp.opmask, OPT_CEXP },
   {   't', "Exec SSI or CGI function on dedicated thread", 0,
      opt_setbool, &optionTmp, OPT_THREAD },
   {   'n', "Parse for form data only, don;t make EFS entry", 0,
      opt_setbool, &optionTmp, OPT_NOEFS },
};

#define NUMOPTIONS   sizeof(options)/sizeof(struct option)


void
usage()
{
   int      i;

   printf("Usage: fxbuild [options] filelist \n");
   printf("   Command line options\n");
   for(i = 0; i < sizeof(options)/sizeof(struct option); i++)
   {
      if(options[i].opt_flags & OF_CMDLINE)
         printf("   -%c %s\n",options[i].opt_char, options[i].opt_help );
   }

   printf("   filelist per-file options\n");
   for(i = 0; i < sizeof(options)/sizeof(struct option); i++)
   {
      if((options[i].opt_flags & OF_CMDLINE) == 0)
         printf("   -%c %s\n",options[i].opt_char, options[i].opt_help );
   }
   printf("   \n");
}


int
setoption(char * argp, char * argnext)
{
   int   i;
   int   aadd;    // amount ot add to arg count

   argp++;   // point past '-' to arg char
   for(i = 0; i < NUMOPTIONS; i++)
   {
      /* If parsing cmd line, skip non-command line options; etc. */
      if(fdlist == NULL)   // no fdlist means cmd line
      {
         if((options[i].opt_flags & OF_CMDLINE) == 0)
            continue;
      }
      else  // not cmd line, skipcmd line options
      {
         if(options[i].opt_flags & OF_CMDLINE)
            continue;
      }

      if(options[i].opt_char == *argp )
      {
         aadd = options[i].opt_setopt( &options[i], argnext);
         if(aadd < 0)   // error parsing arg?
         {
            printf("Fatal error on arg -'%s'\n", argp );
            return -1;
         }
         return aadd;
      }
   }
   return -1;
}

int
parseargs(int argc, char * argv[] )
{
   int      aadd;    // args to add to argx
   int      argx;
   char *   argp;


   for(argx = 1; argx < argc; argx++)
   {
      argp = argv[argx];
      if(*argp != '-')
      {
         // Last option is the input file
         if(argx == (argc - 1) )
         {
            strncpy(listfilename, argv[argx], sizeof(listfilename) );
            break;
         }
         else
         {
            printf("all command line args must start with '-'\n");
            usage();
            app_exit(1);
         }
      }

      aadd = setoption(argp, argv[argx+1]);
      if(aadd < 0)
      {
         printf("Option %c not valid.\n", *argp);
         usage();
         app_exit(1);
      }
      argx += aadd;  // bump past any arg parameters used
   }
   return 0;
}


void
mk_cname(filedata * file)
{
   char * cname;
   char * fname;

   fname = file->filename;
   while (char ch = *fname)
   {
	   if (ch == '\\') *fname = '/';
	   fname++;
   }

   cname = file->caname;
   filterchars(cname, NAMELENGTH, file->filename);

   /* Add the file number to enhance uniquness */
   cname += strlen(cname);
   sprintf(cname, "%u", file->filenumber);

   return;
}


FILE *
open_outfile(char * name, int code)
{
   FILE * fd;

   fd = fopen(name, "wb");
   if(!fd)
   {
      printf("Fatal: unable to create output file %s\n", name);
      return NULL;
   }
   fprintf(fd, "/* %s\n * \n", name);
   fprintf(fd, " * This file was created by an automated utility. \n");
   fprintf(fd, " * It is not intended for manual editing\n */\n\n");

   if(code)
   {
      fprintf(fd, "#include \"webio/websys.h\"\n");
      fprintf(fd, "#include \"webio/webio.h\"\n");
      fprintf(fd, "#include \"webio/webfs.h\"\n");
      fprintf(fd, "#include \"%s\"\n\n\n", def_hdrfile);
   }
   else  /* headerfile */
   {
      fprintf(fd, "extern const em_file efslist[%d];\n\n", infiles);
   }

   return fd;
}


/* output_file()
 *
 * Each content file will have up to two output files, one for code
 * and one for header info. These may vary from file to file, however
 * many content files may share an output file, so either output file
 * may or may not need to be created as each file's output is written.
 *
 * output_file() maintains a list of already opened output files by
 * both name and descriptor. All files are opened in write mode.
 * The file names passed are looked up in the list of open files.
 * If not found, new files are opened and added the list.
 *
 * Returns FILE* of file to use or NULL if error
 */


struct outputfile
{
   char     name[NAMELENGTH];
   FILE *   fd;
} outputfiles[OUTPUTFILES];

FILE *
output_file(char * outfilename)
{
   int   i;
   FILE * fd;

   for(i = 0; i < OUTPUTFILES; i++)
   {
      /* See f we're at end of open file list */
      if(outputfiles[i].name[0] == 0)
         break;

      /* if file name matches, return fd */
      if( argcmp(outputfiles[i].name, outfilename, NAMELENGTH) == 0)
         return(outputfiles[i].fd);
   }
   if(i == OUTPUTFILES)
   {
      dtrap();
      printf("Error: exceeded allowed number of output files (%d)\n",
         OUTPUTFILES);
      return NULL;
   }

   /* Fall to here if file not found - create new one. */
   if(strstr(outfilename, ".h"))
      fd = open_outfile(outfilename, 0);  /* header file*/
   else
      fd = open_outfile(outfilename, 1);  /* code file */

   if(fd == NULL)
      return fd;

   /* for() loop left "I" indexing next free entry - fill it in */
   argcpy(outputfiles[i].name, outfilename, NAMELENGTH);
   outputfiles[i].fd = fd;

   return fd;     // Return new file descriptor
}

/* newform()
 *
 * Calledwhen main routine hits a "<form ...>" tag in an input file.
 * this creates the new form object based on the content of the
 * <form ...> tag.
 *
 * Returns new form object if OK, else NULL.
 */

form *
newform(char * html, int length, filedata * file)
{
   char *   endbracket;
   char     tagname[TAGSIZE];
   form *   new_form;

   endbracket = strchr(html, '>');
   if(!endbracket)
   {
      printf("warning: can't find end of '<form' tag in file %s\n",
         file->filename);
      return NULL;
   }

   if(get_tagparm("name", html, tagname) == NULL)
   {
      printf("warning: can't find name of '<form' tag in file %s\n",
         file->filename);
      return NULL;
   }
   new_form = new form(tagname);

   if(new_form == NULL)
      return NULL;

   // Extract method and action
   get_tagparm("method", html, tagname);
   if(tagcmp(tagname, "GET"))
      new_form->method = GET;
   else if(tagcmp(tagname, "POST"))
      new_form->method = POST;

   get_tagparm("action", html, new_form->action);

   return new_form;
}

int
bldform(filedata * newfile, FILE * outcode)
{
   char *   cname;
   form *   formp;

    if(*newfile->opset.routine == 0)
    {
        dtrap();
        printf("WARNING - form routine name not set for file %s\n",
            newfile->filename);
        return -1;
    }

    // printf the beginning of a form stub routine
    if(newfile->opset.opmask & OPT_DOXYGEN)
    {
        fprintf(outcode, "/**\n * \\brief\n * Stub routine for processing form in %s\n * \n",
            newfile->filename);
        fprintf(outcode, " * \\param\n * sess - the session which invoked the form\n * \n");
        fprintf(outcode, " * \\param\n * eofile - CGI file for the form\n * \n");
        fprintf(outcode, " * \\return\n * Returns NULL if OK, else error message.\n */\n\n");
    }
    else
    {
        fprintf(outcode, "/* %s\n *\n * Stub routine for form processing\n * \n",
            newfile->opset.routine);
        fprintf(outcode, " * Returns NULL if OK, else error message.\n */\n\n");
    }

    fprintf(outcode, "char *\n%s(wi_sess * sess, EOFILE * eofile)\n{\n",
        newfile->opset.routine);

   // look the file up in the forms list
   for(formp = formlist; formp; formp = formp->next)
      if( strcmp(formp->action, newfile->filename) == 0)
         break;

   if(!formp)
   {
      printf("WARNING - form action file %s not referenced in any input html file.\n",
         newfile->filename);

      fprintf(outcode,
         "/* WARNING - form action file %s not referenced in any input html file. */\n",
         newfile->filename);

      fprintf(outcode, "\n   return NULL;\n}\n\n\n");
      return 1;   // warning return code
   }

   /* Two passes through form->controls - 1st loop make control variable
    * names; 2nd pass building code.
    */
   for(int i = 0; i < 2; i++)
   {
      for(control * ctl = formp->controls; ctl; ctl = ctl->next)
      {
         if(*ctl->hname == 0)
            continue;
         if( stricmp(ctl->hname, "submit") == 0)
            continue;

         // convert name to a C expression
         cname = maketoken(ctl->hname, 0, LOWERCASE);

         if(i == 0)
            fprintf(outcode, "   char *   %s;\n", ctl->cname);
         else
         {
            fprintf(outcode, "   %s = wi_formvalue(sess, \"%s\");",
               ctl->cname, ctl->hname);
            fprintf(outcode, "   /* default: %s */\n", ctl->value);
         }
      }
      fprintf(outcode, "\n");
   }

   // Mark the form as having C code generated
   formp->flags |= FF_CCODED;

   fprintf(outcode, "\n   return NULL;\n}\n\n\n");
   return 0;   // OK return code
}

/**
 * extract_html()
 *
 * Extract interesting HTML, such as SSI definitions and forms. We use
 * these for error checking and code generatiohn.
 *
 */

form *   currentform;


void
extract_html(char * cp, int cp_length, filedata * newfile)
{

    if(*cp == '<')
    {
        if( tagcmp("<form", cp) == TRUE)
        {
            currentform = newform(cp, cp_length, newfile);
        }
        if( tagcmp("</form", cp) == TRUE)
            currentform = NULL;

        // Pick up controls inside forms
        if(currentform)
        {
            if( tagcmp("<input", cp) == TRUE)
                new control(currentform, cp);
        }

        // record SSI file names in master list for later checking
        if( tagcmp("<!--#include", cp) == TRUE)
         {
             char ssiname[TAGSIZE];
             get_tagparm("file", cp, ssiname);
             if(*ssiname)
                new ssifile(ssiname, newfile);
         }
    }
    return;
}


int
main(int argc, char * argv[] )
{
   FILE *   listfile;
   FILE *   outdata;
   FILE *   outheader;
   FILE *   outcode;
   char *   lp;
   char *   linearg;
   char *   nextarg;
   int      length;
   int      i;
   int      aadd;
   int      efloop;
   int      ccodes = 0;
   int      efsforms = 0;
   char     emfflags[128];
   filedata * newfile;

   printf("fsbuild 1.1 - Copyright 2007-2008 by John Bartas\n");

   parseargs(argc, argv);

   /* Open and test the input file list */
   if(listfilename[0] == 0)
   {
      printf("You must specify an input list file on the command line \n");
      usage();
      app_exit(1);
   }

   listfile = fopen(listfilename, "rb");
   if(listfile == NULL)
   {
      printf("Unable to open input list file %s \n", listfilename);
      usage();
      app_exit(1);
   }

   infiles = 0;
   int linenum = 1;

   /* Read through the list of input files */
   while( (lp = fgets((char*)readbuf, sizeof(readbuf), listfile)) != 0 )
   {
	  linenum++;
      // skip commants and blank lines
      if((readbuf[0] == '\n') ||
         (readbuf[0] == '\r') ||
         (readbuf[0] == 0)    ||
         (readbuf[0] == '#'))
      {
         continue;
      }

      newfile = new filedata(lp, def_mask);

      /* Reset the scratch option building struct with defaults. This may
       * be overwritten by args read from this file line. When we're done
       * parsing the line, the modified defaults are the options for the
       * file.
       */
      strcpy(optionTmp.datafile, def_datafile);
      strcpy(optionTmp.hdrfile, def_hdrfile);
      strcpy(optionTmp.codefile, def_codefile);
      optionTmp.opmask = def_mask;
      memset( &optionTmp.routine, 0, sizeof(optionTmp.routine));

      /* Parse the arge that may remain in the rest of this input file
       * line. The values are left in optionTmp.
       */
      linearg = argnext(lp);
      while(linearg)
      {
         nextarg = argnext(linearg);
         aadd = setoption(linearg, nextarg);
		 if(aadd < 0)
		 {
		    printf("Error on arg %s, input file line %d\n", linearg, linenum );
			usage();
			app_exit(-1);
		 }
         linearg = nextarg;
         while(aadd--)
            linearg = argnext(linearg);
      }

      /* Now copy the file option values form optionTmp into the new file
       * tracking pobject.
       */
      memcpy( &newfile->opset, &optionTmp, sizeof(optionTmp) );

   }

   /* Done reading input file */
   printf("Read list of %d files from %s; generating output...\n",
      infiles, listfilename );

    outheader = output_file(def_hdrfile);
    if(!outheader)
        app_exit (-1);


    for(newfile = fdlist; newfile; newfile = newfile->next)
    {
        /* Each content file will have two output files, one for code and one
         * for header info. These may vary from file to file, however many
         * content files may share an output file, so either file may or may
         * not need to be created here. dump the work of resolving this on
         * the outfiles() subsystem.
         */

        outdata = output_file(newfile->opset.datafile);
        if(!outdata)
            app_exit (-1);

        if((newfile->opset.opmask & NON_BINARY_FILE) == 0)
        {
            char * cp;
            FILE *   indata;

            indata = fopen(newfile->filename, "rb");
            if(!indata)
            {
                printf("Fatal: Unable to open input file '%s'\n", newfile->filename);
                app_exit(-1);
            }

            /* We're going to make an embedded data array for this file, set up: */
            if((newfile->opset.opmask & OPT_NOEFS) == 0)
            {
                /* Make the C variable name for the data (if any) */
                mk_cname(newfile);
                fprintf(outdata, "const unsigned char %s[] = {\n", newfile->caname );
            }

            /* Read input file and write to outdata as C char array */
            while((length = fread(readbuf, 1, sizeof(readbuf), indata)) > 0)
            {
                for(i = 0; i < length; i++)
                {
                    cp = (char *)(&readbuf[i]);

                    // Check each byte for start of form or SSI text
                    extract_html(cp, sizeof(readbuf) - i, newfile);

                    if((newfile->opset.opmask & OPT_NOEFS) == 0)
                    {
                        fprintf(outdata, "0x%02x, ", readbuf[i]);
                        if(((i+1) % 12) == 0)
                            fprintf(outdata, "\n");
                    }
                }
                newfile->casize += length;
            }

            /* If we're going to make an embedded data array... */
            if((newfile->opset.opmask & OPT_NOEFS) == 0)
            {
                fprintf(outdata, "\n};\n\n" );
                fprintf(outheader, "extern  const unsigned char %s[%ld];\n",
                    newfile->caname, newfile->casize );
            }
        }
    }

    /* Open the default files for building efs linked list */
    outdata = output_file(def_datafile);
    if(!outdata)
        app_exit (-1);

    outheader = output_file(def_hdrfile);
    if(!outheader)
        app_exit (-1);

    outcode = output_file(def_codefile);
    if(!outcode)
        app_exit (-1);

    fprintf(outdata, "const em_file efslist[%u] = {\n", infiles);


    /* Loop through the file list and build the EFS list in the output file.
     * The C vars switch code (if any) is appended after this loop
     */

    efloop = 0;
    for(newfile = fdlist; newfile; newfile = newfile->next)
    {
      if(newfile->opset.opmask & OPT_NOEFS)
          continue;

      efloop++;

      // count files with C code strings attached
      if(newfile->ccode)
         ccodes++;

      if(newfile->next)
         fprintf(outdata, "{   &efslist[%d],   /* list link */\n", efloop);
      else
         fprintf(outdata, "{   NULL,   /* list link */\n");

      fprintf(outdata, "    \"%s\",   /* name of file */\n",
         newfile->filename);

      /* Figure out what kind of EFS entry to make based on option bits */
      if(newfile->opset.opmask & (OPT_SSI|OPT_PUSH|OPT_CEXP|OPT_FORM))
      {
         fprintf(outdata, "    NULL,	     /* name of data array */\n");
         if(newfile->opset.opmask & (OPT_SSI|OPT_PUSH|OPT_FORM))
         {
            fprintf(outdata, "    0,	        /* length of original file data */\n");
            fprintf(outdata, "    %s,	     /* SSI/CGI data routine */\n",
               newfile->opset.routine );
            if(newfile->opset.opmask & OPT_FORM)
            {
                /* CGI version returns "char *" */
                fprintf(outheader, "\nchar *  %s(wi_sess * sess, EOFILE * eofile);\n",
                   newfile->opset.routine );
            }
            else    /* Non-cgi versions all return int */
            {
                fprintf(outheader, "\nint     %s(wi_sess * sess, EOFILE * eofile);\n",
                   newfile->opset.routine );
            }

            if(newfile->opset.opmask & (OPT_SSI|OPT_PUSH))
            {
                if(newfile->opset.opmask & OPT_DOXYGEN)
                {
                    fprintf(outcode, "\n/**\n *\\brief * \n * %s:%s routine stub.\n * \n",
                        (newfile->opset.opmask & OPT_SSI)?"SSI":"PUSH", newfile->opset.routine );
                    fprintf(outcode, " * \\param  sess - session which invoked the SSI\n");
                    fprintf(outcode, " * \\param  eofile - embedded fs structure for the SSI\n");
                    fprintf(outcode, " * \\return\n * 0 if OK, else negative error code\n */\n\n");
				}
                else
				{
					fprintf(outcode, "\n/* %s\n *\n * %s routine stub\n */\n\n",
						(newfile->opset.opmask & OPT_SSI)?"SSI":"PUSH",
						newfile->opset.routine );

                }
                fprintf(outcode, "int\n%s(wi_sess * sess, EOFILE * eofile)\n",
                    newfile->opset.routine);
                fprintf(outcode, "{\n   /* Add your code here */\n   return 0;\n}\n\n\n");
            }
            else if(newfile->opset.opmask & OPT_FORM)
            {
               /* This file is just a virtual form handler, so try to create
                * a form handling routine stub.
                */
               bldform(newfile, outcode);
            }
         }
         else  /* It's C expression - handled below */
         {
            fprintf(outdata, "   %s,	     /* overload length w/ token */\n",
               maketoken(newfile->filename, newfile->filenumber, UPPERCASE) );
            fprintf(outdata, "   NULL,	     /* SSI/CGI data routine */\n");
         }
      }
      else
      {
         fprintf(outdata, "   %s,   /* C data array */\n",
            newfile->caname);
         fprintf(outdata, "   %ld,        /* length of original file data */\n",
            newfile->casize );
         fprintf(outdata, "   NULL,        /* SSI/CGI data routine */\n");
      }

      memset(emfflags, 0, sizeof(emfflags));
      if(newfile->opset.opmask & OPT_SSI)
         strcat(emfflags, "EMF_SSI | ");
      if(newfile->opset.opmask & OPT_PUSH)
         strcat(emfflags, "EMF_PUSH | ");
      if(newfile->opset.opmask & OPT_CEXP)
         strcat(emfflags, "EMF_CEXP | ");
      if(newfile->opset.opmask & OPT_AUTH)
         strcat(emfflags, "EMF_AUTH | ");
      if(newfile->opset.opmask & OPT_THREAD)
         strcat(emfflags, "EMF_THREAD | ");
      if(newfile->opset.opmask & OPT_FORM)
         strcat(emfflags, "EMF_FORM | ");

      if(strlen(emfflags) == 0)
         strcat(emfflags, "0x0000");
      else
         emfflags[strlen(emfflags) - 2] = 0; // step on trailing '|'


      fprintf(outdata, "    (%s),    /* flags  */\n},\n", emfflags);
   }
   fprintf(outdata, "};\n\n");


   /* Build a switch statement in the output file to handle any C vars */
   if(ccodes)
   {
      fprintf(outcode, "int\nwi_cvariables(wi_sess * sess, int token)\n{\n");
      fprintf(outcode, "   int   e;\n\n   switch(token)\n   {\n");
      fprintf(outheader, "\n\n");

      /* Pass to make the C expression code */
      efloop = 0;
      for(newfile = fdlist; newfile; newfile = newfile->next)
      {
         efloop++;

         if(newfile->ccode == NULL)
            continue;
         fprintf(outcode, "%s\n      break;\n", newfile->ccode);

         fprintf(outheader, "#define  %-32s %u\n",
            maketoken(newfile->filename, newfile->filenumber, UPPERCASE),
            newfile->filenumber );
      }
      fprintf(outcode, "   }\n   return e;\n}\n\n");
      fprintf(outheader, "\n\n");
   }

   /* Make sure all the forms have stubs. This helps catch frustrating
    * misspelling problems between the html and the input file.
    */
   for(form * formp = formlist; formp; formp = formp->next)
   {
      if((formp->flags & FF_CCODED) == 0)
         printf("Warning: no form handler defined for form %s",
            formp->action);
   }


   return 0;
}

