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
 * The entry point function for pbs_daemon.
 *
 * Included public functions re:
 *
 *	main	initialization and main loop of pbs_daemon
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <sys/types.h>
#include <sys/stat.h>
#if (PLOCK_DAEMONS & 1)
#include <sys/lock.h>
#endif	/* PLOCK_DAEMONS */
#include <netinet/in.h>
#include "pbs_ifl.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include "list_link.h"
#include "work_task.h"
#include "log.h"
#include "server_limits.h"
#include "attribute.h"
#include "job.h"
#include "queue.h"
#include "server.h"
#include "net_connect.h"
#include "credential.h"
#include "svrfunc.h"
#include "tracking.h"
#include "acct.h"
#include "sched_cmds.h"
#include "rpp.h"
#include "dis.h"
#include "dis_init.h"
#include "batch_request.h"
#include "pbs_proto.h"

/* external functions called */

extern int  pbsd_init A_((int));
extern void shutdown_ack ();
extern void update_nodes_file A_((void));
extern void tcp_settimeout(long);
extern void poll_job_task(struct work_task *);
extern int  schedule_jobs(void);
extern void queue_route A_((pbs_queue *));
extern void svr_shutdown(int);
extern void acct_close(void);
extern int  svr_startjob A_((job *,struct batch_request *,char *,char *)); 
extern int RPPConfigure(int,int);


/* external data items */

extern int    svr_chngNodesfile;


/* Local Private Functions */

static int    get_port A_((char *,unsigned int *,pbs_net_t *));
static time_t next_task A_(());
static int    start_hot_jobs();
static void   lock_out A_((int,int));

/* Global Data Items */

char	       *acct_file = NULL;
char	       *log_file  = NULL;
char	       *path_home = PBS_SERVER_HOME;
char	       *path_acct;
char	        path_log[MAXPATHLEN + 1];
char	       *path_priv;
char	       *path_jobs;
char	       *path_queues;
char	       *path_spool;
char	       *path_svrdb;
char	       *path_svrdb_new;
char 	       *path_track;
char	       *path_nodes;
char	       *path_nodes_new;
char	       *path_nodestate;
char	       *path_resources;
extern char    *msg_daemonname;
extern int	pbs_errno;
char	       *pbs_o_host = "PBS_O_HOST";
pbs_net_t	pbs_mom_addr;
unsigned int	pbs_mom_port;
unsigned int	pbs_rm_port;
pbs_net_t	pbs_scheduler_addr;
unsigned int	pbs_scheduler_port;
extern pbs_net_t	pbs_server_addr;
unsigned int	pbs_server_port_dis;
int		queue_rank = 0;
struct server	server;		/* the server structure */
char	        server_host[PBS_MAXHOSTNAME + 1];	/* host_name */
char	       *mom_host = server_host;
int	 	server_init_type = RECOV_WARM;
char	        server_name[PBS_MAXSERVERNAME + 1]; /* host_name[:service|port] */
int		svr_delay_entry = 0;
int		svr_do_schedule = SCH_SCHEDULE_NULL;
list_head	svr_queues;            /* list of queues                   */
list_head	svr_alljobs;           /* list of all jobs in server       */
list_head	svr_newjobs;           /* list of incoming new jobs        */
list_head	svr_newnodes;          /* list of newly created nodes      */
list_head	task_list_immed;
list_head	task_list_timed;
list_head	task_list_event;

time_t		time_now = 0;

int             LOGLEVEL = 0;
int             DEBUGMODE = 0;
int             TForceUpdate = 0;  /* (boolean) */

char           *ProgName;




void DIS_rpp_reset()

  {
  if (dis_getc != rpp_getc) 
    {
    dis_getc    = rpp_getc;
    dis_puts    = (int (*) A_((int,const char *,size_t)))rpp_write;
    dis_gets    = (int (*) A_((int,char *,size_t)))rpp_read;
    disr_skip   = (int (*) A_((int,size_t)))rpp_skip;
    disr_commit = rpp_rcommit;
    disw_commit = rpp_wcommit;
    }

  return;
  }  /* END DIS_rpp_reset() */




