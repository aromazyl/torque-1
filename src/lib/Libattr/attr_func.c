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

#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pbs_ifl.h"
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"

/*
 * This file contains general functions for manipulating attributes.
 * Included are:
 *	clear_attr()
 *	find_attr()
 *	free_null()
 *	attrlist_alloc()
 *	attrlist_create()
 *	parse_equal_string()
 *	parse_comma_string()
 *	count_substrings()
 *
 * The prototypes are declared in "attr_func.h"
 */

/*
 * clear_attr - clear an attribute value structure and clear ATR_VFLAG_SET
 */

void clear_attr(

  attribute	*pattr, /* O */
  attribute_def	*pdef)  /* I */

  {
#ifndef NDEBUG
  if (pdef == NULL) 
    {
    fprintf(stderr, "Assertion failed, bad pdef in clear_attr\n");

    abort();
    }
#endif /* NDEBUG */

  memset((char *)pattr,0,sizeof(struct attribute));

  pattr->at_type = pdef->at_type;

  if ((pattr->at_type == ATR_TYPE_RESC) ||
      (pattr->at_type == ATR_TYPE_LIST))
    {
    CLEAR_HEAD(pattr->at_val.at_list);
    }

  return;
  }  /*END clear_attr() */





/*
 * str_nc_cmp - string no-case comparision
 */

static int str_nc_cmp(

  char *s1,  /* I */
  char *s2)  /* I */

  {
  do 
    {
    if (tolower((int)*s1) != tolower((int)*s2))
      {
      return(-1);
      }
    } while (*s1++ && *s2++);

  return(0);
  }  /* END str_nc_cmp() */




/*
 * find_attr - find attribute definition by name
 *
 *	Searches array of attribute definition strutures to find one
 *	whose name matches the requested name.
 *
 *	Returns: >= 0 index into definition struture array 
 *		   -1 if didn't find matching name
 */

int find_attr(

  struct attribute_def *attr_def, /* ptr to attribute definitions */
  char                 *name,	  /* attribute name to find	*/
  int                   limit)	  /* limit on size of def array	*/

  {
  int index;

  if (attr_def != NULL) 
    {
    for (index = 0;index < limit;index++) 
      {
      if (!str_nc_cmp(attr_def->at_name,name))
        {
        return(index);
        }

      attr_def++;
      }  /* END for (index) */
    }

  return(-1);
  }


/*
 * attr_ifelse_long - if attr1 is set, return it, else
 *                    if attr2 is set, return it, else
 *                    return the default
 */

long attr_ifelse_long(

  attribute *attr1,
  attribute *attr2,
  long deflong)

  {

  if (attr1->at_flags & ATR_VFLAG_SET)
    return (attr1->at_val.at_long);
  else if (attr2->at_flags & ATR_VFLAG_SET)
    return (attr2->at_val.at_long);
  else
    return (deflong);
  }



/*
 * free_null - A free routine for attributes which do not
 *	have malloc-ed space ( boolean, char, long ).
 */

/*ARGSUSED*/

void free_null(

  struct attribute *attr)

  {
  /*
   * a bit of a kludge here, clear out the value (for unset) 
   * the at_size is the larger of the union that does not have
   * additional allocated space, so we use it here
   */

  attr->at_val.at_size.atsv_num   = 0;
  attr->at_val.at_size.atsv_shift = 0;
  attr->at_val.at_size.atsv_units = 0;
  attr->at_flags &= ~ATR_VFLAG_SET;

  return;
  }  /* END free_null() */




/*
 * free_noop - A free routine for attrs that are too important
 *             to be unset.
 */

void free_noop(

  struct attribute *attr)

  {
  /* no nothing */

  return;
  }  /* END free_noop() */




/*
 * comp_null - A do nothing, except return 0, attribute comparison
 *	       function.
 */

int comp_null(

  struct attribute *attr,   /* I */
  struct attribute *with)   /* I */

  {
  /* SUCCESS */

  return(0);
  }




/*
 * attrlist_alloc - allocate space for an svrattrl structure entry
 *
 *	The space required for the entry is calculated and allocated.
 *	The total size and three string lengths are set in the entry,
 *	but no values are placed in it.
 *
 * Return: ptr to entry or NULL if error
 */

