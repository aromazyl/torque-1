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

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#ifdef sun
#include <sys/stream.h>
#include <sys/stropts.h>
#endif /* sun */
#if defined(HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif
#if !defined(sgi) && !defined(_AIX) && !defined(linux)
#include <sys/tty.h>
#endif  /* ! sgi */
#include "portability.h"
#include "pbs_ifl.h"
#include "server_limits.h"
#include "net_connect.h"

static char cc_array[PBS_TERM_CCA];
static struct winsize wsz;

extern int mom_reader_go;

/*
 * read_net - read data from network till received amount expected
 *
 *	returns >0 amount read
 *		-1 on error
 */
static int read_net(sock, buf, amt)
	int    sock;
	char  *buf;
	int    amt;
{
	int got;
	int total = 0;

	while (amt > 0) {
		got = read(sock, buf, amt);
		if (got > 0) {	/* read (some) data */
			amt   -= got;
			buf   += got;
			total += got;
		} else if (got == 0)
			break;
		else
			return (-1);
	}
	return (total); 
}





/*
 * rcvttype - receive the terminal type of the real terminal
 *
 *	Sent over network as "TERM=type_string"
 */

char *rcvttype(

  int sock)

  {
  static char buf[PBS_TERM_BUF_SZ];

  /* read terminal type as sent by qsub */

  if ((read_net(sock,buf,PBS_TERM_BUF_SZ) != PBS_TERM_BUF_SZ) ||
      (strncmp(buf,"TERM=",5) != 0))
    {
    return(NULL);
    }

  /* get the basic control characters from qsub's termial */

  if (read_net(sock,cc_array,PBS_TERM_CCA) != PBS_TERM_CCA) 
    {
    return(NULL);
    }
	
  return(buf);
  }




/* 
 * set_termcc - set the basic modes for the slave terminal, and set the
 *	control characters to those sent by qsub.
 */

void set_termcc(

  int fd)

  {
  struct termios slvtio;

#ifdef	PUSH_STREAM
  ioctl(fd,I_PUSH,"ptem");
  ioctl(fd,I_PUSH,"ldterm");
#endif	/* PUSH_STREAM */

  if (tcgetattr(fd,&slvtio) < 0)
    {
    return;	/* cannot do it, leave as is */
    }

#ifdef IMAXBEL
	slvtio.c_iflag = (BRKINT|IGNPAR|ICRNL|IXON|IXOFF|IMAXBEL);
#else
	slvtio.c_iflag = (BRKINT|IGNPAR|ICRNL|IXON|IXOFF);
#endif
	slvtio.c_oflag = (OPOST|ONLCR);
#if defined(ECHOKE) && defined(ECHOCTL)
	slvtio.c_lflag = (ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHOKE|ECHOCTL);
#else
	slvtio.c_lflag = (ISIG|ICANON|ECHO|ECHOE|ECHOK);
#endif
	slvtio.c_cc[VEOL]   = '\0';
	slvtio.c_cc[VEOL2]  = '\0';
	slvtio.c_cc[VSTART] = '\021';		/* ^Q */
	slvtio.c_cc[VSTOP]  = '\023';		/* ^S */
#if defined(VDSUSP)
	slvtio.c_cc[VDSUSP] = '\031';		/* ^Y */
#endif
#if defined(VREPRINT)
	slvtio.c_cc[VREPRINT] = '\022';		/* ^R */
#endif
	slvtio.c_cc[VLNEXT] = '\017';		/* ^V */

	slvtio.c_cc[VINTR]  = cc_array[0];
	slvtio.c_cc[VQUIT]  = cc_array[1];
	slvtio.c_cc[VERASE] = cc_array[2];
	slvtio.c_cc[VKILL]  = cc_array[3];
	slvtio.c_cc[VEOF]   = cc_array[4];
	slvtio.c_cc[VSUSP]  = cc_array[5];
	(void)tcsetattr(fd, TCSANOW, &slvtio);
}
	


/*
 * rcvwinsize - receive the window size of the real terminal window
 *
 *	Sent over network as "WINSIZE rn cn xn yn"  where .n is numeric string
 */

