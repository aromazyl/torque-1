/*
 * tmsock_recov.c - This file contains the functions to record a job's
 * demux socket numbers, nodeid, and last task id
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/param.h>
#include "pbs_ifl.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "server_limits.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_job.h"
#include "log.h"
#include "svrfunc.h"


u_long addclient(char *name);

int save_roottask(job *pjob)
  {
  save_struct((char*)&pjob->ji_momsubt,sizeof(pjob->ji_momsubt));
  save_struct((char*)&pjob->ji_mompost,sizeof(pjob->ji_mompost));

  return 0;
  }

int recov_roottask( int fds, job *pjob)
  {
  char *id = "root_task";

  if (read(fds, &pjob->ji_momsubt, sizeof(pjob->ji_momsubt)) != sizeof(pjob->ji_momsubt))
    {
    log_err(errno,id,"read");
    pjob->ji_momsubt = 0;
    pjob->ji_mompost = NULL;
    return 1;
    }

  if (read(fds, &pjob->ji_mompost, sizeof(pjob->ji_mompost)) != sizeof(pjob->ji_mompost))
    {
    log_err(errno,id,"read");
    pjob->ji_momsubt = 0;
    pjob->ji_mompost = NULL;
    return 1;
    }

  return 0;
  }

/*
 * save_tmsock() - Saves the tm sockets of a job to disk
 *
 */

int save_tmsock(

  job *pjob)  /* pointer to job structure */
  {
  static int sizeofint = sizeof(int);

  if ((pjob->ji_stdout > 0) && (pjob->ji_stdout < 1024))
    {
    /* We don't have real port numbers (yet), so don't bother */

    return(0);
    }

  DBPRT(("saving extra job info stdout=%d stderr=%d taskid=%u nodeid=%u\n",

         pjob->ji_stdout,
         pjob->ji_stderr,
         pjob->ji_taskid,
         pjob->ji_nodeid));

  /* FIXME: need error checking here */

  save_struct((char *)&pjob->ji_stdout, sizeofint);
  save_struct((char *)&pjob->ji_stderr, sizeofint);
  save_struct((char *)&pjob->ji_taskid, sizeof(tm_task_id));
  save_struct((char *)&pjob->ji_nodeid, sizeof(tm_node_id));

  return(0);
  }  /* END save_tmsock() */

/*
 * recov_tmsock() - Recovers the tm sockets of a job
 *
 */

int recov_tmsock(

  int fds,
  job *pjob) /* I */   /* pathname to job save file */

  {
  char *id = "recov_tmsock";

  static int sizeofint = sizeof(int);

  if (read(fds, (char *)&pjob->ji_stdout, sizeofint) != sizeofint)
    {
    log_err(errno, id, "read");

    return(1);
    }

  if (read(fds, (char *)&pjob->ji_stderr, sizeofint) != sizeofint)
    {
    log_err(errno, id, "read");

    return(1);
    }

  if (read(fds, (char *)&pjob->ji_taskid, sizeof(tm_task_id)) != sizeof(tm_task_id))
    {
    log_err(errno, id, "read");

    return(1);
    }

  if (read(fds, (char *)&pjob->ji_nodeid, sizeof(tm_node_id)) != sizeof(tm_node_id))
    {
    log_err(errno, id, "read");

    return(1);
    }

  DBPRT(("recovered extra job info stdout=%d stderr=%d taskid=%u nodeid=%u\n",

         pjob->ji_stdout,
         pjob->ji_stderr,
         pjob->ji_taskid,
         pjob->ji_nodeid));

  return(0);
  }  /* END recov_tmsock() */

