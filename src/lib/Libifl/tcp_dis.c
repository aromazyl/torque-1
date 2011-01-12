/*
*         OpenPBS (Portable Batch System) v2.3 Software License
*
* Copyright (c) 1999-2000 Veridian Information Solutions, Inc.
* All rights reserved.
*
* ---------------------------------------------------------------------------
* For a license to use or redistribute the OpenPBS software under conditions
* other than those described below, or to purchase support for this software,
* please contact Veridian Systems, PBS Products Department ("Licensor") at:
*
*    www.OpenPBS.org  +1 650 967-4675                  sales@OpenPBS.org
*                        877 902-4PBS (US toll-free)
* ---------------------------------------------------------------------------
*
* This license covers use of the OpenPBS v2.3 software (the "Software") at
* your site or location, and, for certain users, redistribution of the
* Software to other sites and locations.  Use and redistribution of
* OpenPBS v2.3 in source and binary forms, with or without modification,
* are permitted provided that all of the following conditions are met.
* After December 31, 2001, only conditions 3-6 must be met:
*
* 1. Commercial and/or non-commercial use of the Software is permitted
*    provided a current software registration is on file at www.OpenPBS.org.
*    If use of this software contributes to a publication, product, or
*    service, proper attribution must be given; see www.OpenPBS.org/credit.html
*
* 2. Redistribution in any form is only permitted for non-commercial,
*    non-profit purposes.  There can be no charge for the Software or any
*    software incorporating the Software.  Further, there can be no
*    expectation of revenue generated as a consequence of redistributing
*    the Software.
*
* 3. Any Redistribution of source code must retain the above copyright notice
*    and the acknowledgment contained in paragraph 6, this list of conditions
*    and the disclaimer contained in paragraph 7.
*
* 4. Any Redistribution in binary form must reproduce the above copyright
*    notice and the acknowledgment contained in paragraph 6, this list of
*    conditions and the disclaimer contained in paragraph 7 in the
*    documentation and/or other materials provided with the distribution.
*
* 5. Redistributions in any form must be accompanied by information on how to
*    obtain complete source code for the OpenPBS software and any
*    modifications and/or additions to the OpenPBS software.  The source code
*    must either be included in the distribution or be available for no more
*    than the cost of distribution plus a nominal fee, and all modifications
*    and additions to the Software must be freely redistributable by any party
*    (including Licensor) without restriction.
*
* 6. All advertising materials mentioning features or use of the Software must
*    display the following acknowledgment:
*
*     "This product includes software developed by NASA Ames Research Center,
*     Lawrence Livermore National Laboratory, and Veridian Information
*     Solutions, Inc.
*     Visit www.OpenPBS.org for OpenPBS software support,
*     products, and information."
*
* 7. DISCLAIMER OF WARRANTY
*
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT
* ARE EXPRESSLY DISCLAIMED.
*
* IN NO EVENT SHALL VERIDIAN CORPORATION, ITS AFFILIATED COMPANIES, OR THE
* U.S. GOVERNMENT OR ANY OF ITS AGENCIES BE LIABLE FOR ANY DIRECT OR INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
* This license will be governed by the laws of the Commonwealth of Virginia,
* without reference to its choice of law rules.
*/



#include <pbs_config.h>   /* the master config generated by configure */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>

#if defined(FD_SET_IN_SYS_SELECT_H)
#  include <sys/select.h>
#endif

#include "dis.h"
#include "dis_.h"
#include "dis_init.h"
#include "log.h"

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#define MAX_INT_LEN 256;

static struct tcp_chan **tcparray = NULL;
static int    tcparraymax = 0;

time_t pbs_tcp_timeout = 20;  /* reduced from 60 to 20 (CRI - Nov/03/2004) */



void DIS_tcp_settimeout(

  long timeout)  /* I */

  {
  pbs_tcp_timeout = timeout;

  return;
  }  /* END DIS_tcp_settimeout() */





int DIS_tcp_istimeout(

  int sock)

  {
  if (tcparray == NULL)
    {
    return(0);
    }

  return(tcparray[sock]->IsTimeout);
  }  /* END DIS_tcp_istimeout() */




/*
 * tcp_pack_buff - pack existing data into front of buffer
 *
 * Moves "uncommited" data to front of buffer and adjusts pointers.
 * Does a character by character move since data may over lap.
 */

