/* webutils.c
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


char hdrbuf[HDRBUFSIZE];   /* For building HTTP headers */

int   (*wi_execfunc)(wi_sess * sess, char * args) = NULL;


/* This file contins utility functions for parsing HTTP header items.
 */

char * wi_servername = "Webio Embedded server v1.3";

struct httperror
{
    int      errcode;
    char *   errtext;
} const httperrors[] =
{
    { 202,  "Accepted" },
    { 400,  "Bad HTTP request" },
    { 401,  "Authentication required" },
    { 402,  "Payment required" },
    { 404,  "File not found" },
    { 501,  "Server error" }
};

static int wi_taglen(char * tag);
static int wi_tagcmp(char * tag1, char * tag2);
static unsigned atocode(char * cp);

/* wi_senderr()
 *
 * This is called when a session needs to send an error to the client..
 *
 * Returns: 0 if a;ll went OK, else negative WIE_ error code.
 */

int
wi_senderr(wi_sess * sess, int httpcode )
{
    u_long   i;
    int   cnt;
    char *   errortext = "Unknown HTTP Error";

    for(i = 0; i < (sizeof(httperrors)/sizeof(struct httperror)); i++)
    {
        if(httperrors[i].errcode == httpcode)
        {
            errortext = httperrors[i].errtext;
            break;
        }
    }

    /* Build a header */
    cnt = sprintf(hdrbuf, "HTTP/1.1 %d %s\r\n", httpcode, errortext);
    cnt += sprintf(hdrbuf + cnt, "Date: %s GMT\r\n", wi_getdate(sess, NULL) );
    if(httpcode == 401)
    {
        cnt += sprintf(hdrbuf + cnt, "WWW-Authenticate: Basic realm=\"%s\"\r\n", sess->ws_uri );
    }
    cnt += sprintf(hdrbuf + cnt, "Server: %s\r\n", wi_servername );
    cnt += sprintf(hdrbuf + cnt, "Connection: close\r\n\r\n");

    /* Add some text for browser to display */
    cnt += sprintf(hdrbuf + cnt, "<html><head><title>Error %d</title></head>\r\n", httpcode);
    cnt += sprintf(hdrbuf + cnt, "<body><h2>Error %d: %s<br></h2>\r\n", httpcode, errortext);
    if(sess->ws_uri)
    {
        cnt += sprintf(hdrbuf + cnt, "File: %s<br>\r\n", sess->ws_uri);
    }

    sprintf(hdrbuf + cnt, "</body></html>\r\n");

    send(sess->ws_socket, hdrbuf, strlen(hdrbuf), 0);

    /* Close socket and mark session for deletion */
    closesocket(sess->ws_socket);
    sess->ws_socket = INVALID_SOCKET;
    sess->ws_state = WI_ENDING;

    return 0;      /* OK Return */
}


int
wi_replyhdr(wi_sess * sess, int contentlen)
{
    int		cnt;
    int      hdrlen;
    int      error;

    cnt = sprintf(hdrbuf, "HTTP/1.1 200 OK\r\n");
    cnt += sprintf(hdrbuf + cnt, "Date: %s GMT\r\n", wi_getdate(sess, NULL) );
    cnt += sprintf(hdrbuf + cnt, "Server: %s\r\n", wi_servername );
    cnt += sprintf(hdrbuf + cnt, "Connection: close\r\n");
    cnt += sprintf(hdrbuf + cnt, "Content-Type: %s\r\n", sess->ws_ftype );
    sprintf(hdrbuf + cnt, "Content-Length: %d\r\n\r\n", contentlen );

    hdrlen = strlen(hdrbuf);
    error = send(sess->ws_socket, hdrbuf, hdrlen, 0);
    if(error < hdrlen)
    {
        dtrap();    /* Does this ever happen? */
        error = errno;
        return WIE_SOCKET;
    }
    sess->ws_flags |= WF_HEADERSENT;
    return 0;
}

