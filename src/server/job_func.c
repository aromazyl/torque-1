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
 * Functions which provide basic operation on the job structure
 *
 * Included public functions are:
 *
 *   job_abt	  abort (remove from server) a job
 *   job_alloc    allocate job struct and initialize defaults
 *   job_free	  free space allocated to the job structure and its
 *		  childern structures.
 *   job_purge	  purge job from server
 *
 *   job_clone    clones a job (for use with job_arrays)
 *   job_clone_wt work task for cloning a job
 *
 * Include private function:
 *   job_init_wattr() initialize job working attribute array to "unspecified"
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#ifndef SIGKILL
#include <signal.h>
#endif
#if __STDC__ != 1
#include <memory.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "pbs_ifl.h"
#include "list_link.h"
#include "work_task.h"
#include "attribute.h"
#include "resource.h"
#include "server_limits.h"
#include "server.h"
#include "queue.h"
#include "pbs_job.h"
#include "log.h"
#include "pbs_error.h"
#include "svrfunc.h"
#include "acct.h"
#include "net_connect.h"
#include "portability.h"
#include "array.h"


#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

int conn_qsub A_((char *,long,char *));
void job_purge A_((job *));

/* External functions */


/* Local Private Functions */

static void job_init_wattr A_((job *));

/* Global Data items */

extern struct server   server;
extern int queue_rank;
extern tlist_head svr_jobarrays;
extern char *path_arrays;
extern char *msg_abt_err;
extern char *path_jobs;
extern char *path_spool;
extern char *path_aux;
extern char  server_name[];
extern time_t time_now;
extern int   LOGLEVEL;

extern tlist_head svr_newjobs;
extern tlist_head svr_alljobs;



void send_qsub_delmsg(

  job  *pjob,  /* I */
  char *text)  /* I */

  {
  char      *phost;
  attribute *pattri;
  int        qsub_sock;

  phost = arst_string("PBS_O_HOST",&pjob->ji_wattr[(int)JOB_ATR_variables]);

  if ((phost == NULL) || ((phost = strchr(phost,'=')) == NULL))
    {
    return;
    }

  pattri = &pjob->ji_wattr[(int)JOB_ATR_interactive];

  qsub_sock = conn_qsub(phost + 1,pattri->at_val.at_long,NULL);

  if (qsub_sock < 0)
    {
    return;
    }

  if (write(qsub_sock,"PBS: ",5) == -1)
    {
    return;
    }
  if (write(qsub_sock,text,strlen(text)) == -1)
    {
    return;
    }

  close(qsub_sock);

  return;
  }  /* END send_qsub_delmsg() */




/*
 * job_abt - abort a job
 *
 *	The job removed from the system and a mail message is sent
 *	to the job owner.
 */

/* NOTE:  this routine is called under the following conditions:
 * 1) by req_deletejob whenever deleting a job that is not running,
 *    not transitting, not exiting and does not have a checkpoint
 *    file on the mom.
 * 2) by req_deletearray whenever deleting a job that is not running,
 *    not transitting, not in prerun, not exiting and does not have a
 *    checkpoint file on the mom.
 * 3) by close_quejob when the server fails to enqueue the job.
 * 4) by array_delete_wt for prerun jobs that hang around too long and 
 *    do not have a checkpoint file on the mom.
 * 5) by pbsd_init when recovering jobs.
 * 6) by svr_movejob when done routing jobs around.
 * 7) by queue_route when trying toroute any "ready" jobs in a specific queue.
 * 8) by req_shutdown when trying to shutdown.
 * 9) by req_register when the request oparation is JOB_DEPEND_OP_DELETE.
 */

