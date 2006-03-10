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

#if !defined(_BSD) && defined(_AIX)   /* this is needed by AIX */
#define _BSD	1
#endif

#include <pbs_config.h>   /* the master config generated by configure */

#include	<stdio.h>
#include	<stdlib.h>
#include        <unistd.h>
#include        <string.h>
#include        <signal.h>
#include        <stdlib.h>
#include	<time.h>
#include        <ctype.h>
#include        <limits.h>
#include        <fcntl.h>
#include	<errno.h>
#include	<netdb.h>
#include	<tcl.h>
#include        <sys/types.h>
#if (PLOCK_DAEMONS & 2)
#include        <sys/lock.h>
#endif	/* PLOCK_DAEMONS */
#include        <sys/stat.h>
#include        <sys/socket.h>
#include        <netinet/in.h>
#include        <arpa/inet.h>
#ifdef _CRAY
#include	<sys/category.h>
#endif	/* _CRAY */
#include	<sys/time.h>
#include	<sys/resource.h>

#include	"libpbs.h"
#include	"portability.h"
#include	"pbs_error.h"
#include	"pbs_ifl.h"
#include	"log.h"
#include	"resmon.h"
#include	"sched_cmds.h"
#include	"net_connect.h"

static char ident[] = "@(#) $RCSfile$ $Revision$";

extern	struct	connect_handle connection[];
extern	int	pbs_errno;
extern	int	connector;
int		server_sock;

Tcl_Interp	*interp;
int		verbose = 0;
char		*oldpath;
char		*initfil = NULL;
char		*bodyfil = "sched_tcl";
char		*termfil = NULL;
char		*body = NULL;
extern char		*msg_daemonname;
char		**glob_argv = NULL;
struct	sockaddr_in	saddr;

#if TCL_MAJOR_VERSION >= 8
Tcl_Obj         *body_obj = NULL; /* allow compiled body code for TCL-8 */
#endif
 
#define		START_CLIENTS	2	/* minimum number of clients */
pbs_net_t	*okclients = NULL;	/* accept connections from */
int		numclients = 0;		/* the number of clients */
char		*configfile = NULL;	/* name of config file */

sigset_t	allsigs;
sigset_t	oldsigs;

static char	*logfile = NULL;
static char	 path_log[_POSIX_PATH_MAX];

#ifndef	OPEN_MAX
#ifdef	_POSIX_OPEN_MAX
#define	OPEN_MAX	_POSIX_OPEN_MAX
#else
#define	OPEN_MAX	40
#endif	/* _POSIX_OPEN_MAX */
#endif	/* OPEN_MAX */

/*
**      Clean up after a signal.
*/
void
die(sig)
int     sig;
{
	char    *id = "die";

	if (sig > 0) {
		sprintf(log_buffer, "caught signal %d", sig);
		log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
			id, log_buffer);
	}
	else {
		log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
			id, "abnormal termination");
	}

	if (interp)
		Tcl_DeleteInterp(interp);
	log_close(1);
	exit(1);
}

static int server_disconnect(connect)
int connect;
{

	/* send close-connection message */

	close( connection[connect].ch_socket );

	if( connection[connect].ch_errtxt != (char *)NULL ) 
		free( connection[connect].ch_errtxt );
	connection[connect].ch_errno = 0;
	connection[connect].ch_inuse = 0;
	return 0;
}

/*
**	Got an alarm call.
*/
void
toolong(sig)
int	sig;
{
	char	*id = "toolong";
	struct	stat	sb;
	pid_t	cpid;

	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, "alarm call");
	DBPRT(("alarm call\n"))

	if (connector >= 0 && server_disconnect(connector))
		log_err(errno, id, "server_disconnect");
	if (close(server_sock))
		log_err(errno, id, "close");

	if ((cpid = fork()) > 0) {	/* parent re-execs itself */
		rpp_terminate();
#ifndef linux
		sleep(5);
#endif

		/* hopefully, that gave the child enough */
		/*   time to do its business. anyhow:    */
		(void)waitpid(cpid, NULL, 0);

		if (chdir(oldpath) == -1) {
			sprintf(log_buffer, "chdir to %s", oldpath);
			log_err(errno, id, log_buffer);
		}

		sprintf(log_buffer, "restart dir %s object %s",
			oldpath, glob_argv[0]);
		log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
			id, log_buffer);

		execv(glob_argv[0], glob_argv);
		log_err(errno, id, "execv");
		exit(3);
	}

	/*
	**	Child (or error on fork) gets here and tried
	**	to dump core.
	*/
	if (stat("core", &sb) == -1) {
		if (errno == ENOENT) {
			log_close(1);
			abort();
			rpp_terminate();
			exit(2);	/* not reached (hopefully) */
		}
		log_err(errno, id, "stat");
	}
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
		id, "exiting without core dump");
	log_close(1);
	rpp_terminate();
	exit(0);
}

