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
#include <stdio.h>
#include "libpbs.h"
#include "string.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "server.h"
#include "job.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_error.h"
#include "log.h"
#include "svrfunc.h"

/* Global Data */

extern struct credential conn_credent[PBS_NET_MAX_CONNECTIONS];
extern char *pbs_o_host;
extern char  server_host[];
extern char *msg_badstate;
extern char *msg_permlog;
extern char *msg_unkjobid;
extern time_t time_now;

/*
 * svr_chk_owner - compare a user name from a request and the name of
 *	the user who owns the job.
 *
 *	Return 0 if same, non 0 if user is not the job owner
 */


int svr_chk_owner(

  struct batch_request *preq,
  job                  *pjob)

  {
  char  owner[PBS_MAXUSER + 1];
  char *pu;
  char  rmtuser[PBS_MAXUSER + 1];

  /* map user@host to "local" name */

  pu = site_map_user(preq->rq_user,preq->rq_host);

  if (pu == NULL)
    {
    return(-1);
    }

  strncpy(rmtuser,pu,PBS_MAXUSER);

  /*
   * Get job owner name without "@host" and then map to "local" name.
   *
   * This is a bit of a kludge, the POSIX 1003.15 standard forgot to
   * include a separate attribute for the submitting (qsub) host.  At
   * present, the only place defined where this can be found is the 
   * value of the environmental variable POSIX2_PBS_O_HOST in the
   * "variable-list" job attribute.
   */

  get_jobowner(pjob->ji_wattr[(int)JOB_ATR_job_owner].at_val.at_str,owner);

  pu = site_map_user(owner,get_variable(pjob,pbs_o_host));

  return(strcmp(rmtuser,pu));
  }  /* END svr_chk_owner() */





/*
 * svr_authorize_jobreq - determine if requestor is authorized to make
 *	request against the job.  This is only called for batch requests
 *	against jobs, not manager requests against queues or the server.
 *
 * 	Returns 0 if authorized (job owner, operator, administrator)
 *	 	-1 if not authorized.
 */

int svr_authorize_jobreq(

  struct batch_request *preq,
  job                  *pjob)

  {
  /* Is requestor special privileged? */

  if ((preq->rq_perm & (ATR_DFLAG_OPRD|ATR_DFLAG_OPWR|
                        ATR_DFLAG_MGRD|ATR_DFLAG_MGWR)) != 0)
    {
    return(0);
    }

  /* is requestor is the job owner */

  if (svr_chk_owner(preq,pjob) == 0)
    {
    return(0);
    }

  /* not authorized */

  return(-1);
  }





/*
 * svr_get_privilege - get privilege level of a user.
 *
 *	Privilege is granted to a user at a host.  A user is automatically
 *	granted "user" privilege.  The user@host pair must appear in
 *	the server's administrator attribute list to be granted "manager"
 *	privilege and/or appear in the operators attribute list to be
 *	granted "operator" privilege.  If either acl is unset, then root
 *	on the server machine is granted that privilege.
 *
 *	If "PBS_ROOT_ALWAYS_ADMIN" is defined, then root always has privilege
 *	even if not in the list.
 *
 *	The returns are based on the access permissions of attributes, see
 *	attribute.h.
 */

int svr_get_privilege(

  char *user,
  char *host)

  {
  int   is_root = 0;
  int   priv = (ATR_DFLAG_USRD | ATR_DFLAG_USWR);
  char  uh[PBS_MAXUSER + PBS_MAXHOSTNAME + 2];

  strcpy(uh,user);
  strcat(uh,"@");
  strcat(uh,host);

  /* NOTE:  enable case insensitive host check (CRI) */

  if ((strcmp(user,PBS_DEFAULT_ADMIN) == 0) &&
      !strcasecmp(host,server_host)) 
    {
    is_root = 1;

#ifdef PBS_ROOT_ALWAYS_ADMIN
    return(priv|ATR_DFLAG_MGRD|ATR_DFLAG_MGWR|ATR_DFLAG_OPRD|ATR_DFLAG_OPWR);
#endif
    }

  if (!(server.sv_attr[(int)SRV_ATR_managers].at_flags & ATR_VFLAG_SET)) 
    {
    if (is_root)
      priv |= (ATR_DFLAG_MGRD | ATR_DFLAG_MGWR);
    } 
  else if (acl_check(&server.sv_attr[SRV_ATR_managers],uh,ACL_User))
    {
    priv |= (ATR_DFLAG_MGRD | ATR_DFLAG_MGWR);
    }

  if (!(server.sv_attr[(int)SRV_ATR_operators].at_flags & ATR_VFLAG_SET)) 
    {
    if (is_root)
      priv |= (ATR_DFLAG_OPRD | ATR_DFLAG_OPWR);
    } 
  else if (acl_check(&server.sv_attr[SRV_ATR_operators],uh,ACL_User))
    {
    priv |= (ATR_DFLAG_OPRD | ATR_DFLAG_OPWR);
    }
	
  return(priv);
  }  /* END svr_get_privilege() */