/*
** Read a RPP message from a stream.  Only one kind of message
** is expected -- Inter Server requests from MOM's.
*/

void do_rpp(

  int stream)  /* I */

  {
  static char id[] = "do_rpp";

  int  ret, proto, version;

  void is_request A_((int,int,int *));
  void stream_eof A_((int,u_long,int));

  if (LOGLEVEL >= 4)
    {
    sprintf(log_buffer,"rpp request received on stream %d\n",
      stream);

    log_record(
      PBSEVENT_SCHED,
      PBS_EVENTCLASS_REQUEST,
      id,
      log_buffer);
    }

  DIS_rpp_reset();

  proto = disrsi(stream,&ret);

  if (ret != DIS_SUCCESS) 
    {
    if (LOGLEVEL >= 1)
      {
      sprintf(log_buffer,"corrupt rpp request received on stream %d (invalid protocol)\n",
        stream);

      log_record(
        PBSEVENT_SCHED,
        PBS_EVENTCLASS_REQUEST,
        id,
        log_buffer);
      }

    stream_eof(stream,0,ret);

    return;
    }

  version = disrsi(stream,&ret);

  if (ret != DIS_SUCCESS) 
    {
    if (LOGLEVEL >= 1)
      {
      sprintf(log_buffer,"corrupt rpp request received on stream %d (invalid version)\n",
        stream);

      log_record(
        PBSEVENT_SCHED,
        PBS_EVENTCLASS_REQUEST,
        id,
        log_buffer);
      }

    stream_eof(stream,0,ret);

    return;
    }

  switch (proto) 
    {
    case IS_PROTOCOL:

      if (LOGLEVEL >= 6)
        {
        sprintf(log_buffer,"inter-server request received");

        log_record(
          PBSEVENT_SCHED,
          PBS_EVENTCLASS_REQUEST,
          id,
          log_buffer);
        }

      is_request(stream,version,NULL);

      break;

    default:

      if (LOGLEVEL >= 6)
        {
        sprintf(log_buffer,"unknown request protocol received (%d)\n",
          proto);

        log_err(errno,id,log_buffer);
        }

      rpp_close(stream);

      break;
    }  /* END switch(proto) */

  return;
  }  /* END do_rpp() */





void rpp_request(

  int fd)	/* not used */

  {
  static char id[] = "rpp_request";
  int         stream;

  for (;;) 
    {
    if ((stream = rpp_poll()) == -1) 
      {
      log_err(errno,id,"rpp_poll");

      break;
      }
    
    if (stream == -2)
      break;
    
    do_rpp(stream);
    }
  
  return;
  }  /* END rpp_request() */




int PBSShowUsage(

  char *EMsg)  /* I (optional) */

  {
  const char *Msg = "[ -A <ACCTFILE> ] [ -a <ATTR> ] [ -d <HOMEDIR> ] [ -L <LOGFILE> ] [ -M <MOMPORT> ]\n  [ -p <SERVERPORT> ] [ -R <RMPORT> ] [ -S <SCHEDULERPORT> ] [ -t <TYPE> ]\n  [ --version|--help ]\n";

  fprintf(stderr,"usage:  %s %s\n",
    ProgName,
    Msg);

  if (EMsg != NULL)
    {
    fprintf(stderr,"  %s\n",
      EMsg);
    }

  return(0);
  }  /* END PBSShowUsage() */





/*
 * main - the initialization and main loop of pbs_daemon
 */

