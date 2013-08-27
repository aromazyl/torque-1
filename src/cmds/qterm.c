#include "license_pbs.h" /* See here for the software license */
/*
 * qterm
 *  The qterm command terminates the batch server.
 *
 * Synopsis:
 *  qterm -t type [server ...]
 *
 * Options:
 *  -t  delay   Jobs are (1) checkpointed if possible; otherwise, (2) jobs are
 *  rerun (requeued) if possible; otherwise, (3) jobs are left to
 *              run.
 *
 *      immediate
 *              Jobs are (1) checkpointed if possible; otherwise, (2) jobs are
 *  rerun if possible; otherwise, (3) jobs are aborted.
 *
 * quick
 *  The server will save state and exit leaving running jobs
 *  still running.  Good for shutting down when you wish to
 *  quickly restart the server.
 *
 * Arguments:
 *  server ...
 *      A list of servers to terminate.
 *
 * Written by:
 *  Bruce Kelly
 *  National Energy Research Supercomputer Center
 *  Livermore, CA
 *  May, 1993
 */

#include "net_cache.h"
#include "cmds.h"
#include "../lib/Libifl/lib_ifl.h"
#include <pbs_config.h>   /* the master config generated by configure */

int exitstatus = 0; /* Exit Status */

static void execute(int, const char *);

int main(

  int    argc,  /* I */
  char **argv)  /* I */

  {
  /*
   * This routine sends a Server Shutdown request to the batch server.  If the
   * batch request is accepted, and the type is IMMEDIATE, then no more jobs
   * are accepted and all jobs are checkpointed or killed.  If the type is
   * DELAY, then only privileged users can submit jobs, and jobs will be
   * checkpointed if available.
   */

  static char opts[] = "t:";  /* See man getopt */
  int s;                  /* The execute line option */
  static char usage[] = "Usage: qterm [-t immediate|delay|quick] [server ...]\n";
  char *type = NULL;      /* Pointer to the type of termination */
  int manner;             /* The type of termination */
  int errflg = 0;         /* Error flag */

  /* Command line options */

  while ((s = getopt(argc, argv, opts)) != EOF)
    {
    switch (s)
      {

      case 't':

        type = optarg;

        break;

      case '?':

      default:

        errflg++;

        break;
      }
    }  /* END while() */

  if (errflg)
    {
      fprintf(stderr, "%s",
            usage);

    exit(1);
    }

  if (type == NULL)
    {
    /* use 'quick' as default */

    manner = SHUT_QUICK;
    }
  else
    {
    if (!strcasecmp(type, "delay"))
      {
      manner = SHUT_DELAY;
      }
    else if (!strcasecmp(type, "immediate"))
      {
      manner = SHUT_IMMEDIATE;
      }
    else if (!strcasecmp(type, "quick"))
      {
      manner = SHUT_QUICK;
      }
    else
      {
      /* invalid type specified */

	fprintf(stderr, "%s",
              usage);

      exit(1);
      }
    }

  if (optind < argc)
    {
    /* shutdown each specified server */

    for (;optind < argc;optind++)
      {
      execute(manner, argv[optind]);
      }
    }
  else
    {
    /* shutdown default server */

    execute(manner, "");
    }

  exit(exitstatus);

  /*NOTREACHED*/

  return(0);
  }  /* END main() */





/*
 * void execute(int manner,char *server)
 *
 * manner   The manner in which to terminate the server.
 * server   The name of the server to terminate.
 *
 * Returns:
 *  None
 *
 * File Variables:
 *  exitstatus  Set to two if an error occurs.
 */

static void execute(

  int   manner,  /* I */
  const char *server)  /* I */

  {
  int ct;         /* Connection to the server */
  int err;        /* Error return from pbs_terminate */
  char *errmsg;   /* Error message from pbs_terminate */
  int   local_errno = 0;

  if ((ct = cnt2server(server)) > 0)
    {
    err = pbs_terminate_err(ct, manner, NULL, &local_errno);

    if (err != 0)
      {
      errmsg = pbs_geterrmsg(ct);

      if (errmsg != NULL)
        {
        fprintf(stderr, "qterm: %s", errmsg);
        free(errmsg);
        }
      else
        {
        fprintf(stderr, "qterm: Error (%d - %s) terminating server ",
                local_errno, pbs_strerror(local_errno));
        }

      fprintf(stderr, "%s\n",

              server);

      exitstatus = 2;
      }

    pbs_disconnect(ct);
    }
  else
    {
    /* FAILURE */
    local_errno = -1 * ct;

    fprintf(stderr, "qterm: could not connect to server '%s' (%d) %s\n",
            server,
            local_errno,
            pbs_strerror(local_errno));

    exitstatus = 2;
    }

  return;
  }  /* END execute() */


