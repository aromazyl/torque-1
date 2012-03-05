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
 * req_quejob.c
 *
 * Functions relating to the Queue Job Batch Request sequence, including
 * Queue Job, Job Script, Ready to Commit, and Commit.
 *
 * Included functions are:
 * req_quejob()
 * req_jobcredential()
 * req_jobscript()
 * req_rdycommit()
 * req_commit()
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include "libpbs.h"
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "resource.h"
#include "server.h"
#include "credential.h"
#include "batch_request.h"
#include "pbs_job.h"
#include "queue.h"
#include "net_connect.h"
#include "pbs_error.h"
#include "log.h"
#include "svrfunc.h"
#include "utils.h"

#include <pwd.h>
#include "mom_func.h"
#include "pbs_nodes.h"

/* External Functions Called: */

extern int  reply_jid(char *);
extern void start_exec(job *);
extern int  svr_authorize_jobreq(struct batch_request *, job *);
extern int  svr_chkque(job *, pbs_queue *, char *, int, char *);
extern int  job_route(job *);
extern void check_state(int);
extern void mom_server_all_update_stat();
#ifdef NVIDIA_GPUS
extern void mom_server_all_update_gpustat(void);
extern int  use_nvidia_gpu;
#endif  /* NVIDIA_GPUS */

/* Global Data Items: */

extern int PBSNodeCheckProlog;
extern int internal_state;

extern const char *PJobSubState[];

/* sync w/enum job_file TJobFileType[]) */

const char *TJobFileType[] =
  {
  "jobscript",
  "stdin",
  "stdout",
  "stderr",
  "ckpt",
  NULL
  };

extern int  resc_access_perm;
extern tlist_head svr_alljobs;
extern tlist_head svr_newjobs;
extern attribute_def job_attr_def[];
extern char *path_jobs;
extern char *pbs_o_host;
extern char *msg_script_open;
extern char *msg_script_write;
extern char *msg_init_abt;

extern char *msg_jobnew;
extern time_t time_now;
extern int    LOGLEVEL;

extern  char *msg_daemonname;

extern int decode_arst_merge(struct attribute *,char *,char *,char *);


/* Private Functions in this file */

static job *locate_new_job(int, char *);

#ifdef PNOT
static int user_account_verify(char *, char *);
static char *user_account_default(char *);
static int user_account_read_user(char *);
#endif /* PNOT */





/*
 * req_quejob - Queue Job Batch Request processing routine
 *  NOTE:  calls svr_chkque() to validate queue access
 *
 */

