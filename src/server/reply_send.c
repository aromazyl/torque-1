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
 * This file contains the routines used to send a reply to a client following
 * the processing of a request.  The following routines are provided here:
 *
 * reply_send()  - the main routine, used by all reply senders
 * reply_ack()   - send a basic no error acknowledgement
 * req_reject()  - send a basic error return
 * reply_text()  - send a return with a supplied text string
 * reply_jobid() - used by several requests where the job id must be sent
 * reply_free()  - free the substructure that might hang from a reply
 */

#include <pbs_config.h>   /* the master config generated by configure */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include "libpbs.h"
#include "dis.h"
#include "log.h"
#include "pbs_error.h"
#include "server_limits.h"
#include "list_link.h"
#include "net_connect.h"
#include "attribute.h"
#include "credential.h"
#include "batch_request.h"
#include "work_task.h"


/* External Globals */

extern struct connection svr_conn[];

extern char *msg_daemonname;

#ifndef PBS_MOM
extern tlist_head task_list_event;
extern tlist_head task_list_immed;
extern int LOGLEVEL;
#endif /* PBS_MOM */

#define ERR_MSG_SIZE 127


static void set_err_msg(

  int   code,
  char *msgbuf)

  {
  char *msg = NULL;
  char *msg_tmp;

  /* see if there is an error message associated with the code */

  *msgbuf = '\0';

  if (code == PBSE_SYSTEM)
    {
    strcpy(msgbuf, msg_daemonname);
    strcat(msgbuf, pbse_to_txt(PBSE_SYSTEM));

    msg_tmp = strerror(errno);

    if (msg_tmp)
      strncat(msgbuf, strerror(errno), ERR_MSG_SIZE - strlen(msgbuf));
    else
      strcat(msgbuf, "Unknown error");
    }
  else if (code > PBSE_)
    {
    msg = pbse_to_txt(code);
    }
  else
    {
    msg = strerror(code);
    }

  if (msg)
    {
    strncpy(msgbuf, msg, ERR_MSG_SIZE);
    }

  msgbuf[ERR_MSG_SIZE] = '\0';

  return;
  }  /* END set_err_msg() */






static int dis_reply_write(

  int                 sfds,    /* I */
  struct batch_reply *preply)  /* I */

  {
  int rc;

  /* setup for DIS over tcp */

  DIS_tcp_setup(sfds);

  /* send message to remote client */

  if ((rc = encode_DIS_reply(sfds, preply)) ||
      (rc = DIS_tcp_wflush(sfds)))
    {
    sprintf(log_buffer, "DIS reply failure, %d",
            rc);

    LOG_EVENT(
      PBSEVENT_SYSTEM,
      PBS_EVENTCLASS_REQUEST,
      "dis_reply_write",
      log_buffer);

    close_conn(sfds);
    }

  return(rc);
  }  /* END dis_reply_write() */




/*
 * reply_send - Send a reply to a batch request, reply either goes to
 * remote client over the network:
 * Encode the reply to a "presentation element",
 * allocate the presenetation stream and attach to socket,
 * write out reply, and free ps, pe, and isoreply structures.
 *
 * Or the reply is for a request from the local server:
 * locate the work task associated with the request and dispatch it
 *
 * The request (and reply) structures are freed.
 */