int job_abt(

  job  **pjobp, /* I (modified/freed) */
  char  *text)  /* I (optional) */

  {
  char *myid = "job_abt";
  int	old_state;
  int	old_substate;
  int	rc = 0;

  job *pjob = *pjobp;

  /* save old state and update state to Exiting */

  old_state = pjob->ji_qs.ji_state;
  old_substate = pjob->ji_qs.ji_substate;

  /* notify user of abort if notification was requested */

  if (text != NULL) 
    {	
    /* req_delete sends own mail and acct record */

    account_record(PBS_ACCT_ABT,pjob,"");
    svr_mailowner(pjob,MAIL_ABORT,MAIL_NORMAL,text);

    if ((pjob->ji_qs.ji_state == JOB_STATE_QUEUED) &&
        ((pjob->ji_wattr[(int)JOB_ATR_interactive].at_flags & ATR_VFLAG_SET) &&
         pjob->ji_wattr[(int)JOB_ATR_interactive].at_val.at_long))
      {
      /* interactive and not yet running... send a note to qsub */

      send_qsub_delmsg(pjob,text);
      }
    }

  if (old_state == JOB_STATE_RUNNING) 
    {
    svr_setjobstate(pjob,JOB_STATE_RUNNING,JOB_SUBSTATE_ABORT);

    if ((rc = issue_signal(pjob,"SIGKILL",release_req,0)) != 0) 
      {
      sprintf(log_buffer,msg_abt_err, 
        pjob->ji_qs.ji_jobid,old_substate);

      log_err(-1,myid,log_buffer);

      if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HERE) == 0) 
        {
        /* notify creator that job is exited */

        pjob->ji_wattr[(int)JOB_ATR_state].at_val.at_char = 'E';
   
        issue_track(pjob);
        }

      if (pjob->ji_wattr[(int)JOB_ATR_depend].at_flags & ATR_VFLAG_SET)
        {
        depend_on_term(pjob);
        }

      job_purge(pjob);

      *pjobp = NULL;
      }
    } 
  else if ((old_state == JOB_STATE_TRANSIT) && 
           (old_substate == JOB_SUBSTATE_TRNOUT)) 
    {
    /* I don't know of a case where this could happen */
  
    sprintf(log_buffer, msg_abt_err, 
      pjob->ji_qs.ji_jobid, 
      old_substate);

    log_err(-1,myid,log_buffer);
    } 
  else 
    {
    svr_setjobstate(pjob,JOB_STATE_EXITING,JOB_SUBSTATE_ABORT);

    if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_HERE) == 0) 
      {
      /* notify creator that job is exited */

      issue_track(pjob);
      }

    if (pjob->ji_wattr[(int)JOB_ATR_depend].at_flags & ATR_VFLAG_SET)
      {
      depend_on_term(pjob);
      }

    job_purge(pjob);

    *pjobp = NULL;
    }

  return(rc);
  }  /* END job_abt() */


/*
 * conn_qsub - connect to the qsub that submitted this interactive job
 * return >= 0 on SUCCESS, < 0 on FAILURE
 * (this was moved from resmom/mom_inter.c)
 */



int conn_qsub(

  char *hostname,  /* I */
  long  port,      /* I */
  char *EMsg)      /* O (optional,minsize=1024) */

  {
  pbs_net_t hostaddr;
  int s;

  int flags;

  if (EMsg != NULL)
    EMsg[0] = '\0';

  if ((hostaddr = get_hostaddr(hostname)) == (pbs_net_t)0)
    {
#if !defined(H_ERRNO_DECLARED)
    extern int h_errno;
#endif

    /* FAILURE */

    if (EMsg != NULL)
      {
      snprintf(EMsg,1024,"cannot get address for host '%s', h_errno=%d",
        hostname,
        h_errno);
      }

    return(-1);
    }

  s = client_to_svr(hostaddr,(unsigned int)port,0,EMsg);

  /* NOTE:  client_to_svr() can return 0 for SUCCESS */

  /* assume SUCCESS requires s > 0 (USC) was 'if (s >= 0)' */
  /* above comment not enabled */

  if (s < 0)
    {
    /* FAILURE */

    return(-1);
    }

  /* SUCCESS */

  /* this socket should be blocking */

  flags = fcntl(s,F_GETFL);

  flags &= ~O_NONBLOCK;

  fcntl(s,F_SETFL,flags);

  return(s);
  }  /* END conn_qsub() */




/*
 * job_alloc - allocate space for a job structure and initialize working
 *	attribute to "unset"
 *
 *	Returns: pointer to structure or null is space not available.
 */

