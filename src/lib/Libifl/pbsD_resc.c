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

/*	pbs_resc.c
*/

#include <pbs_config.h>   /* the master config generated by configure */

#include <string.h>
#include <stdio.h>
#include "libpbs.h"
#include "dis.h"

/* Variables for this file */

static int nodes_avail = 0;
static int nodes_alloc = 0;
static int nodes_resrv = 0;
static int nodes_down  = 0;
static char *resc_nodes = "nodes";

/*
 * encode_DIS_resc() - encode a resource related request,
 *	Used by pbs_rescquery(), pbs_rescreserve() and pbs_rescfree()
 *
 *	Data items are:		signed int	resource handle
 *				unsigned int	count of query strings
 *		followed by that number of:
 *				 string	resource query
 */

static int encode_DIS_Resc(sock, rlist, ct, rh)
	int	      sock;
	char	    **rlist;
	int	      ct;
	resource_t    rh;
{
	int    i;
	int    rc;

	if ( (rc = diswsi(sock, rh)) == 0) {  /* resource reservation handle */

		/* next send the number of resource strings */

		if ( (rc = diswui(sock, ct)) == 0) {
		
			/* now send each string (if any) */

			for (i = 0; i < ct; ++i) {
				if ( (rc = diswst(sock, *(rlist + i))) != 0)
					break;
			}
		}
	}
	return rc;
}

/*
 * PBS_resc() - internal common code for sending resource requests
 *
 *	Formats and sends the requests for pbs_rescquery(), pbs_rescreserve(),
 *	and pbs_rescfree().   Note, while the request is overloaded for all
 *	three, each has its own expected reply format.
 */

static int PBS_resc(c, reqtype, rescl, ct, rh)
	int	   c;
	int	   reqtype;
	char     **rescl;
	int	   ct;
	resource_t rh;
{
	int rc;
	int sock;

	sock = connection[c].ch_socket;

	/* setup DIS support routines for following DIS calls */

	DIS_tcp_setup(sock);

	if ( (rc = encode_DIS_ReqHdr(sock, reqtype, pbs_current_user)) ||
	     (rc = encode_DIS_Resc(sock, rescl, ct, rh)) ||
	     (rc = encode_DIS_ReqExtend(sock, (char *)0)) )   {
		connection[c].ch_errtxt = strdup(dis_emsg[rc]);
		return (pbs_errno = PBSE_PROTOCOL);
	}
	if (DIS_tcp_wflush(sock)) {
		return (pbs_errno = PBSE_PROTOCOL);
	}
	return (0);
}




/*
 * pbs_rescquery() - query the availability of resources
 */

int pbs_rescquery(

  int    c,
  char **resclist,	/* In - list of queries */
  int    num_resc,	/* In - number in list  */
  int   *available, 	/* Out - number available per query */
  int   *allocated, 	/* Out - number allocated per query */
  int   *reserved,	/* Out - number reserved  per query */
  int   *down)		/* Out - number down/off  per query */

  {
  int                 i;
  struct batch_reply *reply;
  int                 rc = 0;

  if (resclist == NULL) 
    {
    connection[c].ch_errno = PBSE_RMNOPARAM;

    pbs_errno = PBSE_RMNOPARAM;

    return(pbs_errno);
    }

  /* send request */

  if ((rc = PBS_resc(c,PBS_BATCH_Rescq,resclist,num_resc,(resource_t)0)) != 0)
    {
    return(rc);
    }

  /* read in reply */

  reply = PBSD_rdrpy(c);

  /* FIXME: this should be checking pbs_errno? */
  if (rc == PBSE_NONE)
    {
    /* copy in available and allocated numbers */

    for (i = 0;i < num_resc;++i)
      {
      *(available + i) = *(reply->brp_un.brp_rescq.brq_avail + i);
      *(allocated + i) = *(reply->brp_un.brp_rescq.brq_alloc + i);
      *(reserved  + i) = *(reply->brp_un.brp_rescq.brq_resvd + i);
      *(down      + i) = *(reply->brp_un.brp_rescq.brq_down  + i);
      }
    }

  PBSD_FreeReply(reply);

  return(rc);
  }  /* END pbs_rescquery() */




/* pbs_reserve() - reserve resources */

int pbs_rescreserve(c, rl, num_resc, prh)
	int	  c;		/* connection */
	char	**rl;		/* list of resources */
	int	  num_resc;	/* number of items in list */
	resource_t *prh;	/* ptr to resource reservation handle */
{
	int	rc;
	struct batch_reply *reply;

	if (rl == NULL) {
		connection[c].ch_errno = PBSE_RMNOPARAM;
		return (pbs_errno = PBSE_RMNOPARAM);
	}
	if (prh == NULL) {
		connection[c].ch_errno = PBSE_RMBADPARAM;
		return (pbs_errno = PBSE_RMBADPARAM);
	}

	/* send request */
	
	if ((rc = PBS_resc(c, PBS_BATCH_ReserveResc, rl, num_resc, *prh)) != 0)
		return (rc);

	/*
	 * now get reply, if reservation successful, the reservation handle,
	 * resource_t, is in the  aux field
	 */

	reply = PBSD_rdrpy(c);

	if ( ((rc = connection[c].ch_errno) == PBSE_NONE) ||
	     (rc == PBSE_RMPART) ) {
		*prh = reply->brp_auxcode;
	}
	PBSD_FreeReply(reply);

	return (rc);
}

/*
 * pbs_release() - release a resource reservation 
 *
 *	To encode we send same info as for reserve except that the resource
 *	list is empty.
 */

int pbs_rescrelease(c, rh)
	int	   c;
	resource_t rh;
{
	struct batch_reply *reply;
	int	rc;

	if ((rc = PBS_resc(c, PBS_BATCH_ReleaseResc, (char **)0, 0, rh)) != 0)
		return (rc);

	/* now get reply */

	reply = PBSD_rdrpy(c);

	PBSD_FreeReply(reply);

	return (connection[c].ch_errno);
}

/*
 * The following routines are provided as a convience in converting
 * older schedulers which did addreq() of "totpool", "usepool", and
 * "avail".
 *
 * The "update" flag if non-zero, causes a new resource query to be sent
 * to the server.  If zero, the existing numbers are used.
 */

/* totpool() - return total number of nodes */

int totpool(int con, int update)
{
	if (update)
		if (pbs_rescquery(con, &resc_nodes, 1, &nodes_avail,
				&nodes_alloc, &nodes_resrv, &nodes_down) != 0) {
			return (-1);
		}
	return (nodes_avail + nodes_alloc + nodes_resrv + nodes_down);
}


/* usepool() - return number of nodes in use, includes reserved and down */

int usepool(int con, int update)
{
	if (update)
		if (pbs_rescquery(con, &resc_nodes, 1, &nodes_avail,
				&nodes_alloc, &nodes_resrv, &nodes_down) != 0) {
			return (-1);
		}
	return (nodes_alloc + nodes_resrv + nodes_down);
}

/*
 * avail - returns answer about available of a specified node set
 *	"yes"	if available (job could be run)
 *	"no"	if not currently available
 *	"never"	if can never be satified
 *	"?"	if error in request
 */

char *avail(int con, char *resc)
{
	int av;
	int al;
	int res;
	int dwn;
	
	if (pbs_rescquery(con, &resc, 1, &av, &al, &res, &dwn) != 0)
		return ("?");

	else if (av > 0)
		return ("yes");
	else if (av == 0)
		return ("no");
	else
		return ("never");
}

