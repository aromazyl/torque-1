
/* Copyright 2008 Cluster Resources */

/**
 * checkpoint.c
 *
 * Support for job checkpoint and restart.
 *
 * Note that there are two coexistent systems for implementing
 * checkpoint and restart.  The older one is based on machine
 * dependent code in the mom_mach.c module.  The newer one uses
 * the BLCR system modules.  The working of the code below is
 * based on the variable checkpoint_system_type which can have
 * the values CST_NONE, CST_MACH_DEP or CST_BLCR.  This variable
 * is set by calling mom_does_checkpoint at system startup time.
 *
 * The CST_MACH_DEP system uses a directory based scheme of
 * taking a checkpoint and produces a file for each task.  The
 * restart code iterates over the directory and does a restart
 * task for each file it finds.  The entire set of files must
 * be written for the checkpoint to be valid.  To ensure this,
 * an existing checkpoint directory is renamed with a .old
 * extension while writing a new checkpoint.  If something
 * fails during this process, recovery code will notice the
 * .old directory and deduce that the existing directory is
 * invalid and should be deleted at the same time renaming
 * the backup to the current.
 *
 * In the case of the CST_BLCR system, BLCR is MPI aware and
 * takes care of doing the checkpoints for each task.
 * By the way, a task as defined in Torque is a process group
 * executing on a node.
 * So the checkpoint and restore for BLCR is done on the head
 * node task.  A directory can be specified where the checkpoint
 * image will reside but there is only one file per checkpoint.
 * BLCR also differs in that multiple checkpoint images are
 * allowed to exist on the disk and when the restart is performed,
 * the name in the job is used to decide which one of these is
 * to be used.  This name may be altered to a previous image by
 * the operators use of the qalter command.
 *
 */


#include <pbs_config.h>   /* the master config generated by configure */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>



#include "dis.h"
#include "libpbs.h"
#include "pbs_error.h"
#include "pbs_job.h"
#include "batch_request.h"
#include "attribute.h"
#include "resmon.h"
#include "mom_mach.h"
#include "mom_func.h"
#include "log.h"
#include "mcom.h"
#include "net_connect.h"
#include "resource.h"
#include "csv.h"
#include "svrfunc.h"

extern int exiting_tasks;
extern int LOGLEVEL;
extern     int             lockfds;

extern int task_recov(job *pjob);
extern char *path_spool;

int        checkpoint_system_type = CST_NONE;
char    path_checkpoint[1024];

/* BLCR variables */
char       checkpoint_script_name[1024];
char       restart_script_name[1024];
char       checkpoint_run_exe_name[1024];
int        default_checkpoint_interval = 10; /* minutes */

extern char *mk_dirs A_((char *));

int create_missing_files(job *pjob);

/* The following is used for building command line args
 * and makes sure that at least something is generated
 * for each arg so that the script gets a consistent
 * command line.
 */
#define SET_ARG(x) (((x) == NULL) || (*(x) == 0))?"-":(x)


/**
 * mom_checkpoint_job_is_checkpointable
 *
 * @param pjob Pointer to job structure.
 * @see TMomFinalizeChild
 */


int
mom_checkpoint_job_is_checkpointable(job *pjob)
  {
  attribute    *pattr;
  int           rc;

  pattr = &pjob->ji_wattr[(int)JOB_ATR_checkpoint];

  rc = checkpoint_system_type != CST_NONE &&
       checkpoint_script_name[0] != 0 &&
       (pattr->at_flags & ATR_VFLAG_SET) &&
       ((csv_find_string(pattr->at_val.at_str, "c") != NULL) ||
        (csv_find_string(pattr->at_val.at_str, "s") != NULL) ||
        (csv_find_string(pattr->at_val.at_str, "enabled") != NULL) ||
        (csv_find_string(pattr->at_val.at_str, "shutdown") != NULL) ||
        (csv_find_string(pattr->at_val.at_str, "periodic") != NULL));

  return(rc);
  }


/**
 * mom_checkpoint_execute_job
 *
 * This routine is called from the newly created child process.
 * It is required for the BLCR system because the job must run as
 * a child of the cr_run program.
 *
 * @param pjob Pointer to job structure.
 * @see TMomFinalizeChild
 */
void
mom_checkpoint_execute_job(job *pjob, char *shell, char *arg[], struct var_table *vtable)
  {
  static char          *id = "mom_checkpoint_execute_job";

  /* Launch job executable with cr_run command so that cr_checkpoint command will work. */

  /* shuffle up the existing args */
  arg[5] = arg[4];
  arg[4] = arg[3];
  arg[3] = arg[2];
  arg[2] = arg[1];
  /* replace first arg with shell name
     note, this func is called from a child process that exits after the
     executable is launched, so we don't have to worry about freeing
     this malloc later */
  arg[1] = malloc(strlen(shell) + 1);
  strcpy(arg[1], shell);
  arg[0] = checkpoint_run_exe_name;

  if (LOGLEVEL >= 10)
    {
    char cmd[1024];
    int i;

    strcpy(cmd,arg[0]);
    for (i = 1; arg[i] != NULL; i++)
      {
      strcat(cmd," ");
      strcat(cmd,arg[i]);
      }
    strcat(cmd,")");

    log_buffer[0] = '\0';
    sprintf(log_buffer, "execing checkpoint command (%s)\n", cmd);
    log_err(-1, id, log_buffer);
    }

  execve(checkpoint_run_exe_name, arg, vtable->v_envp);
  }


/**
 * mom_checkpoint_init
 *
 * This routine is called from the mom startup code.
 * @see setup_program_environment
 */
