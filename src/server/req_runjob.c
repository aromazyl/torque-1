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
 * req_runjob.c - functions dealing with a Run Job Request
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "job.h"
#include "queue.h"
#include "work_task.h"
#include "pbs_error.h"
#include "log.h"
#include "acct.h"
#include "svrfunc.h"
#include "net_connect.h"
#include "pbs_proto.h"

#ifdef __TDARWIN
#include <netinet/in.h>
#endif  /* END __TDARWIN */

/* External Functions Called: */

extern int   send_job A_((job *,pbs_net_t,int,int,void (*x)(),struct batch_request *));
extern void  set_resc_assigned A_((job *, enum batch_op));
extern struct batch_request *cpy_stage A_((struct batch_request *,job *,enum job_atr,int));
void      stream_eof A_((int,u_long,int));

/* Public Functions in this file */

int  svr_startjob A_((job *, struct batch_request *));

/* Private Function local to this file */

static void post_sendmom A_((struct work_task *));
static int  svr_stagein A_((job *, struct batch_request *, int, int)); 
static int  svr_strtjob2 A_((job *, struct batch_request *));
static job *chk_job_torun A_((struct batch_request *preq));
static int  assign_hosts A_((job *, char *given, int setflag));

/* Global Data Items: */

extern pbs_net_t pbs_mom_addr;
extern int	 pbs_mom_port;
extern struct server server;
extern char  server_host[PBS_MAXHOSTNAME + 1];
extern char  server_name[PBS_MAXSERVERNAME + 1];
extern char *msg_badexit;
extern char *msg_jobrun;
extern char *msg_manager;
extern char *msg_stageinfail;
extern int   scheduler_jobct;
extern int   scheduler_sock;
extern time_t time_now;
extern int   svr_totnodes;	/* non-zero if using nodes */
extern int   svr_tsnodes;	/* non-zero if time-shared nodes */

extern const char   *PJobSubState[];
extern unsigned int  pbs_rm_port;
extern char         *msg_err_malloc;





/*
 * req_runjob - service the Run Job and Asyc Run Job Requests
 *
 *	This request forces a job into execution.  Client must be privileged.
 */

void req_runjob(

  struct batch_request *preq)  /* I (modified) */

  {
  job  *pjob;
  int   rc;
  void *bp;

  if ((pjob = chk_job_torun(preq)) == NULL)
    {
    /* FAILURE */

    return;
    }

  if (preq->rq_conn == scheduler_sock)
    ++scheduler_jobct;	/* see scheduler_close() */

  sprintf(log_buffer,msg_manager,
    msg_jobrun, 
    preq->rq_user, 
    preq->rq_host);

  log_event(
    PBSEVENT_JOB, 
    PBS_EVENTCLASS_JOB, 
    pjob->ji_qs.ji_jobid, 
    log_buffer);

  /* If async run, reply now; otherwise reply is handled in */
  /* post_sendmom or post_stagein				  */

  /* NOTE:  nodes not assigned to job until svr_startjob() */

  /* perhaps node assignment should be handled immediately in async run? */

  if ((preq != NULL) && 
      (preq->rq_type == PBS_BATCH_AsyrunJob)) 
    {
    reply_ack(preq);

    preq = NULL;  /* cleared so we don't try to reuse */
    }

  if (((rc = svr_startjob(pjob,preq)) != 0) && (preq != NULL)) 
    {
    free_nodes(pjob);

    /* If the job has a non-empty rejectdest list, pass the first host into req_reject() */

    if ((bp = GET_NEXT(pjob->ji_rejectdest)) != NULL)
      {
      req_reject(rc,0,preq,((struct badplace *)bp)->bp_dest,"could not contact host");
      }
    else
      {
      req_reject(rc,0,preq,NULL,NULL);
      }
    }

  return;
  }  /* END req_runjob() */





/*
 * req_stagein - service the Stage In Files for a Job Request
 *
 *	This request causes MOM to start stagin in files. 
 *	Client must be privileged.
 */

