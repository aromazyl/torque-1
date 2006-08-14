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

#include <assert.h>
#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pbs_ifl.h>
#include "list_link.h"
#include "attribute.h"
#include "pbs_error.h"

/*
 * This file contains functions for manipulating attributes of type
 *	Long integer, where "Long" is defined as the largest integer 
 *	available.
 *
 * Each set has functions for:
 *	Decoding the value string to the machine representation.
 *	Encoding the internal attribute to external form
 *	Setting the value by =, + or - operators.
 *	Comparing a (decoded) value with the attribute value.
 *
 * Some or all of the functions for an attribute type may be shared with
 * other attribute types.
 * 
 * The prototypes are declared in "attribute.h"
 *
 * --------------------------------------------------
 * The Set of Attribute Functions for attributes with
 * value type "Long" (_ll)
 * --------------------------------------------------
 */

#define CVNBUFSZ 23

/*
 * decode_ll - decode Long integer into attribute structure
 *	Unlike decode_long, this function will decode octal (leading zero) and
 *	hex (leading 0x or 0X) data as well as decimal
 *
 *	Returns: 0 if ok
 *		>0 error number if error
 *		*patr elements set
 */

int decode_ll(patr, name, rescn, val)
	struct attribute *patr;
	char *name;		/* attribute name */
	char *rescn;		/* resource name, unused here */
	char *val;		/* attribute value */
{
	char *pc;

	if ((val != (char *)0) && (strlen(val) != 0)) {

		patr->at_val.at_ll = (Long)strTouL(val, &pc, 0);
		if (*pc != '\0')
			return (PBSE_BADATVAL);	 /* invalid string */
		patr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
	} else {
		patr->at_flags = (patr->at_flags & ~ATR_VFLAG_SET) |
				 ATR_VFLAG_MODIFY;
		patr->at_val.at_ll = 0;
	}
	return (0);
}

/*
 * encode_ll - encode attribute of type Long into attr_extern
 *
 *	Returns: >0 if ok
 *		 =0 if no value, no attrlist link added
 *		 <0 if error
 */
/*ARGSUSED*/


int encode_ll(attr, phead, atname, rsname, mode)
	attribute	*attr;	  /* ptr to attribute */
	tlist_head	*phead;	  /* head of attrlist list */
	char		*atname;  /* attribute name */
	char		*rsname;  /* resource name or null */
	int		mode;	  /* encode mode, unused here */
{
	size_t	  ct;
	const char *cvn;
	svrattrl *pal;

	if ( !attr )
		return (-1);
	if ( !(attr->at_flags & ATR_VFLAG_SET))
		return (0);

	cvn = uLTostr(attr->at_val.at_ll, 10);
	ct = strlen(cvn) + 1;

	pal = attrlist_create(atname, rsname, ct);
	if (pal == (svrattrl *)0)
		return (-1);

	(void)memcpy(pal->al_value, cvn, ct);
	pal->al_flags = attr->at_flags;
	append_link(phead, &pal->al_link, pal);
	
	return (1);
}

/*
 * set_ll - set attribute A to attribute B,
 *	either A=B, A += B, or A -= B
 *
 *	Returns: 0 if ok
 *		>0 if error
 */

int set_ll(attr, new, op)
	struct attribute *attr;
	struct attribute *new;
	enum batch_op op;
{
	assert(attr && new && (new->at_flags & ATR_VFLAG_SET));

	switch (op) {
	    case SET:	attr->at_val.at_ll = new->at_val.at_ll;
			break;

	    case INCR:	attr->at_val.at_ll += new->at_val.at_ll;
			break;

	    case DECR:	attr->at_val.at_ll -= new->at_val.at_ll;
			break;

	    default:	return (PBSE_INTERNAL);
	}
	attr->at_flags |= ATR_VFLAG_SET | ATR_VFLAG_MODIFY;
	return (0);
}

/*
 * comp_ll - compare two attributes of type Long
 *
 *	Returns: +1 if 1st > 2nd
 *		  0 if 1st == 2nd
 *		 -1 if 1st < 2nd
 */

int comp_ll(attr, with)
	struct attribute *attr;
	struct attribute *with;
{
	if ( !attr || !with )
		return (-1);
	if (attr->at_val.at_ll < with->at_val.at_ll)
		return (-1);
	else if (attr->at_val.at_ll >  with->at_val.at_ll)
		return (1);
	else
		return (0);
}

/*
 * free_ll - use free_null to (not) free space
 */
