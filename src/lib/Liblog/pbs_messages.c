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

#include <string.h>
#include "portability.h"
#include "pbs_error.h"


/*
 * Messages issued by the server.  They are kept here in one place
 * to make translation a bit easier.
 *
 * WARNING - there are places where a message and other info is stuffed
 *	into a buffer, keep the messages short!
 *
 * This first set of messages are recorded by the server or mom to the log.
 */

char *msg_abt_err	= "Unable to abort Job %s which was in substate %d";
char *msg_badexit	= "Abnormal exit status 0x%x: ";
char *msg_badwait	= "Invalid time in work task for waiting, job = %s";
char *msg_deletejob	= "Job deleted";
char *msg_delrunjobsig  = "Job sent signal %s on delete";
char *msg_err_malloc	= "malloc failed";
char *msg_err_noqueue	= "Unable to requeue job, queue is not defined";
char *msg_err_purgejob	= "Unlink of job file failed";
char *msg_err_unlink	= "Unlink of %s file %s failed";
char *msg_illregister	= "Illegal op in register request received for job %s";
char *msg_init_abt	= "Job aborted on PBS Server initialization";
char *msg_init_baddb	= "Unable to read server database";
char *msg_init_badjob	= "Recover of job %s failed";
char *msg_init_chdir	= "unable to change to directory %s";
char *msg_init_expctq   = "Expected %d, recovered %d queues";
char *msg_init_exptjobs = "Expected %d, recovered %d jobs";
char *msg_init_nojobs   = "No jobs to open";
char *msg_init_noqueues = "No queues to open";
char *msg_init_norerun	= "Unable to rerun job at Server initialization";
char *msg_init_queued	= "Requeued in queue: ";
char *msg_init_recovque	= "Recovered queue %s";
char *msg_init_substate = "Requeueing job, substate: %d ";
char *msg_init_unkstate = "Unable to recover job in strange substate: %d";
char *msg_isodedecode	= "Decode of request failed";
char *msg_issuebad	= "attempt to issue invalid request of type %d";
char *msg_job_abort	= "Aborted by PBS Server ";
char *msg_job_del       = "job deleted";
char *msg_jobholdset	= "Holds %s set at request of %s@%s";
char *msg_jobholdrel	= "Holds %s released at request of %s@%s";
char *msg_job_end	= "Execution terminated";
char *msg_job_end_stat	= "Exit_status=%d";
char *msg_job_end_sig	= "Terminated on signal %d";
char *msg_jobmod	= "Job Modified";
char *msg_jobnew	= "Job Queued at request of %s@%s, owner = %s, job name = %s, queue = %s";
char *msg_jobrerun	= "Job Rerun";
char *msg_jobrun	= "Job Run";
char *msg_job_start	= "Begun execution";
char *msg_job_stageinfail = "File stage in failed, see below.\nJob will be retried later, please investigate and correct problem.";
char *msg_job_otherfail = "An error has occurred processing your job, see below.";
char *msg_leftrunning	= "job running on at Server shutdown";
char *msg_manager	= "%s at request of %s@%s";
char *msg_man_cre	= "created";
char *msg_man_del	= "deleted";
char *msg_man_set	= "attributes set: ";
char *msg_man_uns	= "attributes unset: ";
char *msg_messagejob	= "Message request to job status %d";
char *msg_mombadhold	= "MOM rejected hold request: %d";
char *msg_mombadmodify	= "MOM rejected modify request, error: %d";
char *msg_momsetlim	= "Job start failed.  Can't set \"%s\" limit: %s.\n";
char *msg_momnoexec1	= "Job cannot be executed\nSee Administrator for help";
char *msg_momnoexec2	= "Job cannot be executed\nSee job standard error file";
char *msg_movejob	= "Job moved to ";
char *msg_norelytomom	= "Server could not connect to MOM";
char *msg_obitnojob 	= "Job Obit notice received from %s has error %d";
char *msg_obitnocpy	= "Post job file processing error; job %s on host %s";
char *msg_obitnodel	= "Unable to delete files for job %s, on host %s";
char *msg_on_shutdown	= " on Server shutdown";
char *msg_orighost 	= "Job missing PBS_O_HOST value";
char *msg_permlog	= "Unauthorized Request, request type: %d, Object: %s, Name: %s, request from: %s@%s";
char *msg_postmomnojob	= "Job not found after hold reply from MOM";
char *msg_request	= "Type %s request received from %s@%s, sock=%d";
char *msg_regrej	= "Dependency request for job rejected by ";
char *msg_registerdel	= "Job deleted as result of dependency on job %s";
char *msg_registerrel	= "Dependency on job %s released.";
char *msg_routexceed	= "Route queue lifetime exceeded";
char *msg_script_open	= "Unable to open script file";
char *msg_script_write	= "Unable to write script file";
char *msg_shutdown_op	= "Shutdown request from %s@%s ";
char *msg_shutdown_start = "Starting to shutdown the server, type is ";
char *msg_startup1	= "Server %s started, initialization type = %d";
char *msg_startup2	= "Server Ready, pid = %d, loglevel=%d";
char *msg_startup3	= "%s %s: %s mode and %s exists, \ndo you wish to continue y/(n)?";
char *msg_svdbopen	= "Unable to open server data base";
char *msg_svdbnosv	= "Unable to save server data base";
char *msg_svrdown	= "Server shutdown completed";