void req_stagein(

  struct batch_request *preq)  /* I */

  {
  job *pjob;
  int  rc;

  if ((pjob = chk_job_torun(preq)) == NULL) 
    {
    return;
    } 

  if ((pjob->ji_wattr[(int)JOB_ATR_stagein].at_flags & ATR_VFLAG_SET) == 0) 
    {
    log_err(0,"req_stagein","stage-in information not set");

    req_reject(PBSE_IVALREQ,0,preq,NULL,NULL);

    return;
    } 

  if ((rc = svr_stagein(
      pjob, 
      preq,
      JOB_STATE_QUEUED, 
      JOB_SUBSTATE_STAGEIN)) )
    {
    free_nodes(pjob);

    req_reject(rc,0,preq,NULL,NULL);
    }

  return;
  }  /* END req_stagein() */





/*
 * post_stagein - process reply from MOM to stage-in request
 */

static void post_stagein(

  struct work_task *pwt)

  {
  int		      code;
  int		      newstate;
  int		      newsub;
  job		     *pjob;
  struct batch_request *preq;
  attribute	     *pwait;

  preq = pwt->wt_parm1;
  code = preq->rq_reply.brp_code;
  pjob = find_job(preq->rq_extra);

  free(preq->rq_extra);

  if (pjob != NULL) 
    {
    if (code != 0) 
      {
      /* stage in failed - hold job */

      free_nodes(pjob);

      pwait = &pjob->ji_wattr[(int)JOB_ATR_exectime];

      if ((pwait->at_flags & ATR_VFLAG_SET) == 0) 
        {
        pwait->at_val.at_long = time_now + PBS_STAGEFAIL_WAIT;

        pwait->at_flags |= ATR_VFLAG_SET;

        job_set_wait(pwait, pjob, 0);
        }

      svr_setjobstate(pjob, JOB_STATE_WAITING, JOB_SUBSTATE_STAGEFAIL);

      if (preq->rq_reply.brp_choice == BATCH_REPLY_CHOICE_Text)
        {
        /* set job comment */

        /* NYI */

        svr_mailowner(
          pjob, 
          MAIL_STAGEIN, 
          MAIL_FORCE,
          preq->rq_reply.brp_un.brp_txt.brp_str);
        }
      } 
    else 
      {
      /* stage in was successful */

      pjob->ji_qs.ji_svrflags |= JOB_SVFLG_StagedIn;

      if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_STAGEGO) 
        {
        /* continue to start job running */

        svr_strtjob2(pjob,NULL);
        } 
      else 
        {
        svr_evaljobstate(pjob,&newstate,&newsub,0);

        svr_setjobstate(pjob,newstate,newsub);
        }
      }
    }

  release_req(pwt);	/* close connection and release request */

  return;	
  }  /* END post_stagein() */





/*
 * svr_stagein - direct MOM to stage in the requested files for a job
 */

static int svr_stagein(

  job *pjob,
  struct batch_request *preq,
  int state,
  int substate)

  {
  struct batch_request *momreq = 0;
  int		      rc;

  momreq = cpy_stage(momreq, pjob, JOB_ATR_stagein, STAGE_DIR_IN);

  if (momreq == NULL) 
    {	
    /* no files to stage-in, go direct to sending job to mom */

    return(svr_strtjob2(pjob,preq));
    }

  /* have files to stage in */

  /* save job id for post_stagein */

  momreq->rq_extra = malloc(PBS_MAXSVRJOBID+1);

  if (momreq->rq_extra == 0) 
    {
    return(PBSE_SYSTEM);
    }

  strcpy(momreq->rq_extra, pjob->ji_qs.ji_jobid);

  rc = relay_to_mom(
    pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
    momreq, 
    post_stagein);

  if (rc == 0) 
    {
    svr_setjobstate(pjob, state, substate);

    /*
     * stage-in started ok - reply to client as copy may
     * take too long to wait.
     */

    if (preq != NULL)
      reply_ack(preq);
    } 
  else 
    {
    free(momreq->rq_extra);
    }

  return(rc);
  }  /* END svr_stagein() */