static void tcp_pack_buff(

  struct tcpdisbuf *tp)

  {
  size_t amt;
  size_t start;
  size_t i;

  start = tp->tdis_trailp - tp->tdis_thebuf;

  if (start != 0)
    {
    amt  = tp->tdis_eod - tp->tdis_trailp;

    for (i = 0;i < amt;++i)
      {
      *(tp->tdis_thebuf + i) = *(tp->tdis_thebuf + i + start);
      }

    tp->tdis_leadp  -= start;

    tp->tdis_trailp -= start;
    tp->tdis_eod    -= start;
    }

  return;
  }  /* END tcp_pack_buff() */

static int tcp_resize_buff (
			    struct tcpdisbuf *tp,
			    size_t newbufsize) {
#ifndef NDEBUG
  char *id = "tcp_resize_buff";
#endif

  int leadpct, trailpct, eod;
  char *temp;
  leadpct = (int)(tp->tdis_thebuf - tp->tdis_leadp);
  trailpct = (int)(tp->tdis_thebuf - tp->tdis_trailp);
  eod = (int)(tp->tdis_thebuf - tp->tdis_eod);
  temp = (char *)malloc(newbufsize);
  if(!temp)
    {
      /* FAILURE */

      DBPRT(("%s: error!  out of space in buffer and cannot realloc message buffer (bufsize=%ld, buflen=%d, newbufsize=%d)\n",
             id,
             tp->tdis_bufsize,
             (int)(tp->tdis_leadp - tp->tdis_thebuf),
             (int)newbufsize))

	return(-1);
    }

  memcpy(temp, tp->tdis_thebuf, tp->tdis_bufsize);
  free(tp->tdis_thebuf);
  tp->tdis_thebuf = temp;
  tp->tdis_bufsize = newbufsize;
  tp->tdis_leadp = tp->tdis_thebuf - leadpct;
  tp->tdis_trailp = tp->tdis_thebuf - trailpct;
  tp->tdis_eod = tp->tdis_thebuf - eod;
  return 0;
} /* END tcp_resize_buff */



/*
 * tcp_read - read data from tcp stream to "fill" the buffer
 * Update the various buffer pointers.
 *
 * Return: >0 number of characters read
 *   0 if EOD (no data currently avalable)
 *  -1 if error
 *  -2 if EOF (stream closed)
 */

static int tcp_readbuf(

  int               fd, /* I */
  struct tcpdisbuf *tp)	/* I */

  {
  int               i;
#ifdef HAVE_POLL

  struct pollfd pollset;
  int timeout;
#else
  fd_set            readset;

  struct timeval    timeout;
#endif

  /* must compact any uncommitted data into bottom of buffer */

  tcp_pack_buff(tp);

  /*
   * we don't want to be locked out by an attack on the port to
   * deny service, so we time out the read, the network had better
   * deliver promptly
   */

  tcparray[fd]->IsTimeout = 0;
  tcparray[fd]->SelectErrno = 0;
  tcparray[fd]->ReadErrno = 0;

  do
    {
#ifdef HAVE_POLL
    /* poll()'s timeout is only a signed int, must be careful not to overflow */

    if (INT_MAX / 1000 > pbs_tcp_timeout)
      timeout = pbs_tcp_timeout * 1000;
    else
      timeout = INT_MAX;

    pollset.fd = fd;

    pollset.events = POLLIN | POLLHUP;

    i = poll(&pollset, 1, timeout);

#else
    timeout.tv_sec = pbs_tcp_timeout;

    timeout.tv_usec = 0;

    FD_ZERO(&readset);

    FD_SET(fd, &readset);

    i = select(
          fd + 1,
          &readset,
          NULL,
          NULL,
          &timeout);

#endif
    }
  while ((i == -1) && (errno == EINTR));

  if (i == 0)
    {
    /* timeout has occurred */

    tcparray[fd]->IsTimeout = 1;

    return(0);
    }
  else if (i < 0)
    {
    /* select failed */

    tcparray[fd]->SelectErrno = errno;

    return(-1);
    }

 while ((i = read(
                fd,
                tp->tdis_eod,
                tp->tdis_thebuf + tp->tdis_bufsize - tp->tdis_eod)) == -1)
    {
    if (errno != EINTR)
      break;
    }

  if (i < 0)
    {
    /* FAILURE - read failed */

    tcparray[fd]->ReadErrno = errno;

    return(-1);
    }
  else if (i == 0)
    {
    /* FAILURE - no data read */

    return(-2);
    }

  /* SUCCESS */

  tp->tdis_eod += i;

  return(i);
  }  /* END tcp_readbuf() */

