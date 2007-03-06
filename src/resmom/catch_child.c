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
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>
#include "dis.h"
#include "libpbs.h"
#include "portability.h"
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include "list_link.h"
#include "server_limits.h"
#include "attribute.h"
#include "resource.h"
#include "job.h"
#include "log.h"
#include "credential.h"
#include "batch_request.h"
#include "net_connect.h"
#include "svrfunc.h"
#include "mom_mach.h"
#include "mom_func.h"
#include "pbs_error.h"
#include "pbs_proto.h"
#include "rpp.h"
#ifdef ENABLE_CPA
#include "pbs_cpa.h"
#endif

#if defined(PENABLE_DYNAMIC_CPUSETS)
#include <cpuset.h>
#endif /* PENABLE_DYNAMIC_CPUSETS */

/* External Functions */

/* External Globals */

extern char		*path_epilog;
extern char             *path_epiloguser;
extern char             *path_epilogp;
extern char             *path_epiloguserp;
extern char		*path_jobs;
extern unsigned int	default_server_port;
extern tlist_head	svr_alljobs, mom_polljobs;
extern int		exiting_tasks;
extern char		*msg_daemonname;
extern int		termin_child;
extern struct connection svr_conn[];
extern int		resc_access_perm;
extern char		*path_aux;

extern int              LOGLEVEL;

extern char            *PJobSubState[];


/* external prototypes */

u_long resc_used(job *,char *,u_long (*f) A_((resource *)));
static void obit_reply A_((int));
extern int tm_reply A_((int,int,tm_event_t));
extern u_long addclient A_((char *));
extern void encode_used A_((job *,tlist_head *));
extern void job_nodes A_((job *));
extern int task_recov A_((job *));



/* END external prototypes */





/*
 * catch_child() - the signal handler for  SIGCHLD.
 *
 * To keep the signal handler simple for
 *	SIGCHLD  - just indicate there was one.
 */

void catch_child(

  int sig)

  {
  termin_child = 1;

  return;
  }  /* END catch_child() */





hnodent	*get_node(

  job        *pjob,
  tm_node_id  nodeid)

  {
  int      i;
  vnodent *vp = pjob->ji_vnods;

  for (i = 0;i < pjob->ji_numvnod;i++,vp++) 
    {
    if (vp->vn_node == nodeid)
      {
      return(vp->vn_host);
      }
    }

  return(NULL);
  }  /* END get_node() */





#if	MOM_CHECKPOINT == 1
/*
**	Restart each task which has exited and has TI_FLAGS_CHKPT turned on.
**	If all tasks have been restarted, turn off MOM_CHKPT_POST.
*/

void chkpt_partial(

  job *pjob)

  {
  static char	id[] = "chkpt_partial";
  int		i;
  char		namebuf[MAXPATHLEN];
  char		*filnam;
  task		*ptask;
  int		texit = 0;
  extern	char	task_fmt[];
  extern	char	*path_checkpoint;

  assert(pjob != NULL);

  strcpy(namebuf, path_checkpoint);
  strcat(namebuf, pjob->ji_qs.ji_fileprefix);
  strcat(namebuf, JOB_CKPT_SUFFIX);

  i = strlen(namebuf);

  filnam = &namebuf[i];

  for (ptask = (task *)GET_NEXT(pjob->ji_tasks);
       ptask != NULL;
       ptask = (task *)GET_NEXT(ptask->ti_jobtask)) 
    {
    /*
    ** See if the task was marked as one of those that did
    ** actually checkpoint.
    */

    if ((ptask->ti_flags & TI_FLAGS_CHKPT) == 0)
      continue;

    texit++;

    /*
    ** Now see if it was reaped.  We don't want to
    ** fool with it until we see it die.
    */

    if (ptask->ti_qs.ti_status != TI_STATE_EXITED)
      continue;

    texit--;

    sprintf(filnam,task_fmt, 
      ptask->ti_qs.ti_task);

    if (mach_restart(ptask,namebuf) == -1)
      goto fail;

    ptask->ti_qs.ti_status = TI_STATE_RUNNING;
    ptask->ti_flags &= ~TI_FLAGS_CHKPT;

    task_save(ptask);
    }

  if (texit == 0) 
    {
    char        oldname[MAXPATHLEN];
    struct stat	statbuf;

    /*
    ** All tasks should now be running.
    ** Turn off MOM_CHKPT_POST flag so job is back to where
    ** it was before the bad checkpoint attempt.
    */

    pjob->ji_flags &= ~MOM_CHKPT_POST;

    /*
    ** Get rid of incomplete checkpoint directory and
    ** move old chkpt dir back to regular if it exists.
    */

    *filnam = '\0';

    remtree(namebuf);

    strcpy(oldname,namebuf);

    strcat(oldname,".old");

    if (stat(oldname,&statbuf) == 0) 
      {
      if (rename(oldname,namebuf) == -1)
        pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_CHKPT;
      }
    }

  return;

fail:

  sprintf(log_buffer,"%s failed to restart",
    pjob->ji_qs.ji_jobid);

  log_err(errno,id,log_buffer);

  pjob->ji_flags &= ~MOM_CHKPT_POST;

  kill_job(pjob,SIGKILL);

  return;
  }  /* END chkpt_partial() */

