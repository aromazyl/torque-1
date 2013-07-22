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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <new>
using namespace std;

#include "torque.h"
#include "node_info.h"
#include "misc.h"
#include "globals.h"
#include "api.hpp"
#include "global_macros.h"


/* Internal functions */
int set_node_type(node_info *ninfo, char *ntype);


/*
 *      query_nodes - query all the nodes associated with a server
 *
 *   pbs_sd - communication descriptor wit the pbs server
 *   sinfo -  server information
 *
 * returns array of nodes associated with server
 *
 */
node_info **query_nodes(int pbs_sd, server_info *sinfo)
  {

  struct batch_status *nodes;  /* nodes returned from the server */

  struct batch_status *cur_node; /* used to cycle through nodes */
  node_info **ninfo_arr;  /* array of nodes for scheduler's use */
  node_info *ninfo;   /* used to set up a node */
  char *err;    /* used with pbs_geterrmsg() */
  int num_nodes = 0;   /* the number of nodes */
  int i;

  if ((nodes = pbs_statnode(pbs_sd, NULL, NULL, NULL)) == NULL)
    {
    err = pbs_geterrmsg(pbs_sd);
    sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, "", "Error getting nodes: %s", err);
    return NULL;
    }

  cur_node = nodes;

  while (cur_node != NULL)
    {
    num_nodes++;
    cur_node = cur_node -> next;
    }

  if ((ninfo_arr = (node_info **) malloc((num_nodes + 1) * sizeof(node_info *))) == NULL)
    {
    perror("Error Allocating Memory");
    pbs_statfree(nodes);
    return NULL;
    }

  cur_node = nodes;

  for (i = 0; cur_node != NULL; i++)
    {
    if ((ninfo = query_node_info(cur_node, sinfo)) == NULL)
      {
      pbs_statfree(nodes);
      free_nodes(ninfo_arr);
      return NULL;
      }
#if 0
    /* query mom on the node for resources */
    if (!conf.no_mom_talk)
      {
      sched_log(PBSEVENT_DEBUG2, PBS_EVENTCLASS_NODE, "", "Talking with node.");
      talk_with_mom(ninfo);
      }
#endif
    ninfo_arr[i] = ninfo;

    cur_node = cur_node -> next;
    }

  ninfo_arr[i] = NULL;
  sinfo -> num_nodes = num_nodes;
  pbs_statfree(nodes);

  /* setup virtual <-> physical nodes mapping */
  for (i = 0; i < num_nodes; i++)
    {
    if (ninfo_arr[i]->type == NodeVirtual)
      {
      node_info *physical;
      char *cache = xpbs_cache_get_local(ninfo_arr[i]->name,"host");
      if (cache == NULL)
        {
        sched_log(PBSEVENT_ERROR, PBS_EVENTCLASS_NODE, ninfo_arr[i]->name,
                  "Bad configuration, virtual node without physical host.");
        ninfo_arr[i]->is_usable_for_boot = 0;
        ninfo_arr[i]->is_usable_for_run = 0;
        continue;
        }

      char *host = cache_value_only(cache);
      free(cache);

      physical = find_node_info(host,ninfo_arr);
      if (physical == NULL)
      { free(host); continue; }

      ninfo_arr[i]->host = physical;
      physical->hosted.push_back(ninfo_arr[i]);

      free(host);
      }
    }

  return ninfo_arr;
  }

/*
 *
 *      query_node_info - collect information from a batch_status and
 *                        put it in a node_info struct for easier access
 *
 *   node - a node returned from a pbs_statnode() call
 *
 * returns a node_info filled with information from node
 *
 */
