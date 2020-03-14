//range.h - program execution range definitions and interfaces

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

#ifndef RANGE_H
#define RANGE_H

#include<cstdio>

#include "host.h"
#include "misc.h"
#include "machine.h"

enum range_ptype_t {
	pt_addr = 0,			//address position
	pt_inst,			//instruction count position
	pt_cycle,			//cycle count position
	pt_NUM
};

//an execution position
//	by addr:	@<addr>
//	by inst count:	<icnt>
//	by cycle count:	#<cycle>

class range_pos_t
{
	public:
		range_ptype_t ptype;	//type of position
		counter_t pos;		//position
};

//an execution range
class range_range_t
{
	public:
		range_pos_t start,end;
};

//forward declaration
class mem_t;

//parse execution position *PSTR to *POS, returns error string or NULL
const char * range_parse_pos(char *pstr,		//execution position string
	range_pos_t *pos,				//position return buffer
	mem_t* my_mem);

//print execution position *POS
void range_print_pos(range_pos_t *pos,			//execution position
	FILE *stream);					//output stream

//parse execution range *RSTR to *RANGE, returns error string or NULL
const char * range_parse_range(const char *rstr,	//execution range string
	range_range_t *range,				//range return buffer
	mem_t* my_mem);

//print execution range *RANGE
void range_print_range(range_range_t *range,		//execution range
	FILE *stream);					//output stream

//determine if inputs match execution position, returns relation to position
int range_cmp_pos(range_pos_t *pos,			//execution position
	counter_t val);					//position value

//determine if inputs are in range, returns relation to range
int range_cmp_range(range_range_t *range,		//execution range
	counter_t val);					//position value


//determine if inputs are in range, passes all possible info needed, returns relation to range
int range_cmp_range1(range_range_t *range,		//execution range
	md_addr_t addr,					//address value
	counter_t icount,				//instruction count
	counter_t cycle);				//cycle count

//<range> 	:= {<start_val>}:{<end>}
//<end>   	:= <end_val>
//		| +<delta>

#endif //RANGE_H