#endif	/* MOM_CHECKPOINT */




void scan_for_exiting()

  {
  char         *id = "scan_for_exiting";

  static char noconnect[] =
    "No contact with server at hostaddr %x, port %d, jobid %s errno %d";

  pid_t		cpid;
  int		found_one = 0;
  job		*nxjob;
  job		*pjob;
  task		*ptask;
  obitent	*pobit;
  struct batch_request	*preq;
  int		sock;
  int		sock3;
  char		*svrport;
  char		*cookie;
  unsigned int	port;
  u_long	gettime		A_((resource *));
  u_long	getsize		A_((resource *));
  task         *task_find	A_((job	*,tm_task_id));
  int im_compose A_((int,char *,char *,int,tm_event_t,tm_task_id));

#ifdef  PENABLE_DYNAMIC_CPUSETS
  char           cQueueName[8];
  char           cPermFile[1024];
  struct passwd *pwdp;
#endif  /* PENABLE_DYNAMIC_CPUSETS */

  /*
  ** Look through the jobs.  Each one has it's tasks examined
  ** and if the job is EXITING, it meets it's fate depending
  ** on whether this is the Mother Superior or not.
  */

  for (pjob = (job *)GET_NEXT(svr_alljobs);pjob != NULL;pjob = nxjob) 
    {
    nxjob = (job *)GET_NEXT(pjob->ji_alljobs);

#if MOM_CHECKPOINT == 1

    /*
    ** If a checkpoint with aborts is active,
    ** skip it.  We don't want to report any obits
    ** until we know that the whole thing worked.
    */

    if (pjob->ji_flags & MOM_CHKPT_ACTIVE) 
      {
      continue;
      }

    /*
    ** If the job has had an error doing a checkpoint with
    ** abort, the MOM_CHKPT_POST flag will be on.
    */

    if (pjob->ji_flags & MOM_CHKPT_POST) 
      {
      chkpt_partial(pjob);

      continue;
      }
 
#endif	/* MOM_CHECKPOINT */

    if (!(pjob->ji_wattr[(int)JOB_ATR_Cookie].at_flags & ATR_VFLAG_SET))
      continue;

    cookie = pjob->ji_wattr[(int)JOB_ATR_Cookie].at_val.at_str;

    /*
    ** Check each EXITED task.  They transistion to DEAD here.
    */

    for (
        ptask = (task *)GET_NEXT(pjob->ji_tasks);
        ptask != NULL;
        ptask = (task *)GET_NEXT(ptask->ti_jobtask)) 
      {
      if (ptask->ti_qs.ti_status != TI_STATE_EXITED)
        continue;

      /*
      ** Check if it is the top shell.
      */

      if (ptask->ti_qs.ti_parenttask == TM_NULL_TASK) 
        {
        pjob->ji_qs.ji_un.ji_momt.ji_exitstat = ptask->ti_qs.ti_exitstat;

        LOG_EVENT(
          PBSEVENT_JOB, 
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid, 
          "job was terminated");

        DBPRT(("Terminating job, sending IM_KILL_JOB to sisters\n"));

        if (send_sisters(pjob,IM_KILL_JOB) == 0) 
          {
          pjob->ji_qs.ji_substate = JOB_SUBSTATE_EXITING;
          job_save(pjob,SAVEJOB_QUICK);
          }
        }

      /*
      ** process any TM client obits waiting.
      */

      pobit = (obitent *)GET_NEXT(ptask->ti_obits);

      while (pobit != NULL) 
        {
        hnodent	*pnode;

        pnode = get_node(pjob,pobit->oe_info.fe_node);

        /* see if this is me or another MOM */

        if (pjob->ji_nodeid == pnode->hn_node) 
          {
          task *tp;

          /* send event to local child */

          tp = task_find(pjob,pobit->oe_info.fe_taskid);

          assert(tp != NULL); 

          if (tp->ti_fd != -1) 
            {  
            tm_reply(tp->ti_fd,IM_ALL_OKAY,pobit->oe_info.fe_event);

            diswsi(tp->ti_fd,ptask->ti_qs.ti_exitstat);

            DIS_tcp_wflush(tp->ti_fd);
            }
          }
        else if (pnode->hn_stream != -1) 
          {
          /*
          ** Send a response over to MOM
          ** whose brat sent the request.
          */

          im_compose(
            pnode->hn_stream,
            pjob->ji_qs.ji_jobid,cookie,
            IM_ALL_OKAY,
            pobit->oe_info.fe_event,
            pobit->oe_info.fe_taskid);

          diswsi(pnode->hn_stream,ptask->ti_qs.ti_exitstat);

          rpp_flush(pnode->hn_stream);
          }

        delete_link(&pobit->oe_next);
        free(pobit);

        pobit = (obitent *)GET_NEXT(ptask->ti_obits);
        }  /* END while (pobit) */

      ptask->ti_fd = -1;
      ptask->ti_qs.ti_status = TI_STATE_DEAD;
 
      DBPRT(("%s: task is dead\n",
        id));
 
      task_save(ptask);
      }  /* END for (ptask) */

    /*
    ** Look to see if the job has terminated.  If it is
    ** in any state other than EXITING continue on.
    */

    if (pjob->ji_qs.ji_substate != JOB_SUBSTATE_EXITING)
      continue;

    /*
    ** Look to see if I am a regular sister.  If so,
    ** check to see if there is an obit event to
    ** send back to mother superior.
    ** Otherwise, I need to wait for her to send a KILL_JOB
    ** so I can send the obit (unless she died).
    */

    if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HERE) == 0) 
      {
      int stream;

      stream = (pjob->ji_hosts == NULL) ? 
        -1 : 
        pjob->ji_hosts[0].hn_stream;

      /*
      ** Check to see if I'm still in touch with
      ** the head office.  If not, I'm just going to
      ** get rid of this job.
      */

      if (stream == -1) 
        {
        if (LOGLEVEL >= 6)
          {
          LOG_EVENT(
            PBSEVENT_JOB,
            PBS_EVENTCLASS_JOB,
            pjob->ji_qs.ji_jobid,
            "connection to server lost - killing job");
          }

        DBPRT(("connection to server lost - killing job %s\n",pjob->ji_qs.ji_jobid));

        kill_job(pjob,SIGKILL);

        job_purge(pjob);

        continue;
        }

      /*
      ** No event waiting for sending info to MS
      ** so I'll just sit tight.
      */

      if (pjob->ji_obit == TM_NULL_EVENT)
        continue;

      /*
      ** Check to see if any tasks are running.
      */

      ptask = (task *)GET_NEXT(pjob->ji_tasks);

      while (ptask != NULL) 
        {
        if (ptask->ti_qs.ti_status == TI_STATE_RUNNING)
          break;

        ptask = (task *)GET_NEXT(ptask->ti_jobtask);
        }

      /* Still somebody there so don't send it yet.  */

      if (ptask != NULL)
        continue;
   
      if ((pjob->ji_wattr[(int)JOB_ATR_interactive].at_flags & ATR_VFLAG_SET) &&
           pjob->ji_wattr[(int)JOB_ATR_interactive].at_val.at_long) 
        {
        if (run_pelog(PE_EPILOG,path_epilogp,pjob,PE_IO_TYPE_NULL) != 0)
          {
          log_err(-1,id,"parallel epilog failed");
          }
        if (run_pelog(PE_EPILOGUSER,path_epiloguserp,pjob,PE_IO_TYPE_NULL) != 0)
          {
          log_err(-1,id,"user parallel epilog failed");
          }
        }
      else
        {
        if (run_pelog(PE_EPILOG,path_epilogp,pjob,PE_IO_TYPE_STD) != 0)
          {
          log_err(-1,id,"parallel epilog failed");
          }
        if (run_pelog(PE_EPILOGUSER,path_epiloguserp,pjob,PE_IO_TYPE_STD) != 0)
          {
          log_err(-1,id,"user parallel epilog failed");
          }
        }
                                   
      /*
      ** No tasks running ... format and send a
      ** reply to the mother superior and get rid of
      ** the job.
      */

      im_compose(
        stream, 
        pjob->ji_qs.ji_jobid,
        cookie, 
        IM_ALL_OKAY,
        pjob->ji_obit, 
        TM_NULL_TASK);

      diswul(stream,resc_used(pjob,"cput",gettime));
      diswul(stream,resc_used(pjob,"mem",getsize));
      diswul(stream,resc_used(pjob,"vmem",getsize));

      rpp_flush(stream);

      if (LOGLEVEL >= 6)
        {
        LOG_EVENT(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          "all tasks complete - purging job as sister");
        }

      DBPRT(("all tasks complete - purging job as sister (%s)\n",pjob->ji_qs.ji_jobid));

      job_purge(pjob);

      continue;
      }  /* END if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HERE) == 0) */

    /*
    ** At this point, we know we are Mother Superior for this
    ** job which is EXITING.  Time for it to die.
    */

    if (LOGLEVEL >= 2)
      {
      log_record(
        PBSEVENT_DEBUG,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        "local task termination detected.  killing job");
      }

    pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_Suspend;

    kill_job(pjob,SIGKILL);

