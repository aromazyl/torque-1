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

static char ident[] = "@(#) $RCSfile$ $Revision$";

int main(argc, argv, envp) /* qdel */
int argc;
char **argv;
char **envp;
{
    int c;
    int errflg=0;
    int any_failed=0;
    char *pc;

    char job_id[PBS_MAXCLTJOBID];	/* from the command line */

    char job_id_out[PBS_MAXCLTJOBID];
    char server_out[MAXSERVERNAME];
    char rmt_server[MAXSERVERNAME];

#define MAX_TIME_DELAY_LEN 32
    char warg[MAX_TIME_DELAY_LEN+1];

#define GETOPT_ARGS "W:"
                 
    warg[0]='\0';

    while ((c = getopt (argc, argv, GETOPT_ARGS )) != EOF)
        switch (c) {
        case 'W':
            pc = optarg;
            if ( strlen(pc) == 0 ) {
                fprintf(stderr, "qdel: illegal -W value\n");
                errflg++;
		break;
            }
            while ( *pc != '\0' ) {
                if ( ! isdigit(*pc) ) {
                    fprintf(stderr, "qdel: illegal -W value\n");
                    errflg++;
		    break;
                }
                pc++;
            }
            strcpy(warg, DELDELAY);
            strcat(warg, optarg);
            break;
        default :
            errflg++;
        }

    if (errflg || optind >= argc) {
        static char usage[]="usage: qdel [-W delay] job_identifier...\n";
        fprintf(stderr, usage);
        exit (2);
    }

    for ( ; optind < argc; optind++) {
        int connect;
        int stat=0;
	int located = FALSE;

        strcpy(job_id, argv[optind]);
        if ( get_server(job_id, job_id_out, server_out) ) {
            fprintf(stderr, "qdel: illegally formed job identifier: %s\n", job_id);
            any_failed = 1;
            continue;
        }
cnt:
        connect = cnt2server(server_out);
        if ( connect <= 0 ) {
            fprintf(stderr, "qdel: cannot connect to server %s (errno=%d)\n",
                    pbs_server, pbs_errno);
            any_failed = pbs_errno;
            continue;
        }

        stat = pbs_deljob(connect, job_id_out, warg);
        if ( stat && (pbs_errno != PBSE_UNKJOBID) ) {
	    prt_job_err("qdel", connect, job_id_out);
            any_failed = pbs_errno;
        } else if ( stat && (pbs_errno == PBSE_UNKJOBID) && !located ) {
	    located = TRUE;
	    if ( locate_job(job_id_out, server_out, rmt_server) ) {
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
}
