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
**	This program exists to give a way to mark nodes
**	Down, Offline, or Free in PBS.
**
**	usage: pbsnodes [-s server][-{c|l|o|r}] node node ...
**
**	where the node(s) are the names given in the node
**	description file.
**
**	pbsnodes			clear "DOWN" from all nodes so marked
** 
**	pbsnodes node1 node2		set nodes node1, node2 "DOWN"
**					unmark "DOWN" from any other node
**
**	pbsnodes -a			list all nodes 
**
**	pbsnodes -l			list all nodes marked in any way
**
**	pbsnodes -o node1 node2		mark nodes node1, node2 as OFF_LINE
**					even if currently in use.
**
**	pbsnodes -r node1 node2 	clear OFF_LINE from listed nodes
**
**	pbsnodes -c node1 node2		clear OFF_LINE or DOWN from listed nodes
*/
#include	<pbs_config.h>   /* the master config generated by configure */

#include	<stdio.h>
#include	<unistd.h>
#include	<string.h>
#include	<stdlib.h>
#include	<limits.h>

#include	"portability.h"
#include	"pbs_ifl.h"
#include        "mcom.h"
#include        "cmds.h"
  
#define	DOWN	0
#define	LIST	1
#define	CLEAR	2
#define	OFFLINE	3
#define	RESET	4
#define ALLI	5
#define PURGE   6
#define DIAG    7
#define NOTE    8

int quiet = 0;


/* globals */

mbool_t DisplayXML = FALSE;

/* END globals */


/*
 * set_note - set the note attribute for a node
 *
 */

static int set_note(

  int    con, 
  char  *name,
  char  *msg)

  {
  char	         *errmsg;
  struct attropl  new;
  int             rc;

  new.name     = ATTR_NODE_note;
  new.resource = NULL;
  new.value    = msg;
  new.op       = SET;
  new.next     = NULL;

  rc = pbs_manager(
    con, 
    MGR_CMD_SET, 
    MGR_OBJ_NODE, 
    name, 
    &new, 
    NULL);

  if (rc && !quiet) 
    {
    fprintf(stderr,"Error setting note attribute for %s - ", 
      name);

    if ((errmsg = pbs_geterrmsg(con)) != NULL)
      fprintf(stderr,"%s\n", 
        errmsg);
    else
      fprintf(stderr,"(error %d)\n", 
        pbs_errno);
    }

  return(rc);
  }


/*
 * cmp_node_name - compare two node names, allow the second to match the
 *	first if the same up to a dot ('.') in the second; i.e.
 *	"foo" == "foo.bar"
 *
 *	return 0 if match, 1 if not
 */

static int cmp_node_name(

  char *n1, 
  char *n2)

  {
  while ((*n1 != '\0') && (*n2 != '\0')) 
    {
    if (*n1 != *n2)
      break;

    n1++;
    n2++;
    }

  if (*n1 == *n2) 
    {
    return(0);
    }

  if ((*n1 == '.') && (*n2 == '\0'))
    {
    return(0);
    }

  return(1);
  }  /* END cmp_node_name() */




static void prt_node_attr(

  struct batch_status *pbs,         /* I */
  int                  IsVerbose)   /* I */

  {
  struct attrl *pat;

  for (pat = pbs->attribs;pat;pat = pat->next) 
    {
    if ((pat->value == NULL) || (pat->value[0] == '?'))
      {
      if (IsVerbose == 0)
        continue;
      }

    printf("     %s = %s\n", 
      pat->name, 
      pat->value);
    }  /* END for (pat) */

  return;
  }  /* END prt_node_attr() */




static char *get_nstate(

  struct batch_status *pbs)  /* I */

  {
  struct attrl *pat;

  for (pat = pbs->attribs;pat != NULL;pat = pat->next) 
    {
    if (strcmp(pat->name,ATTR_NODE_state) == 0)
      {
      return(pat->value);
      }
    }

  return("");
  }





/*
 * is_down - returns indication if node is marked down or not
 *
 *	returns 1 if node is down and 0 if not down
 */

static int is_down(

  struct batch_status *pbs)  /* I */

  {
  if (strstr(get_nstate(pbs),ND_down) != NULL)
    {
    return(1);
    }

  return(0);
  }





static int is_offline(

  struct batch_status *pbs)

  {
  if (strstr(get_nstate(pbs),ND_offline) != NULL)
    {
    return(1);
    }

  return(0);
  }