int
socket_to_conn(sock)
    int sock;		/* opened socket */
{
	int     i;

	for (i=0; i<NCONNECTS; i++) {
		if (connection[i].ch_inuse == 0) {

			connection[i].ch_inuse = 1;
			connection[i].ch_errno = 0;
			connection[i].ch_socket= sock;
			connection[i].ch_errtxt = NULL;
			return (i);
		}
	}
	pbs_errno = PBSE_NOCONNECTS;
	return (-1);
}

void
start_tcl()
{
	char	*id = "start_tcl";
	char	buf[BUFSIZ];
	int	fd;
	int	tot, len;

	interp = Tcl_CreateInterp();
	if (Tcl_Init(interp) == TCL_ERROR) {
		sprintf(log_buffer, "Tcl_Init error: %s",
			Tcl_GetStringResult(interp));
		log_err(-1, id, log_buffer);
		die(0);
	}
#if	TCLX
#if	TCL_MINOR_VERSION < 5  && TCL_MAJOR_VERSION < 8
	if (TclX_Init(interp) == TCL_ERROR) {
#else
	if (Tclx_Init(interp) == TCL_ERROR) {
#endif
		sprintf(log_buffer, "Tclx_Init error: %s",
			Tcl_GetStringResult(interp));
		log_err(-1, id, log_buffer);
		die(0);
	}
#endif
	add_cmds(interp);

	if (initfil) {
		int		code;

		code = Tcl_EvalFile(interp, initfil);
		if (code != TCL_OK) {
			char	*trace;

			trace = Tcl_GetVar(interp, "errorInfo", 0);
			if (trace == NULL)
				trace = Tcl_GetStringResult(interp);

			fprintf(stderr, "%s: TCL error @ line %d: %s\n",
				initfil, interp->errorLine, trace);
			sprintf(log_buffer, "%s: TCL error @ line %d: %s",
				initfil, interp->errorLine,
				Tcl_GetStringResult(interp));
			log_err(-1, id, log_buffer);
			die(0);
		}
		sprintf(log_buffer, "init file %s", initfil);
		log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
			id, log_buffer);
	}

	if ((fd = open(bodyfil, O_RDONLY)) == -1) {
		log_err(errno, id, bodyfil);
		die(0);
	}
	sprintf(log_buffer, "body file: %s", bodyfil);
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

	if (body)
		free(body);
	if ((body = malloc(BUFSIZ)) == NULL) {
		log_err(errno, id, "malloc");
		die(0);
	}

	for (tot=0; (len = read(fd, buf, sizeof(buf))) > 0; tot+=len) {
		if ((body = realloc(body, tot+len+1)) == NULL) {
			log_err(errno, id, "realloc");
			die(0);
		}
		memcpy(&body[tot], buf, len);
	}
	if (len == -1) {
		log_err(errno, id, bodyfil);
		die(0);
	}
	body[tot] = '\0';
	close(fd);

#if TCL_MAJOR_VERSION >= 8
         if (body_obj == NULL) {
           body_obj = Tcl_NewStringObj(body, tot);
           Tcl_IncrRefCount(body_obj);
         } else {
           Tcl_SetStringObj(body_obj, body, tot);
         }
#endif
}