/* sync w/enum PBatchReqTypeEnum in libpbs.h */

static const char *PBatchReqType[] = {
  "Connect",
  "QueueJob",
  "JobCred",
  "JobScript",
  "ReadyToCommit",
  "Commit",
  "DeleteJob",
  "HoldJob",
  "LocateJob",
  "Manager",
  "MessageJob",
  "ModifyJob",
  "MoveJob",
  "ReleaseJob",
  "RerunJob",
  "RunJob",
  "SelectJobs",
  "Shutdown",
  "SignalJob",
  "StatusJob",
  "StatusQueue",
  "StatusServer",
  "TrackJob",
  "AsyncRunJob",
  "ResourceQuery",
  "ReserveResource",
  "ReleaseResource", /* 26 */
  "CheckpointJob",
  "AsyncModifyJob",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "NONE",
  "StageIn",   /* 48 */
  "AuthenticateUser",
  "OrderJob",
  "SelStat",
  "RegisterDependency",
  "ReturnFiles",
  "CopyFiles",
  "DeleteFiles",
  "JobObituary",
  "MoveJobFile",
  "StatusNode",
  "Disconnect",
  NULL };

#define NPBatchReqType (sizeof(PBatchReqType)/sizeof(PBatchReqType[0]))

/*
 * This next set of messages are returned to the client on an error.
 * They may also be logged.
 */