int
mom_checkpoint_init(void)
  {
  int c = 0;

  checkpoint_system_type = mom_does_checkpoint(); /* {CST_NONE, CST_MACH_DEP, CST_BLCR} */

  if (strlen(path_checkpoint) == 0) /* if not -C option */
    strcpy(path_checkpoint, mk_dirs("checkpoint/"));


#if !defined(DEBUG) && !defined(NO_SECURITY_CHECK)

  c = chk_file_sec(path_checkpoint, 1, 0, S_IWGRP | S_IWOTH, 1, NULL);

#endif  /* not DEBUG and not NO_SECURITY_CHECK */
  return(c);
  }



/*========================================================================*/
/* Routines called from the config file parsing to set various variables. */
/*========================================================================*/


void
mom_checkpoint_set_directory_path(char *str)
  {
  char *cp;

  strcpy(path_checkpoint, str);
  cp = &path_checkpoint[strlen(path_checkpoint)];

  if (*cp != '/')
    {
    *cp++ = '/';
    *cp++ = 0;
    }
  }


unsigned long
mom_checkpoint_set_checkpoint_interval(char *value)  /* I */

  {
  log_record(
    PBSEVENT_SYSTEM,
    PBS_EVENTCLASS_SERVER,
    "checkpoint_interval",
    value);

  default_checkpoint_interval = atoi(value);

  return(1);
  }  /* END set_checkpoint_script() */





unsigned long
mom_checkpoint_set_checkpoint_script(char *value)  /* I */

  {

  struct stat sbuf;

  log_record(
    PBSEVENT_SYSTEM,
    PBS_EVENTCLASS_SERVER,
    "checkpoint_script",
    value);

  if ((stat(value, &sbuf) == -1) || !(sbuf.st_mode & S_IXUSR))
    {
    /* file does not exist or is not executable */

    return(0);  /* error */
    }

  strncpy(checkpoint_script_name, value, sizeof(checkpoint_script_name));

  return(1);
  }  /* END set_checkpoint_script() */





unsigned long
mom_checkpoint_set_restart_script(char *value)  /* I */

  {

  struct stat sbuf;

  log_record(
    PBSEVENT_SYSTEM,
    PBS_EVENTCLASS_SERVER,
    "restart_script",
    value);

  if ((stat(value, &sbuf) == -1) || !(sbuf.st_mode & S_IXUSR))
    {
    /* file does not exist or is not executable */

    return(0);  /* error */
    }

  strncpy(restart_script_name, value, sizeof(restart_script_name));

  return(1);
  }  /* END set_restart_script() */





unsigned long
mom_checkpoint_set_checkpoint_run_exe_name(char *value)  /* I */

  {

  struct stat sbuf;

  log_record(
    PBSEVENT_SYSTEM,
    PBS_EVENTCLASS_SERVER,
    "checkpoint_run_exe",
    value);

  if ((stat(value, &sbuf) == -1) || !(sbuf.st_mode & S_IXUSR))
    {
    /* file does not exist or is not executable */

    return(0);  /* error */
    }

  strncpy(checkpoint_run_exe_name, value, sizeof(checkpoint_run_exe_name));

  return(1);
  }  /* END set_checkpoint_run_exe() */






/**
 * mom_checkpoint_delete_files
 *
 * This routine is called from the job_purge routine
 * which cleans up all files related to a job.
 *
 * @param pjob Pointer to the job structure
 * @see job_purge
 */

void mom_checkpoint_delete_files(

  job *pjob)

  {
  char namebuf[256];

  if (checkpoint_system_type == CST_MACH_DEP)
    {
    /* delete any checkpoint file */

    strcpy(namebuf, path_checkpoint);
    strcat(namebuf, pjob->ji_qs.ji_fileprefix);
    strcat(namebuf, JOB_CHECKPOINT_SUFFIX);
    remtree(namebuf);
    }

  return;
  }





/**
 * mom_checkpoint_recover
 *
 * This routine is called from init_abort_jobs which in turn is called
 * on mom startup.  The purpose is to recover jobs listed in the mom_priv/jobs
 * directory.
 *
 * This routine does not actually start the job.  This happens in start_exec.c.
 * It's purpose is to remove a partially completed checkpoint directory,
 * signified by the name suffix of ".old".
 *
 * @param pjob Pointer to job data structure
 * @see init_abort_jobs
 */

void mom_checkpoint_recover(

  job *pjob)

  {
  char           path[MAXPATHLEN + 1];
  char           oldp[MAXPATHLEN + 1];

  struct stat statbuf;


  if (checkpoint_system_type == CST_MACH_DEP)
    {
    /*
    ** Check to see if a checkpoint.old dir exists.
    ** If so, remove the regular checkpoint dir
    ** and rename the old to the regular name.
    */

    strcpy(path, path_checkpoint);
    strcat(path, pjob->ji_qs.ji_fileprefix);
    strcat(path, JOB_CHECKPOINT_SUFFIX);
    strcpy(oldp, path);
    strcat(oldp, ".old");

    if (stat(oldp, &statbuf) == 0)
      {
      remtree(path);

      if (rename(oldp, path) == -1)
        remtree(oldp);
      }
    }

  return;
  }





/**
 * mom_checkpoint_check_periodic_timer
 *
 * This routine is called from the main loop routine examine_all_running_jobs.
 * Each job that is checkpointable will have timer variables set up.
 * This routine checks the timer variables and if set and it is time
 * to do a checkpoint, fires the code that starts a checkpoint.
 *
 * @param pjob Pointer to the job structure
 * @see examine_all_running_jobs
 * @see main_loop
 */