/*
 * authenticate_user - authenticate user by checking name against credential
 *		       provided on connection via Authenticate User request.
 *
 * Returns 0 if user is who s/he claims, non-zero error code if not
 */

int authenticate_user(

  struct batch_request *preq,
  struct credential    *pcred)

  {
  char uath[PBS_MAXUSER + PBS_MAXHOSTNAME + 1];

  if (strncmp(preq->rq_user,pcred->username,PBS_MAXUSER))
    {
    return(PBSE_BADCRED);
    }

  if (strncmp(preq->rq_host,pcred->hostname,PBS_MAXHOSTNAME))
    {
    return(PBSE_BADCRED);
    }

  if (pcred->timestamp) 
    {
    if ((pcred->timestamp - CREDENTIAL_TIME_DELTA > time_now) ||
        (pcred->timestamp + CREDENTIAL_LIFETIME < time_now))
      {
      return(PBSE_EXPIRED);
      }
    }

  /* If Server's Acl_User enabled, check if user in list */

  if (server.sv_attr[SRV_ATR_AclUserEnabled].at_val.at_long) 
    {
    strcpy(uath,preq->rq_user);
    strcat(uath,"@");
    strcat(uath,preq->rq_host);

    if (acl_check(&server.sv_attr[SRV_ATR_AclUsers],uath,ACL_User) == 0) 
      {
#ifdef PBS_ROOT_ALWAYS_ADMIN
      if ((strcmp(preq->rq_user,PBS_DEFAULT_ADMIN) != 0) ||
          (strcasecmp(preq->rq_host,server_host) != 0))
#endif /* PBS_ROOT_ALWAYS_ADMIN */
        return(PBSE_PERM);
      }
    }

  /* A site stub for additional checking */

  return(site_allow_u(preq->rq_user,preq->rq_host));
  }





/*
 * chk_job_request - check legality of a request against a job
 *
 *	this checks the most conditions common to most job batch requests.
 *	It also returns a pointer to the job if found and the tests pass.
 */


job *chk_job_request(

  char                 *jobid,
  struct batch_request *preq)

  {
  job *pjob;

  if ((pjob = find_job(jobid)) == NULL) 
    {
    log_event(
      PBSEVENT_DEBUG, 
      PBS_EVENTCLASS_JOB,
      preq->rq_ind.rq_move.rq_jid, 
      msg_unkjobid);

    req_reject(PBSE_UNKJOBID,0,preq,NULL,NULL);

    return(NULL);
    }

  if (svr_authorize_jobreq(preq,pjob) == -1) 
    {
    sprintf(log_buffer,msg_permlog, 
      preq->rq_type,
      "Job", 
      pjob->ji_qs.ji_jobid,
      preq->rq_user, 
      preq->rq_host);

    log_event(
      PBSEVENT_SECURITY, 
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid, 
      log_buffer);

    req_reject(PBSE_PERM,0,preq,NULL,NULL);

    return(NULL);
    }

  if (pjob->ji_qs.ji_state == JOB_STATE_EXITING) 
    {
    sprintf(log_buffer,"%s %d", 
      msg_badstate,
      pjob->ji_qs.ji_state);

    log_event(
      PBSEVENT_DEBUG, 
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid, 
      log_buffer);

    req_reject(PBSE_BADSTATE,0,preq,NULL,NULL);

    return(NULL);
    }

  return(pjob);
  }



