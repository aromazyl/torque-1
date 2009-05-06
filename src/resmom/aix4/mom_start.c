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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include "portability.h"
#include "libpbs.h"
#include "list_link.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "resource.h"
#include "pbs_job.h"
#include "mom_mach.h"
#include "mom_func.h"
#include "resmon.h"
#if IBM_SP2==2 /* IBM SP2 with PSSP 3.1 */
#include "pbs_nodes.h"
#include <st_client.h>
#endif    /* IBM SP2 */

static char ident[] = "@(#) $RCSfile$ $Revision$";

/* Global Variables */

extern int  exiting_tasks;
extern char  mom_host[];
extern tlist_head svr_alljobs;
extern int  termin_child;
extern char *path_home;

extern void    state_to_server A_((int, int));

/* Private variables */
static int     job_key;

/*
 * set_job - set up a new job session
 *  Set session id and whatever else is required on this machine
 * to create a new job.
 *
 *      Return: session/job id or if error:
 *  -1 - if setsid() fails
 *  -2 - if other, message in log_buffer
 */

int
set_job(job *pjob, struct startjob_rtn *sjr)
  {
  return (sjr->sj_session = setsid());
  }

/*
** set_globid - set the global id for a machine type.
*/
void
set_globid(job *pjob, struct startjob_rtn *sjr)
  {
  extern char noglobid[];

  if (pjob->ji_globid == NULL)
    pjob->ji_globid = strdup(noglobid);
  }

/*
 * set_mach_vars - setup machine dependent environment variables
 */

int
set_mach_vars(
  job *pjob,    /* pointer to the job */
  struct var_table *vtab    /* pointer to variable table */
)
  {
#if IBM_SP2==1 || IBM_SP2==2 /* IBM SP2 with PSSP 2.x or 3.x*/
  char  buf[256];
  int   i;

  sprintf(buf, "%d", pjob->ji_numvnod);
  bld_env_variables(vtab, "MP_PROCS", buf);

  for (i = 0; i < vtab->v_used; ++i)
    {
    if ((strncmp(vtab->v_envp[i], "PBS_NODEFILE", 12) == 0) &&
        (*(vtab->v_envp[i] + 12) == '='))
      {
      bld_env_variables(vtab, "MP_HOSTFILE", vtab->v_envp[i] + 13);
      break;
      }
    }

#endif
#if IBM_SP2==2   /* IBM SP2 with PSSP 3.x*/
  sprintf(buf, "%d", job_key);

  bld_env_variables(vtab, "PBS_JOB_KEY", buf);

  sprintf(buf, "%d", pjob->ji_vnods[0].vn_index);

  bld_env_variables(vtab, "MP_MPI_NETWORK", buf);

#endif    /* IBM SP2 */

  return 0;
  }

char *
set_shell(job *pjob, struct passwd *pwdp)
  {
  char *cp;
  int   i;
  char *shell;

  struct array_strings *vstrs;
  /*
   * find which shell to use, one specified or the login shell
   */

  shell = pwdp->pw_shell;

  if ((pjob->ji_wattr[(int)JOB_ATR_shell].at_flags & ATR_VFLAG_SET) &&
      (vstrs = pjob->ji_wattr[(int)JOB_ATR_shell].at_val.at_arst))
    {
    for (i = 0; i < vstrs->as_usedptr; ++i)
      {
      cp = strchr(vstrs->as_string[i], '@');

      if (cp)
        {
        if (!strncmp(mom_host, cp + 1, strlen(cp + 1)))
          {
          *cp = '\0'; /* host name matches */
          shell = vstrs->as_string[i];
          break;
          }
        }
      else
        {
        shell = vstrs->as_string[i]; /* wildcard */
        }
      }
    }

  return (shell);
  }

/*
 * scan_for_terminated - scan the list of runnings jobs for one whose
 * session id matched that of a terminated child pid.  Mark that
 * job as Exiting.
 */