/*
 * svr_startjob - place a job into running state by shipping it to MOM
 */

int svr_startjob(

  job                  *pjob,	/* job to run */
  struct batch_request *preq)	/* NULL or Run Job batch request */

  {
  int     f;
  int     rc;
  int     sock, nodenum;
  struct  hostent *hp;
  char   *nodestr, *cp, *hostlist;
  int     size;
  struct  sockaddr_in saddr;

  badplace *bp;
  char     *id = "svr_startjob";

  /* if not already setup, transfer the control/script file basename */
  /* into an attribute accessable to MOM				   */

  if (!(pjob->ji_wattr[(int)JOB_ATR_hashname].at_flags & ATR_VFLAG_SET))
    {
    if (job_attr_def[(int)JOB_ATR_hashname].at_decode(
          &pjob->ji_wattr[(int)JOB_ATR_hashname],
          NULL, 
          NULL, 
          pjob->ji_qs.ji_fileprefix))
      {
      return(PBSE_SYSTEM);
      }
    }

  /* if exec_host already set and either (hot start or checkpoint)	*/
  /* then use the host(s) listed in exec_host			*/

  rc = 0;

  f = pjob->ji_wattr[(int)JOB_ATR_exec_host].at_flags & ATR_VFLAG_SET;

  if ((f != 0) && 
     ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HOTSTART) ||
      (pjob->ji_qs.ji_svrflags & JOB_SVFLG_CHKPT)) && 
     ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HasNodes) == 0)) 
    {
    rc = assign_hosts(pjob,pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,0);
    } 
  else if (f == 0) 
    {
    /* exec_host not already set, get hosts and set it */

    rc = assign_hosts(pjob,NULL,1);
    }

  if (rc != 0)
    {
    return(rc);
    }

#define BOEING
#ifdef BOEING
  /* Verify that all the nodes are alive via a TCP connect. */

  /* NOTE: Copy the nodes into a temp string because strtok() is destructive. */

  size = strlen(pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str);
  hostlist = malloc(size + 1);

  if (hostlist == NULL)
    {
    sprintf(log_buffer,"could not allocate temporary buffer (malloc failed) -- skipping TCP connect check");
    log_err(errno,id,log_buffer);
    }
  else
    {
    /* Get the first host. */

    strncpy(hostlist,pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,size);
    hostlist[size] = '\0';
    nodestr = strtok(hostlist,"+");
    }

  while (nodestr != NULL)
    {
    /* Truncate from trailing slash on (if one exists). */

    if ((cp = strchr(nodestr,'/')) != NULL)
      {
      cp[0] = '\0';
      }

    /* Lookup IP address of host. */

    if ((hp = gethostbyname(nodestr)) == NULL)
      {
      sprintf(log_buffer,"could not contact %s (gethostbyname failed)",nodestr);
      log_err(errno,id,log_buffer);

      /* Add this host to the reject destination list for the job */

      bp = (badplace *)malloc(sizeof (badplace));
      if (bp == (badplace *)0)
        {
        log_err(errno,id,msg_err_malloc);
        return;
        }
      CLEAR_LINK(bp->bp_link);
      strcpy(bp->bp_dest,nodestr);
      append_link(&pjob->ji_rejectdest,&bp->bp_link, bp);

      return(PBSE_RESCUNAV);
      }

    /* Open a socket. */

    if ((sock = socket(AF_INET,SOCK_STREAM,0)) == -1)
      {
      sprintf(log_buffer,"could not contact %s (socket failed)",nodestr);
      log_err(errno,id,log_buffer);

      /* Add this host to the reject destination list for the job */

      bp = (badplace *)malloc(sizeof (badplace));
      if (bp == (badplace *)0)
        {
        log_err(errno,id,msg_err_malloc);
        return;
        }
      CLEAR_LINK(bp->bp_link);
      strcpy(bp->bp_dest,nodestr);
      append_link(&pjob->ji_rejectdest,&bp->bp_link, bp);

      return(PBSE_RESCUNAV);
      }

    /* Set the host information. */

    memset(&saddr,'\0',sizeof(saddr));
    saddr.sin_family = AF_INET;
    memcpy(&saddr.sin_addr,hp->h_addr,hp->h_length);
    saddr.sin_port = htons(pbs_rm_port);

    /* Connect to the host. */

    if (connect(sock,(struct sockaddr *)&saddr,sizeof(saddr)) < 0)
      {
      sprintf(log_buffer,"could not contact %s (connect failed)",nodestr);
      log_err(errno,id,log_buffer);

      /* Add this host to the reject list for the job */

      bp = (badplace *)malloc(sizeof (badplace));
      if (bp == (badplace *)0)
        {
        log_err(errno,id,msg_err_malloc);
        return;
        }
      CLEAR_LINK(bp->bp_link);
      strcpy(bp->bp_dest,nodestr);
      append_link(&pjob->ji_rejectdest,&bp->bp_link, bp);

      return(PBSE_RESCUNAV);
      }

    /* Clean up and get next host. */

    close(sock);
    nodestr = strtok(NULL,"+");
    }

  if (hostlist != NULL)
    free(hostlist);

  /* END MOM verification check via TCP. */