svrattrl *attrlist_alloc(

  int szname,  /* I */
  int szresc,  /* I */
  int szval)   /* I */

  {
  register size_t  tsize;
  svrattrl        *pal;

  /* alloc memory block <SVRATTRL><NAME><RESC><VAL> */

  tsize = sizeof(svrattrl) + szname + szresc + szval;

  pal = (svrattrl *)calloc(1,tsize);

  if (pal == NULL)
    { 
    return(NULL);
    }

  CLEAR_LINK(pal->al_link);	/* clear link */

  pal->al_atopl.next = 0;
  pal->al_tsize = tsize;		/* set various string sizes */
  pal->al_nameln = szname;
  pal->al_rescln = szresc;
  pal->al_valln  = szval;
  pal->al_flags  = 0;
  pal->al_op     = SET;
  
  /* point ptrs to name, resc, and val strings to memory after svrattrl struct */

  pal->al_name = (char *)pal + sizeof(svrattrl);

  if (szresc > 0)
    pal->al_resc = pal->al_name + szname;
  else
    pal->al_resc = NULL;

  pal->al_value = pal->al_name + szname + szresc;

  return(pal);
  }  /* END attrlist_alloc() */




/* 
 * attrlist_create - create an svrattrl structure entry
 *
 * The space required for the entry is calculated and allocated.
 * The attribute and resource name is copied into the entry.
 * Note, the value string should be inserted by the caller after this returns.
 *
 * Return: ptr to entry or NULL if error
 */

svrattrl *attrlist_create(

  char  *aname,	/* I - attribute name */
  char  *rname,	/* I - resource name if needed or null */
  int    vsize)	/* I - size of resource value         */

  {
  svrattrl *pal;
  size_t    asz;
  size_t    rsz;

  asz = strlen(aname) + 1;     /* attribute name,allow for null term */

  if (rname == NULL)      /* resource name only if type resource */
    rsz = 0;
  else
    rsz = strlen(rname) + 1;

  pal = attrlist_alloc(asz,rsz,vsize);

  strcpy(pal->al_name,aname);	/* copy name right after struct */

  if (rsz > 0)
    strcpy(pal->al_resc,rname);

  return(pal);
  }




/*
 * free_attrlist - free the space allocated to a list of svrattrl
 *	structures
 */

void free_attrlist(pattrlisthead)
	list_head *pattrlisthead;
{
	svrattrl *pal;
	svrattrl *nxpal;

	pal = (svrattrl *)GET_NEXT(*pattrlisthead);
	while (pal != (svrattrl *)0) {
		nxpal = (struct svrattrl *)GET_NEXT(pal->al_link);
		delete_link(&pal->al_link);
		(void)free(pal);
		pal = nxpal;
	}
}

#if 0		/* This code is not used, but is too good to delete */
/*
 * parse_equal_string - parse a string of the form:
 *		name1 = value1[, value2 ...][, name2 = value3 [, value4 ...]]
 *	into <name1> <value1[, value2 ...>
 *	     <name2> <value3 [, value4 ...>
 *
 *	On the first call,
 *		*name will point to "name1"
 *		*value will point to "value1 ..." upto but not
 *			including the comma before "name2".
 *	On a second call, with start = (char *)0,
 *		*name will point to "name2"
 *		*value will point t0 "value3 ..."
 * Arguments:
 *	start is the start of the string to parse.  If called again with
 *	start  being a null pointer, it will resume parsing where it stoped
 * 	on the prior call.
 *
 * Returns:
 *	integer function return = 1 if  name and value are found,
 *				  0 if nothing (more) is parsed (null input)
 *				 -1 if a syntax error was detected.
 *	*name is set to point to the name string
 *	*value is set to point to the value string
 *	each string is null terminated.
 */