static int tcp_read(

  int fd)	/* I */

  {
  struct tcpdisbuf *tp;
#ifdef GSSAPI
  OM_uint32 minor;
  size_t l;
  ssize_t f;
#endif

#ifdef GSSAPI
  if (tcparray[fd]->gssctx == GSS_C_NO_CONTEXT)
#endif
    {
    tp = &tcparray[fd]->readbuf;
    return tcp_readbuf(fd, tp);
    }

#ifdef GSSAPI
leftover:
  if ((l = tcparray[fd]->unwrapped.length) > 0)
    {
    tp = &tcparray[fd]->readbuf;
    tcp_pack_buff(tp);
    f = tp->tdis_bufsize - (tp->tdis_eod - tp->tdis_thebuf);
    if ((size_t)f < l)
      {
      memcpy(tp->tdis_eod, tcparray[fd]->unwrapped.value, f);
      tp->tdis_eod += f;
      memmove(tcparray[fd]->unwrapped.value, 
	      ((char *)tcparray[fd]->unwrapped.value)+f,
	      l-f);
      tcparray[fd]->unwrapped.length = l-f;
      return f;	/* readbuf is now full */
      }
    else
      {
      memcpy(tp->tdis_eod, tcparray[fd]->unwrapped.value, l);
      tp->tdis_eod += l;
      gss_release_buffer(&minor, &tcparray[fd]->unwrapped);
      return l;	/* for simplicity */
      }
    }

readmore:
  if (tcparray[fd]->AtEOF)
    return(-2);
  tp = &tcparray[fd]->gssrdbuf;
  f = tcp_readbuf(fd, tp);
  tcparray[fd]->AtEOF = (f == -2);

  if (tp->tdis_eod - tp->tdis_leadp >= 4)
    {
    int i;
    for (i=0, l=0; i<4; i++)
      l = l<<8 | (*tp->tdis_leadp++ & 0xff);
    /* if the buffer is to small to have read the entire gss token,
       make the buffer bigger and call read again to read the rest from the
       socket.  Then proceed
    */
    if (l+4>tp->tdis_bufsize)
      {
	tcp_resize_buff(tp, l+4);
	f = tcp_readbuf(fd, tp);
	tcparray[fd]->AtEOF = (f == -2);
      }
    if ((size_t)(tp->tdis_eod - tp->tdis_leadp) >= l)
      {
      OM_uint32 major, minor;
      gss_buffer_desc msg_in;
      msg_in.length = l;
      msg_in.value = tp->tdis_leadp;
      major = gss_unwrap(&minor,
		         tcparray[fd]->gssctx,
			 &msg_in,
			 &tcparray[fd]->unwrapped,
			 NULL,
			 NULL);
      tp->tdis_leadp += l;
      tp->tdis_trailp = tp->tdis_leadp;	/* commit */
      if (major != GSS_S_COMPLETE)
        {
	if (getenv("PBSDEBUG") != NULL)
	  pbsgss_display_status("gss_unwrap", major, minor);
	gss_release_buffer(&minor, &tcparray[fd]->unwrapped);
	tcparray[fd]->ReadErrno = 0;
	return(-1);
        }
      if (tcparray[fd]->unwrapped.length > 0)
        goto leftover;
      }
    else
      {
      tp->tdis_leadp = tp->tdis_trailp;	/* uncommit */
      if (!tcparray[fd]->IsTimeout)
        goto readmore;
      }
    }
  return (f<0 ? f : 0);

#endif
  }  /* END tcp_read() */





/*
 * DIS_tcp_wflush - flush tcp/dis write buffer
 *
 * Writes "committed" data in buffer to file discriptor,
 * packs remaining data (if any), resets pointers
 * Returns: 0 on success, -1 on error
 *      NOTE:  does not close fd
 *
 */