#ifdef ENABLE_CPA
    if (CPADestroyPartition(pjob) != 0)
      continue;
#endif

    delete_link(&pjob->ji_jobque);	/* unlink for poll list */

    /*
     * +  Open connection to the Server (for the Job Obituary)
     * +  Set the connection to call obit_reply when the reply 
     *    arrives.
     * +  fork child process, parent looks for more terminated jobs.
     * Child:
     * +  Run the epilogue script (if one)
     * +  Send the Job Obit Request (notice).
     */

    svrport = strchr(pjob->ji_wattr[(int)JOB_ATR_at_server].at_val.at_str,(int)':');

    if (svrport)
      port = atoi(svrport + 1); 
    else
      port = default_server_port;

    sock = client_to_svr(pjob->ji_qs.ji_un.ji_momt.ji_svraddr,port,1,NULL);

    if (sock < 0) 
      {
      sprintf(log_buffer,noconnect,
        pjob->ji_qs.ji_un.ji_momt.ji_svraddr,
        port,
        pjob->ji_qs.ji_jobid, 
        errno);

      LOG_EVENT(
        PBSEVENT_DEBUG,
        PBS_EVENTCLASS_REQUEST,
        "jobobit", 
        log_buffer);

      /*
       * return (break out of loop), leave exiting_tasks set
       * so Mom will retry Obit when server is available
       */

      return;
      } 

    if (sock < 3) 
      {
      /* needs to be 3 or above for epilogue */

      sock3 = fcntl(sock,F_DUPFD,3);
 
      close(sock);
      } 
    else
      {
      sock3 = sock;
      }
			
    pjob->ji_momhandle = sock3;

    add_conn(
      sock3, 
      ToServerDIS,
      pjob->ji_qs.ji_un.ji_momt.ji_svraddr,
      port, 
      obit_reply);

    if (LOGLEVEL >= 2)
      {
      log_record(
        PBSEVENT_DEBUG,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        "performing job clean-up");
      }

    cpid = fork_me(sock3);

    if (cpid > 0) 
      {
      /* parent = mark that it is being sent */

      pjob->ji_qs.ji_substate = JOB_SUBSTATE_OBIT;

      if (found_one++ == 0) 
        {
        continue;	/* look for one more */
        }
 
      break;	/* two at a time is our limit */
      } 
    else if (cpid < 0)
      {
      continue;
      }

    /* child */

    /* check epilog script */

    if ((pjob->ji_wattr[(int)JOB_ATR_interactive].at_flags & ATR_VFLAG_SET) &&
         pjob->ji_wattr[(int)JOB_ATR_interactive].at_val.at_long) 
      {
      /* job is interactive */

      if (run_pelog(PE_EPILOG,path_epilog,pjob,PE_IO_TYPE_NULL) != 0)
        {
        log_err(-1,id,"system epilog failed - interactive job");
        }

      if (run_pelog(PE_EPILOGUSER,path_epiloguser,pjob,PE_IO_TYPE_NULL) != 0)
        {
        log_err(-1,id,"user epilog failed - interactive job");
        }
      } 
    else 
      {
      /* job is not interactive */

      int rc;

      if ((rc = run_pelog(PE_EPILOG,path_epilog,pjob,PE_IO_TYPE_STD)) != 0)
        {
        sprintf(log_buffer,"system epilog failed w/rc=%d",
          rc);

        log_err(-1,id,log_buffer);
        }

      if (run_pelog(PE_EPILOGUSER,path_epiloguser,pjob,PE_IO_TYPE_STD) != 0)
        {
        log_err(-1,id,"user epilog failed");
        }
      }    /* END else (jobisinteractive) */