int
addclient(name)
    char	*name;
{
	static	char	id[] = "addclient";
	struct	hostent		*host, *gethostbyname();
	struct  in_addr saddr;

	if ((host = gethostbyname(name)) == NULL) {
		sprintf(log_buffer, "host %s not found", name);
		log_err(-1, id, log_buffer);
		return -1;
	}
	if (numclients >= START_CLIENTS) {
		pbs_net_t	*newclients;

		newclients = realloc(okclients,
				sizeof(pbs_net_t)*(numclients+1));
		if (newclients == NULL)
			return -1;
		okclients = newclients;
	}
	memcpy((char *)&saddr, host->h_addr, host->h_length);
	okclients[numclients++] = saddr.s_addr;
	return 0;
}

/*
 * read_config - read and process the configuration file (see -c option)
 *
 *	Currently, the only statement is $clienthost to specify which systems
 *	can contact the scheduler.
 */
#define CONF_LINE_LEN 120

static
int
read_config(file)
    char	*file;
{
	static	char *id = "read_config";
	FILE	*conf;
	int	i;
	char	line[CONF_LINE_LEN];
	char	*token;
	struct	specialconfig {
		char	*name;
		int	(*handler)();
	} special[] = {
		{"clienthost",	addclient },
		{ NULL,		NULL }
	};


#if !defined(DEBUG) && !defined(NO_SECURITY_CHECK)
	if (chk_file_sec(file, 0, 0, S_IWGRP|S_IWOTH, 1))
		return (-1);
#endif

	if ((conf = fopen(file, "r")) == NULL) {
		log_err(errno, id, "cannot open config file");
		return (-1);
	}
	while (fgets(line, CONF_LINE_LEN, conf)) {

		if ((line[0] == '#') || (line[0] == '\n'))
			continue;		/* ignore comment & null line */
		else if (line[0] == '$') {	/* special */

			if ((token = strtok(line, " \t")) == NULL)
				token = "";
			for (i=0; special[i].name; i++) {
				if (strcmp(token+1, special[i].name) == 0)
					break;
			}
			if (special[i].name == NULL) {
				sprintf(log_buffer,"config name %s not known",
					token);
				log_record(PBSEVENT_ERROR,
					PBS_EVENTCLASS_SERVER,
					msg_daemonname, log_buffer);
				return (-1);
			}
			token = strtok(NULL, " \t");
			if (*(token+strlen(token)-1) == '\n')
				*(token+strlen(token)-1) = '\0';
			if (special[i].handler(token)) {
				fclose(conf);
				return (-1);
			}

		} else {
			log_record(PBSEVENT_ERROR, PBS_EVENTCLASS_SERVER,
				  msg_daemonname,
				  "invalid line in config file");
			fclose(conf);
			return (-1);
		}
	}
	fclose(conf);
	return (0);
}

void
restart(sig)
    int	sig;
{
	char    *id = "restart";

	if (sig) {
		sprintf(log_buffer, "restart on signal %d", sig);
		log_close(1);
		log_open(logfile, path_log);
	} else {
		sprintf(log_buffer, "restart command");
	}
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
	Tcl_DeleteInterp(interp);
	if (configfile) {
		if (read_config(configfile) != 0)
			die(0);
	}
	start_tcl();
}

void
badconn(msg)
    char	*msg;
{
	static	char	id[] = "badconn";
	struct	in_addr	addr;
	char		buf[5*sizeof(addr) + 100];
	struct	hostent	*phe;

	addr = saddr.sin_addr;
	phe = gethostbyaddr((void *)&addr, sizeof(addr), AF_INET);
	if (phe == NULL) {
		char	hold[6];
		int	i;
		union {
			struct	in_addr aa;
			u_char		bb[sizeof(addr)];
		} uu;

		uu.aa = addr;
		sprintf(buf, "%u", uu.bb[0]);
		for(i=1; i<sizeof(addr); i++) {
			sprintf(hold, ".%u", uu.bb[i]);
			strcat(buf, hold);
		}
	}
	else {
		strncpy(buf, phe->h_name, sizeof(buf));
		buf[sizeof(buf)-1] = '\0';
	}

	sprintf(log_buffer, "%s on port %u %s", buf, ntohs(saddr.sin_port),msg);
	log_err(-1, id, log_buffer);
	return;
}