int DIS_tcp_wflush(

  int fd)  /* I */

  {
  size_t ct;
  int  i,maxattempts;
  char *pb;

  struct tcpdisbuf *tp;
#ifdef GSSAPI
  OM_uint32 major, minor;
  gss_buffer_desc msg_in, msg_out;
  int conf_state;
  unsigned char nct[4];
#endif

  tp = &tcparray[fd]->writebuf;
  pb = tp->tdis_thebuf;

  ct = tp->tdis_trailp - tp->tdis_thebuf;

#ifdef GSSAPI
  msg_out.value = NULL;
  msg_out.length = 0;
  if (tcparray[fd]->gssctx != GSS_C_NO_CONTEXT)
  {
    msg_in.value  = pb;
    msg_in.length = ct;
    major = gss_wrap(&minor,
		     tcparray[fd]->gssctx,
		     tcparray[fd]->Confidential,
		     GSS_C_QOP_DEFAULT,
		     &msg_in,
		     &conf_state,
		     &msg_out);
    if (major != GSS_S_COMPLETE)
      {
      if (getenv("PBSDEBUG") != NULL)
	pbsgss_display_status("gss_wrap", major, minor);
      gss_release_buffer(&minor, &msg_out);
      return(-1);
      }
    if (tcparray[fd]->Confidential && !conf_state)
      {
      if (getenv("PBSDEBUG") != NULL)
        fprintf(stderr, "gss_wrap() failed to encrypt as requested\n");
      gss_release_buffer(&minor, &msg_out);
      return(-1);
      }
    pb = msg_out.value;
    ct = msg_out.length;
    for (i=sizeof(nct); i>0; ct>>=8)
      nct[--i] = ct & 0xff;
    maxattempts=25;
    while ((i = write(fd,nct,sizeof(nct))) != sizeof(nct) && maxattempts > 0)
      {
      if (i == -1)
        {
	if (maxattempts > 0 && errno == EINTR){
		continue;
        }
		maxattempts -=1;
        if (getenv("PBSDEBUG") != NULL)
          {
          fprintf(stderr,"TCP write of message length failed, errno=%ld (%s)\n",
          (long)errno,
          strerror(errno));
          }
	}
      }
    ct = msg_out.length;
  }
#endif

  maxattempts=25;
  while ((i = write(fd, pb, ct)) != (ssize_t)ct)
    {
    if (i == -1)
      {
      if (maxattempts >0 && errno == EINTR)
        {
        maxattempts -=1;
        continue;
        }

      /* FAILURE */

      if (getenv("PBSDEBUG") != NULL)
        {
        fprintf(stderr, "TCP write of %d bytes (%.32s) failed, errno=%d (%s)\n",
                (int)ct,
                pb,
                errno,
                strerror(errno));
        }

#ifdef GSSAPI
      gss_release_buffer(&minor, &msg_out);
#endif
      return(-1);
      }  /* END if (i == -1) */

    ct -= i;

    pb += i;
    }  /* END while (i) */

  /* SUCCESS */

#ifdef GSSAPI
  gss_release_buffer(&minor, &msg_out);
#endif
 
  tp->tdis_eod = tp->tdis_leadp;

  tcp_pack_buff(tp);

  return(0);
  }  /* END DIS_tcp_wflush() */





/*
 * DIS_tcp_clear - reset tpc/dis buffer to empty
 */

static void DIS_tcp_clear(

  struct tcpdisbuf *tp)

  {
  tp->tdis_leadp  = tp->tdis_thebuf;
  tp->tdis_trailp = tp->tdis_thebuf;
  tp->tdis_eod    = tp->tdis_thebuf;

  return;
  }





void DIS_tcp_reset(

  int fd,
  int i)

  {

  struct tcp_chan *tcp;

  tcp = tcparray[fd];

  if (i == 0)
    DIS_tcp_clear(&tcp->readbuf);
  else
    DIS_tcp_clear(&tcp->writebuf);

  return;
  }  /* END DIS_tcp_reset() */





/*
 * tcp_rskip - tcp/dis support routine to skip over data in read buffer
 *
 * Returns: 0 on success, -1 on error
 */

static int tcp_rskip(

  int    fd,
  size_t ct)

  {

  struct tcpdisbuf *tp;

  tp = &tcparray[fd]->readbuf;

  if (tp->tdis_leadp - tp->tdis_eod < (ssize_t)ct)
    {
    /* this isn't the best thing to do, but this isn't used, so */

    return(-1);
    }

  tp->tdis_leadp += ct;

  return(0);
  }