void mom_checkpoint_check_periodic_timer(

  job *pjob)

  {
  resource *prwall;
  extern int start_checkpoint();
  int rc;
  static resource_def *rdwall;

  /* see if need to checkpoint any job */

  if (pjob->ji_checkpoint_time != 0)  /* ji_checkpoint_time gets set below */
    {
    if (rdwall == NULL)
      {
      rdwall = find_resc_def(svr_resc_def, "walltime", svr_resc_size);
      }

    if (rdwall != NULL)
      {
      prwall = find_resc_entry(
                 &pjob->ji_wattr[(int)JOB_ATR_resc_used],
                 rdwall);  /* resource definition cput set in startup */

      if (prwall &&
          (prwall->rs_value.at_val.at_long >= pjob->ji_checkpoint_next))
        {
        pjob->ji_checkpoint_next =
          prwall->rs_value.at_val.at_long +
          pjob->ji_checkpoint_time;

        if ((rc = start_checkpoint(pjob, 0, 0)) != PBSE_NONE)
          {
          sprintf(log_buffer, "Checkpoint failed, error %d", rc);

          message_job(pjob, StdErr, log_buffer);

          log_record(
            PBSEVENT_JOB,
            PBS_EVENTCLASS_JOB,
            pjob->ji_qs.ji_jobid,
            log_buffer);
          }
        }
      }
    }

  return;
  }  /* END mom_checkpoint_check_periodic_timer() */





/**
 * blcr_checkpoint_job
 *
 * This routine lauches the checkpoint script for a BLCR
 * checkpoint system. 
 * currently only supports single process job, so a BLCR job will
 * only have one task associated with the job. 
 *
 * @see start_checkpoint() - parent
 *
 * @returns PBSE_NONE if no error
 */

int blcr_checkpoint_job(

  job *pjob,  /* I */
  int  abort) /* I */

  {
  char *id = "blcr_checkpoint_job";

#if 0
  int   pid;
#endif
  char  sid[20];
  char  *arg[20];
  char  buf[1024];
  char  **ap;
  int rc;
  int cmd_len;
  char *cmd;

  assert(pjob != NULL);
  assert(pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_val.at_str != NULL);

  /* Make sure that the specified directory exists. */

  if (mkdir(pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_val.at_str, 0755)
      == 0)
    {
    /* Change the owner of the checkpint directory to be the user */
    if (chown(pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_val.at_str,
          pjob->ji_qs.ji_un.ji_momt.ji_exuid,
          pjob->ji_qs.ji_un.ji_momt.ji_exgid) == -1)
      {
      log_err(errno, id, "cannot change checkpoint directory owner");
      }
    }
  
  /* if a checkpoint script is defined launch it */

  if (checkpoint_script_name[0] == '\0')
    {
    log_err(PBSE_RMEXIST, id, "No checkpoint script defined");

    return(PBSE_RMEXIST);
    }

  /* launch the script and return success */
#if 0
  pid = fork();

  if (pid < 0)
    {
    /* fork failed */

    return(PBSE_RMSYSTEM);
    }

  if (pid > 0)
    {
    /* parent: pid = child's pid */
#endif
    /* Checkpoint successful (assumed) */

    pjob->ji_qs.ji_svrflags |= JOB_SVFLG_CHECKPOINT_FILE;

    job_save(pjob,SAVEJOB_FULL); /* to save resources_used so far */

    sprintf(log_buffer,"checkpointed to %s / %s",
      pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_val.at_str,
      pjob->ji_wattr[(int)JOB_ATR_checkpoint_name].at_val.at_str);

    log_record(
      PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);

    if (abort == 1)
      {
      /* post_checkpoint() will verify checkpoint successfully completed and only purge 
         at that time */
      }
#if 0
    }
  else if (pid == 0)
    {
#endif
    /* child: execv the script */

    sprintf(sid,"%ld",
      pjob->ji_wattr[(int)JOB_ATR_session_id].at_val.at_long);

    arg[0] = checkpoint_script_name;
    arg[1] = sid;
    arg[2] = SET_ARG(pjob->ji_qs.ji_jobid);
    arg[3] = SET_ARG(pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str);
    arg[4] = SET_ARG(pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_val.at_str);
    arg[5] = SET_ARG(pjob->ji_wattr[(int)JOB_ATR_checkpoint_name].at_val.at_str);
    arg[6] = (abort) ? "15" /*abort*/ : "0" /*run/continue*/;
    arg[7] = SET_ARG(csv_find_value(pjob->ji_wattr[(int)JOB_ATR_checkpoint].at_val.at_str, "depth"));
    arg[8] = NULL;

    strcpy(buf,"checkpoint args:");

    cmd_len = 0;
    for (ap = arg;*ap;ap++)
      {
      strcat(buf, " ");
      strcat(buf, *ap);
      cmd_len += strlen(*ap);
      }
      
    cmd_len += 7;
    cmd = malloc(cmd_len * sizeof(char));
    sprintf(cmd, "%s %s %s %s %s %s %s %s", arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6],
    arg[7]);


    log_err(-1, id, buf);

    rc = system(cmd);
    free(cmd);
#if 0
    }  /* END if (pid == 0) */
#endif
  return(WEXITSTATUS(rc));
  }  /* END blcr_checkpoint_job() */





/*
 * Checkpoint the job.
 *
 * If abort is TRUE, kill it too.  Return a PBS error code.
 */