int rcvwinsize(sock)
	int sock;
{
	char buf[PBS_TERM_BUF_SZ];

	if (read_net(sock, buf, PBS_TERM_BUF_SZ) != PBS_TERM_BUF_SZ)
		return (-1);
	if (sscanf(buf, "WINSIZE %hu,%hu,%hu,%hu", &wsz.ws_row, &wsz.ws_col,
					&wsz.ws_xpixel, &wsz.ws_ypixel) != 4)
		return (-1);
	return 0;
}

int setwinsize(pty)
	int pty;
{
	if (ioctl(pty, TIOCSWINSZ, &wsz) < 0) {
		perror("ioctl TIOCSWINSZ");
		return (-1);
	}
	return (0);
}





/*
 * reader process - reads from the remote socket, and writes
 *	to the master pty
 */

int mom_reader(

  int s,
  int ptc)

  {
  extern ssize_t read_blocking_socket(int fd, void *buf, ssize_t count);
  char buf[1024];
  int c;

  /* read from the socket, and write to ptc */

  while (mom_reader_go) 
    {
    c = read_blocking_socket(s,buf,sizeof(buf));

    if (c > 0) 
      {
      int   wc;
      char *p = buf;
	
      while (c > 0) 
        {
        if ((wc = write(ptc,p,c)) < 0) 
          {
          if (errno == EINTR) 
            {
            /* write interrupted - retry */

            continue;
            }

          /* FAILURE - write failed */

          return(-1);
          }

        c -= wc;
        p += wc;
        }  /* END while (c > 0) */

      continue;
      } 

    if (c == 0) 
      {
      /* SUCCESS - all data written */

      return (0);
      } 

    if (c < 0) 
      {
      if (errno == EINTR)
        {
        /* read interrupted - retry */

        continue;
        }

      /* FAILURE - read failed */

      return(-1);
      }
    }    /* END while (1) */

  /* NOTREACHED*/

  return(0);
  }  /* END mom_reader() */






/*
 * Writer process: reads from master pty, and writes
 * data out to the rem socket
 */

int mom_writer(

  int s,
  int ptc)

  {
  char buf[1024];
  int c;

  /* read from ptc, and write to the socket */

  while (1) 
    {
    c = read(ptc,buf,sizeof(buf));

    if (c > 0) 
      {
      int wc;
      char *p = buf;
	
      while (c > 0) 
        {
        if ((wc = write(s,p,c)) < 0) 
          {
          if (errno == EINTR) 
            {
            continue;
            }

          /* FAILURE - write failed */

          return(-1);
          }

        c -= wc;
        p += wc;
        }  /* END while(c) */

      continue;
      }  /* END if (c > 0) */

    if (c == 0) 
      {
      /* SUCCESS - all data read */

      return(0);
      } 

    if (c < 0) 
      {
      if (errno == EINTR)
        {
        /* read interupted, retry */

        continue;
        }

      /* FAILURE - read failed */

      return(-1);
      }
    }    /* END while(1) */

  /*NOTREACHED*/

  return(-1);
  }  /* END mom_writer */




#if 0
/*
 * conn_qsub - connect to the qsub that submitted this interactive job
 * return >= 0 on SUCCESS, < 0 on FAILURE 
 */

int conn_qsub(

  char *hostname,
  long  port)

  {
  pbs_net_t hostaddr;
  int s;

  int flags;

  if ((hostaddr = get_hostaddr(hostname)) == (pbs_net_t)0)
    {
    return(-1);
    }

  s = client_to_svr(hostaddr,(unsigned int)port,0);

  /* NOTE:  client_to_svr() can return 0 for SUCCESS */

  /* assume SUCCESS requires s > 0 (USC) was 'if (s >= 0)' */
  /* above comment not enabled */

  if (s < 0)
    {
    /* FAILURE */

    return(-1);
    }

  /* this socket should be blocking */

  flags = fcntl(s,F_GETFL);

  flags &= ~O_NONBLOCK;

  fcntl(s,F_SETFL,flags);

  return(s);
  }  /* END conn_qsub() */
#endif


/* END mom_inter.c */