#endif  /* END BOEING */

  /* Next, are there files to be staged-in? */

  if ((pjob->ji_wattr[(int)JOB_ATR_stagein].at_flags & ATR_VFLAG_SET) &&
      (pjob->ji_qs.ji_substate != JOB_SUBSTATE_STAGECMP)) 
    {
    /* yes, we do that first; then start the job */

    rc = svr_stagein(
      pjob, 
      preq, 
      JOB_STATE_RUNNING,
      JOB_SUBSTATE_STAGEGO);

    /* note, the positive acknowledgment is done by svr_stagein */
    } 
  else 
    {
    /* No stage-in or already done, start job executing */

    rc = svr_strtjob2(pjob,preq);
    }

  return (rc);
  }  /* END svr_startjob() */


/* PATH
    svr_startjob()
      svr_strtjob2()
        send_job()
          svr_connect()
          PBSD_queuejob() 
*/




static int svr_strtjob2(

  job                  *pjob,
  struct batch_request *preq)

  {
  extern char *PAddrToString(pbs_net_t *);

  int old_state;
  int old_subst;

  char tmpLine[1024];

  old_state = pjob->ji_qs.ji_state;
  old_subst = pjob->ji_qs.ji_substate;

  /* send the job to MOM */

  svr_setjobstate(pjob,JOB_STATE_RUNNING,JOB_SUBSTATE_PRERUN);

  if (send_job(
        pjob,
        pjob->ji_qs.ji_un.ji_exect.ji_momaddr,
        pbs_mom_port, 
        MOVE_TYPE_Exec, 
        post_sendmom,
        (void *)preq) == 2) 
    {
    /* SUCCESS */

    return(0);
    } 

  sprintf(tmpLine,"unable to run job, send to MOM '%s' failed",
    PAddrToString(&pjob->ji_qs.ji_un.ji_exect.ji_momaddr));
      
  log_event(
    PBSEVENT_JOB, 
    PBS_EVENTCLASS_JOB,
    pjob->ji_qs.ji_jobid,
    tmpLine);

  pjob->ji_qs.ji_destin[0] = '\0';

  svr_setjobstate(
    pjob, 
    old_state, 
    old_subst);

  return(pbs_errno);
  }    /* END svr_strtjob2() */






/*
 * post_sendmom - clean up action for child started in send_job
 *	which was sending a job "home" to MOM
 *
 * If send was successful, mark job as executing, and call stat_mom_job()
 * to obtain session id.
 *
 * If send didn't work, requeue the job.
 *
 * If the work_task has a non-null wt_parm2, it is the address of a batch
 * request to which a reply must be sent.
 *
 * Returns: none.
 */

