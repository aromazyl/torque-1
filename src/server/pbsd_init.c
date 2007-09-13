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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <netdb.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <grp.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#ifdef _CRAY
#include <sys/category.h>
#endif	/* _CRAY */
#include <sys/time.h>
#include <sys/resource.h>

#include "pbs_ifl.h"
#include "log.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "server.h"
#include "queue.h"
#include "job.h"
#include "work_task.h"
#include "tracking.h"
#include "svrfunc.h"
#include "acct.h"
#include "rpp.h"
#include "net_connect.h"
#include "pbs_proto.h"
#include "batch_request.h"


/*#ifndef SIGKILL*/
/* there is some weird stuff in gcc include files signal.h & sys/params.h */
#include <signal.h>
/*#endif*/

#ifndef SUCCESS
#define SUCCESS 1
#endif /* SUCCESS */

#ifndef FAILURE
#define FAILURE 0
#endif /* FAILURE */

#ifndef TRUE
#define TRUE 1
#endif /* TRUE */

#ifndef FALSE
#define FALSE 0
#endif /* FALSE */

/* global Data Items */

extern char	*msg_startup3;
extern char     *msg_daemonname;
extern char	*msg_init_abt;
extern char	*msg_init_queued;
extern char	*msg_init_substate;
extern char	*msg_err_noqueue;
extern char	*msg_err_malloc;
extern char	*msg_init_noqueues;
extern char	*msg_init_recovque;
extern char	*msg_init_expctq;
extern char	*msg_init_nojobs;
extern char	*msg_init_exptjobs;
extern char	*msg_init_norerun;
extern char	*msg_init_unkstate;
extern char	*msg_init_baddb;
extern char	*msg_init_chdir;
extern char	*msg_init_badjob;
extern char	*msg_script_open;

extern char	*acct_file;
extern char	*log_file;
extern char	*path_home;
extern char	*path_acct;
extern char	path_log[];
extern char	*path_priv;
extern char	*path_arrays;
extern char	*path_jobs;
extern char	*path_queues;
extern char	*path_spool;
extern char	*path_svrdb;
extern char	*path_svrdb_new;
extern char	*path_track;
extern char	*path_nodes;
extern char	*path_nodes_new;
extern char	*path_nodestate;
extern char	*path_nodenote;
extern char	*path_nodenote_new;
extern char	*path_resources;

extern int	 queue_rank;
extern char	 server_name[];
extern int	 svr_delay_entry;
extern tlist_head svr_newjobs;
extern tlist_head svr_alljobs;
extern tlist_head svr_queues;
extern tlist_head svr_requests;
extern tlist_head svr_newnodes;
extern tlist_head svr_jobarrays;
extern tlist_head task_list_immed;
extern tlist_head task_list_timed;
extern tlist_head task_list_event;
extern time_t	 time_now;

extern struct server server;

/* External Functions Called */

extern void   on_job_exit A_((struct work_task *));
extern void   on_job_rerun A_((struct work_task *));
extern void   set_resc_assigned A_((job *,enum batch_op));
extern void   set_old_nodes A_((job *));
extern void   acct_close A_((void));
extern struct work_task *apply_job_delete_nanny A_((struct job *,int));
extern int     net_move A_((job *,struct batch_request *));
extern int delete_array_struct(array_job_list *pajl);

/* Private functions in this file */

static void  init_abt_job A_((job *));
static char *build_path A_((char *,char *,char *));
static void  catch_child A_((int));
static void  catch_abort A_((int));
static void  change_logs A_((int));
static int   chk_save_file A_((char *));
static void  need_y_response A_((int));
static int   pbsd_init_job A_((job *,int));
static void  pbsd_init_reque A_((job *,int));
static void  resume_net_move A_((struct work_task *));
static void  rm_files A_((char *));
static void  stop_me A_((int));

/* private data */

#define CHANGE_STATE 1
#define KEEP_STATE   0



/*
 * This file contains the functions to initialize the PBS Batch Server.
 * The code is called once when the server is brought up.
 */