void
scan_for_terminated(void)
  {
  static char id[] = "scan_for_terminated";
  int   exiteval;
  pid_t  pid;
  job  *pjob;
  task  *ptask;
  int  statloc;

  /* update the latest intelligence about the running jobs;         */
  /* must be done before we reap the zombies, else we lose the info */

  termin_child = 0;

  if (mom_get_sample() == PBSE_NONE)
    {
    pjob = (job *)GET_NEXT(svr_alljobs);

    while (pjob)
      {
      mom_set_use(pjob);
      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
      }
    }

  /* Now figure out which tasks(s) have terminated (are zombies) */

  while ((pid = waitpid(-1, &statloc, WNOHANG)) > 0)
    {

    pjob = (job *)GET_NEXT(svr_alljobs);

    while (pjob)
      {
      /*
      ** see if process was a child doing a special
      ** function for MOM
      */
      if (pid == pjob->ji_momsubt)
        break;

      /*
      ** look for task
      */
      ptask = (task *)GET_NEXT(pjob->ji_tasks);

      while (ptask)
        {
        if (ptask->ti_qs.ti_sid == pid)
          break;

        ptask = (task *)GET_NEXT(ptask->ti_jobtask);
        }

      if (ptask != NULL)
        break;

      pjob = (job *)GET_NEXT(pjob->ji_alljobs);
      }

    if (WIFEXITED(statloc))
      exiteval = WEXITSTATUS(statloc);
    else if (WIFSIGNALED(statloc))
      exiteval = WTERMSIG(statloc) + 10000;
    else
      exiteval = 1;

    if (pjob == NULL)
      {
      DBPRT(("%s: pid %d not tracked, exit %d\n",
             id, pid, exiteval))
      continue;
      }

    if (pid == pjob->ji_momsubt)
      {
      if (pjob->ji_mompost)
        {
        pjob->ji_mompost(pjob, exiteval);
        pjob->ji_mompost = 0;
        }

      pjob->ji_momsubt = 0;

      (void)job_save(pjob, SAVEJOB_QUICK);
      continue;
      }

    DBPRT(("%s: task %d pid %d exit value %d\n", id,
           ptask->ti_qs.ti_task, pid, exiteval))

    kill_task(ptask, SIGKILL, 0);
    ptask->ti_qs.ti_exitstat = exiteval;
    ptask->ti_qs.ti_status = TI_STATE_EXITED;
    task_save(ptask);
    sprintf(log_buffer, "task %d terminated", ptask->ti_qs.ti_task);
    LOG_EVENT(PBSEVENT_DEBUG, PBS_EVENTCLASS_JOB,
              pjob->ji_qs.ji_jobid, log_buffer);
    exiting_tasks = 1;
    }
  }  /* END scan_for_terminated() */


/*
 * creat the master pty, this particular
 * piece of code depends on multiplexor /dev/ptc
 */

int
open_master(
  char **rtn_name /* RETURN name of slave pts */
)
  {
  int   ptc;

  if ((ptc = open("/dev/ptc", O_RDWR | O_NOCTTY, 0)) < 0)
    {
    return (-1);
    }

  *rtn_name  = ttyname(ptc); /* name of slave side */

  return (ptc);
  }




/*
 * struct sig_tbl = map of signal names to numbers,
 * see req_signal() in ../requests.c
 */

struct sig_tbl sig_tbl[] =
  {
    { "NULL", 0
    },

  { "HUP", SIGHUP },
  { "INT", SIGINT },
  { "QUIT", SIGQUIT },
  { "ILL", SIGILL },
  { "TRAP", SIGTRAP },
  { "ABRT", SIGABRT },
  { "EMT", SIGEMT },
  { "FPE", SIGFPE },
  { "KILL", SIGKILL },
  { "BUS", SIGBUS },
  { "SEGV", SIGSEGV },
  { "SYS", SIGSYS },
  { "PIPE", SIGPIPE },
  { "ALRM", SIGALRM },
  { "TERM", SIGTERM },
  { "URG", SIGURG },
  { "STOP", SIGSTOP },
  { "suspend", SIGSTOP },
  { "TSTP", SIGTSTP },
  { "CONT", SIGCONT },
  { "resume", SIGCONT },
  { "CHLD", SIGCHLD },
  { "TTIN", SIGTTIN },
  { "TTOU", SIGTTOU },
  { "IO", SIGIO },
  { "XCPU", SIGXCPU },
  { "XFSZ", SIGXFSZ },
  { "MSG", SIGMSG },
  { "WINCH", SIGWINCH },
  { "PWR", SIGPWR },
  { "USR1", SIGUSR1 },
  { "USR2", SIGUSR2 },
  { "PROF", SIGPROF },
  { "DANGER", SIGDANGER },
  { "VTALRM", SIGVTALRM },
  { "MIGRATE", SIGMIGRATE },
  { "PRE", SIGPRE },
  { "VIRT", SIGVIRT },
  { "ALRM1", SIGALRM1 },
  { "WAITING", SIGWAITING },
  { "KAP", SIGKAP },
  { "GRANT", SIGGRANT },
  { "RETRACT", SIGRETRACT },
  { "SOUND", SIGSOUND },
  { "SAK", SIGSAK },
  {(char *)0, -1 }
  };