void req_quejob(

  struct batch_request *preq) /* ptr to the decoded request   */

  {
  char  *id = "req_quejob";

  char   basename[PBS_JOBBASE + 1];
  int    created_here = 0;
  int    index;
  char  *jid;
  attribute_def *pdef;
  job   *pj;
  svrattrl *psatl;
  int    rc;
  int    sock = preq->rq_conn;

  int    IsCheckpoint = 0;

  /* set basic (user) level access permission */

  resc_access_perm = ATR_DFLAG_USWR | ATR_DFLAG_Creat;

  if (PBSNodeCheckProlog)
    {
    check_state(1);

    mom_server_all_update_stat();

    if (internal_state & INUSE_DOWN)
      {
      req_reject(PBSE_MOMREJECT,0,preq,NULL,NULL);

      return;
      }
    }

  if (preq->rq_fromsvr)
    {
    /* from another server - accept the extra attributes */

    resc_access_perm |= ATR_DFLAG_MGWR | ATR_DFLAG_SvWR | ATR_DFLAG_MOM;

    jid = preq->rq_ind.rq_queuejob.rq_jid;
    }
  else
    {
    /* request must be from server */

    log_err(errno, id, "request not from server");

    req_reject(PBSE_IVALREQ, 0, preq, NULL, "request not received from server");

    return;
    }

  /* does job already exist, check both old and new jobs */

  if ((pj = find_job(jid)) == NULL)
    {
    pj = (job *)GET_NEXT(svr_newjobs);

    while (pj != NULL)
      {
      if (!strcmp(pj->ji_qs.ji_jobid, jid))
        break;

      pj = (job *)GET_NEXT(pj->ji_alljobs);
      }
    }

  /*
   * New job ...
   *
   * for MOM - rather than make up a hashname, we use the name sent
   * to us by the server as an attribute.
   */

  psatl = (svrattrl *)GET_NEXT(preq->rq_ind.rq_queuejob.rq_attr);

  while (psatl != NULL)
    {
    if (!strcmp(psatl->al_name,ATTR_hashname))
      {
      strcpy(basename,psatl->al_value);

      break;
      }

    psatl = (svrattrl *)GET_NEXT(psatl->al_link);
    }

  if (pj != NULL)
    {
    /* newly queued job already exists */

    if (pj->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING)
      {
      /* FAILURE - job exists and is running */

      log_err(errno,id,"cannot queue new job, job exists and is running");

      req_reject(PBSE_JOBEXIST,0,preq,NULL,"job is running");

      return;
      }

    /* if checkpointed, then keep old and skip rest of process */

    if (pj->ji_qs.ji_svrflags & JOB_SVFLG_CHECKPOINT_FILE)
      {
      IsCheckpoint = 1;
      }  /* END if (pj->ji_qs.ji_svrflags & JOB_SVFLG_CHECKPOINT_FILE) */
    else
      {
      /* unlink job from svr_alljobs since it will be placed on newjobs */

      delete_link(&pj->ji_alljobs);
      }
    }  /* END if (pj != NULL) */
  else
    {
    /* if not already here, allocate job struct */

    if ((pj = job_alloc()) == NULL)
      {
      /* FAILURE */

      req_reject(PBSE_SYSTEM, 0, preq, NULL, "cannot allocate new job structure");

      return;
      }
    }    /* END else (pj != NULL) */

  if (IsCheckpoint == 0)
    {
    strcpy(pj->ji_qs.ji_jobid,jid);

    strcpy(pj->ji_qs.ji_fileprefix,basename);

    pj->ji_modified       = 1;

    pj->ji_qs.ji_svrflags = created_here;

    pj->ji_qs.ji_un_type  = JOB_UNION_TYPE_NEW;
    }

  /* decode attributes from request into job structure */

  psatl = (svrattrl *)GET_NEXT(preq->rq_ind.rq_queuejob.rq_attr);

  while (psatl != NULL)
    {
    if (IsCheckpoint == 1)
      {
      if (strcmp(psatl->al_name,ATTR_checkpoint_name) &&
          strcmp(psatl->al_name,ATTR_v))
        {
        psatl = (svrattrl *)GET_NEXT(psatl->al_link);

        continue;
        }
      }

    /* identify the attribute by name */

    index = find_attr(job_attr_def,psatl->al_name,JOB_ATR_LAST);

    if (index < 0)
      {
      /* FAILURE */

      /* didn`t recognize the name */

      job_purge(pj);   /* CRI - 12/20/2004 */

      reply_badattr(PBSE_NOATTR,1,psatl,preq);

      return;
      }

    pdef = &job_attr_def[index];

    /* Is attribute not writeable by manager or by a server? */

    if ((pdef->at_flags & resc_access_perm) == 0)
      {
      /* FAILURE */

      job_purge(pj);

      reply_badattr(PBSE_ATTRRO,1,psatl,preq);

      return;
      }

    /* decode attribute */

    if (!strcmp(psatl->al_name,ATTR_v))
      {
      rc = decode_arst_merge(
             &pj->ji_wattr[index],
             psatl->al_name,
             psatl->al_resc,
             psatl->al_value);
      }
    else
      {
      rc = pdef->at_decode(
             &pj->ji_wattr[index],
             psatl->al_name,
             psatl->al_resc,
             psatl->al_value);
      }

    if (rc != 0)
      {
      /* FAILURE */

      /* all errors are fatal for MOM */

      job_purge(pj);

      reply_badattr(rc,1,psatl,preq);

      return;
      }

    if (psatl->al_op == DFLT)
      {
      if (psatl->al_resc)
        {
        resource     *presc;
        resource_def *prdef;

        prdef = find_resc_def(svr_resc_def,psatl->al_resc,svr_resc_size);

        if (prdef == NULL)
          {
          job_purge(pj);

          reply_badattr(rc,1,psatl, preq);

          return;
          }

        presc = find_resc_entry(&pj->ji_wattr[index],prdef);

        if (presc != NULL)
          presc->rs_value.at_flags |= ATR_VFLAG_DEFLT;
        }
      else
        {
        pj->ji_wattr[index].at_flags |= ATR_VFLAG_DEFLT;
        }
      }    /* END if (psatl->al_op == DFLT) */

    psatl = (svrattrl *)GET_NEXT(psatl->al_link);
    }      /* END while (psatl != NULL) */

  if (IsCheckpoint == 1)
    {
    pj->ji_qs.ji_substate = JOB_SUBSTATE_TRANSIN;

    if (reply_jobid(preq,pj->ji_qs.ji_jobid,BATCH_REPLY_CHOICE_Queue) == 0)
      {
      delete_link(&pj->ji_alljobs);

      append_link(&svr_newjobs,&pj->ji_alljobs,pj);

      pj->ji_qs.ji_un_type = JOB_UNION_TYPE_NEW;
      pj->ji_qs.ji_un.ji_newt.ji_fromsock = sock;
      pj->ji_qs.ji_un.ji_newt.ji_fromaddr = get_connectaddr(sock);
      pj->ji_qs.ji_un.ji_newt.ji_scriptsz = 0;

      /* Per Eric R., req_mvjobfile was giving error in open_std_file,
         showed up as fishy error message */

      if (pj->ji_grpcache != NULL)
        {
        free(pj->ji_grpcache);
        pj->ji_grpcache = NULL;
        }
      }
    else
      {
      close_conn(sock);
      }

    /* SUCCESS */

    return;
    }

  /* set remaining job structure elements */

  pj->ji_qs.ji_state =    JOB_STATE_TRANSIT;

  pj->ji_qs.ji_substate = JOB_SUBSTATE_TRANSIN;

  pj->ji_wattr[(int)JOB_ATR_mtime].at_val.at_long = (long)time_now;

  pj->ji_wattr[(int)JOB_ATR_mtime].at_flags |= ATR_VFLAG_SET;

  pj->ji_qs.ji_un_type = JOB_UNION_TYPE_NEW;

  pj->ji_qs.ji_un.ji_newt.ji_fromsock = sock;

  pj->ji_qs.ji_un.ji_newt.ji_fromaddr = get_connectaddr(sock);

  pj->ji_qs.ji_un.ji_newt.ji_scriptsz = 0;

  /* acknowledge the request with the job id */

  if (reply_jobid(preq, pj->ji_qs.ji_jobid, BATCH_REPLY_CHOICE_Queue) != 0)
    {
    /* reply failed, purge the job and close the connection */

    close_conn(sock);

    job_purge(pj);

    return;
    }

  /* link job into server's new jobs list request  */

  append_link(&svr_newjobs, &pj->ji_alljobs, pj);

  return;
  }  /* END req_quejob() */