char *msg_unkjobid	= "Unknown Job Id";
char *msg_noattr	= "Undefined attribute ";
char *msg_attrro	= "Cannot set attribute, read only or insufficient permission ";
char *msg_ivalreq	= "Invalid request";
char *msg_unkreq	= "Unknown request";
char *msg_perm		= "Unauthorized Request ";
char *msg_reqbadhost	= "Access from host not allowed, or unknown host";
char *msg_jobexist	= "Job with requested ID already exists";
char *msg_system	= " System error: ";
char *msg_internal	= "PBS server internal error";
char *msg_regroute	= "Dependent parent job currently in routing queue";
char *msg_unksig	= "Unknown/illegal signal name";
char *msg_badatval	= "Illegal attribute or resource value for ";
char *msg_badnodeatval	= "Illegal value for ";
char *msg_mutualex	= "Mutually exclusive values for ";
char *msg_modatrrun	= "Cannot modify attribute while job running ";
char *msg_badstate	= "Request invalid for state of job";
char *msg_unkque	= "Unknown queue";
char *msg_unknode	= "Unknown node ";
char *msg_unknodeatr	= "Unknown node-attribute ";
char *msg_nonodes	= "Server has no node list";
char *msg_badcred	= "Invalid credential";
char *msg_expired	= "Expired credential";
char *msg_qunoenb	= "Queue is not enabled";
char *msg_qacess	= "Access to queue is denied";
char *msg_baduser	= "Bad UID for job execution";
char *msg_badgrp	= "Bad GID for job execution";
char *msg_hopcount	= "Job routing over too many hops";
char *msg_queexist	= "Queue already exists";
char *msg_attrtype	= "Warning: type of queue %s incompatible with attribute %s";
char *msg_attrtype2	= "Incompatible type";
char *msg_quebusy	= "Cannot delete busy queue";
char *msg_quenbig	= "Queue name too long";
char *msg_nosupport	= "No support for requested service";
char *msg_quenoen	= "Cannot enable queue, incomplete definition";
char *msg_protocol	= "Batch protocol error";
char *msg_noconnects	= "No free connections";
char *msg_noserver	= "No server specified";
char *msg_unkresc	= "Unknown resource type ";
char *msg_excqresc	= "Job exceeds queue resource limits";
char *msg_quenodflt	= "No default queue specified";
char *msg_jobnorerun 	= "job is not rerunnable";
char *msg_routebad	= "Job rejected by all possible destinations";
char *msg_momreject	= "Execution server rejected request";
char *msg_nosyncmstr	= "No master found for sync job set";
char *msg_sched_called 	= "Scheduler sent event %s";
char *msg_sched_nocall  = "Could not contact Scheduler";
char *msg_listnr_called = "Listener %d sent event %s";
char *msg_listnr_nocall  = "Could not contact Listener";
char *msg_stageinfail	= "Stage In of files failed";
char *msg_rescunav	= "Resource temporarily unavailable";
char *msg_maxqueued	= "Maximum number of jobs already in queue";
char *msg_maxuserqued   = "Maximum number of jobs already in queue for user";
char *msg_chkpointbusy	= "Checkpoint busy, may retry";
char *msg_exceedlmt	= "Resource limit exceeds allowable";
char *msg_badacct	= "Invalid Account";
char *msg_baddepend	= "Invalid Job Dependency";
char *msg_duplist	= "Duplicate entry in list ";
char *msg_svrshut	= "Request not allowed: Server shutting down";
char *msg_execthere	= "Cannot execute at specified host because of checkpoint or stagein files";
char *msg_gmoderr	= "Modification failed for ";
char *msg_notsnode	= "No time-share node available";
char *msg_alrdyexit     = "Job already in exit state";
char *msg_badatlst      = "Bad attribute list structure";
char *msg_badscript     = "(qsub) cannot access script file";
char *msg_ckpshort      = "not all tasks could checkpoint";
char *msg_cleanedout    = "unknown job id after clean init";
char *msg_disproto      = "Bad DIS based Request Protocol";
char *msg_nocopyfile    = "Job files not copied";
char *msg_nodeexist     = "Node name already exists";
char *msg_nodenbig      = "Node name is too big";
char *msg_none          = "no error";
char *msg_rmbadparam    = "parameter could not be used";
char *msg_rmexist       = "something specified didn't exist";
char *msg_rmnoparam     = "a parameter needed did not exist";
char *msg_rmpart        = "only part of reservation made";
char *msg_rmsystem      = "a system error occured";
char *msg_rmunknown     = "resource unknown";
char *msg_routeexpd     = "Time in Route Queue Expired";
char *msg_siscomm       = "sister could not communicate";
char *msg_sisreject     = "sister rejected";
char *msg_toomany       = "Too many submit retries";
char *msg_jobtype       = "Wrong job type";
char *msg_badaclhost	= "Bad ACL entry in host list";
char *msg_baddisallowtype = "Bad type in disallowed_types list";
char *msg_nointeractive = "Queue does not allow interactive jobs";
char *msg_nobatch       = "Queue does not allow batch jobs";
char *msg_norerunable   = "Queue does not allow rerunable jobs";
char *msg_nononrerunable = "Queue does not allow nonrerunable jobs";
char *msg_unkarrayid    = "Unknown Array ID";
char *msg_bad_array_req = "Bad Job Array Request";
char *msg_timeout 	= "Time out";
/*
 * The following table connects error numbers with text
 * to be returned to the client.  Each is guaranteed to be pure text.
 * There are no printf formatting strings imbedded.
 */

