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

#include <netdb.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(FD_SET_IN_SYS_SELECT_H)
#  include <sys/select.h>
#endif

#include "portability.h"
#include "server_limits.h"
#include "net_connect.h"


/* External Functions Called */

extern void log_err A_((int, char *, char *));
extern void process_request A_((int));

extern time_t time();

/* Global Data (I wish I could make it private to the library, sigh, but
 * C don't support that scope of control.)
 *
 * This array of connection structures is used by the server to maintain 
 * a record of the open I/O connections, it is indexed by the socket number.  
 */

struct	connection svr_conn[PBS_NET_MAX_CONNECTIONS];

/*
 * The following data is private to this set of network interface routines.
 */

static int	max_connection = PBS_NET_MAX_CONNECTIONS;
static int	num_connections = 0;
static fd_set	readset;
static void	(*read_func[2]) A_((int));
static enum conn_type settype[2];		/* temp kludge */
static char logbuf[256];

/* Private function within this file */

static void accept_conn();





/*
 * init_network - initialize the network interface
 *	allocate a socket and bind it to the service port,
 *	add the socket to the readset for select(),
 *	add the socket to the connection structure and set the
 *	processing function to accept_conn()
 */
	
int init_network(

  unsigned int port,
  void         (*readfunc)())

  {
  int		 i;
  static int	 initialized = 0;
  int 		 sock;
  struct sockaddr_in socname;
  enum conn_type   type;

  if (initialized == 0) 
    {
    for (i = 0;i < PBS_NET_MAX_CONNECTIONS;i++) 
      svr_conn[i].cn_active = Idle;

    FD_ZERO(&readset);

    type = Primary;
    } 
  else if (initialized == 1)
    {
    type = Secondary;
    }
  else 
    {
    return(-1);		/* too many main connections */
    }

  net_set_type(type,FromClientDIS);

  /* save the routine which should do the reading on connections	*/
  /* accepted from the parent socket				*/

  read_func[initialized++] = readfunc;

  sock = socket(AF_INET, SOCK_STREAM, 0);

  if (sock < 0) 
    {
    log_err(errno,"init_network","socket() failed");

    return(-1);
    }

  if (FD_SETSIZE < PBS_NET_MAX_CONNECTIONS)
    max_connection = FD_SETSIZE;

  i = 1;

  setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&i,sizeof(i));

  /* name that socket "in three notes" */

  socname.sin_port= htons((unsigned short)port);
  socname.sin_addr.s_addr = INADDR_ANY;
  socname.sin_family = AF_INET;

  if (bind(sock,(struct sockaddr *)&socname,sizeof(socname)) < 0) 
    {
    close(sock);

    log_err(errno,"init_network" ,"bind failed");

    return(-1);
    }
	
  /* record socket in connection structure and select set */

  add_conn(sock,type,(pbs_net_t)0,0,accept_conn);
	
  /* start listening for connections */

  if (listen(sock,128) < 0) 
    {
    log_err(errno,"init_network","listen failed");

    return(-1);
    }

  return (0);
  }  /* END init_network() */





/*
 * wait_request - wait for a request (socket with data to read)
 *	This routine does a select on the readset of sockets,
 *	when data is ready, the processing routine associated with
 *	the socket is invoked.
 */

int wait_request(

  time_t waittime)  /* I (seconds) */

  {
  int i;
  int n;

  time_t now;

  fd_set selset;

  struct timeval timeout;
  void close_conn();

  timeout.tv_usec = 0;
  timeout.tv_sec  = waittime;

  selset = readset;

  n = select(FD_SETSIZE,&selset,(fd_set *)0,(fd_set *)0,&timeout);

  if (n == -1) 
    {
    if (errno == EINTR)
      {
      n = 0;	/* interrupted, cycle arround */
      }
    else 
      {
      log_err(errno,"wait_request","select failed");

      return(-1);
      }
    }

  for (i = 0;i < max_connection && n;i++) 
    {
    if (FD_ISSET(i,&selset)) 
      {	
      /* this socket has data */

      n--;

      svr_conn[i].cn_lasttime = time((time_t *)0);

      if (svr_conn[i].cn_active != Idle) 
        {
        svr_conn[i].cn_func(i);
        } 
      else 
        {
        log_err(-1,"wait_request","select bad socket");

        FD_CLR(i,&readset);

        close(i);
        }
      }
    }    /* END for (i) */

  /* have any connections timed out ?? */

  now = time((time_t *)0);

  for (i = 0;i < max_connection;i++) 
    {
    struct connection *cp;
    u_long             ipaddr;

    cp = &svr_conn[i];

    if (cp->cn_active != FromClientASN && cp->cn_active != FromClientDIS)
      continue;

    if ((now - cp->cn_lasttime) <= PBS_NET_MAXCONNECTIDLE)
      continue;

    if (cp->cn_authen & PBS_NET_CONN_NOTIMEOUT)
      continue;	/* do not time-out this connection */

    ipaddr = cp->cn_addr;

    sprintf(logbuf,"timeout connection from %lu.%lu.%lu.%lu (%d seconds)",
      (ipaddr & 0xff000000) >> 24,
      (ipaddr & 0x00ff0000) >> 16,
      (ipaddr & 0x0000ff00) >> 8,
      (ipaddr & 0x000000ff),
      (int)waittime);

    /* locate node associated with interface, mark node as down until node responds */

    /* NYI */

    log_err(0,"wait_request",logbuf);

    close_conn(i);
    }  /* END for (i) */
		
  return(0);
  }  /* END wait_request() */





/*
 * accept_conn - accept request for new connection
 *	this routine is normally associated with the main socket,
 *	requests for connection on the socket are accepted and
 *	the new socket is added to the select set and the connection
 *	structure - the processing routine is set to the external
 *	function: process_request(socket)
 */

