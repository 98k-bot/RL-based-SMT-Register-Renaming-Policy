/* resource.c - resource manager routines */

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
#include<cassert>

#include "host.h"
#include "misc.h"
#include "resource.h"

res_pool::res_pool(const char * name, res_desc *pool, int ndesc)
: name(name), num_resources(0), resources(NULL), nents(MAX_RES_CLASSES,0),
table(MAX_RES_CLASSES,std::vector<res_template *>(MAX_INSTS_PER_CLASS,static_cast<res_template *>(NULL)))
{
	//count total instances
	for(int i=0; i<ndesc; i++)
	{
		if(pool[i].quantity > MAX_INSTS_PER_CLASS)
		{
			fatal("too many functional units, increase MAX_INSTS_PER_CLASS");
		}
		num_resources += pool[i].quantity;
	}

	//allocate the instance table
	resources = new res_desc[num_resources];

	//fill in the instance table
	int index = 0;
	for(int i=0; i<ndesc; i++)
	{
		for(int j=0; j<pool[i].quantity; j++)
		{
			resources[index] = pool[i];
			resources[index].quantity = 1;
			resources[index].busy = FALSE;
			for(int k=0; k<MAX_RES_CLASSES && resources[index].x[k].rclass; k++)
				resources[index].x[k].master = &resources[index];
			index++;
		}
	}
	assert(index == num_resources);

	//fill in the resource table map - slow to build, but fast to access
	for(int i=0; i<num_resources; i++)
	{
		res_template *plate;
		for(int j=0; j<MAX_RES_CLASSES; j++)
		{
			plate = &resources[i].x[j];
			if(plate->rclass)
			{
				assert(plate->rclass < MAX_RES_CLASSES);
				table[plate->rclass][nents[plate->rclass]++] = plate;
			}
			else
				/* all done with this instance */
				break;
		}
	}
}

res_pool::~res_pool()
{
	delete [] resources;
}

//get a free resource from resource pool POOL that can execute a operation of class CLASS,
//returns a pointer to the resource template, returns NULL, if there are currently no free
//resources available, follow the MASTER link to the master resource descriptor;
//NOTE: caller is responsible for reseting the busy flag in the beginning of the cycle
//when the resource can once again accept a new operation
res_template * res_get(res_pool *pool, int rclass)
{
	//must be a valid class
	assert(rclass < MAX_RES_CLASSES);

	//must be at least one resource in this class
	assert(pool->table[rclass][0]);

	for(int i=0; i<MAX_INSTS_PER_CLASS; i++)
	{
		if(pool->table[rclass][i])
		{
			if(!pool->table[rclass][i]->master->busy)
			{
				return pool->table[rclass][i];
			}
		}
		else
		{
			break;
		}
	}
	//none found
	return NULL;
}

//dump the resource pool POOL to stream STREAM
void res_dump(res_pool *pool, FILE *stream)
{
	int j;
	if (!stream)
	{
		stream = stderr;
	}

	fprintf(stream, "Resource pool: %s:\n", pool->name);
	fprintf(stream, "\tcontains %d resource instances\n", pool->num_resources);
	for(int i=0; i<MAX_RES_CLASSES; i++)
	{
		fprintf(stream, "\tclass: %d: %d matching instances\n", i, pool->nents[i]);
		fprintf(stream, "\tmatching: ");
		for(j=0; j<MAX_INSTS_PER_CLASS; j++)
		{
			if(!pool->table[i][j])
			{
				break;
			}
			fprintf(stream, "\t%s (busy for %d cycles) ", pool->table[i][j]->master->name,pool->table[i][j]->master->busy);
		}
		assert(j == pool->nents[i]);
		fprintf(stream, "\n");
	}
}