static void post_sendmom(

  struct work_task *pwt)  /* I */

  {
  char	*id = "post_sendmom";
  int	 newstate;
  int	 newsub;
  int	 r;
  int	 stat;
  job	*jobp = (job *)pwt->wt_parm1;
  struct batch_request *preq = (struct batch_request *)pwt->wt_parm2;

  stat = pwt->wt_aux;

  if (WIFEXITED(stat)) 
    {
    r = WEXITSTATUS(stat);
    } 
  else 
    {
    r = 2;

    sprintf(log_buffer,msg_badexit,
      stat);

    strcat(log_buffer,id);

    log_event(
      PBSEVENT_SYSTEM, 
      PBS_EVENTCLASS_JOB,
      jobp->ji_qs.ji_jobid,
      log_buffer);
    }

  switch (r) 
    {
    case 0:  /* send to MOM went ok */

      jobp->ji_qs.ji_svrflags &= ~JOB_SVFLG_HOTSTART;

      if (preq)
        reply_ack(preq);
			
      /* record start time for accounting */

      jobp->ji_qs.ji_stime = time_now;

      /* update resource usage attributes */

      set_resc_assigned(jobp,INCR);

      if (jobp->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN) 
        {
        /* may be EXITING if job finished first */

        svr_setjobstate(jobp,JOB_STATE_RUNNING,JOB_SUBSTATE_RUNNING);	

        /* above saves job structure */
        }

      /* accounting log for start or restart */

      if (jobp->ji_qs.ji_svrflags & JOB_SVFLG_CHKPT)
        account_record(PBS_ACCT_RESTRT,jobp,NULL);
      else
        account_jobstr(jobp);

      /* if any dependencies, see if action required */
		
      if (jobp->ji_wattr[(int)JOB_ATR_depend].at_flags & ATR_VFLAG_SET)
        depend_on_exec(jobp);

      svr_mailowner(jobp,MAIL_BEGIN,MAIL_NORMAL,NULL);

      /*
       * it is unfortunate, but while the job has gone into execution,
       * there is no way of obtaining the session id except by making
       * a status request of MOM.  (Even if the session id was passed
       * back to the sending child, it couldn't get up to the parent.)
       */

      jobp->ji_momstat = 0;

      stat_mom_job(jobp);

      break;

    case 10:

      /* NOTE: if r == 10, connection to mom timed out.  Mark node down */

      stream_eof(0,jobp->ji_qs.ji_un.ji_exect.ji_momaddr,0);

      /* send failed, requeue the job */

      log_event(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        jobp->ji_qs.ji_jobid,
        "unable to run job, MOM rejected");

      free_nodes(jobp);

      if (jobp->ji_qs.ji_substate != JOB_SUBSTATE_ABORT)
        {
        if (preq != NULL)
          req_reject(PBSE_MOMREJECT,0,preq,NULL,"connection to mom timed out");

        svr_evaljobstate(jobp,&newstate,&newsub,1);

        svr_setjobstate(jobp,newstate,newsub);
        }
      else
        {
        if (preq != NULL)
          req_reject(PBSE_BADSTATE,0,preq,NULL,"job was aborted by mom");
        }

      break;

    default:	

      /* send failed, requeue the job */

      log_event(
        PBSEVENT_JOB, 
        PBS_EVENTCLASS_JOB,
        jobp->ji_qs.ji_jobid,
        "unable to run job, MOM rejected");

      free_nodes(jobp);

      if (jobp->ji_qs.ji_substate != JOB_SUBSTATE_ABORT) 
        {
        if (preq != NULL)
          {
          char tmpLine[1024];
  
          sprintf(tmpLine,"send failed, %s",
            PJobSubState[jobp->ji_qs.ji_substate]);
          
          req_reject(PBSE_MOMREJECT,0,preq,NULL,tmpLine);
          }

        svr_evaljobstate(jobp,&newstate,&newsub,1);

        svr_setjobstate(jobp,newstate,newsub);
        } 
      else 
        {
        if (preq != NULL)
          req_reject(PBSE_BADSTATE,0,preq,NULL,"send failed - abort");
        }
			
      break;
    }  /* END switch(r) */

  return;
  }  /* END post_sendmom() */