static int is_busy(

  struct batch_status *pbs)

  {
  if (strstr(get_nstate(pbs),ND_busy) != NULL)
    {
    return(1);
    }

  return(0);
  }




static int is_active(

  struct batch_status *pbs)

  {
  char *ptr;

  ptr = get_nstate(pbs);

  if ((strstr(ptr,ND_busy) != NULL) ||
      (strstr(ptr,ND_job_sharing) != NULL) ||
      (strstr(ptr,ND_job_exclusive) != NULL))
    {
    return(1);
    }

  return(0);
  }




static int is_free(

  struct batch_status *pbs)

  {
  if (strstr(get_nstate(pbs),ND_free) != NULL)
    {
    return(1);
    }

  return(0);
  }




static int is_unknown(

  struct batch_status *pbs)

  {
  if (strstr(get_nstate(pbs),ND_state_unknown) != NULL)
    {
    return(1);
    }

  return(0);
  }




static int marknode(

  int            con, 
  char          *name, 
  char          *state1, 
  enum batch_op  op1,
  char          *state2, 
  enum batch_op  op2)

  {
  char	         *errmsg;
  struct attropl  new[2];
  int             rc;

  new[0].name     = ATTR_NODE_state;
  new[0].resource = NULL;
  new[0].value    = state1;
  new[0].op       = op1;

  if (state2 == NULL) 
    {
    new[0].next     = NULL;
    } 
  else 
    {
    new[0].next     = &new[1];
    new[1].next     = NULL;
    new[1].name     = ATTR_NODE_state;
    new[1].resource = NULL;
    new[1].value    = state2;
    new[1].op	    = op2;
    }

  rc = pbs_manager(
    con, 
    MGR_CMD_SET, 
    MGR_OBJ_NODE, 
    name, 
    new, 
    NULL);

  if (rc && !quiet) 
    {
    fprintf(stderr,"Error marking node %s - ", 
      name);

    if ((errmsg = pbs_geterrmsg(con)) != NULL)
      fprintf(stderr,"%s\n", 
        errmsg);
    else
      fprintf(stderr,"error: %d\n",
        pbs_errno);
    }

  return(rc);
  }  /* END marknode() */




enum NStateEnum {
  tnsNONE = 0, /* default behavior - show down, offline, and unknown nodes */
  tnsActive,   /* one or more jobs running on node */
  tnsAll,      /* list all nodes */
  tnsBusy,     /* node cannot accept additional workload */
  tnsDown,     /* node is down */
  tnsFree,     /* node is idle/free */
  tnsOffline,  /* node is offline */
  tnsUnknown,  /* node is unknown - no contact recieved */
  tnsUp,       /* node is healthy */
  tnsLAST };

const char *NState[] = {
  "NONE",
  "active",
  "all",
  "busy",
  "down",
  "free",
  "offline",
  "up",
  NULL };



