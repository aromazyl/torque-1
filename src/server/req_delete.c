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
/*
 * req_delete.c 
 *
 * Functions relating to the Delete Job Batch Requests.
 *
 * Included funtions are:
 *	
 *
 */
#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <stdio.h>
#include <signal.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "work_task.h"
#include "attribute.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "job.h"
#include "queue.h"
#include "pbs_error.h"
#include "acct.h"
#include "log.h"
#include "svrfunc.h"


/* Global Data Items: */

extern char *msg_deletejob;
extern char *msg_delrunjobsig;
extern char *msg_manager;
extern struct server server;
extern time_t time_now;

/* Private Functions in this file */

static void post_delete_route A_((struct work_task *));
static void post_delete_mom1 A_((struct work_task *));
static void post_delete_mom2 A_((struct work_task *));

/* Private Data Items */

static char *deldelaystr = DELDELAY;

/* 
 * remove_stagein() - request that mom delete staged-in files for a job
 *	used when the job is to be purged after files have been staged in
 */

void remove_stagein(

  job *pjob)

  {
  struct batch_request *preq = 0;

  preq = cpy_stage(preq,pjob,JOB_ATR_stagein,0);

  if (preq != NULL) 
    {		
    /* have files to delete		*/

    /* change the request type from copy to delete  */

    preq->rq_type = PBS_BATCH_DelFiles;

    preq->rq_extra = NULL;

    if (relay_to_mom(
         pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
         preq, 
         release_req) == 0) 
      {
      pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_StagedIn;
      } 
    else 
      {
      /* log that we were unable to remove the files */

      log_event(
        PBSEVENT_JOB, 
        PBS_EVENTCLASS_FILE, 
        pjob->ji_qs.ji_jobid,
        "unable to remove staged in files for job");

      free_br(preq);
      }
    }
  
  return;
  }  /* END remove_stagein() */





/*
 * req_deletejob - service the Delete Job Request
 *
 *	This request deletes a job.
 */

void req_deletejob(

  struct batch_request *preq)

  {
  job		 *pjob;
  struct work_task *pwtold;
  struct work_task *pwtnew;
  int		  rc;
  char		 *sigt = "SIGTERM";

  pjob = chk_job_request(preq->rq_ind.rq_delete.rq_objname,preq);

  if (pjob == NULL)
    {
    return;
    }

  if (pjob->ji_qs.ji_state == JOB_STATE_TRANSIT) 
    {
    /*
     * Find pid of router from existing work task entry,
     * then establish another work task on same child.
     * Next, signal the router and wait for its completion;
     */

    pwtold = (struct work_task *)GET_NEXT(pjob->ji_svrtask);

    while (pwtold != NULL) 
      {
      if ((pwtold->wt_type == WORK_Deferred_Child) ||
          (pwtold->wt_type == WORK_Deferred_Cmp)) 
        {
        pwtnew = set_task(
          pwtold->wt_type,
          pwtold->wt_event, 
          post_delete_route,
          preq);

        if (pwtnew != NULL) 
          {
          /*
           * reset type in case the SIGCHLD came
           * in during the set_task;  it makes
           * sure that next_task() will find the
           * new entry.
           */

          pwtnew->wt_type = pwtold->wt_type;
          pwtnew->wt_aux = pwtold->wt_aux;

          kill((pid_t)pwtold->wt_event,SIGTERM);

          pjob->ji_qs.ji_substate = JOB_SUBSTATE_ABORT;

          return;	/* all done for now */
          } 
        else 
          {
          req_reject(PBSE_SYSTEM,0,preq,NULL,NULL);

          return;
          }
        }

      pwtold = (struct work_task *)GET_NEXT(pwtold->wt_linkobj);
      }

    /* should never get here ...  */

    log_err(-1,"req_delete","Did not find work task for router");

    req_reject(PBSE_INTERNAL,0,preq,NULL,NULL);

    return;
    } 

  if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN) 
    {
    /* being sent to MOM, wait till she gets it going */
    /* retry in one second				  */

    sprintf(log_buffer,"job cannot be deleted, state = PRERUN, requeuing delete request");
      
    log_event(
      PBSEVENT_JOB, 
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);

    pwtnew = set_task(
      WORK_Timed, 
      time_now + 1, 
      post_delete_route,
      preq);
 
    if (pwtnew == 0) 
      req_reject(PBSE_SYSTEM,0,preq,NULL,NULL);

    return;
    }

  /*
   * Log delete and if if requesting client is not job owner, send mail.
   */

  sprintf(log_buffer,"requestor=%s@%s",
    preq->rq_user,
    preq->rq_host);

  account_record(PBS_ACCT_DEL,pjob,log_buffer);

  sprintf(log_buffer,msg_manager, 
    msg_deletejob,
    preq->rq_user, 
    preq->rq_host);

  log_event(
    PBSEVENT_JOB, 
    PBS_EVENTCLASS_JOB,
    pjob->ji_qs.ji_jobid,
    log_buffer);
	
  if (preq->rq_extend != NULL) 
    {
    if (strncmp(preq->rq_extend,deldelaystr,strlen(deldelaystr))) 
      {
      /* have text message in request extention, add it */

      strcat(log_buffer,"\n");
      strcat(log_buffer,preq->rq_extend);
      }
    }

  if (svr_chk_owner(preq,pjob) != 0) 
    {
    svr_mailowner(pjob,MAIL_OTHER,MAIL_FORCE,log_buffer);
    }
	
  if (pjob->ji_qs.ji_state == JOB_STATE_RUNNING) 
    {
    /*
     * Send signal request to MOM.  The server will automagically
     * pick up and "finish" off the client request when MOM replies.
     */

    if ((rc = issue_signal(pjob,sigt,post_delete_mom1,preq)))
      req_reject(rc,0,preq,NULL,NULL);   /* cant send to MOM */

    /* normally will ack reply when mom responds */

    sprintf(log_buffer,msg_delrunjobsig,
      sigt);

    LOG_EVENT(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid, 
      log_buffer);

    return; 
    } 

  if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_CHKPT) != 0) 
    {
    /* job has restart file at mom, do end job processing */

    svr_setjobstate(pjob,JOB_STATE_EXITING,JOB_SUBSTATE_EXITING);

    pjob->ji_momhandle = -1;	

    /* force new connection */

    set_task(WORK_Immed,0,on_job_exit,(void *)pjob);
    } 
  else if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_StagedIn) != 0) 
    {
    /* job has staged-in file, should remove them */

    remove_stagein(pjob);

    job_abt(&pjob,NULL);
    } 
  else 
    {
    /*
     * the job is not transitting (though it may have been) and
     * is not running, so abort it.
     */

    job_abt(&pjob,NULL);
    }

  reply_ack(preq);

  return;
  }  /* END req_deletejob() */