int mom_checkpoint_job(

  job *pjob,  /* I */
  int  abort) /* I */

  {
  int  hasold = 0;
  int  sesid = -1;
  int  ckerr;

  struct stat statbuf;
  char  path[MAXPATHLEN + 1];
  char  oldp[MAXPATHLEN + 1];
  char  file[MAXPATHLEN + 1];
  int  filelen;
  task         *ptask;

  assert(pjob != NULL);

  strcpy(path, path_checkpoint);
  strcat(path, pjob->ji_qs.ji_fileprefix);
  strcat(path, JOB_CHECKPOINT_SUFFIX);

  if (stat(path, &statbuf) == 0)
    {
    strcpy(oldp, path);  /* file already exists, rename it */

    strcat(oldp, ".old");

    if (rename(path, oldp) < 0)
      {
      return(errno);
      }

    hasold = 1;
    }

  mkdir(path, 0755);

  filelen = strlen(path);
  strcpy(file, path);

#ifdef _CRAY

  /*
   * if job is suspended and if <abort> is set, resume job first,
   * this is so job will be "Q"ueued and then back into "R"unning
   * when restarted.
   */

  if ((pjob->ji_qs.ji_svrflags & JOB_SVFLG_Suspend) && abort)
    {
    for (ptask = (task *)GET_NEXT(pjob->ji_tasks);
         ptask != NULL;
         ptask = (task *)GET_NEXT(ptask->ti_jobtask))
      {
      sesid = ptask->ti_qs.ti_sid;

      if (ptask->ti_qs.ti_status != TI_STATE_RUNNING)
        continue;

      /* XXX: What to do if some resume work and others don't? */

      if ((ckerr = resume(C_JOB, sesid)) == 0)
        {
        post_resume(pjob, ckerr);
        }
      else
        {
        sprintf(log_buffer, "checkpoint failed: errno=%d sid=%d",
                errno,
                sesid);

        LOG_EVENT(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          pjob->ji_qs.ji_jobid,
          log_buffer);

        return(errno);
        }
      }
    }

#endif /* _CRAY */

  for (ptask = (task *)GET_NEXT(pjob->ji_tasks);
       ptask != NULL;
       ptask = (task *)GET_NEXT(ptask->ti_jobtask))
    {
    sesid = ptask->ti_qs.ti_sid;

    if (ptask->ti_qs.ti_status != TI_STATE_RUNNING)
      continue;

    if (mach_checkpoint(ptask, file, abort) == -1)
      goto fail;
    }

  /* Checkpoint successful */

  pjob->ji_qs.ji_svrflags |= JOB_SVFLG_CHECKPOINT_FILE;

  job_save(pjob, SAVEJOB_FULL); /* to save resources_used so far */

  sprintf(log_buffer, "checkpointed to %s",
          path);

  log_record(
    PBSEVENT_JOB,
    PBS_EVENTCLASS_JOB,
    pjob->ji_qs.ji_jobid,
    log_buffer);

  if (hasold)
    remtree(oldp);

  return(PBSE_NONE);

fail:

  /* A checkpoint has failed.  Log and return error. */

  ckerr = errno;

  sprintf(log_buffer, "checkpoint failed:errno=%d sid=%d",
          errno,
          sesid);

  LOG_EVENT(
    PBSEVENT_JOB,
    PBS_EVENTCLASS_JOB,
    pjob->ji_qs.ji_jobid,
    log_buffer);

  /*
  ** See if any checkpoints worked and abort is set.
  ** If so, we need to restart these tasks so the whole job is
  ** still running.  This has to wait until we reap the
  ** aborted task(s).
  */

  if (abort)
    {
    return(PBSE_CKPSHORT);
    }

  /* Clean up files */
  remtree(path);

  if (hasold)
    {
    if (rename(oldp, path) == -1)
      pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_CHECKPOINT_FILE;
    }

  if (ckerr == EAGAIN)
    {
    return(PBSE_CKPBSY);
    }

  return(ckerr);
  }  /* END mom_checkpoint_job() */





/*
 * post_checkpoint - post processor for start_checkpoint()
 *
 * @see scan_for_terminated() - parent
 *
 * Called from scan_for_terminated() when found in ji_mompost;
 *
 * This sets the "has checkpoint image" bit in the job.
 *
 * job is referenced by parent after calling this routine - do not 'purge' 
 * job from inside this routine
 */

void post_checkpoint(

  job *pjob,  /* I - may be purged */
  int  ev)    /* I */

  {
  char           path[MAXPATHLEN + 1];
  DIR           *dir;

  struct dirent *pdir;
  tm_task_id     tid;
  task          *ptask;

  int            abort = pjob->ji_flags & MOM_CHECKPOINT_ACTIVE;

  exiting_tasks = 1; /* make sure we call scan_for_exiting() */

  pjob->ji_flags &= ~MOM_CHECKPOINT_ACTIVE;

  if (ev == 0)
    {
    pjob->ji_qs.ji_svrflags |= JOB_SVFLG_CHECKPOINT_FILE;

    return;
    }

  /*
  ** If we get here, an error happened.  Only try to recover
  ** if we had abort set.
  */

  if (abort == 0)
    {
    return;
    }

  /*
  ** Set a flag for scan_for_exiting() to be able to
  ** deal with a failed checkpoint rather than doing
  ** the usual processing.
  */

  pjob->ji_flags |= MOM_CHECKPOINT_POST;

  /*
  ** Set the TI_FLAGS_CHECKPOINT flag for each task that
  ** was checkpointed and aborted.
  */

  strcpy(path,path_checkpoint);

  strcat(path,pjob->ji_qs.ji_fileprefix);

  strcat(path,JOB_CHECKPOINT_SUFFIX);

  dir = opendir(path);

  if (dir == NULL)
    {
    return;
    }

  while ((pdir = readdir(dir)) != NULL)
    {
    if (pdir->d_name[0] == '.')
      continue;

    tid = atoi(pdir->d_name);

    if (tid == 0)
      continue;

    ptask = task_find(pjob,tid);

    if (ptask == NULL)
      continue;

    ptask->ti_flags |= TI_FLAGS_CHECKPOINT;
    }  /* END while ((pdir = readdir(dir)) != NULL) */

  closedir(dir);

  return;
  }  /* END post_checkpoint() */