int
server_command()
{
	static	char	id[] = "server_command";
	int		new_socket;
	int		i, slen;
	int		cmd;
	pbs_net_t	addr;

	slen = sizeof(saddr);
	new_socket = accept(server_sock,
			(struct sockaddr *)&saddr, &slen);
	if (new_socket == -1) {
		log_err(errno, id, "accept");
		return SCH_ERROR;
	}

	if (ntohs(saddr.sin_port) >= IPPORT_RESERVED) {
		badconn("non-reserved port");
		close(new_socket);
		return SCH_ERROR;
	}

	addr = (pbs_net_t)saddr.sin_addr.s_addr;
	for (i=0; i<numclients; i++) {
		if (addr == okclients[i])
			break;
	}
	if (i == numclients) {
		badconn("unauthorized host");
		close(new_socket);
		return SCH_ERROR;
	}

	if ((connector = socket_to_conn(new_socket)) < 0) {
		log_err(errno, id, "socket_to_conn");
		return SCH_ERROR;
	}

	if (get_4byte(new_socket, &cmd) != 1) {
		log_err(errno, id, "get4bytes");
		return SCH_ERROR;
	}
	return cmd;
}


/*
 * lock_out - lock out other daemons from this directory.
 */

static void lock_out(fds, op)
	int fds;
	int op;		/* F_WRLCK  or  F_UNLCK */
{
	struct flock flock;

	flock.l_type   = op;
	flock.l_whence = SEEK_SET;
	flock.l_start  = 0;
	flock.l_len    = 0;	/* whole file */
	if (fcntl(fds, F_SETLK, &flock) < 0) {
	    (void)strcpy(log_buffer, "pbs_sched: another scheduler running\n");
	    log_err(errno, msg_daemonname, log_buffer);
	    fprintf(stderr, log_buffer);
	    exit (1);
	}
}

