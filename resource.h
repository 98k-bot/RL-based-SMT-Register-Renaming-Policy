/* resource.h - resource manager interfaces */

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

#ifndef RESOURCE_H
#define RESOURCE_H

#include<cstdio>
#include<vector>

//maximum number of resource classes supported
#define MAX_RES_CLASSES		16

//maximum number of resource instances for a class supported
#define MAX_INSTS_PER_CLASS	8

class res_template {
	public:
		int rclass;			//matching resource class: insts with this resource class will be
						//able to execute on this unit
		int oplat;			//operation latency: cycles until result is ready for use
		int issuelat;			//issue latency: number of cycles  before another operation can be
						//issued on this resource
		class res_desc *master;		//master resource record (forward declaration required)
};

//resource descriptor
class res_desc
{
	public:
		const char *name;		//name of functional unit
		int quantity;			//total instances of this unit
		int busy;			//non-zero if this unit is busy
		res_template x[MAX_RES_CLASSES];
};

//resource pool: one entry per resource instance
class res_pool
{
	public:
		res_pool(const char *name, res_desc *pool, int ndesc);
		~res_pool();

		const char *name;			//pool name
		int num_resources;			//total number of res instances
		res_desc *resources;			//resource instances
		//res class -> res template mapping table, lists are NULL terminated
		std::vector<int> nents;
		std::vector<std::vector<res_template *> > table;
};

//get a free resource from resource pool POOL that can execute a operation of class CLASS,
//returns a pointer to the resource template, returns NULL, if there are currently no free
//resources available, follow the MASTER link to the master resource descriptor;
//NOTE: caller is responsible for reseting the busy flag in the beginning of the cycle
//when the resource can once again accept a new operation
res_template *res_get(res_pool *pool, int rclass);

//dump the resource pool POOL to stream STREAM
void res_dump(res_pool *pool, FILE *stream);

#endif //RESOURCE_H