#ifdef PENABLE_DYNAMIC_CPUSETS

    /* FIXME: this is the wrong place for this code.
     * it should be called from job_purge() */
    pwdp = getpwuid(pjob->ji_qs.ji_un.ji_momt.ji_exuid);
    strncpy(cQueueName,pwdp->pw_name,3);
    strncat(cQueueName,pjob->ji_qs.ji_jobid,5);

    /* FIXME: use the path_jobs variable */
    strcpy(cPermFile,PBS_SERVER_HOME);
    strcat(cPermFile,"/mom_priv/jobs/");
    strcat(cPermFile,cQueueName);
    strcat(cPermFile,".CS");

    cpusetDestroy(cQueueName);
    unlink(cPermFile);

    memset(cQueueName,0,sizeof(cQueueName));
    memset(cPermFile,0,sizeof(cPermFile));

    /* NOTE:  must clear cpusets even if child not captured, ie, mom is down when job completes */

#endif /* PENABLE_DYNAMIC_CPUSETS */

    /* send the job obiturary notice to the server */

    preq = alloc_br(PBS_BATCH_JobObit);

    strcpy(preq->rq_ind.rq_jobobit.rq_jid,pjob->ji_qs.ji_jobid);

    preq->rq_ind.rq_jobobit.rq_status = 
      pjob->ji_qs.ji_un.ji_momt.ji_exitstat;

    CLEAR_HEAD(preq->rq_ind.rq_jobobit.rq_attr);

    resc_access_perm = ATR_DFLAG_RDACC;

    encode_used(pjob,&preq->rq_ind.rq_jobobit.rq_attr);

    DIS_tcp_setup(sock3);

    encode_DIS_ReqHdr(sock3,PBS_BATCH_JobObit,pbs_current_user);

    encode_DIS_JobObit(sock3,preq);

    encode_DIS_ReqExtend(sock3,0);

    DIS_tcp_wflush(sock3);

    close(sock3);

    log_record(
      PBSEVENT_DEBUG, 
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid, 
      "Obit sent");

    exit(0);
    }  /* END for (pjob) */

  if (pjob == 0) 
    exiting_tasks = 0; /* went through all jobs */

  return;
  }  /* END scan_for_exiting() */