node_info *query_node_info(struct batch_status *node, server_info *sinfo)
  {
  node_info *ninfo;  /* the new node_info */
  int count;
  resource *resp;
  struct attrl *attrp;  /* used to cycle though attribute list */

  if ((ninfo = new_node_info()) == NULL)
    return NULL;

  attrp = node -> attribs;

  ninfo -> name = strdup(node -> name);

  ninfo -> server = sinfo;

  while (attrp != NULL)
    {
    /* Node State... i.e. offline down free etc */
    if (!strcmp(attrp -> name, ATTR_NODE_state))
      set_node_state(ninfo, attrp -> value);

    /* properties from the servers nodes file */
    else if (!strcmp(attrp -> name, ATTR_NODE_properties))
      comma_list_to_set(attrp->value,ninfo->physical_properties);
    else if (!strcmp(attrp -> name, ATTR_NODE_adproperties))
      comma_list_to_set(attrp->value,ninfo->virtual_properties);

    /* the jobs running on the node */
    else if (!strcmp(attrp -> name, ATTR_NODE_jobs))
      ninfo -> jobs = break_comma_list(attrp -> value);

    /* the node type... i.e. timesharing or cluster */
    else if (!strcmp(attrp -> name, ATTR_NODE_ntype))
      set_node_type(ninfo, attrp -> value);

    else if (!strcmp(attrp -> name, ATTR_NODE_npfree))
      ninfo -> npfree = atoi(attrp -> value);

    else if (!strcmp(attrp -> name, ATTR_NODE_np))
      ninfo -> np = atoi(attrp -> value);

    else if (!strcmp(attrp -> name, ATTR_NODE_status))
      ninfo -> big_status = break_comma_list(attrp -> value);

    else if (!strcmp(attrp -> name, ATTR_NODE_no_multinode_jobs))
      ninfo -> no_multinode_jobs = !strcmp(attrp -> value,"True");

    else if (!strcmp(attrp -> name, ATTR_NODE_queue))
      {
      ninfo->queue = strdup(attrp->value);
      }
    else if (!strcmp(attrp -> name, ATTR_NODE_noautoresv))
      ninfo -> no_starving_jobs = !strcmp(attrp->value,"True");

    else if (!strcmp(attrp -> name, ATTR_NODE_exclusively_assigned))
      ninfo -> is_exclusively_assigned = !strcmp(attrp->value,"True");

    else if (!strcmp(attrp -> name, ATTR_NODE_admin_slot_available))
      ninfo -> admin_slot_available = !strcmp(attrp->value,"True");

    else if (!strcmp(attrp -> name, ATTR_NODE_resources_total))
      {
      if (is_num(attrp -> value))
        {
        count = res_to_num(attrp -> value);
        }
      else count = UNSPECIFIED;

      resp = find_alloc_resource(ninfo -> res, attrp -> resource);

      if (ninfo -> res == NULL)
        ninfo -> res = resp;

      if (resp != NULL)
        {
        if (count != UNSPECIFIED)
          {
          resp -> max = count;
          if (resp->avail == UNSPECIFIED) resp->avail = 0;
          }
        else
          { /* uncounted string resource */
          resp -> max = UNSPECIFIED;
          resp -> is_string = 1;
          resp -> str_avail = strdup(attrp -> value);
          resp -> avail = UNSPECIFIED;
          resp -> assigned = UNSPECIFIED;
          }
        }
      }
    else if (!strcmp(attrp -> name, ATTR_NODE_resources_used))
      {
      count = res_to_num(attrp -> value);
      resp = find_alloc_resource(ninfo -> res, attrp -> resource);

      if (ninfo -> res == NULL)
        ninfo -> res = resp;

      if (resp != NULL)
        {
        resp -> assigned = count;
        if (resp->max == UNSPECIFIED) resp->max = 0;
        }
      }

    attrp = attrp -> next;
    }

  return ninfo;
  }

/*
 *
 *      new_node_info - allocates a new node_info
 *
 * returns the new node_info
 *
 */