/* wi_movebinary()
 *
 * This is called, often iterativly, to send a binary file to a socket.
 * It does no processing or scanning of the file contents.
 * It sends until the socket returns EWOULDBLOCK or file reaches EOF.
 * After EWOULDBLOCK the socket blocks on the select call in webio.c
 * until the socket can send again, then this routine is called again.
 *
 * Returns 0 if OK, else negative error code.
 */

int
wi_movebinary(wi_sess * sess, wi_file * fi)
{
    int   filelen;
    int   error;

    if((sess->ws_flags & WF_HEADERSENT) == 0)   /* header sent yet? */
    {
        // Get file size
        filelen = wi_fgetsize(fi);
        wi_replyhdr(sess, filelen);
    }

    while(sess->ws_state == WI_SENDDATA)
    {
        /* see if we need to get another block from the file */
        if(fi->wf_inbuf == 0)
        {
            fi->wf_inbuf = wi_fread(fi->wf_data, 1, sizeof(fi->wf_data), fi );

            if(fi->wf_inbuf < 0)
                return WIE_BADFILE;
        }
        error = send(sess->ws_socket, fi->wf_data, fi->wf_inbuf, 0);
        if(error < 0)
        {
            error = errno;
            if(error == EWOULDBLOCK)
                return 0;      /* try again later */
            else
                return WIE_SOCKET;
        }
        if(fi->wf_inbuf < (int)sizeof(fi->wf_data))  /* end of file? */
        {
            wi_fclose(fi);
            wi_txdone(sess);     /* will cause break from while() loop */
        }
        else
        {
            fi->wf_inbuf = 0;    /* clear buffer for another file read */
        }
    }

    return 0;   /* OK return */
}

int
wi_txdone(wi_sess * sess)
{

    /* If connection is persistent change the state to read the next file  */
    if(sess->ws_flags & WF_PERSIST)
    {
        //dtrap();
        sess->ws_state = WI_HEADER;
        return 0;
    }
    else if(sess->ws_flags & WF_SVRPUSH)
    {
        int	error;
        WI_FILE * fi;

        dtrap();

        /* poll server push routine */
        fi = sess->ws_filelist;
        if(fi->wf_routines->wfs_push == NULL)
            return WIE_BADFILE;
        error = fi->wf_routines->wfs_push(fi->wf_fd, sess);
        return error;
    }
    else
    {
        /* done with normaal connection - close the socket and mark
        * session for deletion; */
        closesocket(sess->ws_socket);
        sess->ws_socket = INVALID_SOCKET;
        sess->ws_state = WI_ENDING;
    }
    return 0;
}


/* wi_nextarg()
 *
 * This is returns the next space-delimited field in an HTTP header.
 * If there are no more args in the line (i.e. it encounters a CR
 * before any more text) it returns a NULL.
 *
 * Returns: pointer to arg, or NULL if no more args.
 *
 */

char *
wi_nextarg( char * argbuf )
{
    while(*argbuf > ' ')    /* move to next space or CR */
        argbuf++;
    /* if we are at the end of a line, return null (no more args) */
    if(*argbuf != ' ')
        return NULL;
    while(*argbuf == ' ')   /* move to next arg */
        argbuf++;
    if(*argbuf < ' ')
        return NULL;
    else
        return argbuf;
}

/* wi_argncpy()
 *
 * This copies the space-delimited field in the first arg into the
 * buffer in the second arg. Copy size is limited to the size passed.
 *
 * Returns: number of bytes copied. This may be 0 the first arg is
 * malformed.
 *
 */

int
wi_argncpy(char * buf, char * arg, int size )
{
    int   count = 0;
    while((++count < size) && (*arg > ' '))
        *buf++ = *arg++;
    return count;
}


/* wi_tagcmp()
 *
 * Compare two html tags. This differs from stricmp() in that tags are
 * terminated by any unprintable char, not just null.
 *
 * Returns 0 if tags match, -1 if not.
 */