/* is sock associated with a peer MOM or with pbs_server? */

/*
 * obit_reply - read and process the reply from the server acknowledging
 *	the job obiturary notice.
 */

static void obit_reply(

  int sock)

  {
  int			irtn;
  job			*nxjob;
  job			*pjob;
  attribute		*pattr;
  struct batch_request	*preq;
  int			 x;	/* dummy */

  /* read and decode the reply */

  preq = alloc_br(PBS_BATCH_JobObit);

  CLEAR_HEAD(preq->rq_ind.rq_jobobit.rq_attr);

  while ((irtn = DIS_reply_read(sock,&preq->rq_reply)) &&
         (errno == EINTR));

  if (irtn != 0) 
    {
    sprintf(log_buffer,"DIS_reply_read failed, rc=%d sock=%d",
      irtn, 
      sock);

    log_err(errno,"obit_reply",log_buffer);

    preq->rq_reply.brp_code = -1;
    }
	 
  /* find the job associated with the reply by the socket number */
  /* saved in the job structure, ji_momhandle */

  pjob = (job *)GET_NEXT(svr_alljobs);

  while (pjob != NULL) 
    {
    nxjob = (job *)GET_NEXT(pjob->ji_alljobs);

    if ((pjob->ji_qs.ji_substate == JOB_SUBSTATE_OBIT) &&
        (pjob->ji_momhandle == sock)) 
      {
      switch (preq->rq_reply.brp_code) 
        {
        case PBSE_NONE:

          /* normal ack, mark job as exited */

          pjob->ji_qs.ji_substate = JOB_SUBSTATE_EXITED;

          job_save(pjob,SAVEJOB_QUICK);

          break;

        case PBSE_ALRDYEXIT:

          /* have already told the server before recovery */
          /* the server will contact us to continue       */

          pjob->ji_qs.ji_substate = JOB_SUBSTATE_EXITED;

          job_save(pjob,SAVEJOB_QUICK);

          break;

        case PBSE_CLEANEDOUT:

          /* all jobs discarded by server, discard job */

          pattr = &pjob->ji_wattr[(int)JOB_ATR_interactive];

          if (((pattr->at_flags & ATR_VFLAG_SET) == 0) ||
               (pattr->at_val.at_long == 0)) 
            {
            /* do this if not interactive */

            unlink(std_file_name(pjob,StdOut,&x));
            unlink(std_file_name(pjob,StdErr,&x));
            unlink(std_file_name(pjob,Chkpt,&x));
            }

          mom_deljob(pjob);

          break;

        case -1:

          /* FIXME - causes epilogue to be run twice! */

          pjob->ji_qs.ji_substate = JOB_SUBSTATE_EXITING;

          exiting_tasks = 1;

          break;

        default:

          {
          char tmpLine[1024];

          switch (preq->rq_reply.brp_code)
            {
            case PBSE_BADSTATE:

              sprintf(tmpLine,"server rejected job obit - unexpected job state");

              break;

            case PBSE_SYSTEM:

              sprintf(tmpLine,"server rejected job obit - server not ready for job completion");

              break;

            default:

              sprintf(tmpLine,"server rejected job obit - %d",
                preq->rq_reply.brp_code);

              break;
            }  /* END switch(preq->rq_reply.brp_code) */

          LOG_EVENT(
            PBSEVENT_ERROR,
            PBS_EVENTCLASS_JOB,
            pjob->ji_qs.ji_jobid,
            tmpLine);
          }  /* END BLOCK */

          mom_deljob(pjob);

          break;
        }  /* END switch (preq->rq_reply.brp_code) */

      break;
      }    /* END if (...) */

    pjob = nxjob;
    }  /* END while (pjob != NULL) */

  if (pjob == NULL) 
    {
    LOG_EVENT(
      PBSEVENT_ERROR,
      PBS_EVENTCLASS_REQUEST, 
      "obit reply",
      "Job not found for obit reply");
    }

  free_br(preq);

  shutdown(sock,2);

  close_conn(sock);

  return;
  }  /* END obit_reply() */