node_info *new_node_info()
  {
  node_info *tmp;

  if ((tmp = new (nothrow) node_info) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  tmp -> is_down = 0;

  tmp -> is_free = 0;
  tmp -> is_offline = 0;
  tmp -> is_unknown = 0;
  tmp -> is_reserved = 0;
  tmp -> is_exclusive = 0;
  tmp -> is_sharing = 0;
  tmp -> no_multinode_jobs = 0;

  tmp -> name = NULL;
  tmp -> jobs = NULL;
  tmp -> big_status = NULL;
  tmp -> queue = NULL;

  tmp -> max_load = 0.0;
  tmp -> ideal_load = 0.0;
  tmp -> arch = NULL;
  tmp -> ncpus = 0;
  tmp -> physmem = 0;
  tmp -> loadave = 0.0;

  tmp -> np = 0;
  tmp -> npfree = 0;
  tmp -> npassigned = 0;

  tmp->temp_assign = NULL;
  tmp->temp_assign_scratch = ScratchNone;
  tmp->temp_assign_alternative = NULL;

  tmp -> res = NULL;

  tmp -> starving_job = NULL;

  tmp -> cluster_name = NULL;

  tmp -> alternatives = NULL;

  tmp -> is_bootable = 0;

  tmp -> admin_slot_available = 0;

  tmp->is_exclusively_assigned  = 0;
  tmp->is_usable_for_boot = 1;
  tmp->is_usable_for_run  = 1;
  tmp->is_full            = 0;

  tmp->host = NULL;
  tmp->hosted.reserve(2);

  return tmp;
  }

/*
 *
 * free_nodes - free all the nodes in a node_info array
 *
 *   ninfo_arr - the node info array
 *
 * returns nothing
 *
 */
void free_nodes(node_info **ninfo_arr)
  {
  int i;

  if (ninfo_arr != NULL)
    {
    for (i = 0; ninfo_arr[i] != NULL; i++)
      free_node_info(ninfo_arr[i]);

    free(ninfo_arr);
    }
  }

/*
 *
 *      free_node_info - frees memory used by a node_info
 *
 *   ninfo - the node to free
 *
 * returns nothing
 *
 */
void free_node_info(node_info *ninfo)
  {
  if (ninfo != NULL)
    {
    free(ninfo -> name);
    free_string_array(ninfo -> jobs);
    free(ninfo -> arch);
    free_string_array(ninfo -> big_status);
    free_resource_list(ninfo -> res);
    free_bootable_alternatives(ninfo->alternatives);
    free(ninfo -> queue);
    free(ninfo -> cluster_name);
    delete ninfo;
    }
  }

/*
 *
 * set_node_type - set the node type bits
 *
 *   ninfo - the node to set the type
 *   ntype - the type string from the server
 *
 * returns non-zero on error
 *
 */
int set_node_type(node_info *ninfo, char *ntype)
  {
  if (ntype != NULL && ninfo != NULL)
    {
    if (!strcmp(ntype, ND_timeshared))
      ninfo -> type = NodeTimeshared;
    else if (!strcmp(ntype, ND_cluster))
      ninfo -> type = NodeCluster;
    else if (!strcmp(ntype, ND_cloud))
      ninfo -> type = NodeCloud;
    else if (!strcmp(ntype, ND_virtual))
      ninfo -> type = NodeVirtual;
    else
      {
      sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo -> name, "Unknown node type: %s", ntype);
      return 1;
      }

    return 0;
    }

  return 1;
  }

/*
 *
 *      set_node_state - set the node state info bits
 *
 *   ninfo - the node to set the state
 *   state - the state string from the server
 *
 * returns non-zero on error
 *
 */
int set_node_state(node_info *ninfo, char *state)
  {
  char *tok;    /* used with strtok() */

  if (ninfo != NULL && state != NULL)
    {
    tok = strtok(state, ",");

    while (tok != NULL)
      {
      while (isspace((int) *tok))
        tok++;

      if (!strcmp(tok, ND_down))
        ninfo -> is_down = 1;
      else if (!strcmp(tok, ND_free))
        ninfo -> is_free = 1;
      else if (!strcmp(tok, ND_offline))
        ninfo -> is_offline = 1;
      else if (!strcmp(tok, ND_state_unknown))
        ninfo -> is_unknown = 1;
      else if (!strcmp(tok, ND_job_exclusive))
        ninfo -> is_exclusive = 1;
      else if (!strcmp(tok, ND_job_sharing))
        ninfo -> is_sharing = 1;
      else if (!strcmp(tok, ND_reserve))
        ninfo -> is_reserved = 1;
      else if (!strcmp(tok, ND_busy))
        ninfo -> is_busy = 1;
      else
        {
        sched_log(PBSEVENT_SCHED, PBS_EVENTCLASS_NODE, ninfo -> name, "Unknown Node State: %s", tok);
        }

      tok = strtok(NULL, ",");
      }

    return 0;
    }

  return 1;
  }



/*
 *
 * node_filter - filter a node array and return a new filterd array
 *
 *   nodes - the array to filter
 *   size  - size of nodes
 *    filter_func - pointer to a function that will filter the nodes
 *  - returns 1: job will be added to filtered array
 *  - returns 0: job will NOT be added to filtered array
 *   arg - an optional arg passed to filter_func
 *
 * returns pointer to filtered array
 *
 * filter_func prototype: int func( node_info *, void * )
 *
 */
node_info **node_filter(node_info **nodes, int size,
                        int (*filter_func)(node_info*, void*), void *arg)
  {
  node_info **new_nodes = NULL;   /* the new node array */
  int i, j;

  if ((new_nodes = (node_info **) malloc((size + 1) * sizeof(node_info *))) == NULL)
    {
    perror("Memory Allocation Error");
    return NULL;
    }

  for (i = 0, j = 0; i < size; i++)
    {
    if (filter_func(nodes[i], arg))
      {
      new_nodes[j] = nodes[i];
      j++;
      }
    }

  new_nodes[j] = NULL;

  if (j == 0)
    {
    free(new_nodes);
    new_nodes = NULL;
    }
  else if ((new_nodes = (node_info **) realloc(new_nodes, (j + 1) * sizeof(node_info *))) == NULL)
    {
    perror("Memory Allocation Error");
    free(new_nodes);
    }

  return new_nodes;
  }

