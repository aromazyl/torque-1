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
 * svr_mail.c - send mail to mail list or owner of job on
 *	job begin, job end, and/or job abort
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include "pbs_ifl.h"
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"
#include "job.h"
#include "log.h"
#include "server.h"


/* External Functions Called */

extern void net_close A_((int));

/* Global Data */

extern struct server server;
extern char *msg_job_abort;
extern char *msg_job_start;
extern char *msg_job_end;
extern char *msg_job_del;
extern char *msg_job_stageinfail;

extern int LOGLEVEL;

void svr_mailowner(

  job	*pjob,       /* I */
  int 	 mailpoint,  /* note, single character  */
  int	 force,	     /* if set, force mail delivery */
  char	*text)	     /* additional message text */

  {
  char	*cmdbuf;
  int	 i;
  char	*mailfrom;
  char	 mailto[512];
  FILE	*outmail;
  struct array_strings *pas;
  char	*stdmessage = NULL;

  if (LOGLEVEL >= 3)
    {
    snprintf(log_buffer,LOG_BUF_SIZE,"sending '%c' mail for job %s to %s (%.64s)\n",
      (char)mailpoint,
      pjob->ji_qs.ji_jobid,
      pjob->ji_wattr[(int)JOB_ATR_job_owner].at_val.at_str,
      (text != NULL) ? text : "---");

    log_event(
      PBSEVENT_ERROR|PBSEVENT_ADMIN|PBSEVENT_JOB,
      PBS_EVENTCLASS_JOB,
      pjob->ji_qs.ji_jobid,
      log_buffer);
    }

  /* if force is true, force the mail out regardless of mailpoint */

  if (force != MAIL_FORCE) 
    {
    /* see if user specified mail of this type */

    if (pjob->ji_wattr[(int)JOB_ATR_mailpnts].at_flags &ATR_VFLAG_SET) 
      {
      if (strchr(
            pjob->ji_wattr[(int)JOB_ATR_mailpnts].at_val.at_str,
            mailpoint) == NULL)
        {
        return;
        }
      } 
    else if (mailpoint != MAIL_ABORT)	/* not set, default to abort */
      {
      return;
      }
    }

  /*
   * ok, now we will fork a process to do the mailing to not
   * hold up the server's other work.
   */

  if (fork()) 
    {
    return;		/* its all up to the child now */
    }

  /*
   * From here on, we are a child process of the server.
   * Fix up file descriptors and signal handlers.
   */

  rpp_terminate();
  net_close(-1);

  /* Who is mail from, if SRV_ATR_mailfrom not set use default */

  if ((mailfrom = server.sv_attr[(int)SRV_ATR_mailfrom].at_val.at_str) == NULL)
    mailfrom = PBS_DEFAULT_MAIL;

  /* Who does the mail go to?  If mail-list, them; else owner */

  *mailto = '\0';
 
  if (pjob->ji_wattr[(int)JOB_ATR_mailuser].at_flags & ATR_VFLAG_SET) 
    {
    /* has mail user list, send to them rather than owner */

    pas = pjob->ji_wattr[(int)JOB_ATR_mailuser].at_val.at_arst;

    if (pas != NULL)	
      {
      for (i = 0;i < pas->as_usedptr;i++) 
        {
        if ((strlen(mailto) + strlen(pas->as_string[i]) + 2) < sizeof(mailto)) 
          {
          strcat(mailto,pas->as_string[i]);
          strcat(mailto," ");
          }		
        }
      }
    }  
  else 
    {
    /* no mail user list, just send to owner */

    strcpy(mailto,pjob->ji_wattr[(int)JOB_ATR_job_owner].at_val.at_str);

    #ifdef TMAILDOMAIN
      strcat(mailto,"@");
      strcat(mailto,TMAILDOMAIN);
    #endif /* TMAILDOMAIN */ 
    }

  /* setup sendmail command line with -f from_whom */

  i = strlen(SENDMAIL_CMD) + strlen(mailfrom) + strlen(mailto) + 6;

  if ((cmdbuf = malloc(i)) == NULL)
    {
    exit(1);
    }

  sprintf(cmdbuf,"%s -f %s %s", 
    SENDMAIL_CMD, 
    mailfrom, 
    mailto);
		
  outmail = (FILE *)popen(cmdbuf,"w");

  if (outmail == NULL)
    {
    exit(1);
    }
	
  /* Pipe in mail headers: To: and Subject: */

  fprintf(outmail,"To: %s\n",
    mailto);

  fprintf(outmail,"Subject: PBS JOB %s\n", 
    pjob->ji_qs.ji_jobid);

  /* Set "Precedence: bulk" to avoid vacation messages, etc */

  fprintf(outmail,"Precedence: bulk\n\n");

  /* Now pipe in "standard" message */

  switch (mailpoint) 
    {
    case MAIL_ABORT:

      stdmessage = msg_job_abort;

      break;

    case MAIL_BEGIN:

      stdmessage = msg_job_start;

      break;

    case MAIL_END:

      stdmessage = msg_job_end;

      break;

    case MAIL_DEL:

      stdmessage = msg_job_del;

      break;

    case MAIL_OTHER:

      /* NOTE:  route other message to stageinfail ??? */

      stdmessage = msg_job_stageinfail;

      break;

    case MAIL_STAGEIN:
    default:

      stdmessage = msg_job_stageinfail;

      break;
    }  /* END switch (mailpoint) */

  fprintf(outmail,"PBS Job Id: %s\n", 
    pjob->ji_qs.ji_jobid);

  fprintf(outmail,"Job Name:   %s\n",
    pjob->ji_wattr[(int)JOB_ATR_jobname].at_val.at_str);

  if (stdmessage)
    fprintf(outmail, "%s\n", 
      stdmessage);

  if (text != NULL)
    fprintf(outmail, "%s\n",
      text);

  fclose(outmail);

  exit(0);

  /*NOTREACHED*/

  return;
  }  /* END svr_mailowner() */

/* END svr_mail.c */