job *job_alloc()

  {
  job *pj;

  pj = (job *)calloc(1,sizeof(job));

  if (pj == NULL) 
    {
    log_err(errno,"job_alloc","no memory");

    return(NULL);
    }

  pj->ji_qs.qs_version = PBS_QS_VERSION;

  CLEAR_LINK(pj->ji_alljobs);
  CLEAR_LINK(pj->ji_jobque);

  CLEAR_HEAD(pj->ji_svrtask);
  CLEAR_HEAD(pj->ji_rejectdest);
  CLEAR_LINK(pj->ji_arrayjobs);
  pj->ji_arraystruct = NULL;
  pj->ji_isparent = FALSE;

  pj->ji_momhandle = -1;		/* mark mom connection invalid */

  /* set the working attributes to "unspecified" */

  job_init_wattr(pj);

  return(pj);
  }  /* END job_alloc() */





/*
 * job_free - free job structure and its various sub-structures
 */

void job_free(

  job *pj)  /* I (modified) */

  {
  int			 i;

  struct work_task	*pwt;
  badplace		*bp;

  if (LOGLEVEL >= 8)
    {
    sprintf(log_buffer,"freeing job");

    log_record(PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pj->ji_qs.ji_jobid,
      log_buffer);
    }

  /* remove any malloc working attribute space */

  for (i = 0;i < (int)JOB_ATR_LAST;i++) 
    {
    job_attr_def[i].at_free(&pj->ji_wattr[i]);
    }

  /* delete any work task entries associated with the job */

  while ((pwt = (struct work_task *)GET_NEXT(pj->ji_svrtask)) != NULL) 
    {
    delete_task(pwt);
    }

  /* free any bad destination structs */

  bp = (badplace *)GET_NEXT(pj->ji_rejectdest);

  while (bp != NULL) 
    {
    delete_link(&bp->bp_link);

    free(bp);

    bp = (badplace *)GET_NEXT(pj->ji_rejectdest);
    }

  /* now free the main structure */

  free((char *)pj);

  return;
  }  /* END job_free() */
 


 

/*
 * job_clone - create a clone of a job for use with job arrays
 */