/*
 * req_jobcredential - receive a set of credentials to be used by the job
 *
 * THIS IS JUST A PLACE HOLDER FOR NOW
 * It does nothing but acknowledge the request
 */

void req_jobcredential(

  struct batch_request *preq)  /* ptr to the decoded request   */

  {
  job *pj;

  pj = locate_new_job(preq->rq_conn, NULL);

  if (pj == NULL)
    {
    req_reject(PBSE_IVALREQ, 0, preq, NULL, NULL);

    return;
    }

  reply_ack(preq);

  return;
  }  /* END req_jobcredential() */




/*
 * req_jobscript - receive job script section
 *
 * Each section is appended to the file
 */

void req_jobscript(

  struct batch_request *preq) /* ptr to the decoded request*/

  {
  char *id = "req_jobscript";

  int  fds;
  char  namebuf[MAXPATHLEN];
  job *pj;
  int  filemode = 0700;
  extern char mom_host[];

  errno = 0;

  pj = locate_new_job(preq->rq_conn, preq->rq_ind.rq_jobfile.rq_jobid);

  if (pj == NULL)
    {
    log_err(errno, id, "cannot locate new job");

    req_reject(PBSE_IVALREQ, 0, preq, NULL, NULL);

    return;
    }

  /* what is the difference between JOB_SUBSTATE_TRANSIN and TRANSICM? */

  if (pj->ji_qs.ji_substate != JOB_SUBSTATE_TRANSIN)
    {
    if (errno == 0)
      {
      sprintf(log_buffer, "job %s in unexpected state '%s'",
              pj->ji_qs.ji_jobid,
              PJobSubState[pj->ji_qs.ji_substate]);
      }
    else
      {
      sprintf(log_buffer, "job %s in unexpected state '%s' (errno=%d - %s)",
              pj->ji_qs.ji_jobid,
              PJobSubState[pj->ji_qs.ji_substate],
              errno,
              strerror(errno));
      }

    log_err(errno, id, log_buffer);

    req_reject(PBSE_IVALREQ, 0, preq, mom_host, log_buffer);

    return;
    }

  /* mom - if job has been checkpointed, discard script,already have it */

  if (pj->ji_qs.ji_svrflags & JOB_SVFLG_CHECKPOINT_FILE)
    {
    /* SUCCESS - do nothing, ignore script */

    reply_ack(preq);

    return;
    }

  strcpy(namebuf, path_jobs);

  strcat(namebuf, pj->ji_qs.ji_fileprefix);
  strcat(namebuf, JOB_SCRIPT_SUFFIX);

  if (pj->ji_qs.ji_un.ji_newt.ji_scriptsz == 0)
    {
    /* NOTE:  fail is job script already exists */

    fds = open(namebuf, O_WRONLY | O_CREAT | O_EXCL | O_Sync, filemode);
    }
  else
    {
    fds = open(namebuf, O_WRONLY | O_APPEND | O_Sync, filemode);
    }

  if (fds < 0)
    {
    char tmpLine[1024];

    snprintf(tmpLine, sizeof(tmpLine), "cannot open '%s' errno=%d - %s",
             namebuf,
             errno,
             strerror(errno));

    /* FAILURE */

    /* NOTE: log_err may modify errno */

    log_err(errno, id, msg_script_open);

    req_reject(PBSE_INTERNAL, 0, preq, mom_host, tmpLine);

    return;
    }

  if (write(
        fds,
        preq->rq_ind.rq_jobfile.rq_data,
        (unsigned)preq->rq_ind.rq_jobfile.rq_size) != preq->rq_ind.rq_jobfile.rq_size)
    {
    /* FAILURE */

    log_err(errno, id, msg_script_write);

    req_reject(PBSE_INTERNAL, 0, preq, mom_host, "cannot write job command file");

    close(fds);

    return;
    }

  close(fds);

  pj->ji_qs.ji_un.ji_newt.ji_scriptsz += preq->rq_ind.rq_jobfile.rq_size;

  /* job has a script file */

  pj->ji_qs.ji_svrflags =
    (pj->ji_qs.ji_svrflags & ~JOB_SVFLG_CHECKPOINT_FILE) | JOB_SVFLG_SCRIPT;

  /* SUCCESS */

  reply_ack(preq);

  return;
  }  /* END req_jobscript() */