static int
wi_tagcmp(char * tag1, char * tag2)
{
    while(*tag1 > ' ')
    {
        if((*tag1++ | 0x20) != (*tag2++ | 0x20))
            return -1;
    }
    /* tags match for length of tag1 - make sure tag2 terminates here. */
    if(*tag2 > ' ')
        return -1;     /* both tags terminated */
    else
        return 0;      /* tag 2 is longer */
}


/* wi_getline()
 *
 * This is returns The first line of the passed type in an HTML header.
 * For example if you pass "Referer" and a text pointer, it will search
 * the passsed text for the "Referer: " line and return a pointer to the
 * first non-space text following "Referer:".
 *
 * This allows for zeros (placed by wi_argterm) embedded in the HTTP
 * header. If considers double CRLF to mark the end of header.
 *
 * Returns: pointer to text, or NULL if no type not found.
 *
 */

char *
wi_getline( char * linetype, char * httphdr )
{
    char *   cp;
    int      typelen;

    typelen = strlen(linetype);
    for(cp = httphdr; cp < (httphdr + WI_RXBUFSIZE); cp++)
    {
        if(*cp == *linetype)    /* Got a match for first char? */
        {
            if(strnicmp(cp, linetype, typelen) == 0)
            {
                cp = wi_nextarg(cp);
                if(!cp)
                    return NULL;
                wi_argterm(cp);
                return(cp);
            }
        }
        if(strncmp(cp, "\r\n\r\n", 4) == 0)
            return NULL;
    }
//   dtrap();       /* Didn't find end OR field??? */
    return NULL;
}

/* wi_argterm()
 *
 * Terminates the passed string, which is assumed to be in an HTML
 * header. String usually ends in space or CR - this is replaced
 * with a null for C language routines
 *
 * Returns: pointer to next text string after arg passed text,
 * or NULL if no more strings are in the passed buffer.
 *
 */

char *
wi_argterm( char * arg )
{
    while(*arg > ' ' )
        arg++;
    *(arg++) = 0;
    while((*arg <= ' ') && (*arg > 0))
        arg++;
    if(*arg)
        return arg;
    else
        return NULL;
}

/* atocode() - return a code for a 2 byte hex value */

static unsigned
atocode(char * cp)
{
    unsigned   value = 0;
    char       ch;
    int        i;

    for(i = 0; i < 2; i++)
    {
        ch = *cp++;
        if ((ch >= '0') && (ch <= '9'))
            value = (value << 4) | (ch - '0');
        else if ((ch >= 'a') && (ch <= 'f'))
            value = (value << 4) | (ch - 'a' + 10);
        else if ((ch >= 'A') && (ch <= 'F'))
            value = (value << 4) | (ch - 'A' + 10);
        else
        {
            value = 0;
            break;
        }
    }

    return value;
}


/* wi_urldecode(char * utext)
 *
 * Decode the % and + characters in URLs. Decoding is done in place
 * in the buffer, since it never grows the strong
 *
 */

void
wi_urldecode(char * utext)
{
    char *icp = utext;
    char *ocp = utext;
    u_char code;

    while(*icp > ' ')
    {
        if(*icp == '+')
        {
            /* plus signs always convert to space */
            *ocp++ = ' ';
            icp++;
        }
        else if(*icp == '%')  /* Get hex code following percent */
        {
            code = (u_char)atocode(icp + 1);
            /* Make sure code was valid (nonzero) */
            if(code)
            {
                *ocp++ = (char)code;
                icp += 3;
            }
        }
        else
            *ocp++ = *icp++;
    }

    return;
}


/* wi_buildform()
 *
 * Extract the name/value pairs from the second parameter, build a form
 * structure with them, and attach the form to the passed session.
 *
 * Returns: 0 if OK else negative WIE_ error code.
 *
 */