/*
 * tcp_getc - tcp/dis support routine to get next character from read buffer
 *
 * Return: >0 number of characters read
 *  -1 if EOD or error
 *  -2 if EOF (stream closed)
 */

static int tcp_getc(

  int fd)

  {
  int x;

  struct tcpdisbuf *tp;

  tp = &tcparray[fd]->readbuf;

  if (tp->tdis_leadp >= tp->tdis_eod)
    {
    /* not enough data, try to get more */

    x = tcp_read(fd);

    if (x <= 0)
      {
      return((x == -2) ? -2 : -1); /* Error or EOF */
      }
    }

  return((int)*tp->tdis_leadp++);
  }  /* END tcp_getc() */





/*
 * tcp_gets - tcp/dis support routine to get a string from read buffer
 *
 * Return: >0 number of characters read
 *   0 if EOD (no data currently avalable)
 *  -1 if error
 *  -2 if EOF (stream closed)
 */

static int tcp_gets(

  int     fd,
  char   *str,
  size_t  ct)

  {
  int             x;

  struct tcpdisbuf *tp;

  tp = &tcparray[fd]->readbuf;

  while (tp->tdis_eod - tp->tdis_leadp < (ssize_t)ct)
    {
    /* not enough data, try to get more */

    x = tcp_read(fd);

    if (x <= 0)
      {
      return(x);  /* Error or EOF */
      }
    }

  memcpy((char *)str, tp->tdis_leadp, ct);

  tp->tdis_leadp += ct;

  return((int)ct);
  }  /* END tcp_gets() */




int PConnTimeout(

  int sock)  /* I */

  {
  if ((tcparray == NULL) || (tcparray[sock] == NULL))
    {
    return(0);
    }

  if (tcparray[sock]->IsTimeout == 1)
    {
    /* timeout occurred, report 'TRUE' */

    return(1);
    }

  return(0);
  }  /* END PConnTimeout() */




int TConnGetReadErrno(

  int sock)  /* I */

  {
  if ((tcparray == NULL) || (tcparray[sock] == NULL))
    {
    return(0);
    }

  return(tcparray[sock]->ReadErrno);
  }  /* END TConnGetReadErrno() */





int TConnGetSelectErrno(

  int sock)  /* I */

  {
  if ((tcparray == NULL) || (tcparray[sock] == NULL))
    {
    return(0);
    }

  return(tcparray[sock]->SelectErrno);
  }  /* END TConnGetSelectErrno() */





/*
 * tcp_puts - tcp/dis support routine to put a counted string of characters
 * into the write buffer.
 *
 * Returns: >= 0, the number of characters placed
 *   -1 if error
 */

static int tcp_puts(

  int         fd,  /* I */
  const char *str, /* I */
  size_t      ct)  /* I */

  {

  struct tcp_chan *tcp;
  struct tcpdisbuf *tp;

  tcp = tcparray[fd];

  /* NOTE:  currently, failures may occur if tp->bufsize is not large enough. */
  /*        this should be changed to allow proper operation with degraded    */
  /*        performance (how?) */

  tp = &tcp->writebuf;
  if ((tp->tdis_thebuf + tp->tdis_bufsize - tp->tdis_leadp) < (ssize_t)ct)
    {
      if (tcp_resize_buff(tp,tp->tdis_bufsize + THE_BUF_SIZE) != 0) {
      return(-1);
      }
    }

  memcpy(tp->tdis_leadp, (char *)str, ct);

  tp->tdis_leadp += ct;

  return(ct);
  }  /* END tcp_puts() */




/*
 * tcp_rcommit - tcp/dis support routine to commit/uncommit read data
 */

static int tcp_rcommit(

  int fd,
  int commit_flag)

  {

  struct tcpdisbuf *tp;

  tp = &tcparray[fd]->readbuf;

  if (commit_flag)
    {
    /* commit by moving trailing up */

    tp->tdis_trailp = tp->tdis_leadp;
    }
  else
    {
    /* uncommit by moving leading back */

    tp->tdis_leadp = tp->tdis_trailp;
    }

  return(0);
  }  /* END tcp_rcommit() */





/*
 * tcp_wcommit - tcp/dis support routine to commit/uncommit write data
 */