/**
 * start_checkpoint - start a checkpoint going
 *
 * checkpoint done from a child because it takes a while
 *
 * @see blcr_checkpoint() - child
 * @see start_checkpoint() - parent
 */

int start_checkpoint(

  job                  *pjob,
  int                   abort, /* I - boolean - 0 or 1 */
  struct batch_request *preq)  /* may be null */

  {

  pid_t     pid;
  char *id = "start_checkpoint";
  int       rc = PBSE_NONE;
  char      name_buffer[1024];

  switch (checkpoint_system_type)

    {
    case CST_MACH_DEP:

      /* NO-OP */

      break;

    case CST_BLCR:

      /* Build the name of the checkpoint file before forking to the child because
       * we want this name to persist and this won't work if we are the child.
       * Notice that the ATR_VFLAG_SEND bit causes this to also go to the pbs_server.
       */

      sprintf(name_buffer,"ckpt.%s.%d", 
        pjob->ji_qs.ji_jobid, 
        (int)time(0));

      decode_str(&pjob->ji_wattr[(int)JOB_ATR_checkpoint_name], NULL, NULL, name_buffer);

      pjob->ji_wattr[(int)JOB_ATR_checkpoint_name].at_flags =
        ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_SEND;

      /* For BLCR, there must be a directory name in the job attributes. */

      if (!(pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_flags & ATR_VFLAG_SET))
        {
        /* No dir specified, use the default job checkpoint directory 
           e.g.  /var/spool/torque/checkpoint/42.host.domain.CK */

        strcpy(name_buffer,path_checkpoint);
        strcat(name_buffer,pjob->ji_qs.ji_fileprefix);
        strcat(name_buffer,JOB_CHECKPOINT_SUFFIX);

        decode_str(&pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir],NULL,NULL,name_buffer);
        }

      break;

    case CST_NONE:

    default:

      return(PBSE_NOSUP);  /* no checkpoint, reject request */

      /*NOTREACHED*/

      break;
    }

  /* now set up as child of MOM */
  pid = fork_me((preq == NULL) ? -1 : preq->rq_conn);

  if (pid > 0)
    {
    /* parent */

    /* MOM_CHECKPOINT_ACTIVE prevents scan_for_exiting from triggering obits while job is checkpointing. */

    pjob->ji_flags |= MOM_CHECKPOINT_ACTIVE;
    pjob->ji_momsubt = pid; /* record pid in job for when child terminates */

    /* Set the address of a function to execute in scan_for_terminated */

    pjob->ji_mompost = (int (*)())post_checkpoint; 

    if (preq)
      free_br(preq); /* child will send reply */

    }
  else if (pid < 0)
    {
    /* error on fork */

    log_err(errno, id, "cannot fork child process for checkpoint");

    return(PBSE_SYSTEM);
    }
  else
    {
    /* child - does the checkpoint */

    switch (checkpoint_system_type)

      {
      case CST_MACH_DEP:

        rc = mom_checkpoint_job(pjob,abort);

        break;

      case CST_BLCR:

        rc = blcr_checkpoint_job(pjob,abort);

        break;
      }

    if (rc == PBSE_NONE)
      {
      rc = site_mom_postchk(pjob,abort); /* Normally, this is an empty routine and does nothing. */
      }

    if (preq != NULL)
      {
      /* rc may be 0, req_reject is used to pass auxcode */

      req_reject(rc,PBS_CHECKPOINT_MIGRATE,preq,NULL,NULL); /* BAD reject is used to send OK??? */
      }


    exit(rc); /* zero exit tells main checkpoint ok */
    }

  return(PBSE_NONE);  /* parent return */
  }  /* END start_checkpoint() */





/*
 * replace a given file descriptor with the new file path
 *
 * This routine exits on error!  Only used by the BLCR restart code, and
 * there's really no good way to recover from an error in restart.
 */
static int fdreopen(const char *path, const char mode, int fd)
  {
  int newfd, dupfd;

  close(fd);

  newfd = open("/dev/null", O_RDONLY);

  if (newfd < 0)
    {
    perror("open");
    exit(-1);
    }

  dupfd = dup2(newfd, fd);

  if (newfd < 0)
    {
    perror("dup2");
    exit(-1);
    }

  close(newfd);

  return dupfd;
  }


/*
** Restart each task which has exited and has TI_FLAGS_CHECKPOINT turned on.
** If all tasks have been restarted, turn off MOM_CHECKPOINT_POST.
*/