/*
 * req_mvjobfile - move the specifled job standard files
 * This is MOM's version.  The files are owned by the user and placed
 * in either the spool area or the user's home directory depending
 * on the compile option, see std_file_name().
 */

void req_mvjobfile(

  struct batch_request *preq)  /* I */

  {
  int          fds;
  enum job_file  jft;
  int          oflag;
  job         *pj;

  struct passwd *pwd;

  jft = (enum job_file)preq->rq_ind.rq_jobfile.rq_type;

  if (preq->rq_ind.rq_jobfile.rq_sequence == 0)
    oflag = O_CREAT | O_WRONLY | O_TRUNC;
  else
    oflag = O_CREAT | O_WRONLY | O_APPEND;

  pj = locate_new_job(preq->rq_conn, NULL);

  if (pj == NULL)
    pj = find_job(preq->rq_ind.rq_jobfile.rq_jobid);

  if (pj == NULL)
    {
    snprintf(log_buffer, 1024, "cannot find job %s for move of %s file",
             preq->rq_ind.rq_jobfile.rq_jobid,
             TJobFileType[jft]);

    log_err(-1, "req_mvjobfile", log_buffer);

    req_reject(PBSE_UNKJOBID, 0, preq, NULL, NULL);

    return;
    }

  if ((pj->ji_grpcache == NULL) && (check_pwd(pj) == NULL))
    {
    req_reject(PBSE_UNKJOBID, 0, preq, NULL, NULL);

    return;
    }

  if ((pwd = getpwnam_ext(pj->ji_wattr[(int)JOB_ATR_euser].at_val.at_str)) == NULL)
    {
    /* FAILURE */
    req_reject(PBSE_MOMREJECT, 0, preq, NULL, "password lookup failed");

    return;
    }

  if ((fds = open_std_file(pj, jft, oflag, pwd->pw_gid)) < 0)
    {
    int keeping = 1;
    char *path = std_file_name(pj, jft, &keeping);

    snprintf(log_buffer,sizeof(log_buffer),
      "Cannot create file %s",
      path);

    req_reject(PBSE_SYSTEM, 0, preq, NULL, log_buffer);

    return;
    }

  if (write(
        fds,
        preq->rq_ind.rq_jobfile.rq_data,
        preq->rq_ind.rq_jobfile.rq_size) != preq->rq_ind.rq_jobfile.rq_size)
    {
    req_reject(PBSE_SYSTEM, 0, preq, NULL, "cannot create file");
    }
  else
    {
    reply_ack(preq);
    }

  close(fds);

  if (LOGLEVEL >= 6)
    {
    sprintf(log_buffer, "successfully moved %s file for job '%s'",
            TJobFileType[jft],
            preq->rq_ind.rq_jobfile.rq_jobid);

    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      (pj != NULL) ? pj->ji_qs.ji_jobid : "NULL",
      log_buffer);
    }

  return;
  }  /* END req_mvjobfile() */