int main(

  int	 argc,  /* I */
  char **argv)  /* I */

  {
  struct batch_status *bstatus = NULL;
  int	 con;
  char	*def_server;
  int	 errflg = 0;
  char	*errmsg;
  int	 i;
  extern char	*optarg;
  extern int	 optind;
  char	       **pa;
  struct batch_status *pbstat;
  int	flag = ALLI;
  char	*note = NULL;
  int	note_flag = 0;
  int	len;
  char  NodeArg[256000];

  enum NStateEnum ListType = tnsNONE;

  /* get default server, may be changed by -s option */

  def_server = pbs_default();

  if (def_server == NULL)
    def_server = "";

  while ((i = getopt(argc,argv,"acdlopqrs:x-:n:")) != EOF)
    {
    switch(i) 
      {
      case 'a':

        flag = ALLI;
 
        break;

      case 'c':

        flag = CLEAR;

        break;

      case 'd':

        flag = DIAG;

        break;

      case 'l':

        flag = LIST;

        break;

      case 'o':

        flag = OFFLINE;

        break;

      case 'p':

        flag = PURGE;

        break;

      case 'q':

        quiet = 1;

        break;

      case 'r':

        flag = RESET;

        break;

      case 's':
   
        def_server = optarg;

        break;

      case 'x':

        flag = ALLI;

        DisplayXML = TRUE;

        break;

      case 'n':

        note_flag = 1;

        /* preserve any previous option other than the default,
         * to allow -n to be combined with -o, -c, etc
         */

        if (flag == ALLI)
          flag = NOTE;

        note = strdup(optarg);

        if (note != NULL)
          {
          /* -n n is the same as -n ""  -- it clears the note */

          if (!strcmp(note,"n"))
            {
            *note = '\0';
            }

          len = strlen(note);

          if (len > MAX_NOTE)
            fprintf(stderr,"Warning: note exceeds length limit (%d) - server may reject it...\n",
              MAX_NOTE);

          if (strchr(note,'\n') != NULL)
            fprintf(stderr,"Warning: note contains a newline - server may reject it...\n");
          }

        break;

      case '-':

        if ((optarg != NULL) && !strcmp(optarg,"version"))
          {
          fprintf(stderr,"version: %s\n",
            PACKAGE_VERSION);

          exit(0);
          }
        else if ((optarg != NULL) && !strcmp(optarg,"about"))
          {
          TShowAbout();

          exit(0);
          }

        errflg = 1;

        break;

      case '?':
      default:

        errflg = 1;

        break;
      }  /* END switch (i) */
    }    /* END while (i == getopt()) */

  if (errflg != 0) 
    {
    if (!quiet)
      {
      fprintf(stderr,"usage:\t%s [-{c|d|l|o|p|r}][-s server] [-n \"note\"] [-q] node node ...\n",
        argv[0]);

      fprintf(stderr,"\t%s -{a|x} [-s server] [-q] [node]\n",
        argv[0]);
      }

    exit(1);
    }

  con = cnt2server(def_server);

  if (con <= 0) 
    {
    if (!quiet)
      {
      fprintf(stderr, "%s: cannot connect to server %s, error=%d\n",
        argv[0],           
        def_server, 
        pbs_errno);
      }

    exit(1);
    }

  /* if flag is ALLI, DOWN or LIST, get status of all nodes */

  NodeArg[0] = '\0';

  if ((flag == ALLI) || (flag == DOWN) || (flag == LIST) || (flag == DIAG)) 
    {
    if ((flag == ALLI) || (flag == LIST) || (flag == DIAG))
      {
      if (flag == LIST)
        {
        /* allow state specification */

        if (argv[optind] != NULL)
          {
          int lindex;

          for (lindex = 1;lindex < tnsLAST;lindex++)
            {
            if (!strcasecmp(NState[lindex],optarg))
              {
              ListType = lindex;

              optind++;

              break;
              }
            }
          } 
        }

      /* allow node specification */

      if (argv[optind] != NULL)
        {
        strcpy(NodeArg,argv[optind]);
        }
      }

    bstatus = pbs_statnode(con,NodeArg,NULL,NULL);

    if (bstatus == NULL) 
      {
      if (pbs_errno) 
        {
        if (!quiet) 
          {
          if ((errmsg = pbs_geterrmsg(con)) != NULL)
            {
            fprintf(stderr,"%s: %s\n",
              argv[0],
              errmsg);
            }
          else
            {
            fprintf(stderr,"%s: Error %d\n",
              argv[0],
              pbs_errno);
            }
          }

        exit(1);
        }
 
      if (!quiet)
        fprintf(stderr,"%s: No nodes found\n", 
          argv[0]);

      exit(0);
      }
    }    /* END if ((flag == ALLI) || (flag == DOWN) || (flag == LIST) || (flag == DIAG)) */

  if (note_flag && (note != NULL))
    {
    /* set the note attrib string on specified nodes */
    for (pa = argv + optind;*pa;pa++) 
      {
      set_note(con,*pa,note);
      }
    }

  switch (flag) 
    {
    case DIAG:

      /* NYI */

      break;

    case DOWN:

      /*
       * loop through the list of nodes returned above:
       *   if node is up and is in argv list, mark it down;
       *   if node is down and not in argv list, mark it up;
       * for all changed nodes, send in request to server
       */

      for (pbstat = bstatus;pbstat != NULL;pbstat = pbstat->next) 
        {
        for (pa = argv + optind;*pa;pa++) 
          {
          if (cmp_node_name(*pa,pbstat->name) == 0) 
            {
            if (is_down(pbstat) == 0) 
              {
              marknode(
                con, 
                pbstat->name,
                ND_down, 
                INCR, 
                NULL, 
                INCR);

              break;
              }
            }
          }

        if (*pa == NULL) 
          {
          /* node not in list, if down now, set up */

          if (is_down(pbstat) == 1)
            {
            marknode(con,pbstat->name, 
              ND_down,DECR,NULL,DECR);
            }
          }
        }

      break;

    case CLEAR:

      /* clear  OFFLINE from specified nodes */

      for (pa = argv + optind;*pa;pa++) 
        {
        marknode(con,*pa,ND_offline,DECR,NULL,DECR);	
        }

      break;

    case RESET:

      /* clear OFFLINE, add DOWN to specified nodes */

      for (pa = argv + optind;*pa;pa++) 
        {
        marknode(con,*pa,ND_offline,DECR,ND_down,INCR);	
        }

      break;

    case OFFLINE:

      /* set OFFLINE on specified nodes */

      for (pa = argv + optind;*pa;pa++) 
        {
        marknode(con,*pa,ND_offline,INCR,NULL,INCR);	
        }

      break;

    case PURGE:

      /* remove node record */

      /* NYI */

      break;

    case ALLI:

      if (DisplayXML == TRUE)
        {
        struct attrl *pat;

        char *tmpBuf=NULL, *tail=NULL;
        int  bufsize;

        mxml_t *DE;
        mxml_t *NE;
        mxml_t *AE;

        DE = NULL;

        MXMLCreateE(&DE,"Data");

        for (pbstat = bstatus;pbstat;pbstat = pbstat->next)
          {
          NE = NULL;

          MXMLCreateE(&NE,"Node");

          MXMLAddE(DE,NE);

          /* add nodeid */

          AE = NULL;
          MXMLCreateE(&AE,"name");
          MXMLSetVal(AE,pbstat->name,mdfString);
          MXMLAddE(NE,AE);

          for (pat = pbstat->attribs;pat;pat = pat->next)
            {
            AE = NULL;

            if (pat->value == NULL)
              continue;

            MXMLCreateE(&AE,pat->name);

            MXMLSetVal(AE,pat->value,mdfString);

            MXMLAddE(NE,AE);
            }
          }    /* END for (pbstat) */

        MXMLToXString(DE,&tmpBuf,&bufsize,INT_MAX,&tail,TRUE);

        MXMLDestroyE(&DE);

        fprintf(stdout,"%s\n",
          tmpBuf);
        }
      else
        {
        for (pbstat = bstatus;pbstat;pbstat = pbstat->next) 
          {
          printf("%s\n", 
            pbstat->name);

          prt_node_attr(pbstat,0);

          putchar('\n');
          }  /* END for (bpstat) */
        }

      break;

    case LIST:

      {
      char *S;
      int   NState;

      int   Display;

      /* list any node that is DOWN, OFFLINE, or UNKNOWN */

      for (pbstat = bstatus;pbstat != NULL;pbstat = pbstat->next) 
        {
        S = get_nstate(pbstat);

        Display = 0;

        switch (ListType)
          {
          case tnsNONE:  /* display down, offline, and unknown nodes */
          default:

            if (!strcmp(S,ND_down) || !strcmp(S,ND_offline) || !strcmp(S,ND_state_unknown)) 
              {
              Display = 1;
              }

            break;

          case tnsActive:   /* one or more jobs running on node */

            if (!strcmp(S,ND_busy) || !strcmp(S,ND_job_exclusive) || !strcmp(S,ND_job_sharing))
              {
              Display = 1;
              }

            break;

          case tnsAll:      /* list all nodes */

            Display = 1;

            break;

          case tnsBusy:     /* node cannot accept additional workload */

            if (!strcmp(S,ND_busy))
              {
              Display = 1;
              }

            break;

          case tnsDown:     /* node is down or unknown */

            if (!strcmp(S,ND_down) || !strcmp(S,ND_state_unknown))
              {
              Display = 1;
              }

            break;

          case tnsFree:     /* node is idle/free */

            if (!strcmp(S,ND_free))
              {
              Display = 1;
              }

            break;

          case tnsOffline:  /* node is offline */

            if (!strcmp(S,ND_offline))
              {
              Display = 1;
              }

            break;

          case tnsUnknown:  /* node is unknown - no contact recieved */

            if (!strcmp(S,ND_state_unknown))
              {
              Display = 1;
              }
   
            break;

          case tnsUp:       /* node is healthy */

            if (strcmp(S,ND_down) && strcmp(S,ND_offline) && strcmp(S,ND_state_unknown))
              {
              Display = 1;
              }

            break;
          }  /* END switch (ListType) */

        if (Display == 1)
          {
          printf("%-20.20s %s\n",
            pbstat->name,
            S);
          }
        }
      }    /* END BLOCK (case LIST) */

      break;
    }  /* END switch (flag) */

  pbs_disconnect(con);
  
  return(0);
  }  /* END main() */

/* END pbsnodes.c */

