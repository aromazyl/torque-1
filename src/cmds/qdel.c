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
 *
 * qdel - (PBS) delete batch job
 *
 * Authors:
 *      Terry Heidelberg
 *      Livermore Computing
 *
 *      Bruce Kelly
 *      National Energy Research Supercomputer Center
 *
 *      Lawrence Livermore National Laboratory
 *      University of California
 */

#include "cmds.h"
#include <pbs_config.h>   /* the master config generated by configure */

void qdel_all(char *extend);

/* qdel */

int main(

  int    argc,
  char **argv)

  {
  int c;
  int errflg = 0;
  int any_failed = 0;
  char *pc;

  char job_id[PBS_MAXCLTJOBID]; /* from the command line */

  char job_id_out[PBS_MAXCLTJOBID];
  char server_out[MAXSERVERNAME];
  char rmt_server[MAXSERVERNAME];

  char extend[1024];

#define GETOPT_ARGS "m:pW:"

  extend[0] = '\0';

  while ((c = getopt(argc, argv, GETOPT_ARGS)) != EOF)
    {
    switch (c)
      {

      case 'm':

        /* add delete message */

        if (extend[0] != '\0')
          {
          /* extension option already specified */

          errflg++;

          break;
          }

        if (strchr(optarg, '='))
          {
          /* message cannot contain '=' character */

          errflg++;

          break;
          }

        strncpy(extend, optarg, sizeof(extend));

        break;

      case 'p':

        if (extend[0] != '\0')
          {
          errflg++;

          break;
          }

        strcpy(extend, DELPURGE);

        strcat(extend, "1");

        break;

      case 'W':

        if (extend[0] != '\0')
          {
          errflg++;

          break;
          }

        pc = optarg;

        if (strlen(pc) == 0)
          {
          fprintf(stderr, "qdel: illegal -W value\n");

          errflg++;

          break;
          }

        while (*pc != '\0')
          {
          if (!isdigit(*pc))
            {
            fprintf(stderr, "qdel: illegal -W value\n");

            errflg++;

            break;
            }

          pc++;
          }

        strcpy(extend, DELDELAY);

        strcat(extend, optarg);

        break;

      default:

        errflg++;

        break;
      }
    }    /* END while (c) */

  if ((errflg != 0) || (optind >= argc))
    {
    static char usage[] = "usage: qdel [{ -p | -W delay | -m message}] [<JOBID>[<JOBID>]|'all'|'ALL']...\n";

    fprintf(stderr, "%s", usage);

    fprintf(stderr, "       -m, -p, and -W are mutually exclusive\n");

    exit(2);
    }

  for (;optind < argc;optind++)
    {
    int connect;
    int stat = 0;
    int located = FALSE;

    /* check to see if user specified 'all' to delete all jobs */

    strcpy(job_id, argv[optind]);

    if ((strcmp("all", job_id) == 0) || (strcmp("ALL", job_id) == 0))
      {
      qdel_all(extend);
      continue;
      }
    else if (get_server(job_id, job_id_out, server_out))
      {
      fprintf(stderr, "qdel: illegally formed job identifier: %s\n",
              job_id);

      any_failed = 1;

      continue;
      }

cnt:

    connect = cnt2server(server_out);

    if (connect <= 0)
      {
      fprintf(stderr, "qdel: cannot connect to server %s (errno=%d) %s\n",
              pbs_server,
              pbs_errno,
              pbs_strerror(pbs_errno));

      any_failed = pbs_errno;

      continue;
      }

    stat = pbs_deljob(connect, job_id_out, extend);

    if (stat && (pbs_errno != PBSE_UNKJOBID))
      {
      prt_job_err("qdel", connect, job_id_out);

      any_failed = pbs_errno;
      }
    else if (stat && (pbs_errno == PBSE_UNKJOBID) && !located)
      {
      located = TRUE;

      if (locate_job(job_id_out, server_out, rmt_server))
        {
        pbs_disconnect(connect);

        strcpy(server_out, rmt_server);

        goto cnt;
        }

      prt_job_err("qdel", connect, job_id_out);

      any_failed = pbs_errno;
      }

    pbs_disconnect(connect);
    }

  exit(any_failed);
  }  /* END main() */

/* if a user requested deleting 'all' then this routine will get the list of
 * jobs from the server and try to delete all jobs that are not in a
 * 'C'omplete or 'E'xiting state
 */
void qdel_all(
  char *extend)   /* I */

  {
  char *jobid;
  char *state = 0;
  int connect;
  int stat;
  int retries;

  struct batch_status *p_status;

  struct batch_status *p;

  struct attropl *p_atropl = 0;

  struct attrl *a;

  connect = cnt2server('\0');

  if (connect <= 0)
    {
    fprintf(stderr, "qdel: cannot connect to default server (errno=%d) %s\n",
            pbs_errno,
            pbs_strerror(pbs_errno));

    return;
    }

  p_status = pbs_selstat(connect, p_atropl, NULL);

  if (p_status == NULL)
    {
    fprintf(stderr, "qdel: cannot find any jobs to delete\n");
    }

  for (p = p_status;p != NULL;p = p->next)
    {
    jobid = p->name;
    a = p->attribs;

    while (a != NULL)
      {
      if ((a->name != NULL) && (!strcmp(a->name, ATTR_state)))
        {
        state = a->value;
        break;
        }

      a = a->next;
      }

    /* 
     * Don't bother deleting jobs that are 'C'omplete or 'E'xiting
     * Unless we are Purging, then try these jobs as well
     */
    
    if ((strstr(extend,DELPURGE) != NULL) ||
        ((*state != 'E') && (*state != 'C')))
      {
      retries = 0;

redo:
      stat = pbs_deljob(connect, jobid, extend);

      /*
       * if MOM is too slow to respond, we will retry a few times before
       * before giving up
       */

      if (stat && (pbs_errno == PBSE_NORELYMOM) && (retries < 3))
        {
        sleep(1);
        retries++;
        goto redo;
        }

      if (stat &&
          (pbs_errno != PBSE_UNKJOBID) &&
          (pbs_errno != PBSE_BADSTATE))
        {
        printf("Deletion Error: %d (%s)\n", pbs_errno, pbs_strerror(pbs_errno));
        prt_job_err("qdel", connect, jobid);
        /*
         * if we don't have permission for this job, we probably don't have
         * permission for any other jobs
        */

        if (pbs_errno == PBSE_PERM)
          {
          break;
          }
        }
      }

    }

  pbs_disconnect(connect);

  return;
  }