struct pbs_err_to_txt pbs_err_to_txt[] = {
	{ PBSE_UNKJOBID, &msg_unkjobid },
	{ PBSE_NOATTR, &msg_noattr },
	{ PBSE_ATTRRO, &msg_attrro },
	{ PBSE_IVALREQ, &msg_ivalreq },
	{ PBSE_UNKREQ, &msg_unkreq },
	{ PBSE_PERM, &msg_perm },
	{ PBSE_BADHOST, &msg_reqbadhost },
	{ PBSE_JOBEXIST, &msg_jobexist },
	{ PBSE_SYSTEM, &msg_system },
	{ PBSE_INTERNAL, &msg_internal },
	{ PBSE_REGROUTE, &msg_regroute },
	{ PBSE_UNKSIG, &msg_unksig },
	{ PBSE_BADATVAL, &msg_badatval },
	{ PBSE_BADNDATVAL, &msg_badnodeatval },
	{ PBSE_MUTUALEX, &msg_mutualex },
	{ PBSE_MODATRRUN, &msg_modatrrun },
	{ PBSE_BADSTATE, &msg_badstate },
	{ PBSE_UNKQUE, &msg_unkque },
	{ PBSE_UNKNODE, &msg_unknode },
	{ PBSE_UNKNODEATR, &msg_unknodeatr },
	{ PBSE_NONODES, &msg_nonodes },
	{ PBSE_BADCRED, &msg_badcred },
	{ PBSE_EXPIRED, &msg_expired },
	{ PBSE_QUNOENB, &msg_qunoenb },
	{ PBSE_QACESS, &msg_qacess },
	{ PBSE_BADUSER, &msg_baduser },
	{ PBSE_HOPCOUNT, &msg_hopcount },
	{ PBSE_QUEEXIST, &msg_queexist },
	{ PBSE_QUEBUSY, &msg_quebusy },
	{ PBSE_QUENBIG, &msg_quenbig },
	{ PBSE_NOSUP, &msg_nosupport },
	{ PBSE_QUENOEN, &msg_quenoen },
	{ PBSE_PROTOCOL, &msg_protocol },
	{ PBSE_NOCONNECTS, &msg_noconnects },
	{ PBSE_NOSERVER, &msg_noserver },
	{ PBSE_UNKRESC, &msg_unkresc },
	{ PBSE_EXCQRESC, &msg_excqresc },
	{ PBSE_QUENODFLT, &msg_quenodflt },
	{ PBSE_NORERUN, &msg_jobnorerun },
	{ PBSE_ROUTEREJ, &msg_routebad },
	{ PBSE_MOMREJECT, &msg_momreject },
	{ PBSE_NOSYNCMSTR, &msg_nosyncmstr },
	{ PBSE_STAGEIN, &msg_stageinfail },
	{ PBSE_RESCUNAV, &msg_rescunav },
	{ PBSE_BADGRP,  &msg_badgrp },
	{ PBSE_MAXQUED, &msg_maxqueued },
	{ PBSE_MAXUSERQUED, &msg_maxuserqued },
	{ PBSE_CKPBSY, &msg_chkpointbusy },
	{ PBSE_EXLIMIT, &msg_exceedlmt },
	{ PBSE_BADACCT, &msg_badacct },
	{ PBSE_BADDEPEND, &msg_baddepend },
	{ PBSE_DUPLIST, &msg_duplist },
	{ PBSE_EXECTHERE, &msg_execthere },
	{ PBSE_SVRDOWN, &msg_svrshut },
	{ PBSE_ATTRTYPE, &msg_attrtype2 },
	{ PBSE_GMODERR, &msg_gmoderr },
	{ PBSE_NORELYMOM, &msg_norelytomom },
	{ PBSE_NOTSNODE, &msg_notsnode },
        { PBSE_ALRDYEXIT, &msg_alrdyexit },
        { PBSE_BADATLST, &msg_badatlst },
        { PBSE_BADSCRIPT, &msg_badscript },
        { PBSE_CKPSHORT, &msg_ckpshort },
        { PBSE_CLEANEDOUT, &msg_cleanedout },
        { PBSE_DISPROTO, &msg_disproto },
        { PBSE_NOCOPYFILE, &msg_nocopyfile },
        { PBSE_NODEEXIST, &msg_nodeexist },
        { PBSE_NODENBIG, &msg_nodenbig },
        { PBSE_RMBADPARAM, &msg_rmbadparam },
        { PBSE_RMEXIST, &msg_rmexist },
        { PBSE_RMNOPARAM, &msg_rmnoparam },
        { PBSE_RMPART, &msg_rmpart },
        { PBSE_RMSYSTEM, &msg_rmsystem },
        { PBSE_RMUNKNOWN, &msg_rmunknown },
        { PBSE_ROUTEEXPD, &msg_routeexpd },
        { PBSE_SISCOMM, &msg_siscomm },
        { PBSE_SISREJECT, &msg_sisreject },
        { PBSE_TOOMANY, &msg_toomany },
	{ PBSE_JOBTYPE, &msg_jobtype },
        { PBSE_BADACLHOST, &msg_badaclhost },
        { PBSE_BADDISALLOWTYPE, &msg_baddisallowtype },
        { PBSE_NOINTERACTIVE, &msg_nointeractive },
        { PBSE_NOBATCH, &msg_nobatch },
	{ PBSE_NORERUNABLE, &msg_norerunable },
	{ PBSE_NONONRERUNABLE, &msg_nononrerunable },
	{ PBSE_UNKARRAYID, &msg_unkarrayid },
	{ PBSE_BAD_ARRAY_REQ, &msg_bad_array_req },
	{ PBSE_TIMEOUT, &msg_timeout },
        { PBSE_NONE, &msg_none },
	{ 0, (char **)0 }		/* MUST be the last entry */
};





/*
 * pbse_to_txt() - return a text message for an PBS error number
 *	if it exists
 */

char *pbse_to_txt(

  int err)  /* I */

  {
  int i = 0;

  while ((pbs_err_to_txt[i].err_no != 0) && (pbs_err_to_txt[i].err_no != err))
    ++i;

  if (pbs_err_to_txt[i].err_txt != (char **)0)
    {
    return(*pbs_err_to_txt[i].err_txt);
    }

  return(NULL);
  }  /* END pbse_to_txt() */

/*
 * pbs_strerror() - return a pbs text message for an PBS error number
 *	if it exists or return strerror results
 */

char *pbs_strerror(

  int err)  /* I */

  {
  if (err > PBSE_)
    {
    return (pbse_to_txt(err));
    }

  return (strerror(err));
  }  /* END pbs_strerror() */


/*
 * rqtype_to_txt()	- Return the printable name of a request type
 */

const char *reqtype_to_txt(

  int reqtype)

  {
  if (reqtype >= 0 && reqtype < (int)NPBatchReqType && PBatchReqType[reqtype])
    return(PBatchReqType[reqtype]);
  else
    return("NONE");
  }  /* END reqtype_to_txt() */