int main(

  int   argc,    /* I */
  char *argv[])  /* I */

  {
  int	 c;
  FILE  *dummyfile;
  int	 i;
  int	 lockfds;
  int	 rppfd;			/* fd to receive is HELLO's */
  int	 privfd;		/* fd to send is messages */
  uint	 tryport;
  char	 lockfile[MAXPATHLEN + 1];
  char	*pc;
  job	*pjob;
  pbs_queue *pque;
  char	*servicename = NULL;
  pbs_net_t def_pbs_server_addr;
  pid_t	 sid;
  long  *state;
  time_t waittime;
  time_t last_jobstat_time;
  int    when;

  void	 ping_nodes A_((struct work_task *ptask));
  void   check_nodes A_((struct work_task *ptask));
  void   check_log A_((struct work_task *ptask));

  static struct {
    char *it_name;
    int   it_type;
    } init_name_type[] = {
   { "hot",	RECOV_HOT },
   { "warm",	RECOV_WARM },
   { "cold",	RECOV_COLD },
   { "create",	RECOV_CREATE },
   { "",	RECOV_Invalid } };

  extern int   optind;
  extern char *optarg;
  extern char *msg_svrdown;	/* log message   */
  extern char *msg_startup1;	/* log message   */
  extern char *msg_startup2;	/* log message   */

  ProgName = argv[0];

  strcpy(pbs_current_user,"PBS_Server");
  msg_daemonname=strdup(pbs_current_user);

  /* if we are not running with real and effective uid of 0, forget it */

  if ((getuid() != 0) || (geteuid() != 0)) 
    {
    fprintf(stderr,"%s: Must be run by root\n", 
      ProgName);

    return(1);
    }

  /* set standard umask */

  umask(022);

  time_now = time((time_t *)0);

  last_jobstat_time = time_now;

  /* find out who we are (hostname) */

  server_host[0] = '\0';

  if (((server_host[0] == '\0') && (gethostname(server_host,PBS_MAXHOSTNAME) == -1)) ||
       (get_fullhostname(server_host,server_host,PBS_MAXHOSTNAME) == -1)) 
    {
    log_err(-1,"pbsd_main","Unable to get my host name");

    return(-1);
    }

  /* initialize service port numbers for self, Scheduler, and MOM */

  pbs_server_port_dis = get_svrport(PBS_BATCH_SERVICE_NAME,"tcp", 
    PBS_BATCH_SERVICE_PORT_DIS);

  pbs_scheduler_port = get_svrport(PBS_SCHEDULER_SERVICE_NAME,"tcp",
    PBS_SCHEDULER_SERVICE_PORT);

  pbs_mom_port = get_svrport(PBS_MOM_SERVICE_NAME,"tcp",
    PBS_MOM_SERVICE_PORT);

  pbs_rm_port = get_svrport(PBS_MANAGER_SERVICE_NAME,"tcp",
    PBS_MANAGER_SERVICE_PORT);

  strcpy(server_name,server_host);	/* by default server = host */

  pbs_server_addr    = get_hostaddr(server_host);
  pbs_mom_addr 	     = pbs_server_addr;   /* assume on same host */
  pbs_scheduler_addr = pbs_server_addr;   /* assume on same host */

  /* parse the parameters from the command line */

  while ((c = getopt(argc,argv,"A:a:d:fh:p:t:L:M:R:S:-:")) != -1) 
    {
    switch (c) 
      {
      case '-':

        if ((optarg == NULL) || (optarg[0] == '\0'))
          {
          PBSShowUsage("empty command line arg");

          exit(1);
          }

        if (!strcmp(optarg,"version"))
          {
          fprintf(stderr,"version: %s\n",
            PACKAGE_VERSION);

          exit(0);
          }
        else if (!strcmp(optarg,"help"))
          {
          PBSShowUsage(NULL);
    
          exit(1);
          }
        else
          {
          PBSShowUsage("invalid command line arg");

          exit(1);
          }

        break;

      case 'a':

        if (decode_b(
             &server.sv_attr[(int)SRV_ATR_scheduling],
             NULL,
             NULL, 
             optarg) != 0) 
          {
          (void)fprintf(stderr,"%s: bad -a option\n", 
            argv[0]);

          return(1);
          }

        break;

      case 'd':

        path_home = optarg;

        break;

      case 'f':

        TForceUpdate = 1;
 
        break;

      case 'h':

        /* overwrite locally detected hostname with specified hostname */
        /*  (used for multi-homed hosts) */

        strncpy(server_host,optarg,PBS_MAXHOSTNAME);

        if (get_fullhostname(server_host,server_host,PBS_MAXHOSTNAME) == -1)
          {
          log_err(-1,"pbsd_main","Unable to get host name");

          return(1);
          }

        strcpy(server_name,server_host);

        def_pbs_server_addr = pbs_server_addr;

        pbs_server_addr = get_hostaddr(server_host);

        if (pbs_mom_addr == def_pbs_server_addr)
          pbs_mom_addr = pbs_server_addr;

        if (pbs_scheduler_addr == def_pbs_server_addr)
          pbs_scheduler_addr = pbs_server_addr;

        if ((servicename != NULL) && (strlen(servicename) > 0))
          {
          if (strlen(server_name) + strlen(servicename) + 1 > (size_t)PBS_MAXSERVERNAME)
            {
            fprintf(stderr,"%s: -h host too long\n", 
              argv[0]);

            return(1);
            }

          strcat(server_name,":");
          strcat(server_name,servicename);

          if ((pbs_server_port_dis = atoi(servicename)) == 0)
            {
            fprintf(stderr,"%s: host: %s, port: %s, max: %i\n", 
              argv[0],
              server_name,
              servicename,
              PBS_MAXSERVERNAME);

            fprintf(stderr, "%s: -h host invalid\n", 
              argv[0]);

            return(1);
            }
          }    /* END if (strlen(servicename) > 0) */

        break;

      case 'p':

        servicename = optarg;

        if (strlen(server_name) + strlen(servicename) + 1 > (size_t)PBS_MAXSERVERNAME) 
          {
          fprintf(stderr,"%s: -p host:port too long\n",
            argv[0]);

          return(1);
          }

        strcat(server_name,":");
        strcat(server_name,servicename);

        if ((pbs_server_port_dis = atoi(servicename)) == 0) 
          {
          fprintf(stderr,"%s: -p host:port invalid\n", 
            argv[0]);

          return(1);
          }

        break;

      case 't':

        for (i = RECOV_HOT;i < RECOV_Invalid;i++) 
          {
          if (strcmp(optarg,init_name_type[i].it_name) == 0) 
            {
            server_init_type = init_name_type[i].it_type;

            break;
            }
          }    /* END for (i) */

        if (i == RECOV_Invalid) 
          {
          fprintf(stderr, "%s -t bad recovery type\n",
            argv[0]);

          return(1);
          }

        break;

      case 'A':

        acct_file = optarg;

        break;

      case 'L':

        log_file = optarg;

        break;

      case 'M':

        if (get_port(optarg,&pbs_mom_port,&pbs_mom_addr)) 
          {
          fprintf(stderr,"%s: bad -M %s\n", 
            argv[0],
            optarg);

          return(1);
          }

        if (isalpha((int)*optarg)) 
          {
          if ((pc = strchr(optarg, (int)':')) != NULL)
            *pc = '\0';

          mom_host = optarg;
          }

        break;

      case 'R':

        if ((pbs_rm_port = atoi(optarg)) == 0) 
          {
          fprintf(stderr, "%s: bad -R %s\n",
            argv[0], 
            optarg);

          return(1);
          }

        break;

      case 'S':

        if (get_port(
              optarg, 
              &pbs_scheduler_port,
              &pbs_scheduler_addr)) 
          {
          fprintf(stderr,"%s: bad -S %s\n", 
            argv[0],
            optarg);

          return(1);
          }

        break;

      default:

        fprintf(stderr,"%s: unknown option: %c\n",
          argv[0],
          c);

        return(1);

        break;
      }  /* END switch(c) */
    }    /* END while (c) */

  if (optind < argc) 
    {
    fprintf(stderr, "%s: invalid operand\n", 
      argv[0]);

    return(1);
    }

  /* make sure no other server is running with this home directory */

  sprintf(lockfile,"%s/%s/server.lock",
    path_home,
    PBS_SVR_PRIVATE);

  if ((lockfds = open(lockfile,O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0) 
    {
    sprintf(log_buffer, "%s: unable to open lock file",
      msg_daemonname);

    fprintf(stderr,"%s\n", 
      log_buffer);

    log_err(errno,msg_daemonname,log_buffer);

    return(2);
    }

  lock_out(lockfds,F_WRLCK);
	
  server.sv_started = time(&time_now);	/* time server started */

  /*
   * Open the log file so we can start recording events 
   *
   * set log_event_mask to point to the log_event attribute value so
   * it controls which events are logged.
   */

  log_event_mask = &server.sv_attr[SRV_ATR_log_events].at_val.at_long;

  sprintf(path_log,"%s/%s",
    path_home,
    PBS_LOGFILES);

  log_open(log_file,path_log);


  sprintf(log_buffer,msg_startup1,server_name,server_init_type);

  log_event(
    PBSEVENT_SYSTEM | PBSEVENT_ADMIN | PBSEVENT_FORCE,
    PBS_EVENTCLASS_SERVER, 
    msg_daemonname, 
    log_buffer);

  /* initialize the server objects and perform specified recovery */
  /* will be left in the server's private directory		*/

  if ((pc = getenv("PBSLOGLEVEL")) != NULL)
    {
    LOGLEVEL = (int)strtol(pc,NULL,10);
    }

  if ((pc = getenv("PBSDEBUG")) != NULL)
    {
    DEBUGMODE = 1;
    }

  /* NOTE:  env cleared in pbsd_init() */

  if (pbsd_init(server_init_type) != 0) 
    {
    log_err(-1,msg_daemonname,"pbsd_init failed");

    return(3);
    }

  /* initialize the network interface */

  sprintf(log_buffer,"Using ports Server:%d  Scheduler:%d  MOM:%d",
    pbs_server_port_dis, 
    pbs_scheduler_port, 
    pbs_mom_port);

  log_event(
    PBSEVENT_SYSTEM|PBSEVENT_ADMIN, 
    PBS_EVENTCLASS_SERVER,
    msg_daemonname, 
    log_buffer);

  if (init_network(pbs_server_port_dis,process_request) != 0) 
    {
    perror("pbs_server: network");

    log_err(-1,msg_daemonname,"init_network failed dis");

    return(3);
    }

  net_set_type(Secondary,FromClientDIS);	/* "there" */

  if (DEBUGMODE == 0)
    {
    /* go into the background and become own session/process group */

    lock_out(lockfds,F_UNLCK);
	
    if (fork() > 0)
      {
      /* parent goes away */

      exit(0);
      }

    if ((sid = setsid()) == -1) 
      {
      log_err(errno,msg_daemonname,"setsid failed");

      return(2);
      }

    lock_out(lockfds,F_WRLCK);

    fclose(stdin);
    fclose(stdout);
    fclose(stderr);

    dummyfile = fopen("/dev/null","r");
    assert((dummyfile != 0) && (fileno(dummyfile) == 0));

    dummyfile = fopen("/dev/null","w");
    assert((dummyfile != 0) && (fileno(dummyfile) == 1));

    dummyfile = fopen("/dev/null","w");
    assert((dummyfile != 0) && (fileno(dummyfile) == 2));
    }
  else
    {
    if (isdigit(pc[0]))
      LOGLEVEL = (int)strtol(pc,NULL,0);

    sid = getpid();
  
    setvbuf(stdout,NULL,_IOLBF,0);
    setvbuf(stderr,NULL,_IOLBF,0);
    }

  sprintf(log_buffer,"%ld\n", 
    (long)sid);

  if (write(lockfds,log_buffer,strlen(log_buffer)) != (ssize_t)strlen(log_buffer))
    {
    log_err(errno,msg_daemonname,"failed to write pid to lockfile");

    return(-1);
    }
#if (PLOCK_DAEMONS & 1)
  plock(PROCLOCK);
#endif

  if ((rppfd = rpp_bind(pbs_server_port_dis)) == -1) 
    {
    log_err(errno,msg_daemonname,"rpp_bind");

    return(1);
    }

  rpp_fd = -1;		/* force rpp_bind() to get another socket */

  tryport = IPPORT_RESERVED;

  privfd = -1;

  while (--tryport > 0) 
    {
    if ((privfd = rpp_bind(tryport)) != -1)
      break;

    if ((errno != EADDRINUSE) && (errno != EADDRNOTAVAIL))
      break;
    }

  if (privfd == -1) 
    {
    log_err(errno,msg_daemonname,"no privileged ports");

    return(1);
    }

  if (LOGLEVEL >= 5)
    {
    log_event(
      PBSEVENT_SYSTEM | PBSEVENT_FORCE,
      PBS_EVENTCLASS_SERVER,
      msg_daemonname,
      "creating rpp and private interfaces");
    }

  add_conn(rppfd,Primary,(pbs_net_t)0,0,rpp_request);
  add_conn(privfd,Primary,(pbs_net_t)0,0,rpp_request);

  /* record the fact that we are up and running */

  sprintf(log_buffer,msg_startup2,sid,LOGLEVEL);

  log_event(
    PBSEVENT_SYSTEM | PBSEVENT_FORCE, 
    PBS_EVENTCLASS_SERVER,
    msg_daemonname, 
    log_buffer);

  /* do not check nodes immediately as they will initially be marked 
     down unless they have already reported in */

  set_task(WORK_Immed,time_now + 60,check_nodes,NULL);

  /* Just check the nodes with check_nodes above and don't ping anymore. */

  set_task(WORK_Timed,time_now+5,ping_nodes,NULL); 
  

  set_task(WORK_Immed, time_now + 5, check_log, NULL);

  /*
   * Now at last, we are ready to do some batch work.  The
   * following section constitutes the "main" loop of the server
   */

  state = &server.sv_attr[(int)SRV_ATR_State].at_val.at_long;

  DIS_tcp_settimeout(server.sv_attr[(int)SRV_ATR_tcp_timeout].at_val.at_long);

  if (server_init_type == RECOV_HOT)
    *state = SV_STATE_HOT;
  else
    *state = SV_STATE_RUN;

  DBPRT(("pbs_server is up\n"));

  while (*state != SV_STATE_DOWN) 
    {
    /* first process any task whose time delay has expired */

    if (server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long)
      waittime = MIN(next_task(),JobStatRate - (time_now - last_jobstat_time));
    else
      waittime = next_task();

    waittime = MAX(0,waittime);

    if (*state == SV_STATE_RUN) 
      {	
      /* In normal Run state */

      /* if time or event says to run scheduler, do it */

      if ((svr_do_schedule != SCH_SCHEDULE_NULL) && 
           server.sv_attr[(int)SRV_ATR_scheduling].at_val.at_long) 
        {
        server.sv_next_schedule = 
          time_now + 
          server.sv_attr[(int)SRV_ATR_schedule_iteration].at_val.at_long;

        schedule_jobs();
        }
      } 
    else if (*state == SV_STATE_HOT) 
      {
      /* Are there HOT jobs to rerun */
      /* only try every _CYCLE seconds */

      if (time_now > server.sv_hotcycle + SVR_HOT_CYCLE) 
        {
        server.sv_hotcycle = time_now + SVR_HOT_CYCLE;

        c = start_hot_jobs();
        }

      /* If more than _LIMIT seconds since start, stop */

      if ((c == 0) || (time_now > server.sv_started + SVR_HOT_LIMIT)) 
        {
        server_init_type = RECOV_WARM;

        *state = SV_STATE_RUN;
        }
      }

    /* any jobs to route today */

    pque = (pbs_queue *)GET_NEXT(svr_queues);

    while (pque != NULL) 
      {
      if (pque->qu_qs.qu_type == QTYPE_RoutePush)
        queue_route(pque);

      pque = (pbs_queue *)GET_NEXT(pque->qu_link);
      }

    /* touch the rpp streams that need to send */

    rpp_request(42);

    /* wait for a request and process it */

    if (wait_request(waittime,state) != 0) 
      {
      log_err(-1,msg_daemonname,"wait_request failed");
      }

    /* update dynamic loglevel specification */

    /* FIXME: This overrides PBSLOGLEVEL and PBSDEBUG */

    /* if (!getenv("PBSLOGLEVEL") && !getenv("PBSDEBUG"))
     * (and SRV_ATR_LogLevel's at_action can set LOGLEVEL */

    if (!getenv("PBSLOGLEVEL"))
      LOGLEVEL = server.sv_attr[(int)SRV_ATR_LogLevel].at_val.at_long;
    

    /* any running jobs need a status update? */ 

    if (server.sv_attr[(int)SRV_ATR_PollJobs].at_val.at_long && 
       (last_jobstat_time + JobStatRate <= time_now))
      {
      struct work_task *ptask;

      for (pjob = (job *)GET_NEXT(svr_alljobs);
           pjob != NULL;
           pjob = (job *)GET_NEXT(pjob->ji_alljobs)) 
        {
        if (pjob->ji_qs.ji_substate == JOB_SUBSTATE_RUNNING)
          {
          /* spread these out over the next JobStatRate seconds */

          when = pjob->ji_wattr[(int)JOB_ATR_qrank].at_val.at_long % JobStatRate;
    
          ptask = set_task(WORK_Timed,when + time_now,poll_job_task,pjob);

          if (ptask)
            {
            append_link(&pjob->ji_svrtask,&ptask->wt_linkobj,ptask);
            }

          }
        }

      last_jobstat_time = time_now;
      }  /* END if (should poll jobs now) */

    if (*state == SV_STATE_SHUTSIG) 
      svr_shutdown(SHUT_SIG);	/* caught sig */

    /*
     * if in process of shuting down and all running jobs
     * and all children are done, change state to DOWN
     */

    if ((*state > SV_STATE_RUN) &&
        (server.sv_jobstates[JOB_STATE_RUNNING] == 0) &&
        (server.sv_jobstates[JOB_STATE_EXITING] == 0) &&
        (GET_NEXT(task_list_event) == NULL))
      {
      *state = SV_STATE_DOWN;
      }
    }    /* END while (*state != SV_STATE_DOWN) */

  svr_save(&server,SVR_SAVE_FULL);	/* final recording of server */

  track_save(NULL);                     /* save tracking data */

  /* save any jobs that need saving */

  for (pjob = (job *)GET_NEXT(svr_alljobs);
       pjob != NULL;
       pjob = (job *)GET_NEXT(pjob->ji_alljobs)) 
    {
    if (pjob->ji_modified)
      job_save(pjob,SAVEJOB_FULL);
    }

  if (svr_chngNodesfile) 
    {
    /*nodes created/deleted, or props changed and*/
    /*update in req_manager failed; try again    */

    update_nodes_file();
    }

  RPPConfigure(1,0);  /* help rpp_shutdown go a bit faster */
  rpp_shutdown();

  shutdown_ack();

  net_close(-1);		/* close all network connections */

  log_event(
    PBSEVENT_SYSTEM|PBSEVENT_FORCE,
    PBS_EVENTCLASS_SERVER,
    msg_daemonname,
    msg_svrdown);

  acct_close();

  log_close(1);

  return(0);
  }  /* END main() */


void check_log(

 struct work_task *ptask)

 {
 long depth = 1;

 if ((server.sv_attr[(int)SRV_ATR_LogFileMaxSize].at_flags 
      & ATR_VFLAG_SET) != 0)
   {
   if (log_size() >= server.sv_attr[(int)SRV_ATR_LogFileMaxSize].at_val.at_long)
     {
     log_event(
       PBSEVENT_SYSTEM | PBSEVENT_FORCE,
       PBS_EVENTCLASS_SERVER,
       msg_daemonname,
       "Rolling log file");

      if ((server.sv_attr[(int)SRV_ATR_LogFileRollDepth].at_flags 
           & ATR_VFLAG_SET) != 0)
        {
        depth = server.sv_attr[(int)SRV_ATR_LogFileRollDepth].at_val.at_long;
        }

      if ((depth >= INT_MAX) || (depth < 1))
        {
        log_err(-1,"check_log","log roll cancelled, logfile depth is out of range");
        }
      else
        {
        log_roll(depth);
        }
      }
    }
 set_task(WORK_Timed,time_now + PBS_LOG_CHECK_RATE, check_log, NULL);   
 } /* END check_log */



/*
 * get_port - parse host:port for -M and -S option
 *	Returns into *port and *addr if and only if that part is specified
 *	Both port and addr are returned in HOST byte order.
 *	Function return: 0=ok, -1=error
 */

static int get_port(

  char	       *arg,	/* "host", "port", ":port", or "host:port" */
  unsigned int *port,	/* RETURN: new port iff one given 	   */
  pbs_net_t    *addr)	/* RETURN: daemon's address iff host given */

  {
  char *name;

  if (*arg == ':')
    ++arg;

  if (isdigit(*arg)) 
    {	
    /* port only specified */

    *port = (unsigned int)atoi(arg);
    } 
  else 
    {
    name = parse_servername(arg,port);

    if (name != NULL) 
      {
      *addr = get_hostaddr(name);
      } 
    else 
      {
      return(-1);
      }
    }

  if ((*port <= 0) || (*addr == 0))
    {
    return(-1);
    }

  return(0);
  }  /* END get_port() */





/*
 * next_task - look for the next work task to perform:
 *	1. If svr_delay_entry is set, then a delayed task is ready so
 *	   find and process it.
 *	2. All items on the immediate list, then
 *	3. All items on the timed task list which have expired times
 *
 *	Returns: amount of time till next task
 */

static time_t next_task()

  {
  time_t	     delay;
  struct work_task  *nxt;
  struct work_task  *ptask;
  time_t	     tilwhen = server.sv_attr[(int)SRV_ATR_schedule_iteration].at_val.at_long;

  time_now = time((time_t *)0);

  if (svr_delay_entry) 
    {
    ptask = (struct work_task *)GET_NEXT(task_list_event);

    while (ptask != NULL) 
      {
      nxt = (struct work_task *)GET_NEXT(ptask->wt_linkall);

      if (ptask->wt_type == WORK_Deferred_Cmp)
        dispatch_task(ptask);

      ptask = nxt;
      }

    svr_delay_entry = 0;
    }

  while ((ptask = (struct work_task *)GET_NEXT(task_list_immed)) != NULL)
    dispatch_task(ptask);

  while ((ptask = (struct work_task *)GET_NEXT(task_list_timed)) != NULL) 
    {
    if ((delay = ptask->wt_event - time_now) > 0) 
      {
      if (tilwhen > delay) 
        tilwhen = delay;
 
      break;
      } 
    else 
      {
      dispatch_task(ptask);	/* will delete link */
      }
    }

  /* should the scheduler be run?  If so, adjust the delay time  */

  if ((delay = server.sv_next_schedule - time_now) <= 0)
    svr_do_schedule = SCH_SCHEDULE_TIME;
  else if (delay < tilwhen)
    tilwhen = delay;

  return(tilwhen);
  }  /* END next_task() */





/*
 * start_hot_jobs - place any job which is state QUEUED and has the
 *	HOT start flag set into execution.
 * 
 *	Returns the number of jobs to be hot started.
 */

static int start_hot_jobs(void)

  {
  int  ct = 0;
  job *pjob;

  pjob = (job *)GET_NEXT(svr_alljobs);

  while (pjob != NULL) 
    {
    if ((pjob->ji_qs.ji_substate == JOB_SUBSTATE_QUEUED) &&
        (pjob->ji_qs.ji_svrflags & JOB_SVFLG_HOTSTART)) 
      {
      log_event(
        PBSEVENT_SYSTEM, 
        PBS_EVENTCLASS_JOB,
        pjob->ji_qs.ji_jobid, 
        "attempting to hot start job");

      svr_startjob(pjob,NULL,NULL,NULL);

      ct++;
      }

    pjob = (job *)GET_NEXT(pjob->ji_alljobs);
    }  /* END while (pjob != NULL) */

  return(ct);
  }  /* END start_hot_jobs() */





/*
 * lock_out - lock out other daemons from this directory.
 */

static void lock_out(

  int fds,
  int op)		/* F_WRLCK  or  F_UNLCK */

  {
  struct flock flock;

  flock.l_type   = op;
  flock.l_whence = SEEK_SET;
  flock.l_start  = 0;
  flock.l_len    = 0;

  if (fcntl(fds,F_SETLK,&flock) < 0) 
    {
    strcpy(log_buffer,"pbs_server: another server running\n");

    log_err(errno,msg_daemonname,log_buffer);

    fprintf(stderr,log_buffer);

    exit(1);
    }

  return;
  }

/* END pbsd_main.c */