static void accept_conn(

  int sd)  /* main socket with connection request pending */

  {
  int newsock;
  struct sockaddr_in from;

#if defined _SOCKLEN_T
  socklen_t fromsize;
#else /* _SOCKLEN_T */
  int fromsize;
#endif /* _SCOKLEN_T */
	
  /* update lasttime of main socket */

  svr_conn[sd].cn_lasttime = time((time_t *)0);

  fromsize = sizeof(from);

  newsock = accept(sd,(struct sockaddr *)&from,&fromsize);

  if (newsock == -1) 
    {
    log_err(errno, "accept_conn", "accept failed");

    return;
    }

  if ((num_connections >= max_connection) ||
      (newsock >= PBS_NET_MAX_CONNECTIONS)) 
    {
    close(newsock);

    return;		/* too many current connections */
    }
	
  /* add the new socket to the select set and connection structure */

  add_conn(
    newsock, 
    FromClientDIS, 
    (pbs_net_t)ntohl(from.sin_addr.s_addr),
    (unsigned int)ntohs(from.sin_port),
    read_func[(int)svr_conn[sd].cn_active]);

  return;
  }  /* END accept_conn() */




/*
 * add_conn - add a connection to the svr_conn array.
 *	The params addr and port are in host order.
 */

void add_conn(

  int sock,		   /* socket associated with connection */
  enum conn_type type,	   /* type of connection */
  pbs_net_t      addr,	   /* IP address of connected host */
  unsigned int   port,	   /* port number (host order) on connected host */
  void (*func) A_((int)))  /* function to invoke on data rdy to read */

  {
  num_connections++;

  FD_SET(sock,&readset);

  svr_conn[sock].cn_active   = type;
  svr_conn[sock].cn_addr     = addr;
  svr_conn[sock].cn_port     = (unsigned short)port;
  svr_conn[sock].cn_lasttime = time((time_t *)0);
  svr_conn[sock].cn_func     = func;
  svr_conn[sock].cn_oncl     = 0;

#if defined(__TDARWIN) && !defined(__TDARWINBIND)
  svr_conn[sock].cn_authen = PBS_NET_CONN_FROM_PRIVIL;
#else /* __TDARWIN && !__TDARWINBIND */
  if (port < IPPORT_RESERVED)
    svr_conn[sock].cn_authen = PBS_NET_CONN_FROM_PRIVIL;
  else
    svr_conn[sock].cn_authen = 0;
#endif /* __TDARWIN && !__TDARWINBIND */

  return;
  }  /* END add_conn() */
	



	
/*
 * close_conn - close a network connection
 *	does physical close, also marks the connection table
 */

void close_conn(

  int sd) /* I */

  {
  if ((sd < 0) || (max_connection <= sd))
    {
    return;
    }

  if (svr_conn[sd].cn_active == Idle)
    {
    return;
    }

  close(sd);

  /* if there is a function to call on close, do it */

  if (svr_conn[sd].cn_oncl != 0)
    svr_conn[sd].cn_oncl(sd);

  FD_CLR(sd,&readset);

  svr_conn[sd].cn_addr = 0;
  svr_conn[sd].cn_handle = -1;
  svr_conn[sd].cn_active = Idle;
  svr_conn[sd].cn_func = (void (*)())0;
  svr_conn[sd].cn_authen = 0;

  num_connections--;

  return;
  }  /* END close_conn() */




/*
 * net_close - close all network connections but the one specified,
 *	if called with impossible socket number (-1), all will be closed.
 *	This function is typically called when a server is closing down and
 *	when it is forking a child.
 *
 *	We clear the cn_oncl field in the connection table to prevent any
 *	"special on close" functions from being called.
 */

void net_close(

  int but)  /* I */

  {
  int i;

  for (i = 0;i < max_connection;i++) 
    {
    if (i != but) 
      {
      svr_conn[i].cn_oncl = 0;

      close_conn(i);
      }
    }

  return;
  }




/*
 * get_connectaddr - return address of host connected via the socket
 *	This is in host order.
 */

pbs_net_t get_connectaddr(

  int sock)

  {
  return(svr_conn[sock].cn_addr);
  }



int find_conn(

  pbs_net_t addr)  /* I */

  {
  int index;

  /* NOTE:  there may be multiple connections per addr (not handled) */

  for (index = 0;index < PBS_NET_MAX_CONNECTIONS;index++)
    {
    if (addr == svr_conn[index].cn_addr)
      {
      return(index);
      }
    }    /* END for (index) */

  return(-1);
  }





/*
 * get_connecthost - return name of host connected via the socket
 */

int get_connecthost(sock, namebuf, size)
	int   sock;
	char *namebuf;
	int   size;
{
	struct hostent *phe;
	struct in_addr  addr;
	int	namesize = 0;

	size--;
	addr.s_addr = htonl(svr_conn[sock].cn_addr);

	if ((phe = gethostbyaddr((char *)&addr, sizeof(struct in_addr), 
	     AF_INET)) == (struct hostent *)0) {
		(void)strcpy(namebuf, inet_ntoa(addr));
	}
	else {
		namesize = strlen(phe->h_name);
		(void)strncpy(namebuf, phe->h_name, size);
		*(namebuf+size) = '\0';
	}
	if (namesize > size)
		return (-1);
	else
		return (0);
}

/*
 * net_set_type() - a temp kludge for supporting two protocols during
 *	the conversion from ASN.1 to PBS DIS
 */

void net_set_type(which, type)
	enum conn_type which;
	enum conn_type type;
{
	settype[(int)which] = type;
}