job *job_clone(

  job *poldjob, /* I */  /* job to clone */
  int  taskid)  /* I */
  
  {
  static char   id[] = "job_clone";

  job		*pnewjob;
  attribute	tempattr;

  char		*oldid;
  char		*hostname;
  char		*tmpstr;
  char		basename[PBS_JOBBASE+1];
  char		namebuf[MAXPATHLEN + 1];
  char		buf[256];
  char		*pc;
  int		fds;

  int 		i;
  int           slen;
  
  job_array *pa;

  if (taskid > PBS_MAXJOBARRAY)
    {
    log_err(-1,id,"taskid out of range");

    return(NULL);
    }
  
  pnewjob = job_alloc();

  if (pnewjob == NULL)
    {
    log_err(errno,id,"no memory");

    return(NULL);
    }

  job_init_wattr(pnewjob);
  
  /* new job structure is allocated, 
     now we need to copy the old job, but modify based on taskid */

  CLEAR_LINK(pnewjob->ji_alljobs);
  CLEAR_LINK(pnewjob->ji_jobque);
  CLEAR_LINK(pnewjob->ji_svrtask); 
  CLEAR_HEAD(pnewjob->ji_rejectdest);
  pnewjob->ji_modified = 1;			/* struct changed, needs to be saved */
	
  /* copy the fixed size quick save information */

  memcpy(&pnewjob->ji_qs,&poldjob->ji_qs,sizeof(struct jobfix));

  /* pnewjob->ji_qs.ji_arrayid = taskid; */

  /* find the job id for the cloned job */

  oldid = strdup(poldjob->ji_qs.ji_jobid);

  if (oldid == NULL)
    {
    log_err(errno,id,"no memory");
    job_free(pnewjob);

    return(NULL);
    }

  hostname = index(oldid,'.');
  *(hostname++) = '\0';

  pnewjob->ji_qs.ji_jobid[PBS_MAXSVRJOBID] = '\0';

  snprintf(pnewjob->ji_qs.ji_jobid,PBS_MAXSVRJOBID,"%s-%d.%s",
    oldid,
    taskid,
    hostname);

  free(oldid);

  /* update the job filename
   * We could optimize the sub-jobs to all use the same file. We would need a 
   * way to track the number of tasks still using the job file so we know when
   * to delete it.  
   */

  /*
   * make up new job file name, it is based on the new jobid 
   */

  strncpy(basename,pnewjob->ji_qs.ji_jobid,PBS_JOBBASE);
  basename[PBS_JOBBASE] = '\0';

  do {
    strcpy(namebuf,path_jobs);
    strcat(namebuf,basename);
    strcat(namebuf,JOB_FILE_SUFFIX);

    fds = open(namebuf,O_CREAT|O_EXCL|O_WRONLY,0600);

    if (fds < 0) 
      {
      if (errno == EEXIST) 
        {
        pc = basename + strlen(basename) - 1;

        while (!isprint((int)*pc) || (*pc == '-')) 
          {
          pc--;

          if (pc <= basename) 
            {
            /* FAILURE */

            log_err(errno,id,"job file is corrupt");
            job_free(pnewjob);

            return(NULL);
            }
          }

        (*pc)++;
        } 
      else 
        {
        /* FAILURE */

        log_err(errno,id,"cannot create job file");
        job_free(pnewjob);

        return(NULL);
        }
      }
    } while (fds < 0);

  close(fds);

  strcpy(pnewjob->ji_qs.ji_fileprefix,basename);
 
  /* copy job attributes. some of these are going to have to be modified */

  for (i = 0; i < JOB_ATR_LAST; i++)
    {
    if (poldjob->ji_wattr[i].at_flags & ATR_VFLAG_SET)
      {
      if ((i == JOB_ATR_errpath) || (i == JOB_ATR_outpath) || (i == JOB_ATR_jobname))
        {
	/* modify the errpath and outpath */

	slen = strlen(poldjob->ji_wattr[i].at_val.at_str);

	tmpstr = (char*)malloc(sizeof(char) * (slen + PBS_MAXJOBARRAYLEN + 1));

	sprintf(tmpstr,"%s-%d", 
          poldjob->ji_wattr[i].at_val.at_str, 
          taskid);

	clear_attr(&tempattr,&job_attr_def[i]);

	job_attr_def[i].at_decode(
          &tempattr,
	  NULL,
	  NULL,
	  tmpstr);

	job_attr_def[i].at_set(
	  &pnewjob->ji_wattr[i],
	  &tempattr,
	  SET);

        job_attr_def[i].at_free(&tempattr);

	free(tmpstr);
	}
      else
        {
        job_attr_def[i].at_set(
          &(pnewjob->ji_wattr[i]),
          &(poldjob->ji_wattr[i]),
          SET);
        }
      }
    }

  /* put a system hold on the job.  we'll take the hold off once the 
   * entire array is cloned */
  pnewjob->ji_wattr[(int)JOB_ATR_hold].at_val.at_long |= HOLD_a;
  pnewjob->ji_wattr[(int)JOB_ATR_hold].at_flags |= ATR_VFLAG_SET;

  /* set JOB_ATR_job_array_id */
  pnewjob->ji_wattr[(int)JOB_ATR_job_array_id].at_val.at_long = taskid;
  pnewjob->ji_wattr[(int)JOB_ATR_job_array_id].at_flags |= ATR_VFLAG_SET;
  
  /* set PBS_ARRAYID var */
  clear_attr(&tempattr,&job_attr_def[(int)JOB_ATR_variables]);
  sprintf(buf,",PBS_ARRAYID=%d",taskid);

  job_attr_def[(int)JOB_ATR_variables].at_decode(&tempattr,
    NULL, 
    NULL, 
    buf);

  job_attr_def[(int)JOB_ATR_variables].at_set(
    &pnewjob->ji_wattr[(int)JOB_ATR_variables],
    &tempattr, 
    INCR);  

  job_attr_def[(int)JOB_ATR_variables].at_free(&tempattr);

  /* we need to link the cloned job into the array task list */
  pa = get_array(poldjob->ji_qs.ji_jobid);
  
  CLEAR_LINK(pnewjob->ji_arrayjobs);

  append_link(&pa->array_alljobs,&pnewjob->ji_arrayjobs,(void*)pnewjob);

  pnewjob->ji_arraystruct = pa;

  return(pnewjob);
  } /* END job_clone() */





/*
 * job_clone_wt - worktask to clone jobs for job array
 */