void checkpoint_partial(

  job *pjob)

  {
  static char id[] = "checkpoint_partial";
  char  namebuf[MAXPATHLEN];
  task  *ptask;
  int  texit = 0;

  assert(pjob != NULL);

  strcpy(namebuf, path_checkpoint);
  strcat(namebuf, pjob->ji_qs.ji_fileprefix);
  strcat(namebuf, JOB_CHECKPOINT_SUFFIX);

  for (ptask = (task *)GET_NEXT(pjob->ji_tasks);
       ptask != NULL;
       ptask = (task *)GET_NEXT(ptask->ti_jobtask))
    {
    /*
    ** See if the task was marked as one of those that did
    ** actually checkpoint.
    */

    if ((ptask->ti_flags & TI_FLAGS_CHECKPOINT) == 0)
      continue;

    texit++;

    /*
    ** Now see if it was reaped.  We don't want to
    ** fool with it until we see it die.
    */

    if (ptask->ti_qs.ti_status != TI_STATE_EXITED)
      continue;

    texit--;

    if (mach_restart(ptask, namebuf) == -1)
      {
      pjob->ji_flags &= ~MOM_CHECKPOINT_POST;
      kill_job(pjob, SIGKILL, id, "failed to restart");
      return;
      }

    ptask->ti_qs.ti_status = TI_STATE_RUNNING;

    ptask->ti_flags &= ~TI_FLAGS_CHECKPOINT;

    task_save(ptask);
    }

  if (texit == 0)
    {
    char        oldname[MAXPATHLEN];

    struct stat statbuf;

    /*
    ** All tasks should now be running.
    ** Turn off MOM_CHECKPOINT_POST flag so job is back to where
    ** it was before the bad checkpoint attempt.
    */

    pjob->ji_flags &= ~MOM_CHECKPOINT_POST;

    /*
    ** Get rid of incomplete checkpoint directory and
    ** move old checkpoint dir back to regular if it exists.
    */

    remtree(namebuf);
    strcpy(oldname, namebuf);
    strcat(oldname, ".old");

    if (stat(oldname, &statbuf) == 0)
      {
      if (rename(oldname, namebuf) == -1)
        pjob->ji_qs.ji_svrflags &= ~JOB_SVFLG_CHECKPOINT_FILE;
      }
    }
  }  /* END checkpoint_partial() */







/* BLCR version of restart */

int blcr_restart_job(

  job *pjob)

  {
  char *id = "blcr_restart_job";
  int   pid;
  char  sid[20];
  char  *arg[20];
  extern  char    restart_script_name[1024];
  task *ptask;
  char  buf[1024];
  char  namebuf[MAXPATHLEN];
  char  **ap;


  /* if a restart script is defined launch it */

  if (restart_script_name[0] == '\0')
    {
    log_err(PBSE_RMEXIST, id, "No restart script defined");

    return(PBSE_RMEXIST);
    }

  /* BLCR is not for parallel jobs, there can only be one task in the job. */

  ptask = (task *) GET_NEXT(pjob->ji_tasks);

  if (ptask == NULL)
    {


    /* turns out if we are restarting a complete job then ptask will be
       null and we need to create a task We'll just create one task*/
    if ((ptask = pbs_task_create(pjob, TM_NULL_TASK)) == NULL)
      {
      log_err(PBSE_RMNOPARAM, id, "Job has no tasks");
      return(PBSE_RMNOPARAM);
      }

    strcpy(ptask->ti_qs.ti_parentjobid, pjob->ji_qs.ji_jobid);

    ptask->ti_qs.ti_parentnode = 0;
    ptask->ti_qs.ti_parenttask = 0;
    ptask->ti_qs.ti_task = 0;


    }

  /* launch the script and return success */
  
  pid = fork();

  if (pid < 0)
    {
    /* fork failed */

    return(PBSE_RMSYSTEM);
    }
  else if (pid > 0)
    {
    /* parent */

    ptask->ti_qs.ti_sid = pid;  /* Apparently torque doesn't do anything with the session ID that we pass back here... */
    pjob->ji_wattr[(int)JOB_ATR_session_id].at_val.at_long = pid;
    pjob->ji_wattr[(int)JOB_ATR_session_id].at_flags =
      ATR_VFLAG_SET | ATR_VFLAG_MODIFY | ATR_VFLAG_SEND;

    ptask->ti_qs.ti_status = TI_STATE_RUNNING;
    task_save(ptask);

    return(PBSE_NONE);
    }
  else if (pid == 0)
    {   
    /* child: execv the script */

    /* if there are missing .OU or .ER files create them, they were probably
       empty and server didn't send them */
    /* TODO: check return value? */
    create_missing_files(pjob);

    if (pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_flags & ATR_VFLAG_SET)
      {
      /* The job has a checkpoint directory specified, use it. */
      strcpy(namebuf, pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_val.at_str);
      }
    else
      {
      strcpy(namebuf, path_checkpoint);
      strcat(namebuf, pjob->ji_qs.ji_fileprefix);
      strcat(namebuf, JOB_CHECKPOINT_SUFFIX);
      }



    sprintf(sid, "%ld",

            pjob->ji_wattr[(int)JOB_ATR_session_id].at_val.at_long);

    arg[0] = restart_script_name;
    arg[1] = sid;
    arg[2] = SET_ARG(pjob->ji_qs.ji_jobid);
    arg[3] = SET_ARG(pjob->ji_wattr[(int)JOB_ATR_euser].at_val.at_str);
    arg[4] = SET_ARG(namebuf);
    arg[5] = SET_ARG(pjob->ji_wattr[(int)JOB_ATR_checkpoint_name].at_val.at_str);
    arg[6] = NULL;

    strcpy(buf, "restart args:");

    for (ap = arg;*ap;ap++)
      {
      strcat(buf, " ");
      strcat(buf, *ap);
      }

    log_err(-1, id, buf);

    log_close(0);

    if (lockfds >= 0)
      {
      close(lockfds);
      lockfds = -1;
      }

    net_close(-1);

    fdreopen("/dev/null", O_RDONLY, 0);
    fdreopen("/dev/null", O_WRONLY, 1);
    fdreopen("/dev/null", O_WRONLY, 2);

    /* set us up with a new session */

    pid = set_job(pjob, NULL);

    if (pid < 0)
      {
      perror("setsid");
      exit(-1);
      }

    execv(arg[0], arg);
    }  /* END if (pid == 0) */

  return(PBSE_NONE);
  }  /* END blcr_restart_job() */





/* start each task based on task checkpoint records located job-specific checkpoint directory */