/*
 * init_abort_jobs - on mom initialization, recover all running jobs.
 *
 *	Called on initialization
 *	   If the -p option was given (recover = 2), Mom will allow the jobs
 *	   to continue to run.   She depends on detecting when they terminate
 *	   via the slow poll method rather than SIGCHLD.
 *
 *	   If the -r option was given (recover = 1), MOM is recovering on a
 *  	   running system and the session id of the jobs should be valid;
 *	   the jobs are killed.
 *
 *	   If -r was not given (recover = 0), it is assumed that the whole 
 *	   system, not just MOM, is comming up, the session ids are not valid;
 *	   so no attempt is made to kill the job processes.  But the jobs are
 *	   terminated and requeued.
 */

void init_abort_jobs(

  int recover)  /* I (boolean) */

  {
  char          *id = "init_abort_jobs";

  DIR		*dir;
  int            i;
  int            j;
  int            sisters, rc;
  struct dirent	*pdirent;
  job		*pj;
  char		*job_suffix = JOB_FILE_SUFFIX;
  int            job_suf_len = strlen(job_suffix);
  char		*psuffix;
#if	MOM_CHECKPOINT == 1
  char           path[MAXPATHLEN + 1];
  char           oldp[MAXPATHLEN + 1];
  struct stat    statbuf;
  extern char   *path_checkpoint;
#endif

  if (LOGLEVEL >= 6)
    {
    sprintf(log_buffer,"%s: recover=%d",
      id,
      recover);

    log_record(
      PBSEVENT_ERROR,
      PBS_EVENTCLASS_SERVER,
      msg_daemonname,
      log_buffer);
    }

  dir = opendir(path_jobs);

  if (dir == NULL) 
    {
    sprintf(log_buffer,"cannot open job directory '%s'",
      path_jobs);

    log_record(
      PBSEVENT_ERROR, 
      PBS_EVENTCLASS_SERVER, 
      msg_daemonname, 
      log_buffer);

    exit(1);
    }

  while ((pdirent = readdir(dir)) != NULL) 
    {
    if ((i = strlen(pdirent->d_name)) <= job_suf_len)
      continue;

    psuffix = pdirent->d_name + i - job_suf_len;

    if (strcmp(psuffix,job_suffix))
      continue;

    pj = job_recov(pdirent->d_name);

    if (pj == NULL)
      {
      sprintf(log_buffer,"%s: NULL job pointer",
        id);

      log_record(
        PBSEVENT_ERROR,
        PBS_EVENTCLASS_SERVER,
        msg_daemonname,
        log_buffer);

      continue;
      }

    /* PW:  mpiexec patch - set the globid so mom does not coredump in response to tm_spawn */

    set_globid(pj,NULL);

    append_link(&svr_alljobs,&pj->ji_alljobs,pj);

    job_nodes(pj);

    rc = task_recov(pj);

    if (LOGLEVEL >= 2)
      {
      sprintf(log_buffer,"task recovery %s for job %s, rc=%d",
        (rc == 0) ? "succeeded" : "failed",
        pj->ji_qs.ji_jobid,
        rc);

      log_record(
        PBSEVENT_DEBUG,
        PBS_EVENTCLASS_JOB,
        id,
        log_buffer);
      }

#if MOM_CHECKPOINT == 1
    /*
    ** Check to see if a checkpoint.old dir exists.
    ** If so, remove the regular checkpoint dir
    ** and rename the old to the regular name.
    */

    strcpy(path,path_checkpoint);
    strcat(path,pj->ji_qs.ji_fileprefix);
    strcat(path,JOB_CKPT_SUFFIX);
    strcpy(oldp,path);
    strcat(oldp,".old");

    if (stat(oldp,&statbuf) == 0) 
      {
      remtree(path);

      if (rename(oldp,path) == -1)
        remtree(oldp);
      }
#endif  /* END MOM_CHECKPOINT == 1 */

    /*
     * make sure we trust connections from sisters in case we get an
     * IM request before we get the real addr list from server.
     * Note: this only works after the job_nodes() call above.
     */

    for (j = 0;j < pj->ji_numnodes;j++)
      {
      if (LOGLEVEL >= 6)
        {
        sprintf(log_buffer,"%s: adding client %s",
          id,
          pj->ji_hosts[j].hn_host);

        log_record(
          PBSEVENT_ERROR,
          PBS_EVENTCLASS_SERVER,
          msg_daemonname,
          log_buffer);
        }

      addclient(pj->ji_hosts[j].hn_host);
      }  /* END for (j) */

    if (LOGLEVEL >= 4)
      {
      sprintf(log_buffer,"successfully recovered job %s",
        pj->ji_qs.ji_jobid);

      log_record(
        PBSEVENT_DEBUG,
        PBS_EVENTCLASS_JOB,
        id,
        log_buffer);
      }

    if ((recover != 2) &&
       ((pj->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING) ||
        (pj->ji_qs.ji_substate == JOB_SUBSTATE_PRERUN ) ||
        (pj->ji_qs.ji_substate == JOB_SUBSTATE_SUSPEND) ||
        (pj->ji_qs.ji_substate == JOB_SUBSTATE_EXITED) ||
        (pj->ji_qs.ji_substate == JOB_SUBSTATE_EXITING)))  
      {
      if (LOGLEVEL >= 2)
        {
        sprintf(log_buffer,"job %s recovered in active state %s (full recover not enabled)",
          pj->ji_qs.ji_jobid,
          PJobSubState[pj->ji_qs.ji_substate]);

        log_record(
          PBSEVENT_DEBUG,
          PBS_EVENTCLASS_JOB,
          id,
          log_buffer);
        }

      if (recover != 0)
        { 
        if (LOGLEVEL >= 2)
          {
          sprintf(log_buffer,"recover is non-zero, killing job %s",
            pj->ji_qs.ji_jobid);

          log_record(
            PBSEVENT_DEBUG,
            PBS_EVENTCLASS_JOB,
            id,
            log_buffer);
          }

        kill_job(pj,SIGKILL);
        }

      /*
      ** Check to see if I am Mother Superior.  The
      ** JOB_SVFLG_HERE flag is overloaded for MOM
      ** for this purpose.
      ** If I'm an ordinary sister, just throw the job
      ** away.  If I am MS, send a KILL_JOB request to
      ** any sisters that happen to still be alive.
      */

      if ((pj->ji_qs.ji_svrflags & JOB_SVFLG_HERE) == 0) 
        {
        if (LOGLEVEL >= 2)
          {
          sprintf(log_buffer,"local host is not mother-superior, deleting job %s",
            pj->ji_qs.ji_jobid);

          log_record(
            PBSEVENT_DEBUG,
            PBS_EVENTCLASS_JOB,
            id,
            log_buffer); 
          }

        mom_deljob(pj);

        continue;
        }

      if (LOGLEVEL >= 2)
        {
        sprintf(log_buffer,"setting job state to exiting for job %s in state %s",
          pj->ji_qs.ji_jobid,
          PJobSubState[pj->ji_qs.ji_substate]);

        log_record(
          PBSEVENT_DEBUG,
          PBS_EVENTCLASS_JOB,
          id,
          log_buffer);
        }

      /* set exit status to:
       *   JOB_EXEC_INITABT - init abort and no chkpt
       *   JOB_EXEC_INITRST - init and chkpt, no mig
       *   JOB_EXEC_INITRMG - init and chkpt, migrate
       * to indicate recovery abort
       */

      if (pj->ji_qs.ji_svrflags & (JOB_SVFLG_CHKPT|JOB_SVFLG_ChkptMig)) 
        {
#if PBS_CHKPT_MIGRATE
      
        pj->ji_qs.ji_un.ji_momt.ji_exitstat = JOB_EXEC_INITRMG;
#else
        pj->ji_qs.ji_un.ji_momt.ji_exitstat = JOB_EXEC_INITRST;
#endif
        } 
      else 
        {
        pj->ji_qs.ji_un.ji_momt.ji_exitstat = JOB_EXEC_INITABT;
        }

      sisters = pj->ji_numnodes - 1;

      /*
      ** A sisterhood exists... send a KILL request.
      */

      if (sisters > 0) 
        {
        DBPRT(("init_abort_jobs: Sending to sisters\n"))

        pj->ji_resources = (noderes *)calloc(sisters,sizeof(noderes));

        send_sisters(pj,IM_KILL_JOB);

        continue;
        }

      pj->ji_qs.ji_substate = JOB_SUBSTATE_EXITING;

      job_save(pj,SAVEJOB_QUICK);

      exiting_tasks = 1;
      }  /* END if ((recover != 2) && ...) */
    else if (recover == 2)
      {
      /*
       * add: 8/11/03 David.Singleton@anu.edu.au 
       * 
       * Lots of job structure components need to be
       * initialized if we are leaving this job
       * running,  this is just a few.
       */

      if (LOGLEVEL >= 2)
        {
        sprintf(log_buffer,"attempting to recover job %s in state %s",
          pj->ji_qs.ji_jobid,
          PJobSubState[pj->ji_qs.ji_substate]);

        log_record(
          PBSEVENT_DEBUG,
          PBS_EVENTCLASS_JOB,
          id,
          log_buffer);
        }

      sisters = pj->ji_numnodes - 1;

      if (sisters > 0)
        pj->ji_resources = (noderes *)calloc(sisters,sizeof(noderes));

      if (mom_do_poll(pj))
        append_link(&mom_polljobs,&pj->ji_jobque,pj);

      if (pj->ji_grpcache == NULL)
        {
        DBPRT(("init_abort_jobs: setting grpcache for job %s\n",
          pj->ji_qs.ji_jobid));
        check_pwd(pj);
        }
      }
    }    /* while ((pdirent = readdir(dir)) != NULL) */

  closedir(dir);

#if defined(PENABLE_LINUX26_CPUSETS)
  /* Create the top level torque cpuset if it doesn't already exist. */
  initialize_root_cpuset();
#endif

  return;
  }  /* END init_abort_jobs() */




/* 
 * mom_deljob - delete the job entry, MOM no longer knows about the job
 */

void mom_deljob(

  job *pjob)  /* I (modified) */

  {

#ifdef _CRAY
  /* remove any temporary directories */

  rmtmpdir(pjob->ji_qs.ji_jobid);
#endif	/* _CRAY */

  if (LOGLEVEL >= 3)
    {
    sprintf(log_buffer,"deleting job %s in state %s",
      pjob->ji_qs.ji_jobid,
      PJobSubState[pjob->ji_qs.ji_substate]);

    log_record(
      PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);
    }

  job_purge(pjob);

  return;
  }  /* END mom_deljob() */

/* END catch_child() */