/*
 *
 *      is_node_timeshared - check if a node is timeshared
 *
 *        node - node to check
 *        arg  - unused argument
 *
 *      returns
 *        1: is a timeshared node
 *        0: is not a timeshared node
 *
 *      NOTE: this function used for node_filter
 *
 */
int is_node_timeshared(node_info *node, void * UNUSED(arg))
  {
  if (node != NULL)
    return (node->type == NodeTimeshared);

  return 0;
  }


int is_node_non_dedicated(node_info *node, void * UNUSED(arg))
  {
  if (node != NULL)
    return (node->queue == NULL);

  return 0;
  }

/*
 *
 * find_node_info - find a node in a node array
 *
 *   nodename - the node to find
 *   ninfo_arr - the array of nodes to look in
 *
 * returns the node or NULL of not found
 *
 */
node_info *find_node_info(char *nodename, node_info **ninfo_arr)
  {
  int i;

  if (nodename != NULL && ninfo_arr != NULL)
    {
    for (i = 0; ninfo_arr[i] != NULL &&
         strcmp(ninfo_arr[i] -> name, nodename); i++)
      ;

    return ninfo_arr[i];
    }

  return NULL;
  }

/*
 *
 * print_node - print all the information in a node.  Mainly used for
 *        debugging purposes
 *
 *   ninfo - the node to print
 *   brief - boolean: only print the name ?
 *
 * returns nothing
 *
 */
void print_node(node_info *ninfo, int brief)
  {
  int i;

  if (ninfo != NULL)
    {
    if (ninfo -> name != NULL)
      printf("Node: %s\n", ninfo -> name);

    if (!brief)
      {
      printf("is_down: %s\n", ninfo -> is_down ? "TRUE" : "FALSE");
      printf("is_free: %s\n", ninfo -> is_free ? "TRUE" : "FALSE");
      printf("is_offline: %s\n", ninfo -> is_offline ? "TRUE" : "FALSE");
      printf("is_unknown: %s\n", ninfo -> is_unknown ? "TRUE" : "FALSE");
      printf("is_reserved: %s\n", ninfo -> is_reserved ? "TRUE" : "FALSE");
      printf("is_exclusive: %s\n", ninfo -> is_exclusive ? "TRUE" : "FALSE");
      printf("is_sharing: %s\n", ninfo -> is_sharing ? "TRUE" : "FALSE");

      printf("np: %d | npfree: %d | npassigned: %d\n",
             ninfo->np, ninfo->npfree, ninfo->npassigned);

      set<string>::iterator it;

      if (ninfo->physical_properties.size() > 0)
        {
        printf("Properties: ");

        for (it = ninfo->physical_properties.begin(); it != ninfo->physical_properties.end(); i++)
          {
          if (it != ninfo->physical_properties.begin())
            printf(", ");
          printf("%s", it->c_str());
          }

        printf("\n");
        }

      if (ninfo->virtual_properties.size() > 0)
        {
        printf("Additional Properties: ");

        for (it = ninfo->virtual_properties.begin(); it != ninfo->virtual_properties.end(); i++)
          {
          if (it != ninfo->virtual_properties.begin())
            printf(", ");
          printf("%s", it->c_str());
          }

        printf("\n");
        }

      if (ninfo -> res != NULL)
        {
        resource *res = ninfo->res;
        printf("Resources:\n");

        while (res != NULL)
          {
          printf("\t%s : max %lld | avail %lld | assigned %lld | string: %s\n",
              res->name,res->max, res->avail, res->assigned,
              res->str_avail?res->str_avail:"");
          res = res->next;
          }
        }

      if (ninfo -> jobs != NULL)
        {
        printf("Running Jobs: ");

        for (i = 0; ninfo -> jobs[i] != NULL; i++)
          {
          if (i)
            printf(", ");

          printf("%s", ninfo -> jobs[i]);
          }
        }

      if (ninfo -> big_status != NULL)
        {
        printf("Status: \n");

        for (i = 0; ninfo -> big_status[i] != NULL; i++)
            printf("\t%s\n", ninfo -> big_status[i]);
        }
      }
    }
  }