int mom_restart_job(job  *pjob)
  {
  static char id[] = "mom_restart_job";
  char  path[MAXPATHLEN];
  int  i;
  char  namebuf[MAXPATHLEN];
  char  *filnam;
  DIR  *dir;

  struct dirent *pdir;
  tm_task_id taskid;
  task         *ptask;
  int  tcount = 0;
  long  mach_restart A_((task *, char *path));

  strcpy(namebuf, path_checkpoint);
  strcat(namebuf, pjob->ji_qs.ji_fileprefix);
  strcat(namebuf, JOB_CHECKPOINT_SUFFIX);

  if ((dir = opendir(path)) == NULL)
    {
    sprintf(log_buffer, "opendir %s",
            path);

    log_err(errno, id, log_buffer);

    return(PBSE_RMEXIST);
    }

  strcpy(namebuf, path);

  strcat(namebuf, "/");

  i = strlen(namebuf);

  filnam = &namebuf[i];

  while ((pdir = readdir(dir)) != NULL)
    {
    if (strlen(pdir->d_name) <= 2)
      continue;

    if ((taskid = (tm_task_id)atoi(pdir->d_name)) == 0)
      {
      sprintf(log_buffer, "%s: garbled filename %s",
              pjob->ji_qs.ji_jobid,
              pdir->d_name);

      goto fail;
      }

    if ((ptask = task_find(pjob, taskid)) == NULL)
      {
      sprintf(log_buffer, "%s: task %d not found",
              pjob->ji_qs.ji_jobid,
              (int)taskid);

      goto fail;
      }

    strcpy(filnam, pdir->d_name);

    if (mach_restart(ptask, namebuf) == -1)
      {
      sprintf(log_buffer, "%s: task %d failed from file %s",
              pjob->ji_qs.ji_jobid,
              (int)taskid,
              namebuf);

      goto fail;
      }

    ptask->ti_qs.ti_status = TI_STATE_RUNNING;

    if (LOGLEVEL >= 6)
      {
      log_record(
        PBSEVENT_ERROR,
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid,
        "task set to running (mom_restart_job)");
      }

    task_save(ptask);

    tcount++;
    }

  closedir(dir);

  sprintf(log_buffer, "Restarted %d tasks",
          tcount);

  LOG_EVENT(
    PBSEVENT_JOB,
    PBS_EVENTCLASS_JOB,
    pjob->ji_qs.ji_jobid,
    log_buffer);

  return(PBSE_NONE);

fail:

  log_err(errno, id, log_buffer);

  closedir(dir);

  return(PBSE_RMEXIST);
  }  /* END mom_restart_job() */






/**
 * mom_checkpoint_init_job_periodic_timer
 *
 * The routine is called from TMomFinalizeJob1 in start_exec.c.
 * This code initializes checkpoint related variables in the job struct.
 *
 * @param pjob Pointer to job structure
 * @see TMomFinalizeJob1
 */
void
mom_checkpoint_init_job_periodic_timer(job *pjob)
  {
  attribute    *pattr;
  char         *vp;

  /* Should we set up the job for periodic checkpoint? */

  pattr = &pjob->ji_wattr[(int)JOB_ATR_checkpoint];

  if ((pattr->at_flags & ATR_VFLAG_SET) &&
      (csv_find_string(pattr->at_val.at_str, "c") ||
       csv_find_string(pattr->at_val.at_str, "periodic")))
    {
    /* Yes, what is the interval in minutes. */

    if ((vp = csv_find_value(pattr->at_val.at_str, "c")) ||
        (vp = csv_find_value(pattr->at_val.at_str, "interval")))
      {
      /* has checkpoint time (in minutes), convert to seconds */

      pjob->ji_checkpoint_time = atoi(vp) * 60;
      pjob->ji_checkpoint_next = pjob->ji_checkpoint_time;
      }
    else
      {
      /* pick a default number of minutes */

      pjob->ji_checkpoint_time = default_checkpoint_interval * 60;
      pjob->ji_checkpoint_next = pjob->ji_checkpoint_time;
      }
    }
  }



/**
 * mom_checkpoint_job_has_checkpoint
 *
 * The routine is called from TMomFinalizeJob1 in start_exec.c.
 * It checks to see if the job has a checkpoint file to restart from.
 *
 * @param pjob Pointer to job structure
 * @see TMomFinalizeJob1
 */

int mom_checkpoint_job_has_checkpoint(

  job *pjob)

  {
  /* Has the job has been checkpointed? */

  switch (checkpoint_system_type)

    {
    case CST_MACH_DEP:

      if (pjob->ji_qs.ji_svrflags & (JOB_SVFLG_CHECKPOINT_FILE | JOB_SVFLG_CHECKPOINT_MIGRATEABLE))
        {
        char           buf[MAXPATHLEN + 2];

        struct stat    sb;

        /* Does the checkpoint directory exist? */

        if (pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_flags & ATR_VFLAG_SET)
          {
          /* The job has a checkpoint directory specified, use it. */
          strcpy(buf, pjob->ji_wattr[(int)JOB_ATR_checkpoint_dir].at_val.at_str);
          }
        else
          {
          /* Otherwise, use the default job checkpoint directory /var/spool/torque/checkpoint/42.host.domain.CK */
          strcpy(buf, path_checkpoint);
          strcat(buf, pjob->ji_qs.ji_fileprefix);
          strcat(buf, JOB_CHECKPOINT_SUFFIX);
          }

        if (stat(buf, &sb) != 0) /* stat(buf) tests if the checkpoint directory exists */
          {
          /* We thought there was a checkpoint but the directory was not there. */
          pjob->ji_qs.ji_svrflags &= ~(JOB_SVFLG_CHECKPOINT_FILE | JOB_SVFLG_CHECKPOINT_MIGRATEABLE);
          break;
          }
        }

      log_err(-1, "mom_checkpoint_job_has_checkpoint", "TRUE");

      return(TRUE); /* Yes, there is a checkpoint. */
      break;

    case CST_BLCR:

      if (pjob->ji_wattr[(int)JOB_ATR_checkpoint_name].at_flags & ATR_VFLAG_SET)
        {
        log_err(-1, "mom_checkpoint_job_has_checkpoint", "TRUE");
        return(TRUE);
        }

      break;
    }

  log_err(-1, "mom_checkpoint_job_has_checkpoint", "FALSE");

  return(FALSE); /* No checkpoint attribute on job. */
  }





