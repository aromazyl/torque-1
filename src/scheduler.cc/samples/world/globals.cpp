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

#include <stdio.h>
#include "globals.h"
#include "constant.h"
#include "sort.h"

/*
 * res_to_check[] - a list of resources which controls what resources
 *    are checked by the scheduler.
 *
 * Format: resource_name comment_message debug_message
 *
 * resource_name: name the server knows the resource as
 * comment_message: what the comment should be set to if there is not
 *    enough of this resource available
 * debug_message: what should be logged as the debug message if there
 *         is not enough of this resource available
 *
 */

const struct rescheck res_to_check[] = {
  { "mem",   "Not Running: Not enough memory available", "Not enough memory available", ResCheckStat },
  { "vmem",  "Not Running: Not enough virtual memory available", "Not enough virtual memory available", ResCheckStat },
  { "ncpus", "Not Running: Not enough cpus available", "Not enough cpus available", ResCheckStat},
  { "nodect","Not Running: Not enough nodes available", "Not enough nodes available", ResCheckStat},
  { "cput",  "Not Running: The job time requirement is over the max time limit", "The job time requirement is over the max time limit", ResCheckStat },
  { "scratch", "Not Running: Not enough scratch space available", "Not enough scratch space available", ResCheckCache },
  { "gpu", "Not Running: Not enough GPU cards available", "Not enough gpu cards available", ResCheckStat },
  { "machine_cluster", " ", " ", ResCheckCache },
  { "magrathea", " ", " ", ResCheckCache },

  /* licenses */
  { "matlab", " ", " ", ResCheckDynamic },
  { "fluent", " ", " ", ResCheckDynamic },
  { "ansys", " ", " ", ResCheckDynamic },
  { "marc", " ", " ", ResCheckDynamic },
  { "marcn", " ", " ", ResCheckDynamic },
  { "maple10", " ", " ", ResCheckDynamic },
  { "maple10p", " ", " ", ResCheckDynamic },
  { "maple11", " ", " ", ResCheckDynamic },
  { "maple", " ", " ", ResCheckDynamic },

  /* matlab toolbox licenses */
  { "matlab_Statistics_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_SIMULINK", " ", " ", ResCheckDynamic },
  { "matlab_Bioinformatics_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Communication_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Video_and_Image_Blockset", " ", " ", ResCheckDynamic },
  { "matlab_Control_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Curve_Fitting_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Signal_Blocks", " ", " ", ResCheckDynamic },
  { "matlab_Data_Acq_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Database_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Datafeed_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Econometrics_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_RTW_Embedded_Coder", " ", " ", ResCheckDynamic },
  { "matlab_Fin_Derivatives_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Financial_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Fixed_Income_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Fixed_Point_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Fuzzy_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Image_Acquisition_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Image_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Instr_Control_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_MATLAB_Coder", " ", " ", ResCheckDynamic },
  { "matlab_Compiler", " ", " ", ResCheckDynamic },
  { "matlab_Neural_Network_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Optimization_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Distrib_Computing_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_PDE_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Signal_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_SimHydraulics", " ", " ", ResCheckDynamic },
  { "matlab_SimMechanics", " ", " ", ResCheckDynamic },
  { "matlab_Power_System_Blocks", " ", " ", ResCheckDynamic },
  { "matlab_Simscape", " ", " ", ResCheckDynamic },
  { "matlab_Simulink_Control_Design", " ", " ", ResCheckDynamic },
  { "matlab_Simulink_HDL_Coder", " ", " ", ResCheckDynamic },
  { "matlab_Excel_Link", " ", " ", ResCheckDynamic },
  { "matlab_Symbolic_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Identification_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Vehicle_Network_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Wavelet_Toolbox", " ", " ", ResCheckDynamic },
  { "matlab_Embedded_IDE_Link", " ", " ", ResCheckDynamic },
  { "matlab_Target_Support_Package", " ", " ", ResCheckDynamic },
  { "matlab_MATLAB_Distrib_Comp_Engine", " ", " ", ResCheckDynamic },
  { "ncbr_SUITE", " ", " ", ResCheckDynamic },
  { "ncbr_MAESTRO_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_MMOD_XCLUSTER", " ", " ", ResCheckDynamic },
  { "ncbr_MMOD_MBAE", " ", " ", ResCheckDynamic },
  { "ncbr_MMOD_CONFGEN", " ", " ", ResCheckDynamic },
  { "ncbr_MMLIBS", " ", " ", ResCheckDynamic },
  { "ncbr_CANVAS_SHARED", " ", " ", ResCheckDynamic },
  { "ncbr_CANVAS_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_FFLD_SERVER", " ", " ", ResCheckDynamic },
  { "ncbr_KNIME_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_CANVAS_FULL", " ", " ", ResCheckDynamic },
  { "ncbr_COMBIGLIDE_CORE_HOPPING", " ", " ", ResCheckDynamic },
  { "ncbr_COMBIGLIDE_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_EPIK_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_GLIDE_COMBIGLIDE", " ", " ", ResCheckDynamic },
  { "ncbr_GLIDE_CORE_HOPPING", " ", " ", ResCheckDynamic },
  { "ncbr_GLIDE_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_GLIDE_PRIMEX_LIGFIT", " ", " ", ResCheckDynamic },
  { "ncbr_GLIDE_SP_DOCKING", " ", " ", ResCheckDynamic },
  { "ncbr_GLIDE_XP_DESC", " ", " ", ResCheckDynamic },
  { "ncbr_GLIDE_XP_DOCKING", " ", " ", ResCheckDynamic },
  { "ncbr_IMPACT_COMBIGLIDE", " ", " ", ResCheckDynamic },
  { "ncbr_IMPACT_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_JAGUAR_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_JAGUAR_PKA", " ", " ", ResCheckDynamic },
  { "ncbr_JAGUAR_QSITE", " ", " ", ResCheckDynamic },
  { "ncbr_LIAISON_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_LIGPREP_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_MCPRO_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_MMOD_MACROMODEL", " ", " ", ResCheckDynamic },
  { "ncbr_MMOD_MINTA", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_CORE_HOPPING", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_DBCREATE", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_DBSEARCH", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_FEATURE", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_PARTITION", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_QSAR", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_SCORING", " ", " ", ResCheckDynamic },
  { "ncbr_PHASE_SELECTIVITY", " ", " ", ResCheckDynamic },
  { "ncbr_PRIMEX_LIGPREP_EPIK", " ", " ", ResCheckDynamic },
  { "ncbr_PRIMEX_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_BB", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_FR", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_PLOP", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_PLOP_MEMBRANE", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_RB", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_SKA", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_SSP", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_STA", " ", " ", ResCheckDynamic },
  { "ncbr_PSP_STA_GPCR", " ", " ", ResCheckDynamic },
  { "ncbr_QIKPROP_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_SITEMAP_MAIN", " ", " ", ResCheckDynamic },
  { "ncbr_STRIKE_MAIN", " ", " ", ResCheckDynamic },

};

/*
 *
 * sorting_info[] - holds information about all the different ways you
 *    can sort the jobs
 *
 * Format: { sort_type, config_name, cmp_func_ptr }
 *
 *   sort_type    : an element from the enum sort_type
 *   config_name  : the name which appears in the scheduling policy config
 *           file (sched_config)
 *   cmp_func_ptr : function pointer the qsort compare function
 *    (located in sort.c)
 *
 */

const struct sort_info sorting_info[] =
  {
    {
    NO_SORT, "no_sort", NULL
    },

  {SHORTEST_JOB_FIRST, "shortest_job_first", cmp_job_cput_asc},
  {LONGEST_JOB_FIRST, "longest_job_first", cmp_job_cput_dsc},
  {SMALLEST_MEM_FIRST, "smallest_memory_first", cmp_job_mem_asc},
  {LARGEST_MEM_FIRST, "largest_memory_first", cmp_job_mem_dsc},
  {HIGH_PRIORITY_FIRST, "high_priority_first", cmp_job_prio_dsc},
  {LOW_PRIORITY_FIRST, "low_priority_first", cmp_job_prio_asc},
  {LARGE_WALLTIME_FIRST, "large_walltime_first", cmp_job_walltime_dsc},
  {SHORT_WALLTIME_FIRST, "short_walltime_first", cmp_job_walltime_asc},
  {FAIR_SHARE, "fair_share", cmp_fair_share},
  {MULTI_SORT, "multi_sort", multi_sort}
  };

/*
 * res_to_get - resources to get from each nodes mom
 */
const char *res_to_get[] =
  {
  "ncpus",  /* number of CPUS */
  "arch",  /* the architecture of the machine */
  "physmem",  /* the amount of physical memory */
  "loadave",  /* the current load average */
  "max_load",  /* static max_load value */
  "ideal_load"  /* static ideal_load value */
  };

/* number of indicies in the res_to_check array */
const int num_res = sizeof(res_to_check) / sizeof(struct rescheck);

/* number of indicies in the sorting_info array */
const int num_sorts = sizeof(sorting_info) / sizeof(struct sort_info);

/* number of indicies in the res_to_get array */
const int num_resget = sizeof(res_to_get) / sizeof(char *);


struct config conf;

struct status cstat;