#if IBM_SP2==2 /* IBM SP2 with PSSP 3.1 */

/*
 * The following routines are used to load and unload the routing information
 * for the IBM high speed switch on the SP.  Each node must specify the same
 * connection information.   At the end of the job the information is unloaded.
 */

static int
name_to_sw_num(char *nn)
  {
  int   i;
  char *pa;
  char *pb;

  extern struct swtbl_num swtbl_num[];
  extern int    ibm_sp2_num_nodes;

  for (i = 0; i < ibm_sp2_num_nodes; i++)
    {
    pa = nn;
    pb = swtbl_num[i].sw_name;

    while (*pa && *pb && (*pa == *pb))
      {
      pa++;
      pb++;
      }

    if (((*pa == '\0') && (*pb == '\0')) ||
        ((*pa == '\0') && (*pb == '.'))  ||
        ((*pa == '.')  && (*pb == '\0')))
      return swtbl_num[i].sw_num;
    }

  return -1;
  }


static int
build_swtbl_array(job *pjob, struct ST_NODE_INFO **psa)
  {
  int    i;
  int    j;

  struct ST_NODE_INFO *swtbl;

  swtbl = (struct ST_NODE_INFO *)calloc(pjob->ji_numvnod,
                                        sizeof(struct ST_NODE_INFO));

  if (swtbl == NULL)
    return PBSE_SYSTEM;

  for (i = 0; i < pjob->ji_numvnod; i++)
    {
    /* pull node name out of vnodent/hnodent struct */
    (void)strcpy((swtbl + i)->st_node_name,
                 pjob->ji_vnods[i].vn_host->hn_host);

    /* now find switch node number that matches name */

    if ((j = name_to_sw_num((swtbl + i)->st_node_name)) == -1)
      {
      (void)free(swtbl);
      return PBSE_UNKNODE;
      }

    (swtbl + i)->st_virtual_task_id = i;
    (swtbl + i)->st_switch_node_num = j;
    (swtbl + i)->st_window_id = pjob->ji_vnods[i].vn_index;
    }

  *psa = swtbl;

  return PBSE_NONE;
  }

/*
 * load_sp_switch - load the IBM SP switch table with the nodes/window_id
 * to be used by this job.  Also write a file into the "aux" directory
 * with the array of window_ids in task order.  This file is used by
 * pbspd.c to place the correct window_id into the environment based on
 * the value of MP_CHILD passed by IBM's poe.
 */

int
load_sp_switch(job *pjob)
  {
  int       i;
  int       rc = 0;

  struct ST_NODE_INFO *pswa;
  static char *id = "load_sp_switch";
  char buf[MAXPATHLEN+1];
  FILE *fwin;
  extern int internal_state;


  sscanf(pjob->ji_wattr[(int)JOB_ATR_Cookie].at_val.at_str, "%x", &job_key);

  if (job_key < 0)
    job_key = -job_key;

  if ((rc = build_swtbl_array(pjob, &pswa)) != PBSE_NONE)
    {
    sprintf(log_buffer, "build swtbl node array failed %d", rc);
    log_record(PBSEVENT_DEBUG, 0, id, log_buffer);
    return -1;
    }

  rc = swtbl_load_table(ST_VERSION, pjob->ji_qs.ji_un.ji_momt.ji_exuid,

                        getpid(), job_key, mom_host, pjob->ji_numvnod,
                        pjob->ji_qs.ji_jobid, pswa);

  if (rc != ST_SUCCESS)
    {
    sprintf(log_buffer, "swtbl_load_table failed with %d", rc);
    log_record(PBSEVENT_SYSTEM | PBSEVENT_ADMIN | PBSEVENT_JOB,
               PBS_EVENTCLASS_JOB, pjob->ji_qs.ji_jobid, log_buffer);
    internal_state |= INUSE_DOWN;
    /* FIXME: need to send state to all servers, not just index 0 */
    state_to_server(0, 1); /* tell server we are down */
    }
  else
    {

    /* success */
    (void)sprintf(buf, "%s/aux/%s.SW", path_home, pjob->ji_qs.ji_jobid);

    if ((fwin = fopen(buf, "w")) == NULL)
      {
      sprintf(log_buffer, "cannot open %s", buf);
      log_err(errno, id, log_buffer);
      return -1;
      }

    (void)fchmod(fileno(fwin), 0644);

    for (i = 0; i < pjob->ji_numvnod; i++)
      fprintf(fwin, "%d\n", pjob->ji_vnods[i].vn_index);

    (void)fclose(fwin);
    }

  (void)free(pswa);
  return rc;
  }