static int tcp_wcommit(

  int fd,
  int commit_flag)

  {

  struct tcpdisbuf *tp;

  tp = &tcparray[fd]->writebuf;

  if (commit_flag)
    {
    /* commit by moving trailing up */

    tp->tdis_trailp = tp->tdis_leadp;
    }
  else
    {
    /* uncommit by moving leading back */

    tp->tdis_leadp = tp->tdis_trailp;
    }

  return(0);
  }




void
DIS_tcp_funcs(void)

  {
  if (dis_getc == tcp_getc)
    {
    return;
    }

  dis_getc = tcp_getc;

  dis_puts = tcp_puts;
  dis_gets = tcp_gets;
  disr_skip = tcp_rskip;
  disr_commit = tcp_rcommit;
  disw_commit = tcp_wcommit;

  return;
  }




/*
 * DIS_tcp_setup - setup supports routines for dis, "data is strings", to
 * use tcp stream I/O.  Also initializes an array of pointers to
 * buffers and a buffer to be used for the given fd.
 * 
 * NOTE:  tmpArray is global
 *
 * NOTE:  does not return FAILURE - FIXME
 */

void DIS_tcp_setup(

  int fd)

  {
  struct tcp_chan *tcp;
  struct tcpdisbuf *tp;

  /* check for bad file descriptor */

  if (fd < 0)
    {
    return;
    }

  /* set DIS function pointers to tcp routines */

  DIS_tcp_funcs();

  if (fd >= tcparraymax)
    {
    int hold = tcparraymax;
    int flags;
    
    /*
    ** Check if this is a valid file descriptor, if not log an error and don't
    ** do any memory allocations
    */

    flags = fcntl(fd, F_GETFL);

    if (errno == EBADF)
      {
      sprintf(log_buffer, "invalid file descriptor (%d) for socket",
        fd);
      log_err(errno, "DIS_tcp_setup", log_buffer);

      return;
      }

    tcparraymax = fd + 10;

    if (tcparray == NULL)
      {
      tcparray = (struct tcp_chan **)calloc(
        tcparraymax,
        sizeof(struct tcp_chan *));

      if (tcparray == NULL)
        {
        /* FAILURE */

        log_err(errno,"DIS_tcp_setup","calloc failure");

        return;
        }
      }
    else
      {
      struct tcp_chan **tmpTA;

      tmpTA = (struct tcp_chan **)realloc(
        tcparray,
        tcparraymax * sizeof(struct tcp_chan *));

      if (tmpTA == NULL)
        {
        /* FAILURE */

        log_err(errno,"DIS_tcp_setup","realloc failure");

        return;
        }

      tcparray = tmpTA;

      memset(&tcparray[hold], '\0', (tcparraymax - hold) * sizeof(struct tcp_chan *));
      }
    }    /* END if (fd >= tcparraymax) */

  tcp = tcparray[fd];

  if (tcp == NULL)
    {
    tcparray[fd] = (struct tcp_chan *)malloc(sizeof(struct tcp_chan));

    tcp = tcparray[fd];

    if (tcp == NULL)
      {
      log_err(errno, "DIS_tcp_setup", "malloc failure");

      return;
      }

    tp = &tcp->readbuf;

    tp->tdis_thebuf = (char *)malloc(THE_BUF_SIZE);
    if(tp->tdis_thebuf == NULL)
      {
      log_err(errno,"DIS_tcp_setup","malloc failure");

      return;
      }
    tp->tdis_bufsize = THE_BUF_SIZE;

    tp = &tcp->writebuf;

    tp->tdis_thebuf = (char *)malloc(THE_BUF_SIZE);
    if(tp->tdis_thebuf == NULL)
      {
      log_err(errno,"DIS_tcp_setup","malloc failure");

      return;
      }
    tp->tdis_bufsize = THE_BUF_SIZE;


#ifdef GSSAPI
    tcp->unwrapped.length = 0;
    tcp->unwrapped.value = NULL;
    tcp->gssctx = GSS_C_NO_CONTEXT;

    tp = &tcp->gssrdbuf;

    tp->tdis_thebuf = (char *)malloc(THE_BUF_SIZE);
    if(tp->tdis_thebuf == NULL)
      {
      log_err(errno,"DIS_tcp_setup","malloc failure");

      return;
      }
    tp->tdis_bufsize = THE_BUF_SIZE;

#endif
    }


  /* initialize read and write buffers */

#ifdef GSSAPI
  assert (tcp->unwrapped.value == NULL);
  DIS_tcp_clear(&tcp->gssrdbuf);
#endif
  DIS_tcp_clear(&tcp->readbuf);

  DIS_tcp_clear(&tcp->writebuf);

  return;
  }  /* END DIS_tcp_setup() */