main(argc, argv)
    int		argc;
    char	*argv[];
{
	char		*id = "main";
	int		code;
	struct	hostent	*hp;
	int		go, c, errflg = 0;
	int		lockfds;
	int		t = 1;
	char		*ptr;
	pid_t		pid;
	char		*cp, host[100];
	char		*homedir = PBS_SERVER_HOME;
	unsigned int	port;
	char		path_priv[_POSIX_PATH_MAX];
	char		*dbfile = "sched_out";
	int		alarm_time = 180;
	struct	sigaction	act;
	caddr_t		curr_brk, next_brk;
	extern	char	*optarg;
	extern	int	optind, opterr;
	extern	int	rpp_fd;
	fd_set		fdset;

#ifndef DEBUG
	if ((geteuid() != 0) || (getuid() != 0)) {
		fprintf(stderr, "%s: Must be run by root\n", argv[0]);
		return (1);
	}
#endif	/* DEBUG */

	glob_argv = argv;
	if ((cp = strrchr(argv[0], '/')) == NULL)
		cp = argv[0];
	else
		cp++;
	msg_daemonname = strdup(cp);
	port = get_svrport(PBS_SCHEDULER_SERVICE_NAME, "tcp",
			   PBS_SCHEDULER_SERVICE_PORT);

	while ((c = getopt(argc, argv, "L:S:d:i:b:t:p:a:vc:")) != EOF) {
		switch (c) {
		case 'L':
			logfile = optarg;
			break;
		case 'S':
			port = (unsigned int)atoi(optarg);
			if (port == 0) {
				fprintf(stderr,
					"%s: illegal port\n", optarg);
				errflg = 1;
			}
			break;
		case 'd':
			homedir = optarg;
			break;
		case 'i':		/* initialize */
			initfil = optarg;
			break;
		case 'b':
			bodyfil = optarg;
			break;
		case 't':
			termfil = optarg;
			break;
		case 'p':
			dbfile = optarg;
			break;
		case 'a':
			alarm_time = strtol(optarg, &ptr, 10);
			if (alarm_time <= 0 || *ptr != '\0') {
				fprintf(stderr,
					"%s: bad alarm time\n", optarg);
				errflg = 1;
			}
			break;
		case 'c':
			configfile = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			errflg = 1;
			break;
		}
	}
	if (errflg || optind != argc) {
		static	char	*options[] = {
			"[-L logfile]",
			"[-S port]",
			"[-d home]",
			"[-i init]",
			"[-b body]",
			"[-t term]",
			"[-p output]",
			"[-a alarm]",
			"[-c configfile]",
			"[-v]",
			NULL
		};
		int	i;

		fprintf(stderr, "usage: %s\n", argv[0]);
		for (i = 0; options[i]; i++)
			fprintf(stderr, "\t%s\n", options[i]);
		exit(1);
	}

	/* Save the original working directory for "restart" */
	if ((oldpath = getcwd((char *)NULL, MAXPATHLEN)) == NULL) {
		fprintf(stderr, "cannot get current working directory\n");
		exit(1);
	}

	(void)sprintf(path_priv, "%s/sched_priv", homedir);
#if !defined(DEBUG) && !defined(NO_SECURITY_CHECK)
	c  = chk_file_sec(path_priv, 1, 0, S_IWGRP|S_IWOTH, 1);
	c |= chk_file_sec(PBS_ENVIRON, 0, 0, S_IWGRP|S_IWOTH, 0);
	if (c != 0) exit(1);
#endif  /* not DEBUG and not NO_SECURITY_CHECK */
	if (chdir(path_priv) == -1) {
		perror(path_priv);
		exit(1);
	}
	(void)sprintf(path_log, "%s/sched_logs", homedir);
	(void)strcpy(pbs_current_user, "Scheduler");

	/* The following is code to reduce security risks                */
	/* start out with standard umask, system resource limit infinite */

	umask(022);
	if (setup_env(PBS_ENVIRON)==-1)
		exit(1);
	c = getgid();
	(void)setgroups(1, (gid_t *)&c);	/* secure suppl. group ids */
	c = sysconf(_SC_OPEN_MAX);
	while (--c > 2)
		(void)close(c);	/* close any file desc left open by parent */
#ifndef DEBUG
#ifdef _CRAY
	(void)limit(C_JOB,      0, L_CPROC, 0);
	(void)limit(C_JOB,      0, L_CPU,   0);
	(void)limit(C_JOBPROCS, 0, L_CPU,   0);
	(void)limit(C_PROC,     0, L_FD,  255);
	(void)limit(C_JOB,      0, L_FSBLK, 0);
	(void)limit(C_JOBPROCS, 0, L_FSBLK, 0);
	(void)limit(C_JOB,      0, L_MEM  , 0);
	(void)limit(C_JOBPROCS, 0, L_MEM  , 0);
#else	/* not  _CRAY */
	{
	struct rlimit rlimit;

	rlimit.rlim_cur = RLIM_INFINITY;
	rlimit.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU,   &rlimit);
	(void)setrlimit(RLIMIT_FSIZE, &rlimit);
	(void)setrlimit(RLIMIT_DATA,  &rlimit);
	(void)setrlimit(RLIMIT_STACK, &rlimit);
#ifdef  RLIMIT_RSS
        (void)setrlimit(RLIMIT_RSS  , &rlimit);
#endif  /* RLIMIT_RSS */
#ifdef  RLIMIT_VMEM
        (void)setrlimit(RLIMIT_VMEM  , &rlimit);
#endif  /* RLIMIT_VMEM */
	}
#endif	/* not _CRAY */

#if !defined(NO_SECURITY_CHECK)
	c = 0;
	if (initfil) {
		if (*initfil != '/') {
			(void)sprintf(log_buffer, "%s/%s", path_priv, initfil);
			c |= chk_file_sec(log_buffer, 0, 0, S_IWGRP|S_IWOTH, 1);
		} else {
			c |= chk_file_sec(initfil, 0, 0, S_IWGRP|S_IWOTH, 1);
		}
	}
	if (bodyfil) {
		if (*bodyfil != '/') {
			(void)sprintf(log_buffer, "%s/%s", path_priv, bodyfil);
			c |= chk_file_sec(log_buffer, 0, 0, S_IWGRP|S_IWOTH, 1);
		} else {
			c |= chk_file_sec(bodyfil, 0, 0, S_IWGRP|S_IWOTH, 1);
		}
	}
	if (termfil) {
		if (*termfil != '/') {
			(void)sprintf(log_buffer, "%s/%s", path_priv, termfil);
			c |= chk_file_sec(log_buffer, 0, 0, S_IWGRP|S_IWOTH, 1);
		} else {
			c |= chk_file_sec(termfil, 0, 0, S_IWGRP|S_IWOTH, 1);
		}
	}
	if (c) exit(1);
			