int
wi_buildform(wi_sess * sess, char * pairs)
{
    char *      cp;
    wi_form *   form;
    int         i;
    int         pairct = 0;

    /* first, count the name/value pairs */
    for(cp = pairs; *cp; cp++)
    {
        if(*cp == '=')
            pairct++;
        if(*cp <= ' ')
            break;
    }

    /* get a buffer big enough for the form, including all the
     * name/value pair pointers.
     */
    form = (wi_form *)wi_alloc( sizeof(wi_form) + (pairct * sizeof(wi_pair)), 'MROF');
    if(!form)
        return WIE_MEMORY;

    form->paircount = pairct;
    cp = pairs;
    for(i = 0; i < pairct; i++)
    {
        form->pairs[i].name = cp;
        while ((*cp != ' ') && (*cp != '='))
            cp++;
        if(*cp != '=')
            return WIE_CLIENT;
        *cp++ = 0;
        form->pairs[i].value = cp;
        while ((*cp != ' ') && (*cp != '&'))
            cp++;
        if(*cp < ' ')
            wi_argterm(form->pairs[i].value);
        else
            *cp++ = 0;
        /* Decode spaces ("+") and %20 type inserts */
        wi_urldecode(form->pairs[i].name);
        wi_urldecode(form->pairs[i].value);
    }

    /* Add form to head of sesison's form list */
    form->next = sess->ws_formlist;
    sess->ws_formlist = form;

    return 0;
}

#define FT_BINARY    0x01

struct wi_ftype
{
    u_long   ext;        /* encoded 1st four chars of extension */
    char *   mimetype;   /* Mime description */
    int      flags;      /* bitmask of the FT_ flags */
} const wi_ftypes[] =
{
    { 0x4A504700, "image/jpeg", FT_BINARY },   /* JPG */
    { 0x4A504547, "image/jpeg", FT_BINARY },   /* JPEG */
    { 0x504E4700, "image/png",  FT_BINARY },   /* PNG */
    { 0x47494600, "image/gif",  FT_BINARY },   /* GIF */
    { 0x57415600, "audio/wav", FT_BINARY },    /* WAV */
    { 0x4D503300, "audio/mp3", FT_BINARY },    /* MP3 */
    { 0x574D5600, "video/x-ms-wmv", FT_BINARY },     /* WMV */
    { 0x50444600, "application/pdf", FT_BINARY },    /* PDF */
    { 0x53574600, "application/x-shockwave-flash", FT_BINARY },    /* SWF */
};


int
wi_setftype(wi_sess * sess)
{
    u_long   i;
    char *   lastdot;
    u_long   type;

    lastdot = strrchr(sess->ws_uri, '.');
    if(!lastdot)
        return 0;  /* no dot, assume not binary */

    lastdot++;
    type = 0;

    /* find the last dot & get the next 4 chars after it. */
    for(i = 0; i < 4; i++)
    {
        type <<= 8;
        /* get next char */
        type |= (u_long)((*lastdot) & 0x000000FF);
        type &= ~0x20;    /* Make uppercase */
        if(*lastdot >= ' ')  /* bump pointer if not at end */
            lastdot++;
    }

    /* see if the file is one of the binary types */
    for(i = 0; i < sizeof(wi_ftypes)/sizeof(struct wi_ftype); i++)
    {
        if(wi_ftypes[i].ext == type)
        {
            if(wi_ftypes[i].flags & FT_BINARY)
                sess->ws_flags |= WF_BINARY;
            sess->ws_ftype = wi_ftypes[i].mimetype;;
            return 1;
        }
    }
    sess->ws_flags &= ~WF_BINARY; /* not listed as binary type */
    sess->ws_ftype = "text/html"; /* default for unknown types */
    return 0;
}


/* wi_ssi()
 *
 * Handle an SSI request. This is called from wi_readfiles(), and for
 * simple SSI files it will recurse back into wi_readfiles(). This is
 * also where the SSI file is checked to see if it is implemented
 * in C code, and if so calls the C routine associated with the file.
 *
 * Returns 0 if OK, else negative error code
 */