int pbsd_init(

  int type)		/* type of initialization   */

  {
  int	a_opt = -1;
  int	baselen;
  char	basen[MAXPATHLEN+1];
  struct dirent *pdirent;
  DIR	*dir;
  int	 fd;
  int	 had;
  int	 i;
  static char id[] = "pbsd_init";
  char	*job_suffix = JOB_FILE_SUFFIX;
  int	 job_suf_len = strlen(job_suffix);
  int    array_suf_len = strlen(ARRAY_FILE_SUFFIX);
  int	 logtype;
  char	*new_tag = ".new";
  job	*pjob;
  pbs_queue *pque;
  char	*psuffix;
  int	 rc;
  struct stat statbuf;
  char	*suffix_slash = "/";
  struct sigaction act;
  struct sigaction oact;
  
  array_job_list *pajl;

  char   EMsg[1024];

  extern int TForceUpdate;

  /* The following is code to reduce security risks */

  if (setup_env(PBS_ENVIRON) == -1) 
    {
    return(-1);
    }

  i = getgid();

  setgroups(1,(gid_t *)&i);	/* secure suppl. groups */

  i = sysconf(_SC_OPEN_MAX);

  while (--i < 2)
    close(i); /* close any file desc left open by parent */

#ifndef DEBUG
#ifdef _CRAY
  limit(C_JOB,      0,L_CPROC, 0);
  limit(C_JOB,      0,L_CPU,   0);
  limit(C_JOBPROCS, 0,L_CPU,   0);
  limit(C_PROC,     0,L_FD,    255);
  limit(C_JOB,      0,L_FSBLK, 0);
  limit(C_JOBPROCS, 0,L_FSBLK, 0);
  limit(C_JOB,      0,L_MEM  , 0);
  limit(C_JOBPROCS, 0,L_MEM  , 0);
#else	/* not  _CRAY */
  {
  struct rlimit rlimit;

  rlimit.rlim_cur = RLIM_INFINITY;
  rlimit.rlim_max = RLIM_INFINITY;
  setrlimit(RLIMIT_CPU,   &rlimit);
  setrlimit(RLIMIT_FSIZE, &rlimit);
  setrlimit(RLIMIT_DATA,  &rlimit);
  setrlimit(RLIMIT_STACK, &rlimit);
#ifdef	RLIMIT_RSS
  setrlimit(RLIMIT_RSS,   &rlimit);
#endif	/* RLIMIT_RSS */
#ifdef	RLIMIT_VMEM
  setrlimit(RLIMIT_VMEM,  &rlimit);
#endif	/* RLIMIT_VMEM */
  }
#endif	/* not _CRAY */
#endif	/* DEBUG */

  /* 1. set up to catch or ignore various signals */

  sigemptyset(&act.sa_mask);

  act.sa_flags   = 0;
  act.sa_handler = change_logs;

  if (sigaction(SIGHUP,&act,&oact) != 0) 
    {
    log_err(errno, id, "sigaction for HUP");

    return(2);
    }

  act.sa_handler = stop_me;

  if (sigaction(SIGINT,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigaction for INT");

    return(2);
    }

  if (sigaction(SIGTERM,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigactin for TERM");

    return(2);
    }

#ifdef NDEBUG

  if (sigaction(SIGQUIT,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigactin for QUIT");

    return(2);
    }

#endif	/* NDEBUG */

#ifdef SIGSHUTDN

  if (sigaction(SIGSHUTDN,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigactin for SHUTDN");

    return(2);
    }
#endif /* SIGSHUTDN */

  /*
   * Catch these signals to ensure we core dump even if
   * our rlimit for core dumps is set to 0 initially.
   *
   * Chris Samuel - VPAC
   * csamuel@vpac.org - 29th July 2003
   *
   * Now conditional on PBSCOREDUMP environment variable.
   * 13th August 2003.
   */

  if (getenv("PBSCOREDUMP"))
    {
    act.sa_handler = catch_abort;   /* make sure we core dump */

    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGBUS,  &act, NULL);
    sigaction(SIGFPE,  &act, NULL);
    sigaction(SIGILL,  &act, NULL);
    sigaction(SIGTRAP, &act, NULL);
    sigaction(SIGSYS,  &act, NULL);
    }

  act.sa_handler = catch_child;

  if (sigaction(SIGCHLD,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigaction for CHLD");

    return(2);
    }

  act.sa_handler = SIG_IGN;

  if (sigaction(SIGPIPE,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigaction for PIPE");

    return(2);
    }

  if (sigaction(SIGUSR1,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigaction for USR1");

    return(2);
    }

  if (sigaction(SIGUSR2,&act,&oact) != 0) 
    {
    log_err(errno,id,"sigaction for USR2");

    return(2);
    }

  /* 2. set up the various paths and other global variables we need */

  path_priv      = build_path(path_home, PBS_SVR_PRIVATE, suffix_slash);
  path_arrays	 = build_path(path_priv, PBS_ARRAYDIR, suffix_slash);
  path_spool     = build_path(path_home, PBS_SPOOLDIR, suffix_slash);
  path_queues    = build_path(path_priv, PBS_QUEDIR,   suffix_slash);
  path_jobs      = build_path(path_priv, PBS_JOBDIR,   suffix_slash);
  path_acct	 = build_path(path_priv, PBS_ACCT,     suffix_slash);
  path_svrdb     = build_path(path_priv, PBS_SERVERDB, NULL);
  path_svrdb_new = build_path(path_priv, PBS_SERVERDB, new_tag);
  path_track	 = build_path(path_priv, PBS_TRACKING, NULL);
  path_nodes	 = build_path(path_priv, NODE_DESCRIP, NULL);
  path_nodes_new = build_path(path_priv, NODE_DESCRIP, new_tag);
  path_nodestate = build_path(path_priv, NODE_STATUS,  NULL);
  path_nodenote  = build_path(path_priv, NODE_NOTE,    NULL);
  path_nodenote_new = build_path(path_priv, NODE_NOTE, new_tag);
  path_resources = build_path(path_home, PBS_RESOURCES, NULL);

  init_resc_defs(path_resources);

#if !defined(DEBUG) && !defined(NO_SECURITY_CHECK)

  rc  = chk_file_sec(path_jobs,  1,0,S_IWGRP|S_IWOTH,1,EMsg);
  rc |= chk_file_sec(path_queues,1,0,S_IWGRP|S_IWOTH,0,EMsg);
  rc |= chk_file_sec(path_spool, 1,1,S_IWOTH,        0,EMsg);
  rc |= chk_file_sec(path_acct,	 1,0,S_IWGRP|S_IWOTH,0,EMsg);
  rc |= chk_file_sec(PBS_ENVIRON,0,0,S_IWGRP|S_IWOTH,1,EMsg);

  if (rc != 0) 
    {
    return(3);
    }

#endif	/* not DEBUG and not NO_SECURITY_CHECK */
	
  CLEAR_HEAD(svr_requests);
  CLEAR_HEAD(task_list_immed);
  CLEAR_HEAD(task_list_timed);
  CLEAR_HEAD(task_list_event);
  CLEAR_HEAD(svr_queues);
  CLEAR_HEAD(svr_alljobs);
  CLEAR_HEAD(svr_newjobs);
  CLEAR_HEAD(svr_newnodes);
  CLEAR_HEAD(svr_jobarrays);

  time_now = time((time_t *)0);

  /* 3. Set default server attibutes values */

  if (server.sv_attr[(int)SRV_ATR_scheduling].at_flags & ATR_VFLAG_SET)
    {
    a_opt = server.sv_attr[(int)SRV_ATR_scheduling].at_val.at_long;
    }

  for (i = 0;i < SRV_ATR_LAST;i++)
    clear_attr(&server.sv_attr[i],&svr_attr_def[i]);

  server.sv_attr[(int)SRV_ATR_schedule_iteration].at_val.at_long = 
    PBS_SCHEDULE_CYCLE;

  server.sv_attr[(int)SRV_ATR_schedule_iteration].at_flags =
    ATR_VFLAG_SET;

  server.sv_attr[(int)SRV_ATR_State].at_val.at_long = SV_STATE_INIT;
  server.sv_attr[(int)SRV_ATR_State].at_flags = ATR_VFLAG_SET;

  svr_attr_def[(int)SRV_ATR_mailfrom].at_decode(
    &server.sv_attr[(int)SRV_ATR_mailfrom], 
    0, 
    0,
    PBS_DEFAULT_MAIL);

  /* disable ping_rate.  no longer used */

/*
  server.sv_attr[(int)SRV_ATR_ping_rate].at_val.at_long =
    PBS_NORMAL_PING_RATE;
  server.sv_attr[(int)SRV_ATR_ping_rate].at_flags = ATR_VFLAG_SET;
*/

  server.sv_attr[(int)SRV_ATR_tcp_timeout].at_val.at_long =
    PBS_TCPTIMEOUT;
  server.sv_attr[(int)SRV_ATR_tcp_timeout].at_flags = ATR_VFLAG_SET;

  server.sv_attr[(int)SRV_ATR_check_rate].at_val.at_long =
    PBS_NORMAL_PING_RATE / 2;
  server.sv_attr[(int)SRV_ATR_check_rate].at_flags = ATR_VFLAG_SET;

  server.sv_attr[(int)SRV_ATR_JobStatRate].at_val.at_long =
    PBS_RESTAT_JOB;

  server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long = PBS_POLLJOBS;

  /* 4. force logging of all types */

  server.sv_attr[(int)SRV_ATR_log_events].at_val.at_long = PBSEVENT_MASK;
  server.sv_attr[(int)SRV_ATR_log_events].at_flags = ATR_VFLAG_SET;

  /* 5. If not a "create" initialization, recover server db */

  rc = chk_save_file(path_svrdb);

  if (type != RECOV_CREATE) 
    {
    /* Open the server database (save file) and read it in */

    if ((rc != 0) || ((rc = svr_recov(path_svrdb)) == -1)) 
      {
      log_err(rc,"pbsd_init",msg_init_baddb);

      return(-1);
      }

    if (server.sv_attr[(int)SRV_ATR_resource_assn].at_flags & ATR_VFLAG_SET) 
      {
      svr_attr_def[(int)SRV_ATR_resource_assn].at_free(
        &server.sv_attr[(int)SRV_ATR_resource_assn]);  
      }
    } 
  else 
    {
    if (rc == 0) 
      {	
      /* path_svrdb exists */

      /* will exit on (n)o response */

      if (TForceUpdate == 0)
        {
        need_y_response(type);
        }

      /* (y)es response received */

      rm_files(path_priv);

      svr_save(&server,SVR_SAVE_FULL);
      }
    }

  svr_attr_def[(int)SRV_ATR_version].at_decode(
    &server.sv_attr[(int)SRV_ATR_version], 
    0, 
    0,
    PACKAGE_VERSION);

  /* 6. open accounting file */

  if (acct_open(acct_file) != 0)
    {
    return(-1);
    }

  /* 7. Set up other server and global variables */

  if (a_opt != -1) 
    {
    /* a_option was set, overrides saved value of scheduling attr */

    server.sv_attr[(int)SRV_ATR_scheduling].at_val.at_long = a_opt;
    server.sv_attr[(int)SRV_ATR_scheduling].at_flags |=
      ATR_VFLAG_SET;
    }

  /* Open and read in node list if one exists */
 
  if ((rc = setup_nodes()) == -1) 
    {
    /* log_buffer set in setup_nodes */

    log_err(-1,"pbsd_init(setup_nodes)",log_buffer);

    return(-1);
    }
 
  /*
   * 8. If not a "create" initialization, recover queues.
   *    If a create, remove any queues that might be there.
   */

  if (chdir(path_queues) != 0) 
    {
    sprintf(log_buffer,msg_init_chdir,path_queues);

    log_err(errno,"pbsd_init",log_buffer);
 
    return(-1);
    }

  had = server.sv_qs.sv_numque;
  server.sv_qs.sv_numque = 0;

  dir = opendir(".");

  if (dir == NULL) 
    {
    log_err(-1,"pbsd_init",msg_init_noqueues);

    return(-1);
    }

  while ((pdirent = readdir(dir)) != NULL) 
    {
    if (pdirent->d_name == NULL)
      {
      /* invalid name returned */

      continue;
      }

    if (chk_save_file(pdirent->d_name) == 0) 
      {
      /* recover queue */

      if ((pque = que_recov(pdirent->d_name)) != NULL) 
        {
        /* que_recov increments sv_numque */

        sprintf(log_buffer,msg_init_recovque,
          pque->qu_qs.qu_name);

        log_event(
          PBSEVENT_SYSTEM|PBSEVENT_ADMIN|PBSEVENT_DEBUG, 
          PBS_EVENTCLASS_SERVER,
          msg_daemonname, 
          log_buffer);

        if (pque->qu_attr[(int)QE_ATR_ResourceAssn].at_flags & ATR_VFLAG_SET) 
          {
          que_attr_def[(int)QE_ATR_ResourceAssn].at_free(
            &pque->qu_attr[(int)QE_ATR_ResourceAssn]);  
          }
        }
      }
    }

  closedir(dir);
		
  if ((had != server.sv_qs.sv_numque) && (type != RECOV_CREATE))
    logtype = PBSEVENT_ERROR|PBSEVENT_SYSTEM;
  else
    logtype = PBSEVENT_SYSTEM;

  sprintf(log_buffer,msg_init_expctq,
    had,server.sv_qs.sv_numque);

  log_event(logtype,PBS_EVENTCLASS_SERVER,msg_daemonname,log_buffer);

  /*
   * 9. If not "create" or "clean" recovery, recover the jobs.
   *    If a a create or clean recovery, delete any jobs.
   */

 /* 9.a, recover job array info */
  
   if (chdir(path_arrays) != 0) 
    {
    sprintf(log_buffer,msg_init_chdir,
      path_arrays);

    log_err(errno,"pbsd_init",log_buffer);

    return(-1);
    }
    
  dir = opendir(".");  
  while ((pdirent = readdir(dir)) != NULL) 
     {       

     if (chk_save_file(pdirent->d_name) == 0) 
       {
       /* if not create or clean recovery, recover arrays */
       
       if ((type != RECOV_CREATE) && (type != RECOV_COLD))
         {
	 /* skip files without the proper suffix */
	 baselen = strlen(pdirent->d_name) - array_suf_len;

         psuffix = pdirent->d_name + baselen;

         if (strcmp(psuffix,ARRAY_FILE_SUFFIX))
           continue;
	 
	 
	 pajl = (array_job_list*)malloc(sizeof(array_job_list));
	 CLEAR_LINK(pajl->all_arrays);
	 CLEAR_HEAD(pajl->array_alljobs);
	 
	 fd = open(pdirent->d_name, O_RDONLY,0);
	 	 	 
	 if (read(fd, &(pajl->ai_qs), sizeof(pajl->ai_qs)) != sizeof(pajl->ai_qs))
	   {
	   sprintf(log_buffer,"unable to read %s", pdirent->d_name);

           log_err(errno,"pbsd_init",log_buffer);

           return(-1);
	   }
	   
	 close(fd);
	 
	 append_link(&svr_jobarrays, &pajl->all_arrays, (void*)pajl);
	 
	 }
       else
         {
	 unlink(pdirent->d_name);
	 }
       
       }
              
     }
  closedir(dir);    
  
  /* 9.b,  recover jobs */

  if (chdir(path_jobs) != 0) 
    {
    sprintf(log_buffer,msg_init_chdir,
      path_jobs);

    log_err(errno,"pbsd_init",log_buffer);

    return(-1);
    }
		
  had = server.sv_qs.sv_numjobs;

  server.sv_qs.sv_numjobs = 0;

  dir = opendir(".");

  if (dir == NULL) 
    {
    if ((type != RECOV_CREATE) && (type != RECOV_COLD)) 
      {
      if (had == 0) 
        {
        log_event(
          PBSEVENT_DEBUG, 
          PBS_EVENTCLASS_SERVER,
          msg_daemonname, 
          msg_init_nojobs);
        } 
      else 
        {
        sprintf(log_buffer,msg_init_exptjobs,
          had,0);

        log_err(-1,"pbsd_init",log_buffer);
        }
      }
    } 
  else 
    {
    /* Now, for each job found ... */
	
    while ((pdirent = readdir(dir)) != NULL) 
      {
      if (chk_save_file(pdirent->d_name) == 0) 
        {
        /* recover the jobs */

        baselen = strlen(pdirent->d_name) - job_suf_len;

        psuffix = pdirent->d_name + baselen;

        if (strcmp(psuffix,job_suffix))
          continue;

        if ((pjob = job_recov(pdirent->d_name)) != NULL) 
          {
          if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_COMPLETE)
            {
            /* ignore/remove completed job */

	    /* for some reason, if a completed job is recovered, and it is
             * forcibly purged with 'qdel -p', it will get deleted a second
             * time resulting in a segfault */

            job_purge(pjob);

            continue;
            }

          if (pbsd_init_job(pjob,type) == FAILURE)
            {
            log_event(
              PBSEVENT_ERROR|PBSEVENT_SYSTEM|PBSEVENT_ADMIN|PBSEVENT_JOB|PBSEVENT_FORCE,
              PBS_EVENTCLASS_JOB,
              pdirent->d_name,
              msg_script_open);

            continue;
            }

          if ((type != RECOV_COLD) &&
              (type != RECOV_CREATE) &&
              (pjob->ji_qs.ji_svrflags & JOB_SVFLG_SCRIPT))
            {
            strcpy(basen,pdirent->d_name);

            psuffix = basen + baselen;

            strcpy(psuffix,JOB_SCRIPT_SUFFIX);

            if (chk_save_file(basen) != 0) 
              {
              log_event(
                PBSEVENT_ERROR|PBSEVENT_SYSTEM|PBSEVENT_ADMIN|PBSEVENT_JOB|PBSEVENT_FORCE, 
                PBS_EVENTCLASS_JOB,
                pjob->ji_qs.ji_jobid,
                msg_script_open);

              init_abt_job(pjob);
              }
            }
          } 
        else 
          {
          sprintf(log_buffer,msg_init_badjob,
            pdirent->d_name);

          log_err(-1,"pbsd_init",log_buffer);

          /* remove corrupt job */

          strcpy(basen,pdirent->d_name);

          psuffix = basen + baselen;

          strcpy(psuffix,JOB_BAD_SUFFIX);
      
          if (link(pdirent->d_name,basen) < 0)
            {
            log_err(errno,"pbsd_init","failed to link corrupt .JB file to .BD");
            }
          else
            {
            unlink(pdirent->d_name);
            }
          }
        }
      }

    closedir(dir);

    if ((had != server.sv_qs.sv_numjobs) && 
        (type != RECOV_CREATE) &&
        (type != RECOV_COLD))
      {
      logtype = PBSEVENT_ERROR | PBSEVENT_SYSTEM;
      }
    else
      {
      logtype = PBSEVENT_SYSTEM;
      }

    sprintf(log_buffer,msg_init_exptjobs,
      had,server.sv_qs.sv_numjobs);

    log_event(
      logtype, 
      PBS_EVENTCLASS_SERVER,
      msg_daemonname, 
      log_buffer);
    }  /* END else */

  /* If queue_rank has gone negative, renumber all jobs and reset rank */

  if (queue_rank < 0) 
    {
    queue_rank = 0;

    pjob = (job *)GET_NEXT(svr_alljobs);

    while (pjob != NULL) 
      {
      pjob->ji_wattr[(int)JOB_ATR_qrank].at_val.at_long = ++queue_rank;

      job_save(pjob,SAVEJOB_FULL);

      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
      }
    }
    
  /* look for empty arrays and delete them */
  pajl = (array_job_list*)GET_NEXT(svr_jobarrays);  
  while (pajl != NULL)
    {
    if (GET_NEXT(pajl->array_alljobs) == pajl->array_alljobs.ll_struct)
      {
      array_job_list *temp = (array_job_list*)GET_NEXT(pajl->all_arrays);
      delete_array_struct(pajl);
      pajl = temp;
      }
    else
      {
      pajl = (array_job_list*)GET_NEXT(pajl->all_arrays);
      }
    }
    
    
  /* Put us back in the Server's Private directory */

  if (chdir(path_priv) != 0) 
    {
    sprintf(log_buffer,msg_init_chdir,path_priv);

    log_err(-1,id,log_buffer);

    return(3);
    }

     
  /* 10. Open and read in tracking records */

  fd = open(path_track,O_RDONLY|O_CREAT,0600);

  if (fd < 0) 
    {
    log_err(errno,"pbsd_init","unable to open tracking file");

    return(-1);
    }

#if !defined(DEBUG) && !defined(NO_SECURITY_CHECK)
  
  if (chk_file_sec(path_track,0,0,S_IWGRP|S_IWOTH,0,EMsg) != 0)
    {
    return(-1);
    }

#endif  /* not DEBUG and not NO_SECURITY_CHECK */

  if (fstat(fd,&statbuf) < 0) 
    {
    log_err(errno,"pbs_init","unable to stat tracking file");

    return(-1);
    } 

  i = (statbuf.st_size + sizeof (struct tracking) - 1) / sizeof (struct tracking);

  if (i < PBS_TRACK_MINSIZE)
    server.sv_tracksize = PBS_TRACK_MINSIZE;
  else 
    server.sv_tracksize = i;

  server.sv_track = (struct tracking *)calloc(server.sv_tracksize,sizeof(struct tracking));

  for (i = 0;i < server.sv_tracksize;i++)
    (server.sv_track + i)->tk_mtime = 0;

  /* NOTE:  tracking file records are optional */

  i = read(fd,(char *)server.sv_track,server.sv_tracksize * sizeof(struct tracking));

  if (i < 0)
    {
    log_err(errno,"pbs_init","unable to read tracksize from tracking file");
    }

  close(fd);

  server.sv_trackmodifed = 0;

  /* set work task to periodically save the tracking records */

  set_task(WORK_Timed,(long)(time_now + PBS_SAVE_TRACK_TM),track_save,0);

  return(0);
  }  /* END pbsd_init() */




/*
 * build_path - build the pathname for a PBS directory
 */

static char *build_path(

  char *parent,	 /* parent directory name (dirname) */
  char *name,	 /* sub directory name */
  char *suffix)	 /* suffix string to append */

  {
  int   prefixslash;
  char *ppath;
  size_t len;

  /*
   * allocate space for the names + maybe a slash between + the suffix
   */

  if (*(parent + strlen(parent) - 1)  == '/')
    prefixslash = 0;
  else
    prefixslash = 1;

  len = strlen(parent) + strlen(name) + prefixslash + 1;

  if (suffix != NULL)
    len += strlen(suffix);

  ppath = malloc(len);

  if (ppath != NULL) 
    {
    strcpy(ppath,parent);

    if (prefixslash)
      strcat(ppath, "/");

    strcat(ppath, name);

    if (suffix) 
      strcat(ppath, suffix);

    return(ppath);
    }

  log_err(errno,"build_path",msg_err_malloc);

  log_close(1);

  exit(3);
  }  /* END build_path() */





/*
 * pbsd_init_job - decide what to do with the recovered job structure
 *
 *	The action depends on the type of initialization.
 */

static int pbsd_init_job(

  job *pjob,  /* I */
  int  type)  /* I */

  {
  unsigned int d;
  struct work_task *pwt;

  pjob->ji_momhandle = -1;

  /* update at_server attribute in case name changed */

  job_attr_def[(int)JOB_ATR_at_server].at_free(
    &pjob->ji_wattr[(int)JOB_ATR_at_server]);

  job_attr_def[(int)JOB_ATR_at_server].at_decode(
    &pjob->ji_wattr[(int)JOB_ATR_at_server],
    NULL, 
    NULL, 
    server_name);

  /* update queue_rank if this job is higher than current */

  if ((unsigned long)pjob->ji_wattr[(int)JOB_ATR_qrank].at_val.at_long > (unsigned long)queue_rank)
    queue_rank = pjob->ji_wattr[(int)JOB_ATR_qrank].at_val.at_long;

  /* now based on the initialization type */

  if ((type == RECOV_COLD) || (type == RECOV_CREATE)) 
    {
    need_y_response(type);

    init_abt_job(pjob);

    return(FAILURE);
    } 

  if (type != RECOV_HOT)
    pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_HOTSTART;

  switch (pjob->ji_qs.ji_substate) 
    {
    case JOB_SUBSTATE_TRANSICM:

      if (pjob->ji_qs.ji_svrflags & JOB_SVFLG_HERE) 
        {
        /*
         * This server created the job, so client 
         * was qsub (a transient client), it won't be
         * around to recommit, so auto-commit now
         */

        pjob->ji_qs.ji_state = JOB_STATE_QUEUED;
        pjob->ji_qs.ji_substate = JOB_SUBSTATE_QUEUED;

        pbsd_init_reque(pjob,CHANGE_STATE);
        } 
      else 
        {
        /*
         * another server is sending, append to new job
         * list and wait for commit; need to clear 
         * receiving socket number though
         */

        pjob->ji_qs.ji_un.ji_newt.ji_fromsock = -1;

        append_link(&svr_newjobs,&pjob->ji_alljobs,pjob);
        }

      break;
			
    case JOB_SUBSTATE_TRNOUT:

      pjob->ji_qs.ji_state = JOB_STATE_QUEUED;
      pjob->ji_qs.ji_substate = JOB_SUBSTATE_QUEUED;

      /* requeue as queued */

      pbsd_init_reque(pjob,CHANGE_STATE);

      break;

    case JOB_SUBSTATE_TRNOUTCM:

      /* requeue as is - rdy to cmt */

      pbsd_init_reque(pjob,KEEP_STATE);	

      /* resend rtc */

      pwt = set_task(WORK_Immed,0,resume_net_move,(void *)pjob);

      if (pwt)
        {
        append_link(&pjob->ji_svrtask,&pwt->wt_linkobj,pwt);
        }

      break;
		
    case JOB_SUBSTATE_QUEUED:
    case JOB_SUBSTATE_PRESTAGEIN:
    case JOB_SUBSTATE_STAGEIN:
    case JOB_SUBSTATE_STAGECMP:
    case JOB_SUBSTATE_STAGEFAIL:
    case JOB_SUBSTATE_STAGEGO:
    case JOB_SUBSTATE_HELD:
    case JOB_SUBSTATE_SYNCHOLD:
    case JOB_SUBSTATE_DEPNHOLD:
    case JOB_SUBSTATE_WAITING:
    case JOB_SUBSTATE_PRERUN:

      pbsd_init_reque(pjob,CHANGE_STATE);

      break;

    case JOB_SUBSTATE_RUNNING:

      pbsd_init_reque(pjob,KEEP_STATE);

      pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_RescAssn;

      set_resc_assigned(pjob,INCR);

      /* suspended jobs don't get reassigned to nodes */
      if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_Suspend) == 0)
        {
        set_old_nodes(pjob);
        }

      if (type == RECOV_HOT)
        pjob->ji_qs.ji_svrflags |= JOB_SVFLG_HOTSTART;

      break;
			
    case JOB_SUBSTATE_SYNCRES:

      /* clear all dependent job ready flags */

      depend_clrrdy(pjob);

      pbsd_init_reque(pjob,CHANGE_STATE);

      break;

    case JOB_SUBSTATE_EXITING:
    case JOB_SUBSTATE_STAGEOUT:
    case JOB_SUBSTATE_STAGEDEL:
    case JOB_SUBSTATE_EXITED:
    case JOB_SUBSTATE_ABORT:

      /* This is delayed because it is highly likely MS is "state-unknown"
       * at this time, and there's no real hurry anyways. */

      apply_job_delete_nanny(pjob,time_now + 60);

      pwt = set_task(WORK_Immed,0,on_job_exit,(void *)pjob);

      if (pwt)
        {
        append_link(&pjob->ji_svrtask,&pwt->wt_linkobj,pwt);
        }

      pbsd_init_reque(pjob,KEEP_STATE);

      break;

    case JOB_SUBSTATE_COMPLETE:

      /* NOOP - completed jobs are already purged above */
      /* for some reason, this doesn't actually work */

      pwt = set_task(WORK_Immed,0,on_job_exit,(void *)pjob);

      if (pwt)
        {
        append_link(&pjob->ji_svrtask,&pwt->wt_linkobj,pwt);
        }

      pbsd_init_reque(pjob,KEEP_STATE);

      break;

    case JOB_SUBSTATE_RERUN:

      if (pjob->ji_qs.ji_state == JOB_STATE_EXITING)
        {
        pwt = set_task(WORK_Immed,0,on_job_rerun,(void *)pjob);

        if (pwt)
          {
          append_link(&pjob->ji_svrtask,&pwt->wt_linkobj,pwt);
          }
        }

      pbsd_init_reque(pjob,KEEP_STATE);

      break;

    case JOB_SUBSTATE_RERUN1:
    case JOB_SUBSTATE_RERUN2:

      pwt = set_task(WORK_Immed,0,on_job_rerun,(void *)pjob);

      if (pwt)
        {
        append_link(&pjob->ji_svrtask,&pwt->wt_linkobj,pwt);
        }

      pbsd_init_reque(pjob,KEEP_STATE);

      break;

    default:

      sprintf(log_buffer,msg_init_unkstate, 
        pjob->ji_qs.ji_substate);

      log_event(
        PBSEVENT_ERROR, 
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid, 
        log_buffer);

      job_abt(&pjob,log_buffer);  /* pjob is not freed */

      if (pjob == NULL)
        {
        return(FAILURE);
        }

      break;
    }    /* END switch (pjob->ji_qs.ji_substate) */

  /* if job has IP address of Mom, it may have changed */
  /* reset based on hostname                           */
 
  if ((pjob->ji_qs.ji_un_type == JOB_UNION_TYPE_EXEC) &&
      (pjob->ji_qs.ji_un.ji_exect.ji_momaddr != 0)) 
    {
    if (pjob->ji_wattr[(int)JOB_ATR_exec_host].at_flags & ATR_VFLAG_SET) 
      { 
      pjob->ji_qs.ji_un.ji_exect.ji_momaddr = get_hostaddr(
        parse_servername(pjob->ji_wattr[(int)JOB_ATR_exec_host].at_val.at_str,&d));
      } 
    else 
      {
      pjob->ji_qs.ji_un.ji_exect.ji_momaddr = 0;
      }
    }

  return(SUCCESS);
  }  /* END pbsd_init_job() */




			
static void pbsd_init_reque(

  job *pjob,         /* I (modified/possibly freed) */
  int  change_state) /* I */

  {
  char logbuf[265];
  int newstate;
  int newsubstate;

  sprintf(logbuf,msg_init_substate,
    pjob->ji_qs.ji_substate);

  /* re-enqueue the job into the queue it was in */

  if (change_state) 
    {
    /* update the state, typically to some form of QUEUED */

    svr_evaljobstate(pjob,&newstate,&newsubstate,0);

    svr_setjobstate(pjob,newstate,newsubstate);
    } 
  else 
    {
    set_statechar(pjob);
    }

  if (svr_enquejob(pjob) == 0) 
    {
    strcat(logbuf, msg_init_queued);
    strcat(logbuf, pjob->ji_qs.ji_queue);

    log_event(
      PBSEVENT_SYSTEM | PBSEVENT_ADMIN | PBSEVENT_DEBUG,
      PBS_EVENTCLASS_JOB, 
      pjob->ji_qs.ji_jobid, 
      logbuf);
    } 
  else 
    {
    /* Oops, this should never happen */

    sprintf(logbuf,"%s; job %s queue %s", 
      msg_err_noqueue,
      pjob->ji_qs.ji_jobid, 
      pjob->ji_qs.ji_queue);

    log_err(-1,"pbsd_init_reque",logbuf);

    job_abt(&pjob,logbuf);

    /* NOTE:  pjob freed but dangling pointer remains */
    }

  return;
  }  /* END pbsd_init_reque() */




/*
 * Catch core dump signals - set core size so we can see what happened!
 *
 * Chris Samuel - VPAC
 * csamuel@vpac.org - 29th July 2003
 */

static void catch_abort(

  int sig)  /* I */

  {
  struct rlimit rlimit;
  struct sigaction act;

  /*
   * Reset ourselves to the default signal handler to try and
   * prevent recursive core dumps.
   */

  sigemptyset(&act.sa_mask);
  act.sa_flags   = 0;
  act.sa_handler = SIG_DFL;

  sigaction(SIGSEGV,&act,NULL);
  sigaction(SIGBUS,&act,NULL);
  sigaction(SIGFPE,&act,NULL);
  sigaction(SIGILL,&act,NULL);
  sigaction(SIGTRAP,&act,NULL);
  sigaction(SIGSYS,&act,NULL);

  log_err(sig,"mom_main","Caught fatal core signal");

  rlimit.rlim_cur = RLIM_INFINITY;
  rlimit.rlim_max = RLIM_INFINITY;

  setrlimit(RLIMIT_CORE,&rlimit);
  abort();

  return;
  }  /* END catch_abort() */





/*
 * catch_child() - the signal handler for  SIGCHLD.
 *
 * Collect child status and add to work list entry for that child.
 * The list entry is marked as immediate to show the child is gone and
 * svr_delay_entry is incremented to indicate to next_task() to check for it.
 */

static void catch_child(

  int sig)

  {
  struct work_task *ptask;
  pid_t		  pid;
  int		  statloc;
	
  while (1) 
    {
    if (((pid = waitpid(-1,&statloc,WNOHANG)) == -1) &&
        (errno != EINTR)) 
      {
      return;
      } 

    if (pid == 0) 
      {
      return;
      }

    ptask = (struct work_task *)GET_NEXT(task_list_event);

    while (ptask) 
      {
      if ((ptask->wt_type == WORK_Deferred_Child) &&
          (ptask->wt_event == pid)) 
        {
        ptask->wt_type = WORK_Deferred_Cmp;
        ptask->wt_aux = (int)statloc;	/* exit status */

        svr_delay_entry++;	/* see next_task() */
        }

      ptask = (struct work_task *)GET_NEXT(ptask->wt_linkall);
      }
    }

  return;
  }  /* END catch_child() */




/*
 * changs_logs - signal handler for SIGHUP
 *	Causes the accounting file and log file to be closed and reopened.
 *	Thus the old one can be renamed.
 */

static void change_logs(

  int sig)

  {
  acct_close();
  log_close(1);

  log_open(log_file, path_log);

  acct_open(acct_file);

  rpp_dbprt = 1 - rpp_dbprt;	/* toggle debug prints for RPP */

  return;
  }




/* 
 * stop_me - signal handler for all caught signals which terminate the server
 *
 *	Record the signal so an log_event call can be made outside of
 *	the handler, and set the server state to indicate we should shut down.
 */




/*ARGSUSED*/

static void stop_me(

  int sig)

  {
  server.sv_attr[(int)SRV_ATR_State].at_val.at_long = SV_STATE_SHUTSIG;

  return;
  }



static int chk_save_file(

  char *filename)

  {
  struct stat sb;

  if (*filename == '.')
    {
    return(-1);
    }

  if (stat(filename,&sb) == -1)
    {
    return(errno);
    }

  if (S_ISREG(sb.st_mode))
    {
    return(0);
    }

  return(-1);
  }





/*
 * resume_net_move - call net_move() to complete the routing of a job
 *	This is invoked via a work task created on recovery of a job
 *	in JOB_SUBSTATE_TRNOUTCM state.
 */

static void resume_net_move(

  struct work_task *ptask)

  {
  net_move((job *)ptask->wt_parm1,0);

  return;
  }




/*
 * need_y_response - on create/clean initialization that would delete
 *	information, obtain the operator approval first.
 */

static void need_y_response(

  int type)  /* I */

  {
  static int answ = -2;
  int c;

  if (answ > 0)
    {
    return;		/* already received a response */
    }

  fflush(stdin);

  if (type == RECOV_CREATE)
    printf(msg_startup3,msg_daemonname,server_name,"Create","server database");
  else
    printf(msg_startup3,msg_daemonname,server_name,"Cold","jobs");
  
  while (1) 
    {
    answ = getchar();

    c = answ;

    while ((c != '\n') && (c != EOF))
      c = getchar();

    switch (answ) 
      {
      case 'y':
      case 'Y':

        return;
	
        /*NOTREACHED*/

        break;

      case  EOF:
      case '\n':
      case 'n':
      case 'N':

        printf("PBS server %s initialization aborted\n", 
          server_name);

        exit(0);

        /*NOTREACHED*/

        break;
      }

    printf("y(es) or n(o) please:\n");
    }

  return;
  }  /* END need_y_response() */





/*
 * rm_files - on an RECOV_CREATE, remove all files under the specified
 *	directory (path_priv) and any subdirectory except under "jobs".
 */

static void rm_files(

  char *dirname)

  {
  DIR *dir;
  int  i;
  struct stat    stb;
  struct dirent *pdirt;
  char path[1024];

  /* list of directories in which files are removed */

  static char *byebye[] = {
    "acl_groups",
    "acl_hosts",
    "acl_svr",
    "acl_users",
    "hostlist",
    "queues",
    NULL };      /* keep as last entry */

  dir = opendir(dirname);

  if (dir != NULL) 
    {
    while ((pdirt = readdir(dir)) != NULL) 
      {
      strcpy(path,dirname);
      strcat(path,"/");
      strcat(path,pdirt->d_name);

      if (stat(path,&stb) == 0) 
        {
        if (S_ISDIR(stb.st_mode)) 
          {
          for (i = 0;byebye[i];++i) 
            {
            if (strcmp(pdirt->d_name,byebye[i]) == 0) 
              {
              rm_files(path);
              }
            }
          } 
        else if (unlink(path) == -1) 
          {
          strcpy(log_buffer,"cannot unlink");
          strcat(log_buffer,path);

          log_err(errno,"pbsd_init",log_buffer);
          }
        }
      }
    }

  return;
  }  /* END rm_files() */





/*
 * init_abt_job() - log and email owner message that job is being aborted at
 *	initialization; then purge job (must be called after job is enqueued.
 */

static void init_abt_job(

  job *pjob)

  {
  log_event(
    PBSEVENT_SYSTEM|PBSEVENT_ADMIN|PBSEVENT_DEBUG,
    PBS_EVENTCLASS_JOB, 
    pjob->ji_qs.ji_jobid, 
    msg_init_abt);

  svr_mailowner(pjob,MAIL_ABORT,MAIL_NORMAL,msg_init_abt);

  job_purge(pjob);

  return;
  }