/*
 * req_rdytocommit - Ready To Commit Batch Request
 *
 * Set substate to JOB_SUBSTATE_TRANSICM and
 * record job to permanent storage, i.e. written to the job save file
 *      (called by both pbs_server and pbs_mom)
 */

void req_rdytocommit(

  struct batch_request *preq)  /* I */

  {
  job *pj;
  int  sock = preq->rq_conn;

  int  OrigState;
  int  OrigSState;
  char OrigSChar;
  long OrigFlags;

  pj = locate_new_job(sock, preq->rq_ind.rq_rdytocommit);

  if (LOGLEVEL >= 6)
    {
    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      (pj != NULL) ? pj->ji_qs.ji_jobid : "NULL",
      "ready to commit job");
    }

  if (pj == NULL)
    {
    log_err(errno, "req_rdytocommit", "unknown job id");

    req_reject(PBSE_UNKJOBID, 0, preq, NULL, NULL);

    /* FAILURE */

    return;
    }

  if (pj->ji_qs.ji_substate != JOB_SUBSTATE_TRANSIN)
    {
    log_err(errno, "req_rdytocommit", "cannot commit job in unexpected state");

    req_reject(PBSE_IVALREQ, 0, preq, NULL, NULL);

    /* FAILURE */

    return;
    }

  OrigState  = pj->ji_qs.ji_state;

  OrigSState = pj->ji_qs.ji_substate;
  OrigSChar  = pj->ji_wattr[(int)JOB_ATR_state].at_val.at_char;
  OrigFlags  = pj->ji_wattr[(int)JOB_ATR_state].at_flags;

  pj->ji_qs.ji_state    = JOB_STATE_TRANSIT;
  pj->ji_qs.ji_substate = JOB_SUBSTATE_TRANSICM;
  pj->ji_wattr[(int)JOB_ATR_state].at_val.at_char = 'T';
  pj->ji_wattr[(int)JOB_ATR_state].at_flags |= ATR_VFLAG_SET;

  if (job_save(pj, SAVEJOB_NEW) == -1)
    {
    char tmpLine[1024];

    sprintf(tmpLine, "cannot save job - errno=%d - %s",
            errno,
            strerror(errno));

    log_err(errno, "req_rdytocommit", tmpLine);

    /* commit failed, backoff state changes */

    pj->ji_qs.ji_state    = OrigState;
    pj->ji_qs.ji_substate = OrigSState;
    pj->ji_wattr[(int)JOB_ATR_state].at_val.at_char = OrigSChar;
    pj->ji_wattr[(int)JOB_ATR_state].at_flags = OrigFlags;

    req_reject(PBSE_SYSTEM, 0, preq, NULL, tmpLine);

    /* FAILURE */

    return;
    }

  /* acknowledge the request with the job id */

  if (reply_jobid(preq, pj->ji_qs.ji_jobid, BATCH_REPLY_CHOICE_RdytoCom) != 0)
    {
    /* reply failed, purge the job and close the connection */

    sprintf(log_buffer, "cannot report jobid - errno=%d - %s",
            errno,
            strerror(errno));

    log_err(errno, "req_rdytocommit", log_buffer);

    close_conn(sock);

    job_purge(pj);

    /* FAILURE */

    return;
    }

  if (LOGLEVEL >= 6)
    {
    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      (pj != NULL) ? pj->ji_qs.ji_jobid : "NULL",
      "ready to commit job completed");
    }

  return;
  }  /* END req_rdytocommit() */