/*
 * post_delete_route - complete the task of deleting a job which was
 *	being routed at the time the delete request was received.
 *
 *	Just recycle the delete request, the job will either be here or not.
 */

static void post_delete_route(

  struct work_task *pwt)

  {
  req_deletejob(
    (struct batch_request *)pwt->wt_parm1);

  return;
  }





/*
 * post_delete_mom1 - first of 2 work task trigger functions to finish the
 *	deleting of a running job.  This first part is invoked when MOM
 *	responds to the SIGTERM signal request.  
 */

static void post_delete_mom1(

  struct work_task *pwt)

  {
  int 		      delay = 0;
  int		      dellen = strlen(deldelaystr);
  job		     *pjob;
  struct work_task   *pwtnew;
  pbs_queue	     *pque;
  struct batch_request *preq_sig;		/* signal request to MOM */
  struct batch_request *preq_clt;		/* original client request */
  int		      rc;

  preq_sig = pwt->wt_parm1;
  rc       = preq_sig->rq_reply.brp_code;
  preq_clt = preq_sig->rq_extra;

  release_req(pwt);

  pjob = find_job(preq_clt->rq_ind.rq_delete.rq_objname);

  if (pjob == NULL) 
    {
    /* job has gone away */

    req_reject(PBSE_UNKJOBID,0,preq_clt,NULL,NULL);

    return;
    }

  if (rc) 
    {
    /* mom rejected request */

    if (rc == PBSE_UNKJOBID) 
      {
      /* MOM claims no knowledge, so just purge it */

      log_event(
        PBSEVENT_JOB, 
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        "MOM rejected signal during delete");

      /* removed the resources assigned to job */

      free_nodes(pjob);

      set_resc_assigned(pjob,DECR);

      job_purge(pjob);

      reply_ack(preq_clt);
      } 
    else 
      {
      req_reject(rc,0,preq_clt,NULL,NULL);
      }

    return;
    }

  if (preq_clt->rq_extend) 
    {
    if (strncmp(preq_clt->rq_extend,deldelaystr,dellen) == 0) 
      {
      delay = atoi(preq_clt->rq_extend + dellen);
      }
    }

  reply_ack(preq_clt);		/* dont need it, reply now */

  /*
   * if no delay specified in original request, see if kill_delay
   * queue attribute is set.
   */
	
  if (delay == 0) 
    {
    pque = pjob->ji_qhdr;

    if (pque->qu_attr[(int)QE_ATR_KillDelay].at_flags & ATR_VFLAG_SET)
      delay = pque->qu_attr[(int)QE_ATR_KillDelay].at_val.at_long;
    else
      delay = 2;
    }

  pwtnew = set_task(WORK_Timed,delay + time_now,post_delete_mom2,pjob);

  if (pwtnew) 
    {
    /* insure that work task will be removed if job goes away */

    append_link(&pjob->ji_svrtask, &pwtnew->wt_linkobj, pwtnew);
    }

  return;
  }  /* END post_delete_mom1() */





static void post_delete_mom2(

  struct work_task *pwt)

  {
  job  *pjob;
  char *sigk = "SIGKILL";

  pjob = (job *)pwt->wt_parm1;

  if (pjob->ji_qs.ji_state == JOB_STATE_RUNNING) 
    {
    issue_signal(pjob,sigk,release_req,0);

    sprintf(log_buffer,msg_delrunjobsig,sigk);

    LOG_EVENT(
      PBSEVENT_JOB, 
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid, 
      log_buffer);
    }

  return;
  }


/* END req_delete.c */