#endif	/* not NO_SECURITY_CHECK */
#endif	/* not DEBUG */

	if (log_open(logfile, path_log) == -1) {
		fprintf(stderr, "%s: logfile could not be opened\n", argv[0]);
		exit(1);
	}

	if (gethostname(host, sizeof(host)) == -1) {
		char	*prob = "gethostname";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}
	if ((hp =gethostbyname(host)) == NULL) {
		char	*prob = "gethostbyname";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}
	if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		char	*prob = "socket";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,
			(char *)&t, sizeof(t)) == -1) {
		char	*prob = "setsockopt";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons((unsigned short)port);
	memcpy (&saddr.sin_addr, hp->h_addr, hp->h_length);
	if (bind (server_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		char	*prob = "bind";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}
	if (listen (server_sock, 5) < 0) {
		char	*prob = "listen";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}

	okclients = (pbs_net_t *)calloc(START_CLIENTS, sizeof(pbs_net_t));
	addclient("localhost");   /* who has permission to call MOM */
	addclient(host);
	if (configfile) {
		if (read_config(configfile) != 0)
			die(0);
	}

	lockfds = open("sched.lock", O_CREAT | O_TRUNC | O_WRONLY, 0644);
	if (lockfds < 0) {
		char	*prob = "lock file";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}
	lock_out(lockfds, F_WRLCK);

#ifndef	DEBUG
	lock_out(lockfds, F_UNLCK);
	if ((pid = fork()) == -1) {     /* error on fork */
		char	*prob = "fork";

		log_err(errno, id, prob);
		perror(prob);
		die(0);
	}
	else if (pid > 0)               /* parent exits */
		exit(0);
	if ((pid =setsid()) == -1) {
		log_err(errno, id, "setsid");
		die(0);
	}
	lock_out(lockfds, F_WRLCK);
	freopen(dbfile, "a", stdout);
	setvbuf(stdout, NULL, _IOLBF, 0);
	dup2(fileno(stdout), fileno(stderr));
#else
	pid = getpid();
	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);
#endif
	freopen("/dev/null", "r", stdin);

	/* write schedulers pid into lockfile */
	(void)sprintf(log_buffer, "%d\n", pid);
	(void)write(lockfds, log_buffer, strlen(log_buffer)+1);

#if (PLOCK_DAEMONS & 2)
	(void)plock(PROCLOCK);	/* lock daemon into memory */
#endif

	sprintf(log_buffer, "%s startup pid %d", argv[0], pid);
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

 	sprintf(log_buffer, "%s using TCL %s (%s)", argv[0],
 		TCL_VERSION, TCL_PATCH_LEVEL);
 	fprintf(stderr, "%s\n", log_buffer);
 	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);
 
	fullresp(0);
	sigemptyset(&allsigs);
	act.sa_flags = 0;
	sigaddset(&allsigs, SIGHUP);    /* remember to block these */
	sigaddset(&allsigs, SIGINT);    /* during critical sections */
	sigaddset(&allsigs, SIGTERM);   /* so we don't get confused */
	act.sa_mask = allsigs;

	act.sa_handler = restart;       /* do a restart on SIGHUP */
	sigaction(SIGHUP, &act, NULL);

	act.sa_handler = toolong;	/* handle an alarm call */
	sigaction(SIGALRM, &act, NULL);

	act.sa_handler = die;           /* bite the biscuit for all following */
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	start_tcl();

	FD_ZERO(&fdset);
	for (go=1; go;) {
		int	cmd;

		if (rpp_fd != -1)
			FD_SET(rpp_fd, &fdset);
		FD_SET(server_sock, &fdset);
		if (select(FD_SETSIZE, &fdset, NULL, NULL, NULL) == -1) {
			if (errno != EINTR)
				log_err(errno, id, "select");
			continue;
		}

		if (rpp_fd != -1 && FD_ISSET(rpp_fd, &fdset)) {
			if (rpp_io() == -1)
				log_err(errno, id, "rpp_io");
		}
		if (!FD_ISSET(server_sock, &fdset))
			continue;

		cmd = server_command();
		if (cmd == SCH_ERROR || cmd == SCH_SCHEDULE_NULL)
			continue;

		if (sigprocmask(SIG_BLOCK, &allsigs, &oldsigs) == -1)
			log_err(errno, id, "sigprocmaskSIG_BLOCK)");

		if (verbose) {
			sprintf(log_buffer, "command %d", cmd);
			log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
				id, log_buffer);
		}

		switch (cmd) {

		case SCH_SCHEDULE_NEW:
		case SCH_SCHEDULE_TERM:
		case SCH_SCHEDULE_TIME:
		case SCH_SCHEDULE_RECYC:
		case SCH_SCHEDULE_CMD:
		case SCH_SCHEDULE_FIRST:
			alarm(alarm_time);

#if TCL_MAJOR_VERSION >= 8
                        /* execute compiled body code for TCL-8 */
 			code = Tcl_EvalObj(interp, body_obj);
#else
			code = Tcl_Eval(interp, body);
#endif
			alarm(0);
 			switch (code) {
 			  case TCL_OK:
 			  case TCL_RETURN:
 			    break;
 			  default:
 			    {

				char	*trace;
				char 	codename[20];
 
 				switch (code) {
 				  case TCL_BREAK:
 				    strcpy(codename, "break");
 				    break;
 				  case TCL_CONTINUE:
 				    strcpy(codename, "continue");
				    break;
 				  default:
 				    strcpy(codename, "<unknown>");
 				    break;
 				}

				trace = Tcl_GetVar(interp, "errorInfo", 0);
				if (trace == NULL)
					trace = Tcl_GetStringResult(interp);

				fprintf(stderr, "%s: TCL interpreter return code %d (%s) @ line %d: %s\n",
 					bodyfil, code, codename,
 					interp->errorLine, trace);

				sprintf(log_buffer,
					"%s: TCL error @ line %d: %s",
					bodyfil, interp->errorLine,
					Tcl_GetStringResult(interp));
				log_err(-1, id, log_buffer);
				die(0);
			    }
			}
			break;

		case SCH_CONFIGURE:
		case SCH_RULESET:
			restart(0);
			break;

		case SCH_QUIT:
			go = 0;
			break;

		default:
			log_err(-1, id, "unknown command");
			break;
		}
		if (connector >= 0 && server_disconnect(connector)) {
			log_err(errno, id, "server_disconnect");
			die(0);
		}
		connector = -1;
		if (verbose) {
			next_brk = (caddr_t)sbrk(0);
			if (next_brk > curr_brk) {
				sprintf(log_buffer, "brk point %ld", next_brk);
				log_record(PBSEVENT_SYSTEM,
					PBS_EVENTCLASS_SERVER,
					id, log_buffer);
				curr_brk = next_brk;
			}
		}
		if (sigprocmask(SIG_SETMASK, &oldsigs, NULL) == -1)
			log_err(errno, id, "sigprocmask(SIG_SETMASK)");
	}

	if (termfil) {
		code = Tcl_EvalFile(interp, termfil);
		if (code != TCL_OK) {
			char	*trace;

			trace = Tcl_GetVar(interp, "errorInfo", 0);
			if (trace == NULL)
				trace = Tcl_GetStringResult(interp);

			fprintf(stderr, "%s: TCL error @ line %d: %s\n",
				termfil, interp->errorLine, trace);
			sprintf(log_buffer, "%s: TCL error @ line %d: %s",
				termfil, interp->errorLine,
				Tcl_GetStringResult(interp));
			log_err(-1, id, log_buffer);
			die(0);
		}
		sprintf(log_buffer, "term file: %s", termfil);
		log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER,
			id, log_buffer);
	}
	sprintf(log_buffer, "%s normal finish pid %d", argv[0], pid);
	log_record(PBSEVENT_SYSTEM, PBS_EVENTCLASS_SERVER, id, log_buffer);

	(void)close(server_sock);
	exit(0);
}