int reply_send(

  struct batch_request *request)  /* I (freed) */

  {
  int      rc = 0;
  int      sfds = request->rq_conn;  /* socket */

#ifndef PBS_MOM
  static char    *id = "reply_send";

  struct work_task *ptask;
#endif /* PBS_MOM */

  /* determine where the reply should go, remote or local */

  if (sfds == PBS_LOCAL_CONNECTION)
    {

#ifndef PBS_MOM

    /*
     * reply stays local, find work task and move it to
     * the immediate list for dispatching.
     */

    ptask = (struct work_task *)GET_NEXT(task_list_event);

    while (ptask != NULL)
      {
      if ((ptask->wt_type == WORK_Deferred_Local) &&
          (ptask->wt_parm1 == (void *)request))
        {
        delete_link(&ptask->wt_linkall);

        append_link(&task_list_immed, &ptask->wt_linkall, ptask);

        return(0);
        }

      ptask = (struct work_task *)GET_NEXT(ptask->wt_linkall);
      }

    /* should have found a task and didn't */

    log_err(-1, id, "did not find work task for local request");

#endif /* PBS_MOM */

    rc = PBSE_SYSTEM;
    }
  else if (sfds >= 0)
    {
    /* Otherwise, the reply is to be sent to a remote client */

#ifndef PBS_MOM
#ifdef AUTORUN_JOBS
    if (request->rq_noreply != TRUE)
      {
#endif
#endif
      rc = dis_reply_write(sfds, &request->rq_reply);
#ifndef PBS_MOM
      if (LOGLEVEL >= 7)
        {
        sprintf(log_buffer, "Reply sent for request type %s on socket %d",
          reqtype_to_txt(request->rq_type),
          sfds);

        log_record(
          PBSEVENT_JOB,
          PBS_EVENTCLASS_JOB,
          id,
          log_buffer);
        }
#ifdef AUTORUN_JOBS
      }

#endif
#endif
    }

#ifndef PBS_MOM
  if ((request->rq_type != PBS_BATCH_AsyModifyJob) || (request->rq_noreply == TRUE))
    {
#endif
    free_br(request);
#ifndef PBS_MOM
    }

#endif

  return(rc);
  }  /* END reply_send() */





/*
 * reply_ack - Send a normal acknowledgement reply to a request
 *
 * Always frees the request structure.
 */

void
reply_ack(struct batch_request *preq)
  {

  preq->rq_reply.brp_code    = PBSE_NONE;
  preq->rq_reply.brp_auxcode = 0;
  preq->rq_reply.brp_choice  = BATCH_REPLY_CHOICE_NULL;
  (void)reply_send(preq);
  }

/*
 * reply_free - free any sub-struttures that might hang from the basic
 * batch_reply structure, the reply structure itself IS NOT FREED.
 */

void reply_free(

  struct batch_reply *prep)

  {

  struct brp_status  *pstat;

  struct brp_status  *pstatx;

  struct brp_select  *psel;

  struct brp_select  *pselx;

  if (prep->brp_choice == BATCH_REPLY_CHOICE_Text)
    {
    if (prep->brp_un.brp_txt.brp_str)
      {
      (void)free(prep->brp_un.brp_txt.brp_str);
      prep->brp_un.brp_txt.brp_str = (char *)0;
      prep->brp_un.brp_txt.brp_txtlen = 0;
      }

    }
  else if (prep->brp_choice == BATCH_REPLY_CHOICE_Select)
    {
    psel = prep->brp_un.brp_select;

    while (psel)
      {
      pselx = psel->brp_next;
      (void)free(psel);
      psel = pselx;
      }

    }
  else if (prep->brp_choice == BATCH_REPLY_CHOICE_Status)
    {
    pstat = (struct brp_status *)GET_NEXT(prep->brp_un.brp_status);

    while (pstat)
      {
      pstatx = (struct brp_status *)GET_NEXT(pstat->brp_stlink);
      free_attrlist(&pstat->brp_attr);
      (void)free(pstat);
      pstat = pstatx;
      }
    }
  else if (prep->brp_choice == BATCH_REPLY_CHOICE_RescQuery)
    {
    (void)free(prep->brp_un.brp_rescq.brq_avail);
    (void)free(prep->brp_un.brp_rescq.brq_alloc);
    (void)free(prep->brp_un.brp_rescq.brq_resvd);
    (void)free(prep->brp_un.brp_rescq.brq_down);
    }

  prep->brp_choice = BATCH_REPLY_CHOICE_NULL;

  return;
  }  /* END reply_free() */





/*
 * req_reject - create a reject (error) reply for a request and send it
 *
 * Always frees the request structure.
 */