/*
 * DIS_tcp_release - release data structures for the given fd
 */
void DIS_tcp_release(
  int fd)

  {
#ifdef GSSAPI
  OM_uint32 minor;
#endif
  struct tcp_chan *tcp;
  struct tcpdisbuf *tp;


  assert (fd >= 0);
  if (fd >= tcparraymax || tcparray[fd] == NULL)
    return;	/* Might be an RPP connection */

  tcp=tcparray[fd];
  tp = &tcp->readbuf;
  if (tp->tdis_thebuf != NULL){
	  free(tp->tdis_thebuf);
	  tp->tdis_thebuf = NULL;
  }
  tp = &tcp->writebuf;
  if (tp->tdis_thebuf != NULL){
	  free(tp->tdis_thebuf);
	  tp->tdis_thebuf = NULL;
  }

#ifdef GSSAPI
  if (tcparray[fd]->gssctx != GSS_C_NO_CONTEXT){
	  OM_uint32 minor,major;
	  /*gss_buffer_desc out_buf;*/
/*	  major=gss_delete_sec_context (&minor, &tcparray[fd]->gssctx,&out_buf);*/
	  major=gss_delete_sec_context (&minor, &tcparray[fd]->gssctx,GSS_C_NO_BUFFER);
      if(major != GSS_S_COMPLETE ){
    	  log_err(major,"DIS_tcp_release","gss_delete_sec_context failure");
    	 /* gss_delete_sec_context (&minor, &tcparray[fd]->gssctx,GSS_C_NO_BUFFER);*/
      }
     /*gss_release_buffer(&minor,&out_buf);*/
      tcparray[fd]->gssctx = GSS_C_NO_CONTEXT;
  }
  if (tcparray[fd]->unwrapped.value)
    gss_release_buffer (&minor, &tcparray[fd]->unwrapped);
  /*fix memory loss in DIS_scp_setup */
   tp = &tcp->gssrdbuf;
   if (tp->tdis_thebuf != NULL) free(tp->tdis_thebuf);
#endif
  free(tcparray[fd]);
  tcparray[fd] = NULL;
  }  /* END DIS_tcp_release() */

#ifdef GSSAPI
/*
 * DIS_tcp_set_gss - associate GSSAPI information with a TCP channel
 */
void DIS_tcp_set_gss(

  int          fd,    /* I */
  gss_ctx_id_t ctx,   /* I */
  OM_uint32    flags) /* I */

  {
  OM_uint32 major, minor, bufsize;

  assert (fd >= 0 && fd < tcparraymax && tcparray[fd]);
  assert (tcparray[fd]->gssctx == GSS_C_NO_CONTEXT);
  tcparray[fd]->gssctx = ctx;
  tcparray[fd]->AtEOF = 0;
  tcparray[fd]->Confidential = (flags & GSS_C_CONF_FLAG);
  major = gss_wrap_size_limit (&minor, ctx, (flags & GSS_C_CONF_FLAG),
                             GSS_C_QOP_DEFAULT, THE_BUF_SIZE, &bufsize);

  /* reallocate the gss buffer if it's too small to handle the wrapped
     version of the largest unwrapped message
  */
  if (major == GSS_S_COMPLETE)
    {

    struct tcpdisbuf *tp = &tcparray[fd]->gssrdbuf;
    if (tp->tdis_bufsize < bufsize)
      {
      if (tp->tdis_thebuf != NULL)
        {
        free(tp->tdis_thebuf);
        }
      tp->tdis_thebuf = (char *)malloc(bufsize);
      if(tp->tdis_thebuf == NULL)
        {
        log_err(errno,"DIS_tcp_set_gss","malloc failure");
        
        return;
        }
      tp->tdis_bufsize = bufsize;
      }
    }

  } /* END DIS_tcp_set_gss */
#endif


/* END tcp_dis.c */