/*
 * chk_job_torun - check state and past execution host of a job for which
 *	files are about to be staged in or the job is about to be run.
 * 	Returns pointer to job if all is ok, else returns null.
 */

static job *chk_job_torun(

  struct batch_request *preq)  /* I */

  {
  job		 *pjob;
  struct rq_runjob *prun;
  int 		  rc;

  prun = &preq->rq_ind.rq_run;

  if ((pjob = chk_job_request(prun->rq_jid,preq)) == 0)
    {
    return(NULL);
    }

  if ((pjob->ji_qs.ji_state == JOB_STATE_TRANSIT) ||
      (pjob->ji_qs.ji_state == JOB_STATE_EXITING) ||
      (pjob->ji_qs.ji_substate == JOB_SUBSTATE_STAGEGO) ||
      (pjob->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN)  ||
      (pjob->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING))  
    {
    /* job already started */

    req_reject(PBSE_BADSTATE,0,preq,NULL,NULL);

    return(NULL);
    }

  if (preq->rq_type == PBS_BATCH_StageIn) 
    {
    if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_STAGEIN) 
      {
      req_reject(PBSE_BADSTATE,0,preq,NULL,NULL);

      return(NULL);
      }
    }

  if ((preq->rq_perm & (ATR_DFLAG_MGWR|ATR_DFLAG_OPWR)) == 0) 
    {
    /* run request non-authorized */

    req_reject(PBSE_PERM,0,preq,NULL,NULL);

    return(NULL);
    }

  if (pjob->ji_qhdr->qu_qs.qu_type != QTYPE_Execution) 
    {
    /* the job must be in an execution queue */

    log_err(0,"chk_job_torun","attempt to start job in non-execution queue");

    req_reject(PBSE_IVALREQ,0,preq,NULL,"job not in execution queue");

    return(NULL);
    }

  /* where to execute the job */

  if (pjob->ji_qs.ji_svrflags & (JOB_SVFLG_CHKPT|JOB_SVFLG_StagedIn)) 
    {
    /* job has been checkpointed or files already staged in */
    /* in this case, exec_host must be already set	 	*/

    if (prun->rq_destin[0] != '\0') 
      {
      /* specified destination must match exec_host */

      if (strcmp(
           prun->rq_destin,
           pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str) != 0) 
        {
        req_reject(PBSE_EXECTHERE,0,preq,NULL,NULL);

        return(NULL);
        }
      }

    if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HasNodes) == 0)
      {
      /* re-reserve nodes and leave exec_host as is */

      if ((rc = assign_hosts(
            pjob,
            pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,
            0)) != 0) 
        {
        req_reject(PBSE_EXECTHERE,0,preq,NULL,"cannot assign hosts");

        return(NULL);
        }
      }
    } 
  else 
    {
    /* job has not run before or need not run there again	*/
    /* reserve nodes and set exec_host anew			*/

    if (prun->rq_destin[0] == '\0') 
      {
      rc = assign_hosts(pjob,0,1);
      } 
    else 
      {
      rc = assign_hosts(pjob,prun->rq_destin,1);
      }

    if (rc != 0) 
      {
      req_reject(rc,0,preq,NULL,NULL);

      return(NULL);
      }
    }
			
  return(pjob);
  }  /* END chk_job_torun() */





/*
 * assign_hosts - assign hosts (nodes) to job by the following rules:
 *	1. use nodes that are "given"; from exec_host when required by
 *		checkpoint-restart or file stage-in, or from run command.
 *	2. use nodes that match user's resource request.
 *	3. use default (local system or a single node).
 */