/*
 * req_commit - commit ownership of job
 *
 * Set state of job to JOB_STATE_QUEUED (or Held or Waiting) and
 * enqueue the job into its destination queue.
 */

void req_commit(

  struct batch_request *preq)  /* I */

  {
  job   *pj;

  pj = locate_new_job(preq->rq_conn, preq->rq_ind.rq_commit);

  if (LOGLEVEL >= 6)
    {
    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      (pj != NULL) ? pj->ji_qs.ji_jobid : "NULL",
      "committing job");
    }

  if (pj == NULL)
    {
    req_reject(PBSE_UNKJOBID, 0, preq, NULL, NULL);

    return;
    }

  if (pj->ji_qs.ji_substate != JOB_SUBSTATE_TRANSICM)
    {
    log_err(errno, "req_commit", "cannot commit job in unexpected state");

    req_reject(PBSE_IVALREQ, 0, preq, NULL, NULL);

    return;
    }

  /* move job from new job list to "all" job list, set to running state */

  delete_link(&pj->ji_alljobs);

  append_link(&svr_alljobs, &pj->ji_alljobs, pj);

  /*
  ** Set JOB_SVFLG_HERE to indicate that this is Mother Superior.
  */

  pj->ji_qs.ji_svrflags |= JOB_SVFLG_HERE;

  pj->ji_qs.ji_state = JOB_STATE_RUNNING;

  pj->ji_qs.ji_substate = JOB_SUBSTATE_PRERUN;

  pj->ji_qs.ji_un_type = JOB_UNION_TYPE_MOM;

  pj->ji_qs.ji_un.ji_momt.ji_svraddr = get_connectaddr(preq->rq_conn);

  pj->ji_qs.ji_un.ji_momt.ji_exitstat = 0;

  /* For MOM - start up the job (blocks) */

  if (LOGLEVEL >= 6)
    {
    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      (pj != NULL) ? pj->ji_qs.ji_jobid : "NULL",
      "starting job execution");
    }

  start_exec(pj);

  if (LOGLEVEL >= 6)
    {
    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      (pj != NULL) ? pj->ji_qs.ji_jobid : "NULL",
      "job execution started");
    }

  /* if start request fails, reply with failure string */

  if (pj->ji_qs.ji_substate == JOB_SUBSTATE_EXITING)
    {
    char tmpLine[1024];

    if ((pj->ji_hosts != NULL) &&
        (pj->ji_nodekill >= 0) &&
        (pj->ji_hosts[pj->ji_nodekill].hn_host != NULL))
      {
      sprintf(tmpLine, "start failed on node %s",
              pj->ji_hosts[pj->ji_nodekill].hn_host);
      }
    else
      {
      sprintf(tmpLine, "start failed on unknown node");
      }

    if (LOGLEVEL >= 6)
      {
      log_record(
        PBSEVENT_JOB,
        PBS_EVENTCLASS_JOB,
        (pj != NULL) ? pj->ji_qs.ji_jobid : "NULL",
        tmpLine);
      }

    reply_text(preq, 0, tmpLine);
    }
  else
    {
    reply_jobid(preq, pj->ji_qs.ji_jobid, BATCH_REPLY_CHOICE_Commit);
    }

  job_save(pj, SAVEJOB_FULL);