void req_reject(

  int                   code,      /* I */
  int                   aux,       /* I */
  struct batch_request *preq,      /* I */
  char                 *HostName,  /* I (optional) */
  char                 *Msg)       /* I (optional) */

  {
  char msgbuf[ERR_MSG_SIZE + 256 + 1];
  char msgbuf2[ERR_MSG_SIZE + 256 + 1];

  set_err_msg(code, msgbuf);

  snprintf(msgbuf2, sizeof(msgbuf2), "%s", msgbuf);

  if ((HostName != NULL) && (*HostName != '\0'))
    {
    snprintf(msgbuf, sizeof(msgbuf), "%s REJHOST=%s",
             msgbuf2,
             HostName);

    snprintf(msgbuf2, sizeof(msgbuf2), "%s", msgbuf);
    }

  if ((Msg != NULL) && (*Msg != '\0'))
    {
    snprintf(msgbuf, sizeof(msgbuf), "%s MSG=%s",
             msgbuf2,
             Msg);

    /* NOTE: Don't need this last snprintf() unless another message is concatenated. */
    }

  sprintf(log_buffer, "Reject reply code=%d(%s), aux=%d, type=%s, from %s@%s",

          code,
          msgbuf,
          aux,
          reqtype_to_txt(preq->rq_type),
          preq->rq_user,
          preq->rq_host);

  LOG_EVENT(
    PBSEVENT_DEBUG,
    PBS_EVENTCLASS_REQUEST,
    "req_reject",
    log_buffer);

  preq->rq_reply.brp_auxcode = aux;

  reply_text(preq, code, msgbuf);

  return;
  }  /* END req_reject() */




/*
 * reply_badattr - create a reject (error) reply for a request including
 * the name of the bad attribute/resource
 */

void reply_badattr(

  int                   code,
  int                   aux,
  svrattrl        *pal,
  struct batch_request *preq)

  {
  int   i = 1;
  char  msgbuf[ERR_MSG_SIZE+1];

  set_err_msg(code, msgbuf);

  while (pal)
    {
    if (i == aux)
      {
      strcat(msgbuf, " ");
      strcat(msgbuf, pal->al_name);

      if (pal->al_resc)
        {
        strcat(msgbuf, ".");
        strcat(msgbuf, pal->al_resc);
        }

      break;
      }

    pal = (svrattrl *)GET_NEXT(pal->al_link);

    ++i;
    }

  reply_text(preq, code, msgbuf);

  return;
  }  /* END reply_badattr() */




/*
 * reply_text - return a reply with a supplied text string
 */

void reply_text(

  struct batch_request *preq,
  int    code,
  char   *text) /* I */

  {
  if (preq->rq_reply.brp_choice != BATCH_REPLY_CHOICE_NULL)
    {
    /* in case another reply was being built up, clean it out */

    reply_free(&preq->rq_reply);
    }

  preq->rq_reply.brp_code    = code;

  preq->rq_reply.brp_auxcode = 0;

  if ((text != NULL) && (text[0] != '\0'))
    {
    preq->rq_reply.brp_choice                = BATCH_REPLY_CHOICE_Text;
    preq->rq_reply.brp_un.brp_txt.brp_str    = strdup(text);
    preq->rq_reply.brp_un.brp_txt.brp_txtlen = strlen(text);
    }
  else
    {
    preq->rq_reply.brp_choice = BATCH_REPLY_CHOICE_NULL;
    }

  reply_send(preq);

  return;
  }  /* END reply_text() */




/*
 * reply_jobid - return a reply with the job id, used by
 * req_queuejob(), req_rdytocommit(), and req_commit()
 *
 * Always frees the request structure.
 */

int reply_jobid(

  struct batch_request *preq,
  char   *jobid,
  int    which)

  {
  preq->rq_reply.brp_code    = 0;
  preq->rq_reply.brp_auxcode = 0;
  preq->rq_reply.brp_choice  = which;

  strncpy(preq->rq_reply.brp_un.brp_jid, jobid, PBS_MAXSVRJOBID);

  return(reply_send(preq));
  }  /* END reply_jobid() */