/**
 * mom_checkpoint_start_restart
 *
 * The routine is called from TMomFinalizeJob1 in start_exec.c.
 * This code initializes checkpoint related variables in the job struct.
 * If there is a checkpoint file, the job is restarted from this image.
 *
 * @param pjob Pointer to job structure
 * @see TMomFinalizeJob1
 */

int mom_checkpoint_start_restart(

  job *pjob)

  {
  /* static char *id = "mom_checkpoint_start_restart"; */

  int            rc = PBSE_NONE;

  /* At this point we believe that there is a checkpoint image, try to restart it. */

  switch (checkpoint_system_type)

    {
    case CST_MACH_DEP:

      /* perform any site required setup before restart, normally empty and does nothing */

      if ((rc = site_mom_prerst(pjob)) != PBSE_NONE)
        {
        return(rc);
        }

      rc = mom_restart_job(pjob); /* Iterate over files in checkpoint dir, restarting all files found. */

      break;

    case CST_BLCR:

      /* NOTE:  partition creation handled in blcr_restart_job() */

      /* perform any site required setup before restart, normally empty and does nothing */

      if ((rc = site_mom_prerst(pjob)) != PBSE_NONE)
        {
        return(rc);
        }

      rc = blcr_restart_job(pjob);

      break;

    case CST_NONE:

    default:

      return(PBSE_NOSUP);

      /*NOTREACHED*/

      break;
    }

  return(rc);
  }  /* END mom_checkpoint_start_restart() */



/* this file creates missing stderr/stdout files before restarting
   the checkpointed job. This was designed for BLCR checkpointing.
   empty .OU or .ER files are not retained by the server, so if we
   are restarting a checkpointed job then they will not get sent back
   out to use.  the blcr restart command will expect these files to exist,
   even if empty.  If any expected files are missing we create them here */

/* TODO: this needs to be modified to work with user .pbs_spool directories */
int create_missing_files(job *pjob)
  {
  int should_have_stderr;
  int should_have_stdout;
  attribute *pattr;
  char *pstr;
  char *namebuf;
  int bufsize;
  int files_created = 0;
  int fd;


  should_have_stderr = TRUE;
  should_have_stdout = TRUE;
  pattr = &pjob->ji_wattr[(int)JOB_ATR_join];

  if (pattr->at_flags & ATR_VFLAG_SET)
    {
    pstr = pattr->at_val.at_str;

    if ((pstr != NULL) && (*pstr != '\0') && (*pstr != 'n'))
      {
      /* if not the first letter, and in list - is joined */

      if ((*pstr != 'e') && (strchr(pstr + 1, (int)'e')))
        {
        should_have_stderr = FALSE; /* being joined */
        }
      else if ((*pstr != 'o') && (strchr(pstr + 1, (int)'o')))
        {
        should_have_stdout = FALSE;
        }
      }
    }



  if (should_have_stdout)
    {
    bufsize = strlen(pjob->ji_qs.ji_fileprefix) + strlen(path_spool) + strlen(JOB_STDOUT_SUFFIX) + 1;
    namebuf = malloc(bufsize * sizeof(char));

    if (namebuf == NULL)
      {
      return -1;
      }

    strcpy(namebuf, path_spool);

    strcat(namebuf, pjob->ji_qs.ji_fileprefix);
    strcat(namebuf, JOB_STDOUT_SUFFIX);

    if (access(namebuf, F_OK) != 0)
      {
      if ((fd = creat(namebuf, S_IRUSR | S_IWUSR)) > 0)
        {
        /* TODO check return value of fchown */
        fchown(fd,  pjob->ji_qs.ji_un.ji_momt.ji_exuid, pjob->ji_qs.ji_un.ji_momt.ji_exgid);
        close(fd);
        ++files_created;
        }
      else
        {
        /* couldn't create the file, why could this happen, TODO: what should we do? */
        }

      }

    free(namebuf);
    }

  if (should_have_stderr)
    {
    bufsize = strlen(pjob->ji_qs.ji_fileprefix) + strlen(path_spool) + strlen(JOB_STDOUT_SUFFIX) + 1;
    namebuf = malloc(bufsize * sizeof(char));

    if (namebuf == NULL)
      {
      return -1;
      }

    strcpy(namebuf, path_spool);

    strcat(namebuf, pjob->ji_qs.ji_fileprefix);
    strcat(namebuf, JOB_STDERR_SUFFIX);

    if (access(namebuf, F_OK) != 0)
      {
      if ((fd = creat(namebuf, S_IRUSR | S_IWUSR)) > 0)
        {
        /* TODO check return value of fchown */
        fchown(fd,  pjob->ji_qs.ji_un.ji_momt.ji_exuid, pjob->ji_qs.ji_un.ji_momt.ji_exgid);
        close(fd);
        ++files_created;
        }
      else
        {
        /* couldn't create the file, why could this happen, TODO: what should we do? */
        }

      }

    free(namebuf);

    }

  return files_created;
  }