void job_clone_wt(

  struct work_task *ptask)

  {
  static char id[] = "job_clone_wt";
  job *pjob;
  job *pjobclone;
  struct work_task *new_task;
  int i;
  int num_cloned;
  int newstate;
  int newsub;
  int rc;
  char namebuf[MAXPATHLEN];
  job_array *pa;
  
  array_request_node *rn;
  int start;
  int end;
  int loop;
  
  pjob = (job*)(ptask->wt_parm1);
  
  pa = get_array(pjob->ji_qs.ji_jobid);
  rn = (array_request_node*)GET_NEXT(pa->request_tokens);
   
  strcpy(namebuf, path_jobs);
  strcat(namebuf, pjob->ji_qs.ji_fileprefix);
  strcat(namebuf, ".AR");
  
  /* do the clones in batches of 256 */

  num_cloned = 0;
  loop = TRUE;
  
  while (loop)
    {
    start = rn->start;
    end = rn->end;
    
    if (end - start > 256)
      {
      end = start + 255;
      }
      
    for (i = start; i <= end; i++)
      {
      pjobclone = job_clone(pjob, i);
      if (pjobclone == NULL)
        {
        log_err(-1, id, "unable to clone job in job_clone_wt");
        continue;
        }

      svr_evaljobstate(pjobclone,&newstate,&newsub,1);
      svr_setjobstate(pjobclone,newstate,newsub);
      pjobclone->ji_wattr[(int)JOB_ATR_qrank].at_val.at_long = ++queue_rank;
      pjobclone->ji_wattr[(int)JOB_ATR_qrank].at_flags |= ATR_VFLAG_SET;
    
      if ((rc = svr_enquejob(pjobclone))) 
        {
        job_purge(pjobclone);
        }
      
      if (job_save(pjobclone,SAVEJOB_FULL) != 0) 
        {
        job_purge(pjobclone);
        }
      
      pa->ai_qs.num_cloned++;
      
      rn->start++;
      
      array_save(pa);
      num_cloned++;
      }  /* END for (i) */
      
    if (rn->start > rn->end)
      {
      delete_link(&rn->request_tokens_link);
      free(rn);
      rn = (array_request_node*)GET_NEXT(pa->request_tokens);
      array_save(pa);
      }
      
    if (num_cloned == 256 || rn == NULL)
      {
      loop = FALSE;
      }
    }    /* END while (loop) */
  
  if (rn != NULL)
    {
    new_task = set_task(WORK_Timed,time_now + 1,job_clone_wt,ptask->wt_parm1);
    }
  else
    {
    /* this is the last batch of jobs, we can purge the "parent" job */

    job_purge(pjob);
    
    /* scan over all the jobs in the array and unset the hold */
    pjob = GET_NEXT(pa->array_alljobs);
    while (pjob != NULL)
      {	
      pjob->ji_wattr[(int)JOB_ATR_hold].at_val.at_long &= ~HOLD_a;
      if (pjob->ji_wattr[(int)JOB_ATR_hold].at_val.at_long == 0)
        {
        pjob->ji_wattr[(int)JOB_ATR_hold].at_flags &= ~ATR_VFLAG_SET;	
        }
      else
        {
        pjob->ji_wattr[(int)JOB_ATR_hold].at_flags |= ATR_VFLAG_SET;
        }
      
      svr_evaljobstate(pjob,&newstate,&newsub,1);
      svr_setjobstate(pjob,newstate,newsub);
      
      job_save(pjob,SAVEJOB_FULL);
     
      
      pjob = (job*)GET_NEXT(pjob->ji_arrayjobs);
        
      }
    }

  return;
  } /* END job_clone_tw */
  



/*
 * job_init_wattr - initialize job working attribute array
 *	set the types and the "unspecified value" flag
 */

static void job_init_wattr(

  job *pj)

  {
  int i;

  for (i = 0;i < (int)JOB_ATR_LAST;i++) 
    {
    clear_attr(&pj->ji_wattr[i],&job_attr_def[i]);
    }

  return;
  }   /* END job_init_wattr() */ 





/*
 * job_purge - purge job from system
 *
 * The job is dequeued; the job control file, script file and any spooled
 * output files are unlinked, and the job structure is freed.
 * If we are MOM, the task files and checkpoint files are also
 * removed.
 */