/*
 * unload_sp_switch - unload the job/node information from the switch
 * Also remove the aux *.SW file created when the switch is loaded above.
 */

void
unload_sp_switch(job *pjob)
  {
  int       i;
  int       rc = 0;
  char       buf[MAXPATHLEN+1];

  struct vnodent     *pvp;
  extern int       internal_state;

  for (i = 0; i < pjob->ji_numvnod; ++i)
    {
    pvp = &pjob->ji_vnods[i];

    if (pvp->vn_host->hn_node == pjob->ji_nodeid)
      {
      rc = swtbl_unload_table(ST_VERSION, "css0",
                              pjob->ji_qs.ji_un.ji_momt.ji_exuid,
                              pvp->vn_index);
      DBPRT(("Unloading switch window %d\n", pvp->vn_index))

      if (rc != ST_SUCCESS)
        {
        sprintf(log_buffer, "error %d unloading switch table window %d for job %s", rc, pvp->vn_index, pjob->ji_qs.ji_jobid);
        log_err(PBSE_SYSTEM, "unload_sp_switch", log_buffer);

        if (rc != ST_SWITCH_NOT_LOADED)
          {
          rc = swtbl_clean_table(ST_VERSION, "css0",
                                 ST_ALWAYS_KILL, pvp->vn_index);
          }
        else if (rc != ST_SUCCESS)
          {
          sprintf(log_buffer, "error %d cleaning switch table window %d for job %s", rc, pvp->vn_index, pjob->ji_qs.ji_jobid);
          log_err(PBSE_SYSTEM, "unload_sp_switch", log_buffer);
          internal_state |= INUSE_DOWN;
          /* FIXME: need to send state to all servers, not just index 0 */
          state_to_server(0, 1); /* tell server we are down */
          }
        }
      }
    }

  (void)sprintf(buf, "%s/aux/%s.SW", path_home, pjob->ji_qs.ji_jobid);
  (void)unlink(buf);
  return;
  }

/*
 * query_adp - query the SP switch adaptor, are we on line
 */

void
query_adp(void)
  {
  int  rc;
  enum ST_ADAPTER_STATUS st;
  extern int internal_state;
  static char *id = "query_adp";

  if ((rc = swtbl_query_adapter(ST_VERSION, "css0", &st)) != ST_SUCCESS)
    {
    if ((internal_state & INUSE_DOWN) == 0)
      {
      log_record(PBSEVENT_SYSTEM, rc, id,
                 "cannot query adaptor");
      internal_state |= INUSE_DOWN | UPDATE_MOM_STATE;
      }

    return;
    }

  switch (st)
    {

    case ADAPTER_READY:

      if (internal_state & INUSE_DOWN)
        {
        internal_state &= ~INUSE_DOWN; /* we are not down */
        internal_state |=  UPDATE_MOM_STATE;
        log_record(PBSEVENT_SYSTEM, 0, id, "adaptor up");
        }

      break;

    case ADAPTER_NOTREADY:

      if ((internal_state & INUSE_DOWN) == 0)
        {
        /* mark that we are down */
        internal_state |= INUSE_DOWN | UPDATE_MOM_STATE;
        log_record(PBSEVENT_SYSTEM, 0, id, "adaptor down");
        }

      break;
    }
  }

#endif /* IBM SP */