int
wi_ssi(wi_sess * sess)
{
    char *      ssitext;
    char *      ssifname;      /* name of SSI file */
    char *      endname;
    char *      pairs;
    char *      args;
    wi_file *   wrapper;       /* info about containing file */
    int         error;
    char        paren;
#ifdef USE_EMFILES
    wi_file *   ssi;           /* info about SSI file */
#endif

    wrapper = sess->ws_filelist;
    ssitext = &wrapper->wf_data[wrapper->wf_nextbuf];
    ssifname = strstr(ssitext, "file=");
    if(!ssifname)
    {
        dtrap();
        return WIE_FORMAT;
    }

    ssifname += 5;    /* poinst past file=" text */
    paren = *ssifname++;
    endname = strchr(ssifname, paren);
    if(!endname)   /* no terminating quote sign? */
    {
        dtrap();
        return WIE_FORMAT;
    }
    *endname = 0;     /* ssifname (and optional name/value args) now a C string */
    pairs = strchr(ssifname, '?');
    if(pairs)
    {
        error = wi_buildform(sess, pairs + 1);  /* best effort... */
        *pairs = 0;
    }
    args = strchr(ssifname, ' ');
    if(args)
        *args = 0;

    error = wi_fopen(sess, ssifname, "r");
    if(args)
        *args = ' ';
    if(error)
    {
        dtrap();       /* No SSI file? Probably a bug; Tell programmer */
        return error;
    }

#ifdef USE_EMFILES
    /* See if the SSI file is a code-based embedded file */
    ssi = sess->ws_filelist;
    if(ssi->wf_routines == &emfs)
    {
        EOFILE * eofile = (EOFILE*)ssi->wf_fd;
        if(eofile->eo_emfile->em_flags & EMF_CEXP)
        {
            /* SSI is a CVAR - em_size is overloaded with the token */
            error = wi_cvariables(sess, eofile->eo_emfile->em_size);
        }
        else if(eofile->eo_emfile->em_flags & EMF_SSI)
        {
            /* SSI is an EFS routine */
            SSI_ROUTINE *  ssifunc;

            if(!eofile->eo_emfile->em_routine)
            {
                dtrap();
                return 0;
            }
            ssifunc = (SSI_ROUTINE*)eofile->eo_emfile->em_routine;
            ssifunc(sess, eofile);
        }
        else     /* "normal" EFS read */
            return(wi_readfile(sess));

        /* If we layered on a form, release it now */
        if(pairs && sess->ws_formlist)
        {
            wi_form * form = sess->ws_formlist->next;
            wi_free(sess->ws_formlist, 'MORF');
            sess->ws_formlist = form;
        }

        wi_fclose(ssi);
        return 0;
    }
#endif /* USE_EMFILES */

    error = wi_readfile(sess);
    return error;
}

/* wi_ssi()
 *
 * Crude version of SSI request. This is called from wi_readfiles(),
 * This calls the optional C routine to "exec" the "cmd" - what it
 * actually does it totally up to the port. The C routine should
 * write any output to fi->wf_data at fi->wf_nextbuf.
 *
 * Returns 0 if OK, else negative error code
 */

int
wi_exec(wi_sess * sess)
{
    char *   cp;
    char *   args;
    char     paren;
    int      err = 0;
    int      len;
    wi_file *   fi;     /* info about current file */


    /* start loading file to return. */
    fi = sess->ws_filelist;
    len = fi->wf_nextbuf;

    cp = &fi->wf_data[len + 10];
    if( strncmp(cp, "cmd_argument=", 13) != 0)
    {
        dtrap();
        return WIE_FORMAT;
    }
    cp += 13;

    /* see if there is an opening paren */
    paren = *cp;
    if((paren != '\'') && (paren != '\"'))
    {
        dtrap();
        return WIE_FORMAT;
    }
    args = cp + 1;

    /* Find closing paren and replace with null */
    cp = strchr(args, paren);

    if(!cp)
    {
        dtrap();
        return WIE_FORMAT;
    }
    *cp = 0;

    if(wi_execfunc)
        err = (*wi_execfunc)(sess, args);

    return err;
}