#ifdef NVIDIA_GPUS
  /*
   * Does this job have a gpuid assigned?
   * if so, then update gpu status
   */
  if ((use_nvidia_gpu) && 
      ((pj->ji_wattr[JOB_ATR_exec_gpus].at_flags & ATR_VFLAG_SET) != 0) &&
      (pj->ji_wattr[JOB_ATR_exec_gpus].at_val.at_str != NULL))
    {
    mom_server_all_update_gpustat();
    }
#endif  /* NVIDIA_GPUS */


  /* NOTE: we used to flag JOB_ATR_errpath, JOB_ATR_outpath,
   * JOB_ATR_session_id, and JOB_ATR_altid as modified at this point to make sure
   * pbs_server got these attr values.  This worked fine before TORQUE modified
   * job launched into an async process.  At 2.0.0p6, a new attribute "SEND" flag
   * was added to handle this process. */

  return;
  }  /* END req_commit() */




/*
 * locate_new_job - locate a "new" job which has been set up req_quejob on
 * the servers new job list.
 *
 * This function is used by the sub-requests which make up the global
 * "Queue Job Request" to locate the job structure.
 *
 * If the jobid is specified (will be for rdytocommit and commit, but not
 * for script), we search for a matching jobid.
 *
 * The job must (also) match the socket specified and the host associated
 * with the socket unless ji_fromsock == -1, then its a recovery situation.
 */
/* FIXME: why bother checking for matching sock if a jobid is supplied?  Seems
 * to me that a reconnect immediately invalidates fromsock.
 */

static job *locate_new_job(

  int   sock,   /* I */
  char *jobid)  /* I (optional) */

  {
  job *pj;

  pj = (job *)GET_NEXT(svr_newjobs);

  while (pj != NULL)
    {
    if ((pj->ji_qs.ji_un.ji_newt.ji_fromsock == -1) ||
        ((pj->ji_qs.ji_un.ji_newt.ji_fromsock == sock) &&
         (pj->ji_qs.ji_un.ji_newt.ji_fromaddr == get_connectaddr(sock))))
      {
      if ((jobid != NULL) && (*jobid != '\0'))
        {
        if (!strncmp(pj->ji_qs.ji_jobid, jobid, PBS_MAXSVRJOBID))
          {
          /* requested job located */

          break;
          }
        }
      else if (pj->ji_qs.ji_un.ji_newt.ji_fromsock == -1)
        {
        /* empty job slot located */

        break;
        }
      else
        {
        /* matching job slot located */

        break;
        }
      }    /* END if ((pj->ji_qs.ji_un.ji_newt.ji_fromsock == -1) || ...) */

    pj = (job *)GET_NEXT(pj->ji_alljobs);
    }  /* END while(pj != NULL) */

  /* return job slot located (NULL on FAILURE) */

  return(pj);
  }  /* END locate_new_job() */

/* END req_quejob.c() */

