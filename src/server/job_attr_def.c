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
 * job_attr_def is the array of attribute definitions for jobs.
 * Each legal job attribute is defined here.
 */

#include <pbs_config.h>		/* the master config generated by configure */

#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "server_limits.h"

/* Extern functions (at_action) called */

extern int job_set_wait (attribute *, void *, int);
extern int action_resc (attribute *, void *, int);
extern int ck_checkpoint (attribute *, void *, int);
extern int depend_on_que (attribute *, void *, int);
extern int comp_checkpoint (attribute *, attribute *);

#define ATR_DFLAG_SSET  (ATR_DFLAG_SvWR | ATR_DFLAG_SvRD)

/*
 * The entries for each attribute are (see attribute.h):
 * name,
 * decode function,
 * encode function,
 * set function,
 * compare function,
 * free value space function,
 * action function,
 * access permission flags,
 * value type,
 * parent object type
 */

/* sync w/enum job_atr in src/include/pbs_job.h */
/* sync w/ TJobAttr[] in src/resmom/requests.c */

attribute_def job_attr_def[] = {

  /* JOB_ATR_jobname */
  {ATTR_N,			/* "Job_Name" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_job_owner */
  {ATTR_owner,			/* "Job_Owner" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_resc_used */
  {ATTR_used,			/* "resources_used" */
   decode_resc,
   encode_resc,
   set_resc,
   comp_resc,
   free_resc,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SvWR,
   ATR_TYPE_RESC,
   PARENT_TYPE_JOB},

  /* JOB_ATR_state */
  {ATTR_state,			/* "job_state" */
   decode_c,
   encode_c,
   set_c,
   comp_c,
   free_null,
   NULL_FUNC,
   READ_ONLY,
   ATR_TYPE_CHAR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_in_queue */
  {ATTR_queue,			/* "queue" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_MOM /* | ATR_DFLAG_ALTRUN */ ,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_at_server */
  {ATTR_server,			/* "server" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_account */
  {ATTR_A,			/* "Account_Name" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_checkpoint */
  {ATTR_c,			/* "Checkpoint" */
   decode_str,
   encode_str,
   set_str,
#ifdef PBS_MOM
   comp_str,
#else /* PBS_MOM - server side */
   comp_checkpoint,
#endif /* PBS_MOM */
   free_str,
#ifdef PBS_MOM
   NULL_FUNC,
#else /* PBS_MOM - server side */
#if 0
   ck_checkpoint,
#else
   0,
#endif
#endif /* PBS_MOM */
   READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_ctime *//* create time, set when the job is queued */
  {ATTR_ctime,			/* "ctime" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_depend */
  {ATTR_depend,			/* "depend" */
#ifndef PBS_MOM
   decode_depend,
   encode_depend,
   set_depend,
   comp_depend,
   free_depend,
   depend_on_que,
#else
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
#endif /* PBS_MOM */
   READ_WRITE,
   ATR_TYPE_LIST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_errpath */
  {ATTR_e,			/* "Error_Path" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_exec_host */
  {ATTR_exechost,		/* "exec_host" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_MOM | ATR_DFLAG_OPWR | ATR_DFLAG_SvWR,	/* allow operator/scheduler to modify exec_host */
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_exectime */
  {ATTR_a,			/* "Execution_Time" (aka release_date) */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
#ifndef PBS_MOM
   job_set_wait,
#else
   NULL_FUNC,
#endif
   READ_WRITE | ATR_DFLAG_ALTRUN,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_grouplst */
  {ATTR_g,			/* "group_list" */
   decode_arst,
   encode_arst,
   set_arst,
   comp_arst,
   free_arst,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ,
   ATR_TYPE_ARST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_hold */
  {ATTR_h,			/* "Hold_Types" */
   decode_hold,
   encode_hold,
   set_b,
   comp_hold,
   free_null,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_interactive */
  {ATTR_inter,			/* "interactive" */
   decode_l,
   encode_inter,
   set_l,
   comp_b,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SvRD | ATR_DFLAG_Creat | ATR_DFLAG_SELEQ |
   ATR_DFLAG_MOM,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_join */
  {ATTR_j,			/* "Join_Path" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_keep */
  {ATTR_k,			/* "Keep_Files" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_mailpnts */
  {ATTR_m,			/* "Mail_Points" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_mailuser */
  {ATTR_M,			/* "Mail_Users" */
   decode_arst,
   encode_arst,
   set_arst,
   comp_arst,
   free_arst,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ,
   ATR_TYPE_ARST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_mtime */
  {ATTR_mtime,			/* "mtime" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},


  /* JOB_ATR_outpath */
  {ATTR_o,			/* "Output_Path" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_priority */
  {ATTR_p,			/* "Priority" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_qtime */
  {ATTR_qtime,			/* "qtime"  (time entered queue) */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_rerunable */
  {ATTR_r,			/* "Rerunable" */
   decode_b,
   encode_b,
   set_b,
   comp_b,
   free_null,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_resource */
  {ATTR_l,			/* "Resource_List" */
   decode_resc,
   encode_resc,
   set_resc,
   comp_resc,
   free_resc,
   action_resc,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_MOM,
   ATR_TYPE_RESC,
   PARENT_TYPE_JOB},

  /* JOB_ATR_session_id */
  {ATTR_session,		/* "session_id" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SvWR,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_shell */
  {ATTR_S,			/* "Shell_Path_List" */
   decode_arst,
   encode_arst,
   set_arst,
   comp_arst,
   free_arst,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_ARST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_stagein */
  {ATTR_stagein,		/* "stagein" */
   decode_arst,
   encode_arst,
   set_arst,
   comp_arst,
   free_arst,
   NULL_FUNC,
   READ_WRITE,
   ATR_TYPE_ARST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_stageout */
  {ATTR_stageout,		/* "stageout" */
   decode_arst,
   encode_arst,
   set_arst,
   comp_arst,
   free_arst,
   NULL_FUNC,
   READ_WRITE,
   ATR_TYPE_ARST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_substate */
  {ATTR_substate,		/* "substate" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   ATR_DFLAG_OPRD | ATR_DFLAG_MGRD,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_userlst */
  {ATTR_u,			/* "User_List" */
   decode_arst,
   encode_arst,
#ifndef PBS_MOM
   set_uacl,
#else
   set_arst,
#endif
   comp_arst,
   free_arst,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ,
   ATR_TYPE_ARST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_variables (allow to be dynamically modifiable) */
  {ATTR_v,			/* "Variable_List" */
   decode_arst,
   encode_arst,
   set_arst,
   comp_arst,
   free_arst,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM | ATR_DFLAG_PRIVR |
   ATR_DFLAG_ALTRUN,
   ATR_TYPE_ARST,
   PARENT_TYPE_JOB},

  /* JOB_ATR_euser    */
  {ATTR_euser,			/* "euser" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_MGRD | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_egroup  */
  {ATTR_egroup,			/* "egroup" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_MGRD | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_hashname */
  {ATTR_hashname,		/* "hashname" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_MGRD | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_hopcount */
  {ATTR_hopcount,		/* "hop_count" - undocumented */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   ATR_DFLAG_SSET,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_queuerank */
  {ATTR_qrank,			/* "queue_rank" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   ATR_DFLAG_MGRD,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_queuetype */
  {ATTR_qtype,			/* "queue_type" - exists for Scheduler select */
   decode_c,
   encode_c,
   set_c,
   comp_c,
   free_null,
   NULL_FUNC,
   ATR_DFLAG_MGRD | ATR_DFLAG_SELEQ,
   ATR_TYPE_CHAR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_sched_hint */
  {ATTR_sched_hint,		/* "sched_hint" - inform scheduler re sync */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_SSET | NO_USER_SET | ATR_DFLAG_ALTRUN,	/*ATR_DFLAG_MGRD | ATR_DFLAG_MGWR */
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_security */
  {ATTR_security,		/* "security" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_SSET,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_Comment */
  {ATTR_comment,		/* "comment" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   NO_USER_SET | ATR_DFLAG_SvWR | ATR_DFLAG_ALTRUN,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_Cookie */
  {ATTR_cookie,			/* "cookie" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_SvRD | ATR_DFLAG_SvWR | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_altid */
  {ATTR_altid,			/* "alt_id" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SvWR,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_etime */
  {ATTR_etime,			/* "etime" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_exitstat */
  {ATTR_exitstat,		/* "exit_status" */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_forwardx11 */
  {ATTR_forwardx11,		/* "forward_x11" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_USWR | ATR_DFLAG_MGRD | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_submit_args */
  {ATTR_submit_args,		/* "submit_args - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SvRD | ATR_DFLAG_Creat,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_job_array_id */
  {ATTR_array_id,		/* "array_id - undocumented */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   ATR_DFLAG_SvRD | READ_ONLY,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_job_array_request */
  {ATTR_t,			/* "job_array_request" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_Creat | ATR_DFLAG_SvRD | READ_ONLY,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_umask */
  {ATTR_umask,			/* "umask" - undocumented */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_start_time */
  {ATTR_start_time,		/* "start_time" - undocumented */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET | ATR_DFLAG_MOM,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_start_count */
  {ATTR_start_count,		/* "start_count" - undocumented */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_checkpoint_dir */
  {ATTR_checkpoint_dir,		/* "checkpoint_dir" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_checkpoint_name */
  {ATTR_checkpoint_name,	/* "checkpoint_name" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_MOM | ATR_DFLAG_ALTRUN,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_checkpoint_time */
  {ATTR_checkpoint_time,	/* "checkpoint_time" - undocumented */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   NO_USER_SET | ATR_DFLAG_SvWR | ATR_DFLAG_ALTRUN,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_checkpoint_restart_status */
  {ATTR_checkpoint_restart_status,	/* "checkpoint_restart_status" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SvWR | ATR_DFLAG_ALTRUN,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_restart_name */
  {ATTR_restart_name,		/* "restart_name" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   NO_USER_SET | ATR_DFLAG_SvWR | ATR_DFLAG_ALTRUN | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_f */
  {ATTR_f,		/* fault_tolerant - undocumented */
   decode_b,
   encode_b,
   set_b,
   comp_b,
   free_null,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_ALTRUN | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_comp_time */
  {ATTR_comp_time,		/* comp_time */
   decode_l,
   encode_l,
   set_l,
   comp_l,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_reported */
  {ATTR_reported,		/* "reported" */
   decode_b,
   encode_b,
   set_b,
   comp_b,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SSET,
   ATR_TYPE_LONG,
   PARENT_TYPE_JOB},

  /* JOB_ATR_jobtype */
  {ATTR_jobtype,		/* "jobtype" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_inter_cmd */
  {ATTR_intcmd,			/* "int_cmd" */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_MOM,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

  /* JOB_ATR_proxy_user */
  {ATTR_P,			/* "proxy_user" - undocumented */
   decode_str,
   encode_str,
   set_str,
   comp_str,
   free_str,
   NULL_FUNC,
   ATR_DFLAG_Creat | ATR_DFLAG_MGRD | ATR_DFLAG_USRD | ATR_DFLAG_OPRD,
   ATR_TYPE_STR,
   PARENT_TYPE_JOB},

#ifdef USEJOBCREATE
  /* JOB_ATR_pagg_id */
  {ATTR_pagg,			/* "pagg_id" - undocumented */
   decode_ll,
   encode_ll,
   set_ll,
   comp_ll,
   free_null,
   NULL_FUNC,
   READ_ONLY | ATR_DFLAG_SvWR,
   ATR_TYPE_LL,
   PARENT_TYPE_JOB},
#endif /* USEJOBCREATE */

  /* Site defined attributes if any, see site_job_attr_*.h  */
#include "site_job_attr_def.h"

  /* JOB_ATR_UNKN - THIS MUST BE THE LAST ENTRY */
  {"_other_",
   decode_unkn,
   encode_unkn,
   set_unkn,
   comp_unkn,
   free_unkn,
   NULL_FUNC,
   READ_WRITE | ATR_DFLAG_SELEQ,
   ATR_TYPE_LIST,
   PARENT_TYPE_JOB}
};