void job_purge(

  job *pjob)  /* I (modified) */

  {
  static char   id[] = "job_purge";

  char          namebuf[MAXPATHLEN + 1];
  extern char  *msg_err_purgejob;

  if ((pjob->ji_qs.ji_substate != JOB_SUBSTATE_TRANSIN) &&
      (pjob->ji_qs.ji_substate != JOB_SUBSTATE_TRANSICM))
    {
    svr_dequejob(pjob);
    }
    
  /* if part of job array then remove from array's job list */ 
  if (pjob->ji_arraystruct != NULL &&
      pjob->ji_isparent == FALSE)
    {
    
    delete_link(&pjob->ji_arrayjobs);
    /* if the only thing in the array alljobs list is the head, then we can 
       clean that up too */
    if ( GET_NEXT(pjob->ji_arraystruct->array_alljobs) == pjob->ji_arraystruct->array_alljobs.ll_struct)
      {
      array_delete(pjob->ji_arraystruct);
      }
    }
    
  if (pjob->ji_isparent == TRUE)
    {
    delete_link(&pjob->ji_alljobs);
    }


  if (!(pjob->ji_wattr[(int)JOB_ATR_job_array_request].at_flags & ATR_VFLAG_SET))
    {
    strcpy(namebuf,path_jobs);	/* delete script file */
    strcat(namebuf,pjob->ji_qs.ji_fileprefix);
    strcat(namebuf,JOB_SCRIPT_SUFFIX);

    if (unlink(namebuf) < 0)
      {
      if (errno != ENOENT)
        log_err(errno,id,msg_err_purgejob);
      }
    else if (LOGLEVEL >= 6)
      {
      sprintf(log_buffer,"removed job script");

      log_record(PBSEVENT_DEBUG,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        log_buffer);
      }
    }

  strcpy(namebuf,path_spool);	/* delete any spooled stdout */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_STDOUT_SUFFIX);

  if (unlink(namebuf) < 0) 
    {
    if (errno != ENOENT)
      log_err(errno,id,msg_err_purgejob);
    }
  else if (LOGLEVEL >= 6)
    {
    sprintf(log_buffer,"removed job stdout");

    log_record(PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);
    }

  strcpy(namebuf,path_spool);	/* delete any spooled stderr */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);
  strcat(namebuf,JOB_STDERR_SUFFIX);

  if (unlink(namebuf) < 0) 
    {
    if (errno != ENOENT)
      log_err(errno,id,msg_err_purgejob);
    }
  else if (LOGLEVEL >= 6)
    {
    sprintf(log_buffer,"removed job stderr");

    log_record(PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);
    }

  strcpy(namebuf,path_jobs);	/* delete job file */
  strcat(namebuf,pjob->ji_qs.ji_fileprefix);

  if (pjob->ji_isparent == TRUE)
    {
    strcat(namebuf, JOB_FILE_TMP_SUFFIX);
    }
  else
    {
    strcat(namebuf,JOB_FILE_SUFFIX);
    }
    
  if (unlink(namebuf) < 0)
    {
    if (errno != ENOENT)
      log_err(errno,id,msg_err_purgejob);
    }
  else if (LOGLEVEL >= 6)
    {
    sprintf(log_buffer,"removed job file");

    log_record(PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);
    }

  job_free(pjob);

  return;
  }  /* END job_purge() */





/*
 * find_job() - find job by jobid
 *
 *	Search list of all server jobs for one with same job id
 *	Return NULL if not found or pointer to job struct if found
 */

job *find_job(

  char *jobid)

  {
  char *at;
  job  *pj;

  if ((at = strchr(jobid,(int)'@')) != NULL)
    *at = '\0';	/* strip off @server_name */

  pj = (job *)GET_NEXT(svr_alljobs);

  while (pj != NULL) 
    {
    if (!strcmp(jobid,pj->ji_qs.ji_jobid))
      break;

    pj = (job *)GET_NEXT(pj->ji_alljobs);
    }

  if (at)
    *at = '@';	/* restore @server_name */

  return(pj);  /* may be NULL */
  }   /* END find_job() */ 

/* END job_func.c */