static int assign_hosts(

  job  *pjob,           /* I (modified) */
  char *given,
  int   set_exec_host)  /* I (boolean) */

  {
  unsigned int	 dummy;
  char		*list = NULL;
  char		*hosttoalloc;
  pbs_net_t	 momaddr = 0;
  resource	*pres;
  int		 rc = 0;
  extern char 	*mom_host;
	
  pres = find_resc_entry(
    &pjob->ji_wattr[(int)JOB_ATR_resource],
    find_resc_def(svr_resc_def,"neednodes",svr_resc_size));

  if (given != NULL) 
    {	
    /* assign what was specified in run request */

    hosttoalloc = given;
    } 
  else if (pres != NULL) 
    {	
    /* assign what was in "neednodes" */

    hosttoalloc = pres->rs_value.at_val.at_str;

    if (hosttoalloc == NULL) 
      {
      return(PBSE_UNKNODEATR);
      }
    } 
  else if (svr_totnodes == 0) 
    {	
    /* assign "local" */

    if ((server.sv_attr[(int)SRV_ATR_DefNode].at_flags & ATR_VFLAG_SET) && 
        (server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str != 0)) 
      {
      hosttoalloc = server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str;
      } 
    else 
      {
      hosttoalloc = mom_host;
      momaddr = pbs_mom_addr;
      }
    } 
  else if ((server.sv_attr[(int)SRV_ATR_DefNode].at_flags & ATR_VFLAG_SET) && 
           (server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str != 0)) 
    {
    /* alloc what server's attribute default_node is set to */

    hosttoalloc = server.sv_attr[(int)SRV_ATR_DefNode].at_val.at_str;
    } 
  else if (svr_tsnodes != 0) 
    {
    /* find first time-shared node */

    if ((hosttoalloc = find_ts_node()) == NULL)
      { 
      /* FAILURE */

      return(PBSE_NOTSNODE);
      }
    } 
  else 
    {
    /* fall back to 1 cluster node */

    hosttoalloc = PBS_DEFAULT_NODE;
    }

  /* do we need to allocate the (cluster) node(s)? */

  if (svr_totnodes != 0) 
    {
    if ((rc = is_ts_node(hosttoalloc)) != 0) 
      {
      rc = set_nodes(pjob,hosttoalloc,&list);

      set_exec_host = 1;	/* maybe new VPs, must set */

      hosttoalloc = list;
      }
    } 
 
  if (rc == 0) 
    {
    /* set_nodes succeeded */

    if (set_exec_host != 0) 
      {
      job_attr_def[(int)JOB_ATR_exec_host].at_free(
        &pjob->ji_wattr[(int)JOB_ATR_exec_host]);

      job_attr_def[(int)JOB_ATR_exec_host].at_decode(
        &pjob->ji_wattr[(int)JOB_ATR_exec_host],
        NULL,
        NULL,
        hosttoalloc);  /* O */

      pjob->ji_modified = 1;
      } 
    else 
      {
      /* leave exec_host alone and reuse old IP address */

      momaddr = pjob->ji_qs.ji_un.ji_exect.ji_momaddr;
		
      hosttoalloc = pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str;
      }

    strncpy(
      pjob->ji_qs.ji_destin,
      parse_servername(hosttoalloc,&dummy),
      PBS_MAXROUTEDEST);

    if (momaddr == 0) 
      {
      momaddr = get_hostaddr(pjob->ji_qs.ji_destin);
 
      if (momaddr == 0) 
        {
        free_nodes(pjob);

        if (list != NULL) 
          free(list);

        sprintf(log_buffer,"ALERT:  job cannot allocate node '%s'",
          pjob->ji_qs.ji_destin);

        log_event(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          log_buffer);

        return(PBSE_BADHOST);
        }
      }

    pjob->ji_qs.ji_un.ji_exect.ji_momaddr = momaddr;
    }  /* END if (rc == 0) */

  if (list != NULL)
    free(list);

  return(rc);
  }  /* END assign_hosts() */


/* END req_runjob.c */

