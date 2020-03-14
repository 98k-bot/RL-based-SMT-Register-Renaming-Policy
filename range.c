//range.c - program execution range routines

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved. 
 * 
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 * 
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 * 
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 * 
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 * 
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 * 
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 * 
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 * 
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */

#include<cstdio>
#include<cstdlib>
#include<string>
#include<errno.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "symbol.h"
#include "loader.h"
#include "range.h"

//parse execution position *PSTR to *POS, returns error string or NULL
const char * range_parse_pos(char *pstr,		//execution position string
	range_pos_t *pos,				//position return buffer
	mem_t* my_mem)
{
	char *s, *endp;

	//determine position type
	if (pstr[0] == '@')
	{
		//address position
		pos->ptype = pt_addr;
		s = pstr + 1;
	}
	else if (pstr[0] == '#')
	{
		//cycle count position
		pos->ptype = pt_cycle;
		s = pstr + 1;
	}
	else
	{
		//inst count position
		pos->ptype = pt_inst;
		s = pstr;
	}

	//get position value
	errno = 0;
	pos->pos = (counter_t)strtoul(s, &endp, /*parse base*/0);
	if(!errno && !*endp)
	{
		//good conversion
		return NULL;
	}

	//else, not an integer, attempt double conversion
	errno = 0;
	pos->pos = (counter_t)strtod(s, &endp);
	if(!errno && !*endp)
	{
		//good conversion
		//FIXME: ignoring decimal point!!
		return NULL;
	}

	//else, attempt symbol lookup
	sym_loadsyms(my_mem->ld_prog_fname.c_str(), /*!locals*/FALSE, my_mem);
	sym_sym_t *sym = sym_bind_name(s, NULL, sdb_any);
	if(sym != NULL)
	{
		pos->pos = (counter_t)sym->addr;
		return NULL;
	}

	//else, no binding made
	return "cannot bind execution position to a value";
}

//print execution position *POS
void range_print_pos(range_pos_t *pos,		//execution position
	FILE *stream)				//output stream
{
	switch(pos->ptype)
	{
	case pt_addr:
		myfprintf(stream, "@0x%08p", (md_addr_t)pos->pos);
		break;
	case pt_inst:
		fprintf(stream, "%.0f", (double)pos->pos);
		break;
	case pt_cycle:
		fprintf(stream, "#%.0f", (double)pos->pos);
		break;
	default:
		panic("bogus execution position type");
	}
}

//parse execution range *RSTR to *RANGE, returns error string or NULL
const char * range_parse_range(const char *rstr,	//execution range string
	range_range_t *range,				//range return buffer
	mem_t* my_mem)
{
	const char *errstr;

	//make a copy of the execution range
	std::string buf(rstr);

	//find mid-point
	std::size_t findcolon = buf.find(":");
	if(findcolon==std::string::npos)
		return "Badly formed execution range";

	buf[findcolon] = static_cast<char>(NULL);

	//this is where the second position will start
	findcolon++;

	//parse start position
	if((buf[0]!=static_cast<char>(NULL)) && (buf[0]!=':'))
	{
		errstr = range_parse_pos(&buf[0], &range->start, my_mem);
		if(errstr)
			return errstr;
	}
	else
	{
		//default start range
		range->start.ptype = pt_inst;
		range->start.pos = 0;
	}

	//parse end position
	if(buf[findcolon]!=static_cast<char>(NULL))
	{
		if(buf[findcolon] == '+')
		{
			char *endp;

			//get delta value
			errno = 0;
			int delta = strtol(&buf[findcolon+1], &endp, /*parse base*/0);
			if(!errno && !*endp)
			{
				//good conversion
				range->end.ptype = range->start.ptype;
				range->end.pos = range->start.pos + delta;
			}
			else
			{
				//bad conversion
				return "badly formed execution range delta";
			}
		}
		else
		{
			errstr = range_parse_pos(&buf[findcolon], &range->end, my_mem);
			if(errstr)
				return errstr;
		}
	}
	else
	{
		//default end range
		range->end.ptype = range->start.ptype;
		range->end.pos = ULL(0x7fffffffffffffff);
	}

	//no error
	return NULL;
}

//print execution range *RANGE
void range_print_range(range_range_t *range,	//execution range
	FILE *stream)				//output stream
{
	range_print_pos(&range->start, stream);
	fprintf(stream, ":");
	range_print_pos(&range->end, stream);
}

//determine if inputs match execution position, returns relation to position
int range_cmp_pos(range_pos_t *pos,		//execution position
	      counter_t val)			//position value
{
	if(val < pos->pos)
		return -1; //before
	else if(val == pos->pos)
		return 0; //equal
	else //if(pos->pos < val)
		return 1; //after
}

//determine if inputs are in range, returns relation to position
int range_cmp_range(range_range_t *range,	//execution range
	counter_t val)				//position value
{
	if(range->start.ptype != range->end.ptype)
		panic("invalid range");

	if(val < range->start.pos)
		return -1; //before
	else if(range->start.pos <= val && val <= range->end.pos)
		return 0; //inside
	else 		//if(range->end.pos < val)
		return 1; //after
}

//determine if inputs are in range, passes all possible info needed, returns relation to range
int range_cmp_range1(range_range_t *range,	//execution range
	md_addr_t addr,				//address value
	counter_t icount,			//instruction count
	counter_t cycle)			//cycle count
{
	if (range->start.ptype != range->end.ptype)
		panic("invalid range");

	switch(range->start.ptype)
	{
	case pt_addr:
		if(addr < (md_addr_t)range->start.pos)
			return -1; //before
		else if((md_addr_t)range->start.pos <= addr && addr <= (md_addr_t)range->end.pos)
			return 0; //inside
		else //if(range->end.pos < addr)
			return 1; //after
		break;
	case pt_inst:
		if(icount < range->start.pos)
			return -1; //before
		else if(range->start.pos <= icount && icount <= range->end.pos)
			return 0; //inside
		else //if(range->end.pos < icount)
			return 1; //after
		break;
	case pt_cycle:
		if(cycle < range->start.pos)
			return -1; //before
		else if(range->start.pos <= cycle && cycle <= range->end.pos)
			return 0; //inside
		else //if(range->end.pos < cycle)
			return 1; //after
		break;
	default:
		panic("bogus range type");
	}
}