/*
//  Taken from NCSA HTTP
//
//  NOTE: These conform to RFC1113, which is slightly different then the Unix
//        uuencode and uudecode!
*/

const unsigned char pr2six[256]=
{
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,64,0,1,2,3,4,5,6,7,8,9,
    10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,64,26,27,
    28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64
};


#define BYTE unsigned char

static void
wi_uudecode(char * bufcoded, BYTE * pbuffdecoded)
{
    int     nbytesdecoded;
    char *  bufin;
    BYTE *  bufout;
    int     nprbytes;

    /* Strip leading whitespace. */

    while(*bufcoded==' ' || *bufcoded == '\t') bufcoded++;

    /* Figure out how many characters are in the input buffer.
     * If this would decode into more bytes than would fit into
     * the output buffer, adjust the number of input bytes downwards.
     */
    bufin = bufcoded;
    while(pr2six[(int)*(bufin++)] <= 63);
    nprbytes = (int)(bufin - bufcoded - 1);
    nbytesdecoded = ((nprbytes+3)/4) * 3;

    bufout = pbuffdecoded;
    bufin = bufcoded;

    while (nprbytes > 0)
    {
        *(bufout++) =
            (unsigned char) (pr2six[(int)*bufin] << 2 | pr2six[(int)bufin[1]] >> 4);
        *(bufout++) =
            (unsigned char) (pr2six[(int)bufin[1]] << 4 | pr2six[(int)bufin[2]] >> 2);
        *(bufout++) =
            (unsigned char) (pr2six[(int)bufin[2]] << 6 | pr2six[(int)bufin[3]]);
        bufin += 4;
        nprbytes -= 4;
    }

    if(nprbytes & 03)
    {
        if(pr2six[(int)bufin[-2]] > 63)
            nbytesdecoded -= 2;
        else
            nbytesdecoded -= 1;
    }

    pbuffdecoded[nbytesdecoded] = '\0';

    return;
}


/* wi_taglen()
 *
 * Returns the length of the text data remaining after passed tag. Data
 * is consideredt terminated by CRLF or NULL.
 *
 */

static int
wi_taglen(char * tag)
{
    int len = 0;

    while(*tag++ > '\n')
        len++;
    return len;
}


/* wi_decode_auth()
 *
 * Decodes authentication information from http header into a text
 * name/password format.
 *
 * passed name and password are pointers to caller buffers. Length
 * of these buffers is indicated by passed ints. Absurdly long names
 * and PWs are truncated and probably won't work.
 *
 * Handles errors by returning null strings.
 */

void
wi_decode_auth(wi_sess * sess, char * name, int name_len, char * pass, int pass_len)
{
    char *   authdata;
    char *   divide;
    char     decode[80];

    /* For now, just do basic auth */
    if(wi_tagcmp(sess->ws_auth, "Basic") == 0)
    {
        authdata = sess->ws_auth + strlen("Basic ");
        if(wi_taglen(authdata) > (int)sizeof(decode))
        {
            dtrap();    // crude overflow test failed
            return;
        }
        wi_uudecode(authdata, (u_char*)(&decode[0]));
        divide = strchr(decode, ':');
        if(!divide)
        {
            *name = 0;
            *pass = 0;
            return;
        }
        *divide++ = 0;    /* terminte name, point to password */
        strncpy(name, decode, name_len);
        strncpy(pass, divide, pass_len);
    }
    else  /* Add MD5 here later.... */
    {
        dtrap();    // Unsupported auth type
    }
    return;
}