int parse_equal_string(start, name, value)
	char  *start;
	char **name;
	char **value;
{
	static char *pc;	/* where prior call left off */
	char        *backup;
	int	     quoting = 0;

	if (start != (char *)0)
		pc = start;

	if (*pc == '\0') {
		*name = (char *)0;
		return (0);	/* already at end, return no strings */
	}

	/* strip leading spaces */

	while (isspace((int)*pc) && *pc)
		pc++;

	if (*pc == '\0') {
		*name = (char *)0;	/* null name */
		return (0);
	} else if ((*pc == '=') || (*pc == ','))
		return (-1);	/* no name, return error */

	*name = pc;

	/* have found start of name, look for end of it */

	while ( !isspace((int)*pc) && (*pc != '=') && *pc)
		pc++;

	/* now look for =, while stripping blanks between end of name and = */

	while ( isspace((int)*pc) && *pc)
		*pc++ = '\0';
	if (*pc != '=')
		return (-1);	/* should have found a = as first non blank */
	*pc++ = '\0';

	/* that follows is the value string, skip leading white space */

	while ( isspace((int)*pc) && *pc)
		pc++;

	/* is the value string to be quoted ? */

	if ((*pc == '"') || (*pc == '\''))
		quoting = (int)*pc++;
	*value = pc;

	/*
	 * now go to first equal sign, or if quoted, the first equal sign
	 * after the close quote 
	 */

	if (quoting) {
		while ( (*pc != (char)quoting) && *pc)	/* look for matching */
			pc++;
		if (*pc)
			*pc = ' ';	/* change close quote to space */
		else 
			return (-1);
	}
	while ((*pc != '=') && *pc)
		pc++;

	if (*pc == '\0')
		return (1);	/* no equal, just end of line, stop here */
	
	/* back up to the first comma found prior to the equal sign */

	while (*--pc != ',')
		if (pc <= *value)	/* gone back too far, no comma, error */
			return (-1);
	backup = pc++;
	*backup = '\0';			/* null the comma */

	/* strip off any trailing white space */

	while (isspace((int)*--backup))
		*backup = '\0';
	return (1);
}
#endif

/*
 * parse_comma_string() - parse a string of the form:
 *		value1 [, value2 ...]
 *
 *	On the first call, start is non null, a pointer to the first value
 *	element upto a comma, new-line, or end of string is returned.
 *      Commas escaped by a back-slash '\' are ignored.
 *
 *	On any following calls with start set to a null pointer (char *)0,
 *	the next value element is returned...
 *
 *	A null pointer is returned when there are no (more) value elements.
 */

char *parse_comma_string(

  char *start)  /* I (optional) */

  {
  static char *pc;	/* if start is null, restart from here */

  char	    *back;
  char	    *rv;

  if (start != NULL)
    pc = start;
	
  if (*pc == '\0')
    {
    return(NULL);	/* already at end, no strings */
    }

  /* skip over leading white space */

  while ((*pc != '\n') && isspace((int)*pc) && *pc)
    pc++;

  rv = pc;		/* the start point which will be returned */

  /* go find comma or end of line */

  while (*pc) 
    {
    if (*pc == '\\') 
      {
      if (*++pc == '\0')
        break;
      } 
    else if ((*pc == ',') || (*pc == '\n'))
      {
      break;
      }

    ++pc;
    }

  back = pc;

  while (isspace((int)*--back))	/* strip trailing spaces */
    *back = '\0';

  if (*pc != '\0')
    *pc++ = '\0';	/* if not end, terminate this and adv past */
		
  return(rv);
  }  /* END parse_comma_string() */




/*
 * count_substrings
 * counts number of substrings in a comma separated string
 * see parse_comma_string
 */
int	count_substrings( val, pcnt )
        char	*val;           /*comma separated string of substrings*/
	int	*pcnt;		/*where to return the value*/

{
	int	rc = 0;
        int	ns;
	char   *pc;

	if ( val == (char *)0 )
	     return  (PBSE_INTERNAL);
        /*
         * determine number of substrings, each sub string is terminated
         * by a non-escaped comma or a new-line, the whole string is terminated
         * by a null
         */

        ns = 1;
        for (pc = val; *pc; pc++) {
                if (*pc == '\\') {
                        if (*++pc == '\0')
                                break;
                } else if ((*pc == ',') || (*pc == '\n')) {
                        ++ns;
                }
        }
        pc--;
        if ((*pc == '\n') || ((*pc == ',') && (*(pc-1) != '\\'))) {
                ns--;           /* strip any trailing null string */
                *pc = '\0';
        }

	*pcnt = ns;
	return rc;
}



/*
 * attrl_fixlink - fix up the next pointer within the attropl substructure
 *	within a svrattrl list.
 */

void attrl_fixlink(phead)
	list_head *phead;	/* pointer to head of svrattrl list */
{
	svrattrl *pal;
	svrattrl *pnxt;

	pal = (svrattrl *)GET_NEXT(*phead);
	while (pal) {
		pnxt = (svrattrl *)GET_NEXT(pal->al_link);
		if (pal->al_flags & ATR_VFLAG_DEFLT) {
			pal->al_atopl.op = DFLT;
		} else {
			pal->al_atopl.op = SET;
		}
		if (pnxt) 
			pal->al_atopl.next = &pnxt->al_atopl;
		else
			pal->al_atopl.next = (struct attropl *)0;
		pal = pnxt;
	}
}
