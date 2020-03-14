/*
 * sim-outorder:
 *
 * Copyright (c) 2009 Jason Loew <jloew@cs.binghamton.edu>
 * Copyright (c) 2005 Joseph J. Sharkey <jsharke@cs.binghamton.edu>
 * Copyright (c) 1994-2003 Todd M. Austin, Doug Berger, SimpleScalar, LLC.
 *
 * This program is licensed under the GNU General Public License version 2.
 *
 * NOTE: See the README file for a full revision history.
 *
 * Additional documentation is provided in the included presentation.pdf.
 *
 * Written by:
 *
 * - Jason Loew <jloew@cs.binghamton.edu>, January 2009
 *
 *   Added support for CMP and SMT over CMP.
 *   64-bit support as part of the stats and options handler - long executions may not
 *   run correctly on a native 32-bit machine.
 *   Large portions of code re-written for C++ and to support compiler optimizations
 *   See the wiki at http://www.cs.binghamton.edu/~msim/wiki for details.
 *
 * - Joseph J. Sharkey <jsharke@cs.binghamton.edu>, September 2005
 *
 *   Restructured the out-of-order core to support cycle accurate modeling of
 *   independent structures for the re-order buffer, issue queue, register file
 *   and register renaming.
 *   Added support for Simultaneous Multithreading (SMT). See README for details.
 *   Only supports Alpha ISA now, to simplify the code (ie no support for longs and doubles)
 *
 * - Todd M. Austin, Doug Berger, SimpleScalar, LLC.
 *
 *   Original SimpleScalar 3.0d implementation, without cycle accurate modeling
 *   for some processor structures
 *
 * sim-outorder.c:
 *
 * SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved.
 *
 * Copyright (C) 2005 Joseph J Sharkey <jsharke@cs.binghamton.edu>
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

#include "sim-outorder.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdlib.h>
using namespace std;
/*
 * This file implements a very detailed out-of-order issue superscalar
 * processor with a two-level memory system and speculative execution support.
 * This simulator is a performance simulator, tracking the latency of all
 * pipeline operations.
 */

/**************** Simulation State *******************/
// These variables do not need to be in a core or context object

	//maximum number of insts to execute (this could be done per core/context but isn't)
	long long max_insts;

	//Maximum number of cycles to run, this can be done in tandem with max_insts
	long long max_cycles;

	//cycle counter
	tick_t sim_cycle = 0;

	//total non-speculative bogus addresses seen (debug var)
	counter_t sim_invalid_addrs;

	//L3 cache (data and inst), this is shared among all cores
	cache_t * cache_il3=NULL, *cache_dl3=NULL;

	//L3 cache config, i.e., {<config>|none} */
	char *cache_dl3_opt;
	//L3 cache config, i.e., {<config>|dl1|dl2|dl3|none} */
	char *cache_il3_opt;

	//L3 cache hit latency in cycles
	int cache_dl3_lat, cache_il3_lat;

	//instruction sequence counter, used to assign unique id's to insts
	unsigned long long inst_seq = 0;

	//options for Wattch
	int data_width = 64;

	long long INF = __LONG_LONG_MAX__;

	//Print static power model results?
	int print_power_stats = FALSE;

	//Number of executed instructions
	counter_t sim_num_insn = 0;

	//Loader object, we give it access to sim_num_insn (not sure if we actually need to do this)
	loader_t loader(sim_num_insn);

	//process id object
	pid_handler_t pid_handler;

	//Main Memory pointer and configuration string
	dram_t * main_mem;
	char * main_mem_config;
/****************************************************/

/**************** Core/Context Data *******************/
	//These variables are here in order to be read via options, but are used from core or context objects only

/******************************************************/

//Filename to use when creating an eio file
char * eio_name = NULL;

//number of insts skipped before timing starts
long long fastfwd_count;

//pipeline trace range and output filename
int ptrace_nelt = 0;
char *ptrace_opts[2];

//text-based stat profiles
#define MAX_PCSTAT_VARS 8
int pcstat_nelt = 0;
char *pcstat_vars[MAX_PCSTAT_VARS];

//convert 64-bit inst text addresses to 32-bit inst equivalents
#define IACOMPRESS(A)		(A)
#define ISCOMPRESS(SZ)		(SZ)

//functional unit resource configuration

//resource pool indices, NOTE: update these if you change FU_CONFIG
#define FU_IALU_INDEX			0
#define FU_IMULT_INDEX			1
#define FU_MEMPORT_INDEX		2
#define FU_FPALU_INDEX			3
#define FU_FPMULT_INDEX			4

//resource pool definition, NOTE: update FU_*_INDEX defs if you change this
res_desc fu_config[] =
{
	{
		"integer-ALU",4,0,
		{
			{ IntALU, 1, 1 }
		}
	},
	{
		"integer-MULT/DIV",1,0,
		{
			{ IntMULT, 3, 1 },
			{ IntDIV, 20, 19 }
		}
	},
	{
		"memory-port",2,0,
		{
			{ RdPort, 1, 1 },
			{ WrPort, 1, 1 }
		}
	},
	{
		"FP-adder",4,0,
		{
			{ FloatADD, 2, 1 },
			{ FloatCMP, 2, 1 },
			{ FloatCVT, 2, 1 }
		}
	},
	{
		"FP-MULT/DIV",1,0,
		{
			{ FloatMULT, 4, 1 },
			{ FloatDIV, 12, 12 },
			{ FloatSQRT, 24, 24 }
		}
	},
};

//simulator state variables

//text-based stat profiles
stat_stat_t *pcstat_stats[MAX_PCSTAT_VARS];
counter_t pcstat_lastvals[MAX_PCSTAT_VARS];
stat_stat_t *pcstat_sdists[MAX_PCSTAT_VARS];

//wedge all stat values into a counter_t
#define STATVAL(STAT)							\
	((STAT)->sc == sc_int						\
	? (counter_t)*((STAT)->variant.for_int.var)			\
	: ((STAT)->sc == sc_uint					\
	? (counter_t)*((STAT)->variant.for_uint.var)			\
	: ((STAT)->sc == sc_counter					\
	? *((STAT)->variant.for_counter.var)				\
	: (panic("bad stat class"), 0))))

//cache miss handlers

//Where are the next level access counters here? Shouldn't they be same level? Except for L3 of course...
//l1 data cache l1 block miss handler function
unsigned long long			//latency of block access
dl1_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)			//context_id for the access
{
	if(cores[contexts[context_id].core_id].cache_dl2)
	{
		//access next level of data cache hierarchy
		unsigned long long lat = cores[contexts[context_id].core_id].cache_dl2->cache_access(cmd, baddr, context_id, NULL, bsize, now, NULL, NULL);

		//Wattch -- Dcache2 access
		cores[contexts[context_id].core_id].power.dcache2_access++;

		if(cmd == Read)
			return lat;
		else
		{
			//FIXME: unlimited write buffers
			return 0;
		}
	}
	else
	{
		//access main memory
		if(cmd == Read)
			return cores[contexts[context_id].core_id].main_mem->mem_access_latency(baddr, bsize, now, context_id);
		else
		{
			//FIXME: unlimited write buffers
			return 0;
		}
	}
}

//l2 data cache block miss handler function
unsigned long long			//latency of block access
dl2_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)			//context_id for the access
{
	if(cache_dl3)
	{
		//access next level of data cache hierarchy
		unsigned long long lat = cache_dl3->cache_access(cmd, baddr, context_id, NULL, bsize, now, NULL, NULL);

		//Wattch -- Dcache2 access
		cores[contexts[context_id].core_id].power.dcache3_access++;

		if (cmd == Read)
			return lat;
		else
		{
			//FIXME: unlimited write buffers
			return 0;
		}
	}
	else
	{
		//access main memory
		if(cmd == Read)
			return cores[contexts[context_id].core_id].main_mem->mem_access_latency(baddr, bsize, now, context_id);
		else
		{
			//FIXME: unlimited write buffers
			return 0;
		}
	}
}

//l3 data cache block miss handler function
unsigned long long			//latency of block access
dl3_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)			//context id
{
	//Wattch -- main memory access -- Wattch-FIXME (offchip)

	//this is a miss to the lowest level, so access main memory
	if(cmd == Read)
		return cores[contexts[context_id].core_id].main_mem->mem_access_latency(baddr, bsize, now, context_id);
	else
	{
		//FIXME: unlimited write buffers
		return 0;
	}
}

//l1 inst cache l1 block miss handler function
unsigned long long			//latency of block access
il1_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)			//context_id of the access
{
	if(cores[contexts[context_id].core_id].cache_il2)
	{
		//access next level of inst cache hierarchy
		unsigned long long lat = cores[contexts[context_id].core_id].cache_il2->cache_access(cmd, baddr, context_id, NULL, bsize, now, NULL, NULL);

		//Wattch -- Dcache2 access
		cores[contexts[context_id].core_id].power.dcache2_access++;

		if(cmd == Read)
			return lat;
		else
			panic("writes to instruction memory not supported");
	}
	else
	{
		//access main memory
		if(cmd == Read)
			return cores[contexts[context_id].core_id].main_mem->mem_access_latency(baddr, bsize, now, context_id);
		else
			panic("writes to instruction memory not supported");
	}
}

//l2 inst cache block miss handler function
unsigned long long			//latency of block access
il2_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)			//context_id of the access
{
	if(cache_il3)
	{
		//access next level of inst cache hierarchy
		unsigned long long lat = cache_il3->cache_access(cmd, baddr, context_id, NULL, bsize, now, NULL, NULL);

		//Wattch -- Dcache2 access
		cores[contexts[context_id].core_id].power.dcache3_access++;

		if(cmd == Read)
			return lat;
		else
			panic("writes to instruction memory not supported");
	}
	else
	{
		//access main memory
		if(cmd == Read)
			return cores[contexts[context_id].core_id].main_mem->mem_access_latency(baddr, bsize, now, context_id);
		else
			panic("writes to instruction memory not supported");
	}
}

//l3 inst cache block miss handler function
unsigned long long			//latency of block access
il3_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)
{
	//Wattch -- main memory access -- Wattch-FIXME (offchip)

	//this is a miss to the lowest level, so access main memory
	if(cmd == Read)
		return cores[contexts[context_id].core_id].main_mem->mem_access_latency(baddr, bsize, now, context_id);
	else
		panic("writes to instruction memory not supported");
}

//TLB miss handlers

//inst cache block miss handler function
unsigned long long			//latency of block access
itlb_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)
{
	md_addr_t *phy_page_ptr = (md_addr_t *)blk->user_data;

	//no real memory access, however, should have user data space attached
	assert(phy_page_ptr);

	//fake translation, for now...
	*phy_page_ptr = 0;

	//return tlb miss latency
	return cores[contexts[context_id].core_id].tlb_miss_lat;
}

//data cache block miss handler function
unsigned long long			//latency of block access
dtlb_access_fn(mem_cmd cmd,		//access cmd, Read or Write
	md_addr_t baddr,		//block address to access
	unsigned int bsize,		//size of block to access
	cache_blk_t *blk,		//ptr to block in upper level
	tick_t now,			//time of access
	int context_id)
{
	md_addr_t *phy_page_ptr = (md_addr_t *)blk->user_data;

	//no real memory access, however, should have user data space attached
	assert(phy_page_ptr);

	//fake translation, for now...
	*phy_page_ptr = 0;

	//return tlb miss latency
	return cores[contexts[context_id].core_id].tlb_miss_lat;
}

//register simulator-specific options
void sim_reg_options(opt_odb_t *odb)
{
	opt_reg_header(odb, 
		"sim-outorder: This simulator implements a very detailed out-of-order issue superscalar processor with a two-level memory system and speculative\n"
		"execution support.  This simulator is a performance simulator, tracking the latency of all pipeline operations.\n");

	//instruction limit
	opt_reg_long_long(odb, "-max:inst", "", "maximum number of inst's to execute",
		&max_insts, /* default */1000000,
		/* print */TRUE, /* format */NULL);

	//cycle limit
	opt_reg_long_long(odb, "-max:cycles", "", "maximum number of cycles to execute",
		&max_cycles, /* default */-1,
		/* print */TRUE, /* format */NULL);

	//trace options
	opt_reg_long_long(odb, "-fastfwd", "", "number of insts skipped before timing starts (1 to use value in .arg file)",
		&fastfwd_count, /* default */1000000,
		/* print */TRUE, /* format */NULL);

	opt_reg_string_list(odb, "-ptrace", "",
		"generate pipetrace, i.e., <fname|stdout|stderr> <range>",
		ptrace_opts, /* arr_sz */2, &ptrace_nelt, /* default */NULL,
		/* !print */FALSE, /* format */NULL, /* !accrue */FALSE);

	opt_reg_note(odb,
		"  Pipetrace range arguments are formatted as follows:\n"
		"\n"
		"    {{@|#}<start>}:{{@|#|+}<end>}\n"
		"\n"
		"  Both ends of the range are optional, if neither are specified, the entire execution is traced. Ranges that start with a `@' designate an address\n"
		"  range to be traced, those that start with an `#' designate a cycle count range. All other range values represent an instruction count range.  The\n"
		"  second argument, if specified with a `+', indicates a value relative to the first argument, e.g., 1000:+100 == 1000:1100. Program symbols may\n"
		"  be used in all contexts.\n"
		"\n"
		"    Examples:   -ptrace FOO.trc #0:#1000\n"
		"                -ptrace BAR.trc @2000:\n"
		"                -ptrace BLAH.trc :1500\n"
		"                -ptrace UXXE.trc :\n"
		"                -ptrace FOOBAR.trc @main:+278\n"
		);

	opt_reg_string_list(odb, "-pcstat","",
		"profile stat(s) against text addr's (mult uses ok)",
		pcstat_vars, MAX_PCSTAT_VARS, &pcstat_nelt, NULL,
		/* !print */FALSE, /* format */NULL, /* accrue */TRUE);

	opt_reg_flag(odb, "-power:print_stats","",
		"print power statistics collected by wattch?",
		&print_power_stats, /* default */FALSE,
		/* print */TRUE, /* format */NULL);

	//CMP options
	opt_reg_uint(odb, "-num_cores","",
		"Number of processor cores",
		&num_cores, /* default */1,
		/* print */TRUE, /* format */NULL);

	opt_reg_int(odb, "-max_contexts_per_core","",
		"Number of contexts allowed per core (-1 == limit is number of contexts, 0 is invalid)",
		&max_contexts_per_core, /* default */-1,
		/* print */TRUE, /* format */NULL);


	cores.resize(cores_at_init_time,core_t());
	for(unsigned int i=0;i<cores_at_init_time;i++)
	{
		std::stringstream in;
		in << i;
		std::string offset;
		in >> offset;
		offset.insert(offset.begin(),'_');

		if(cores_at_init_time==1)
		{
			offset = "";
		}

		cores[i].fu_CMP.push_back(fu_config[0]);
		cores[i].fu_CMP.push_back(fu_config[1]);
		cores[i].fu_CMP.push_back(fu_config[2]);
		cores[i].fu_CMP.push_back(fu_config[3]);
		cores[i].fu_CMP.push_back(fu_config[4]);

		//ifetch options
		opt_reg_int(odb, "-fetch:speed",offset,
			"speed of front-end of machine relative to execution core",
			&cores[i].fetch_speed, /* default */1,
			/* print */TRUE, /* format */NULL);

		//decode options
		opt_reg_uint(odb, "-decode:width",offset,
			"instruction decode B/W (insts/cycle)",
			&cores[i].decode_width, /* default */8,
			/* print */TRUE, /* format */NULL);

		//issue options
		opt_reg_uint(odb, "-issue:width",offset,
			"instruction issue B/W (insts/cycle)",
			&cores[i].issue_width, /* default */8,
			/* print */TRUE, /* format */NULL);

		opt_reg_flag(odb, "-issue:inorder",offset,
			 "run pipeline with in-order issue",
			&cores[i].inorder_issue, /* default */FALSE,
			/* print */TRUE, /* format */NULL);

		opt_reg_flag(odb, "-issue:wrongpath",offset,
			"issue instructions down wrong execution paths",
			&cores[i].include_spec, /* default */TRUE,
			/* print */TRUE, /* format */NULL);

		//commit options
		opt_reg_uint(odb, "-commit:width",offset,
			"instruction commit B/W (insts/cycle)",
			&cores[i].commit_width, /* default */8,
			/* print */TRUE, /* format */NULL);

		opt_reg_int(odb, "-iq:issue_exec_delay",offset,
			"minimum cycles between issue and execution",
			&cores[i].ISSUE_EXEC_DELAY, /* default */1,
			/* print */TRUE, /* format */NULL);

		opt_reg_int(odb, "-fetch_rename_delay",offset,
			"number of cycles between fetch and rename stages",
			&cores[i].FETCH_RENAME_DELAY, /* default */4,
			/* print */TRUE, /* format */NULL);

		opt_reg_int(odb, "-rename_dispatch_delay",offset,
			"number cycles between rename and dispatch stages",
			&cores[i].RENAME_DISPATCH_DELAY, /* default */1,
			/* print */TRUE, /* format */NULL);

		//memory scheduler options
		opt_reg_uint(odb, "-lsq:size",offset,
			"load/store queue (LSQ) size",
			&cores[i].LSQ_size, /* default */48,
			/* print */TRUE, /* format */NULL);

		//core structure options
		opt_reg_uint(odb, "-rob:size",offset,
			"reorder buffer (ROB) size",
			&cores[i].ROB_size, /* default */128,
			/* print */TRUE, /* format */NULL);

		opt_reg_string(odb, "-fetch:policy",offset,
			"fetch policy, icount, round_robin, dcra",
			&cores[i].fetch_policy, /* default */"icount",
			/* print */TRUE, /* format */NULL);

		opt_reg_string(odb, "-recovery:model",offset,
			"Alpha squash recovery or perfect predition: |squash|perfect|",
			&cores[i].recovery_model, /* default */"squash",
			/* print */TRUE, /* format */NULL);

		opt_reg_uint(odb, "-iq:size",offset,
			"issue queue (IQ) size",
			&cores[i].iq_size, /* default */32,
			/* print */TRUE, /* format */NULL);

		opt_reg_uint(odb, "-rf:size",offset,
			"register file (RF) size for each the INT and FP physical register file)",
			&cores[i].rf_size, /* default */160,
			/* print */TRUE, /* format */NULL);

		//resource configuration
		opt_reg_uint(odb, "-res:ialu",offset,
			"total number of integer ALU's available",
			&cores[i].res_ialu, /* default */cores[i].fu_CMP[FU_IALU_INDEX].quantity,
			/* print */TRUE, /* format */NULL);

		opt_reg_uint(odb, "-res:imult",offset,
			"total number of integer multiplier/dividers available",
			&cores[i].res_imult, /* default */cores[i].fu_CMP[FU_IMULT_INDEX].quantity,
			/* print */TRUE, /* format */NULL);

		opt_reg_uint(odb, "-res:memport",offset,
			"total number of memory system ports available (to CPU)",
			&cores[i].res_memport, /* default */cores[i].fu_CMP[FU_MEMPORT_INDEX].quantity,
			/* print */TRUE, /* format */NULL);

		opt_reg_uint(odb, "-res:fpalu",offset,
			"total number of floating point ALU's available",
			&cores[i].res_fpalu, /* default */cores[i].fu_CMP[FU_FPALU_INDEX].quantity,
			/* print */TRUE, /* format */NULL);

		opt_reg_uint(odb, "-res:fpmult",offset,
			"total number of floating point multiplier/dividers available",
			&cores[i].res_fpmult, /* default */cores[i].fu_CMP[FU_FPMULT_INDEX].quantity,
			/* print */TRUE, /* format */NULL);

		opt_reg_uint(odb, "-write_buf:size",offset,
			"write buffer size (for stores to L1, not for writeback)",
			&cores[i].write_buf_size, /* default */16,
			/* print */TRUE, /* format */NULL);

		//cache options
		if(i==0)	//Notes should only appear once
		{
			opt_reg_note(odb,
				"  The cache config parameter <config> has the following format:\n"
				"\n"
				"    <name>:<nsets>:<bsize>:<assoc>:<repl>\n"
				"\n"
				"    <name>   - name of the cache being defined\n"
				"    <nsets>  - number of sets in the cache\n"
				"    <bsize>  - block size of the cache\n"
				"    <assoc>  - associativity of the cache\n"
				"    <repl>   - block replacement strategy, 'l'-LRU, 'f'-FIFO, 'r'-random\n"
				"\n"
				"    Examples:   -cache:dl1 dl1:4096:32:1:l\n"
				"                -dtlb dtlb:128:4096:32:r\n"
				);

			opt_reg_note(odb,
				"  Cache levels can be unified by pointing a level of the instruction cache hierarchy at the data cache hiearchy using the \"dl1\" and \"dl2\" cache\n"
				"  configuration arguments.  Most sensible combinations are supported, e.g.,\n"
				"\n"
				"    A unified l2 cache (il2 is pointed at dl2):\n"
				"      -cache:il1 il1:128:64:1:l -cache:il2 dl2\n"
				"      -cache:dl1 dl1:256:32:1:l -cache:dl2 ul2:1024:64:2:l\n"
				"\n"
				"    Or, a fully unified cache hierarchy (il1 pointed at dl1):\n"
				"      -cache:il1 dl1\n"
				"      -cache:dl1 ul1:256:32:1:l -cache:dl2 ul2:1024:64:2:l\n"
				);
		}

		opt_reg_string(odb, "-cache:dl1",offset,
			"l1 data cache config, i.e., {<config>|none}",
			&cores[i].cache_dl1_opt, "dl1:256:64:4:l",
			/* print */TRUE, NULL);

		opt_reg_int(odb, "-cache:dl1lat",offset,
			"l1 data cache hit latency (in cycles)",
			&cores[i].cache_dl1_lat, /* default */1,
			/* print */TRUE, /* format */NULL);

		opt_reg_string(odb, "-cache:dl2",offset,
			 "l2 data cache config, i.e., {<config>|none}",
			 &cores[i].cache_dl2_opt, "ul2:512:64:16:l",
			 /* print */TRUE, NULL);

		opt_reg_int(odb, "-cache:dl2lat",offset,
			"l2 data cache hit latency (in cycles)",
			&cores[i].cache_dl2_lat, /* default */10,
			/* print */TRUE, /* format */NULL);

		opt_reg_string(odb, "-cache:il1",offset,
			"l1 inst cache config, i.e., {<config>|dl1|dl2|none}",
			&cores[i].cache_il1_opt, "il1:512:64:2:l",
			/* print */TRUE, NULL);

		opt_reg_int(odb, "-cache:il1lat",offset,
			"l1 instruction cache hit latency (in cycles)",
			&cores[i].cache_il1_lat, /* default */1,
			/* print */TRUE, /* format */NULL);

		opt_reg_string(odb, "-cache:il2",offset,
			"l2 instruction cache config, i.e., {<config>|dl2|none}",
			&cores[i].cache_il2_opt, "dl2",
			/* print */TRUE, NULL);

		opt_reg_int(odb, "-cache:il2lat",offset,
			"l2 instruction cache hit latency (in cycles)",
			&cores[i].cache_il2_lat, /* default */10,
			/* print */TRUE, /* format */NULL);

		//TLB options
		opt_reg_string(odb, "-tlb:itlb",offset,
			"instruction TLB config, i.e., {<config>|none}",
			&cores[i].itlb_opt, "itlb:16:4096:4:l", /* print */TRUE, NULL);

		opt_reg_string(odb, "-tlb:dtlb",offset,
			"data TLB config, i.e., {<config>|none}",
			&cores[i].dtlb_opt, "dtlb:32:4096:4:l", /* print */TRUE, NULL);

		opt_reg_int(odb, "-tlb:lat",offset,
			"inst/data TLB miss latency (in cycles)",
			&cores[i].tlb_miss_lat, /* default */30,
			/* print */TRUE, /* format */NULL);

		//branch predictor options
		if(i==0)	//Notes should only appear once
		{
			opt_reg_note(odb,
				"  Branch predictor configuration examples for 2-level predictor:\n"
				"    Configurations:   N, M, W, X\n"
				"      N   # entries in first level (# of shift register(s))\n"
				"      W   width of shift register(s)\n"
				"      M   # entries in 2nd level (# of counters, or other FSM)\n"
				"      X   (yes-1/no-0) xor history and address for 2nd level index\n"
				"    Sample predictors:\n"
				"      GAg     : 1, W, 2^W, 0\n"
				"      GAp     : 1, W, M (M > 2^W), 0\n"
				"      PAg     : N, W, 2^W, 0\n"
				"      PAp     : N, W, M (M == 2^(N+W)), 0\n"
				"      gshare  : 1, W, 2^W, 1\n"
				"  Predictor `comb' combines a bimodal and a 2-level predictor.\n"
	        		);
		}

		opt_reg_string(odb, "-bpred",offset,
			"branch predictor type {nottaken|taken|perfect|bimod|2lev|comb}",
			&cores[i].pred_type, /* default */"bimod",
			/* print */TRUE, /* format */NULL);

		opt_reg_int(odb, "-bpred:ras",offset,
			"return address stack size (0 for no return stack)",
			&cores[i].ras_size, /* default */16,
			/* print */TRUE, /* format */NULL);

		opt_reg_string(odb, "-bpred:spec_update",offset,
			"speculative predictors update in {ID|WB} (default non-spec)",
			&cores[i].bpred_spec_opt, /* default */NULL,
			/* print */TRUE, /* format */NULL);

		opt_reg_int_list(odb, "-bpred:bimod",offset,
			"bimodal predictor config (<table size>)",
			cores[i].bimod_config, cores[i].bimod_nelt, &cores[i].bimod_nelt,
			/* default */cores[i].bimod_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);

		opt_reg_int_list(odb, "-bpred:2lev",offset,
			"2-level predictor config "
			"(<l1size> <l2size> <hist_size> <xor>)",
			cores[i].twolev_config, cores[i].twolev_nelt, &cores[i].twolev_nelt,
			/* default */cores[i].twolev_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);

		opt_reg_int_list(odb, "-bpred:comb",offset,
			"combining predictor config (<meta_table_size>)",
			cores[i].comb_config, cores[i].comb_nelt, &cores[i].comb_nelt,
			/* default */cores[i].comb_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);

		opt_reg_int_list(odb, "-bpred:btb",offset,
			"BTB config (<num_sets> <associativity>)",
			cores[i].btb_config, cores[i].btb_nelt, &cores[i].btb_nelt,
			/* default */cores[i].btb_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);

		opt_reg_uint(odb, "-bpred:penalty",offset,
			"penalty (in cycles) for a branch misprediction",
			&cores[i].bpred_misprediction_penalty, /* default */6,
			/* print */TRUE, /* format */NULL);

		/****************** LOAD-LATENCY PREDICTOR **************************/
		opt_reg_string(odb, "-cpred",offset,
			"cache load-latency predictor type {nottaken|taken|perfect|bimod|2lev|comb}",
			&cores[i].cpred_type, /* default */"bimod",
			/* print */TRUE, /* format */NULL);

		opt_reg_int(odb, "-cpred:ras",offset,
			"return address stack size (0 for no return stack)",
			&cores[i].cras_size, /* default */0,
			/* print */TRUE, /* format */NULL);

		opt_reg_int_list(odb, "-cpred:bimod",offset,
			"cache load-latency bimodal predictor config (<table size>)",
			cores[i].cbimod_config, cores[i].cbimod_nelt, &cores[i].cbimod_nelt,
			/* default */cores[i].cbimod_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);

		opt_reg_int_list(odb, "-cpred:2lev",offset,
			"cache load-latency 2-level predictor config "
			"(<l1size> <l2size> <hist_size> <xor>)",
			cores[i].ctwolev_config, cores[i].ctwolev_nelt, &cores[i].ctwolev_nelt,
			/* default */cores[i].ctwolev_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);

		opt_reg_int_list(odb, "-cpred:comb",offset,
			"cache load-latency combining predictor config (<meta_table_size>)",
			cores[i].ccomb_config, cores[i].ccomb_nelt, &cores[i].ccomb_nelt,
			/* default */cores[i].ccomb_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);

		opt_reg_int_list(odb, "-cpred:btb",offset,
			"cache load-latency BTB config (<num_sets> <associativity>)",
			cores[i].cbtb_config, cores[i].cbtb_nelt, &cores[i].cbtb_nelt,
			/* default */cores[i].cbtb_config,
			/* print */TRUE, /* format */NULL, /* !accrue */FALSE);
	}

	//mem options
	opt_reg_note(odb,
		"  The main memory configuration parameter has the following format:\n"
		"\n"
		"    <type>:<width>:<config>\n"
		"\n"
		"    <type>   - Type of main memory modeling - see dram.h\n"
		"    <width>  - Width of the memory bus in bytes\n"
		"    <config> - Configuration string - see dram.h\n"
		"\n"
		"    Examples:   -mem:config chunk:4:300:2\n"
		"                -mem:config basic:4:6:12:90:90:90:8:2048\n"
		);

	opt_reg_string(odb, "-mem:config","",
		 "Main memory configuration",
		 &main_mem_config, "chunk:4:300:2",
		 /* print */TRUE, NULL);

	//Shared L3 Cache
	opt_reg_string(odb, "-cache:dl3","",
		"l3 data cache config, i.e., {<config>|none}",
//		&cache_dl3_opt, "ul3:1024:256:8:l",
		&cache_dl3_opt, "none",
		/* print */TRUE, NULL);

	opt_reg_int(odb, "-cache:dl3lat","",
		"l3 data cache hit latency (in cycles)",
		&cache_dl3_lat, /* default */30,
		/* print */TRUE, /* format */NULL);

	opt_reg_string(odb, "-cache:il3","",
		"l3 instruction cache config, i.e., {<config>|dl3|none}",
//		&cache_il3_opt, "dl3",
		&cache_il3_opt, "none",
		/* print */TRUE, NULL);

	opt_reg_int(odb, "-cache:il3lat","",
		"l3 instruction cache hit latency (in cycles)",
		&cache_il3_lat, /* default */30,
		/* print */TRUE, /* format */NULL);

	opt_reg_string(odb, "-makeeio","",
		"After fast-forwarding, make an eio file called: (\"none\"==no eio file)",
		&eio_name, "none",
		/* print */TRUE, NULL);
}

//check simulator-specific option values
void sim_check_options()
{
	if(num_cores<1)
		fatal("Less than 1 core specified! Nothing to do!");

	if(max_contexts_per_core==0)
		fatal("Cores can't have contexts! Nothing to do!");

	if(max_contexts_per_core==-1)
		max_contexts_per_core = contexts_at_init_time;

	assert(max_contexts_per_core>0);

	if(cores_at_init_time != num_cores)
		fatal("Num_cores detected from command line doesn't match num_cores from option flag");

	if(fastfwd_count < 0 || fastfwd_count == 9223372036854775807LL)
	{
		fprintf(stderr,"bad fast forward count: %lld\n", fastfwd_count);
		fprintf(stderr,"Must be non-negative (1 uses default from .arg file) and less than 9223372036854775806\n");
		assert(false);
	}

	if(max_insts < 0 || max_insts == 9223372036854775807LL)
	{
		fprintf(stderr,"bad max_inst: %lld\n", max_insts);
		fprintf(stderr,"Must be non-negative and less than 9223372036854775806\n");
		assert(false);
	}

	if((std::string(eio_name)!="none") && (max_insts != 0))
	{
		fprintf(stderr,"Checkpoints should not be made from a fully simulated instruction stream\n");
		fprintf(stderr,"Use -max:insts 0 otherwise the checkpoint could possibly be generated from a speculative path\n");
		fprintf(stderr,"Checkpoints will be made after fast-forwarding and before full simulation - it is safe to remove this assertion if this is understood\n");
		assert(max_insts==0);
	}

	for(unsigned int i=0;i<num_cores;i++)
	{
		if(cores[i].fetch_speed < 1)
		{
			printf("Core %d front-end speed must be greater than 0\n",i);
			assert(cores[i].fetch_speed>0);
		}

		if(cores[i].decode_width < 1 || (cores[i].decode_width & (cores[i].decode_width-1)) != 0)
		{
			printf("Core %d decode width must be positive non-zero and a power of two",i);
			assert(0);
		}

		if(cores[i].issue_width < 1 || (cores[i].issue_width & (cores[i].issue_width-1)) != 0)
		{
			printf("Core %d issue width must be positive non-zero and a power of two",i);
			assert(0);
		}

		if(cores[i].commit_width < 1)
		{
			printf("Core %d commit width must be positive non-zero",i);
			assert(0);
		}

		if(cores[i].ROB_size < 2)
		{
			printf("Core %d ROB size must be a positive number > 1",i);
			assert(0);
		}

		if(cores[i].LSQ_size < 2)
		{
			printf("Core %d LSQ size must be a positive number > 1",i);
			assert(0);
		}

		if(cores[i].iq_size < 2)
		{
			printf("Core %d IQ size must be a positive number > 1",i);
			assert(0);
		}

		//Each thread requires 32 integer and 32 floating point registers for the architectural state
		if(cores[i].rf_size < static_cast<unsigned int>(32 * (max_contexts_per_core + 1)))
		{
			printf("Core %d needs at least 32 non-architectural registers per thread. ",i);
			printf("Only has %d registers\n",cores[i].rf_size-(32*max_contexts_per_core));
			printf("Allowing %d threads per core, may need to lower this (-max_contexts_per_core)\n",max_contexts_per_core);
			assert(0);
		}

		if((cores[i].res_ialu < 1)||(cores[i].res_ialu > MAX_INSTS_PER_CLASS))
		{
			printf("Core %d: number of integer ALUs not in range (0<%d<=%d)\n",i,cores[i].res_ialu,MAX_INSTS_PER_CLASS);
			assert(0);
		}

		if((cores[i].res_imult < 1)||(cores[i].res_imult > MAX_INSTS_PER_CLASS))
		{
			printf("Core %d: number of integer multiplier/dividers not in range (0<%d<=%d)\n",i,cores[i].res_imult,MAX_INSTS_PER_CLASS);
			assert(0);
		}

		if((cores[i].res_memport < 1)||(cores[i].res_memport > MAX_INSTS_PER_CLASS))
		{
			printf("Core %d: number of memory system ports not in range (0<%d<=%d)\n",i,cores[i].res_memport,MAX_INSTS_PER_CLASS);
			assert(0);
		}

		if((cores[i].res_fpalu < 1)||(cores[i].res_fpalu > MAX_INSTS_PER_CLASS))
		{
			printf("Core %d: number of floating point ALUs not in range (0<%d<=%d)\n",i,cores[i].res_fpalu,MAX_INSTS_PER_CLASS);
			assert(0);
		}

		if((cores[i].res_fpmult < 1)||(cores[i].res_fpmult > MAX_INSTS_PER_CLASS))
		{
			printf("Core %d: number of floating point multiplier/dividers not in range (0<%d<=%d)\n",i,cores[i].res_fpmult,MAX_INSTS_PER_CLASS);
			assert(0);
		}

		if(cores[i].cache_dl1_lat < 1)
		{
			printf("Core %d L1 data cache latency must be greater than zero",i);
			assert(0);
		}

		if(cores[i].cache_dl2_lat < 1)
		{
			printf("Core %d L2 data cache latency must be greater than zero",i);
			assert(0);
		}

		if(cores[i].cache_il1_lat < 1)
		{
			printf("Core %d L1 instruction cache latency must be greater than zero",i);
			assert(0);
		}

		if(cores[i].cache_il2_lat < 1)
		{
			printf("Core %d L2 instruction cache latency must be greater than zero",i);
			assert(0);
		}

		if(cores[i].tlb_miss_lat < 1)
		{
			printf("Core %d TLB miss latency must be greater than zero",i);
			assert(0);
		}

		if(!strcmp(cores[i].fetch_policy, "icount")){
			cores[i].fetcher = icount_fetch;
		}else if(!strcmp(cores[i].fetch_policy, "round_robin")){
			cores[i].fetcher = RR_fetch;
		}else if(!strcmp(cores[i].fetch_policy, "dcra")){
			cores[i].fetcher = dcra_fetch;
		}else{
			std::cerr << "Invalid fetch policy!" << std::endl;
			assert(0);
		}

		//Set up recovery model value
		cores[i].recovery_model_v = core_t::RECOVERY_MODEL_UNDEFINED;
		if(cores[i].recovery_model == std::string("squash"))
		{
			cores[i].recovery_model_v = core_t::RECOVERY_MODEL_SQUASH;
		}
		else if(cores[i].recovery_model == std::string("perfect"))
		{
			cores[i].recovery_model_v = core_t::RECOVERY_MODEL_PERFECT;
		}

		cores[i].id = i;
		cores[i].reg_file.resize(cores[i].rf_size);
		cores[i].reserveArch(max_contexts_per_core);
		cores[i].ROB_list.resize(max_contexts_per_core,-1);
		cores[i].LSQ_list.resize(max_contexts_per_core,-1);
		cores[i].IFQ_list.resize(max_contexts_per_core,-1);
		cores[i].ROB.resize(max_contexts_per_core,cores[i].ROB_size);
		cores[i].LSQ.resize(max_contexts_per_core,cores[i].LSQ_size);
		cores[i].IFQ.resize(max_contexts_per_core,cores[i].FETCH_RENAME_DELAY * cores[i].decode_width);
		cores[i].iq.resize(cores[i].iq_size);
		cores[i].iq.clear();

		cores[i].pred_list.resize(max_contexts_per_core,-1);
		cores[i].load_lat_pred_list.resize(max_contexts_per_core,-1);
		for(int j=0;j<max_contexts_per_core;j++)
		{
			if(!mystricmp(cores[i].pred_type, "perfect"))
			{
				//perfect predictor
				cores[i].pred.push_back(NULL);
				cores[i].pred_perfect = TRUE;
			}
#ifdef BPRED_TAKEN_H
			else if(!mystricmp(cores[i].pred_type, "taken"))
			{
				//static predictor, taken
				cores[i].pred.push_back(new bpred_bpred_taken());
			}
#endif
#ifdef BPRED_NOT_TAKEN_H
			else if(!mystricmp(cores[i].pred_type, "nottaken"))
			{
				//static predictor, not taken
				cores[i].pred.push_back(new bpred_bpred_not_taken());
			}
#endif
#ifdef BPRED_BIMODAL_H
			else if(!mystricmp(cores[i].pred_type, "bimod"))
			{
				//bimodal predictor, bpred_create checks BTB_SIZE
				if(cores[i].bimod_nelt!=1)
					fatal("bad bimod predictor config (<table_size>)");
				if(cores[i].btb_nelt!=2)
					fatal("bad btb config (<num_sets> <associativity)");
				cores[i].pred.push_back(new bpred_bpred_2bit(cores[i].bimod_config[0],cores[i].btb_config[0],cores[i].btb_config[1],cores[i].ras_size));
				//Type, bimod table size, btb sets, btb assoc, ret-addr stack size
			}
#endif
#ifdef BPRED_TWO_LEVEL_H
			else if(!mystricmp(cores[i].pred_type, "2lev"))
			{
				//2-level adaptive predictor, bpred_create() checks args
				if(cores[i].twolev_nelt != 4)
					fatal("bad 2-level pred config (<l1size> <l2size> <hist_size> <xor>)");
				if(cores[i].btb_nelt != 2)
					fatal("bad btb config (<num_sets> <associativity>)");
				cores[i].pred.push_back(new bpred_bpred_2Level(cores[i].twolev_config[0],cores[i].twolev_config[1],cores[i].twolev_config[2],cores[i].twolev_config[3],cores[i].btb_config[0],cores[i].btb_config[1],cores[i].ras_size));
				//Type, 2lev l1 size, 2lev l2 size, history reg size, history xor address, btb sets, btb assoc, ret-addr stack size
			}
#endif
#ifdef BPRED_COMBINING_H
			else if(!mystricmp(cores[i].pred_type, "comb"))
			{
				//combining predictor, bpred_create() checks args
				if(cores[i].twolev_nelt != 4)
					fatal("bad 2-level pred config (<l1size> <l2size> <hist_size> <xor>)");
				if(cores[i].bimod_nelt != 1)
					fatal("bad bimod predictor config (<table_size>)");
				if(cores[i].comb_nelt != 1)
					fatal("bad combining predictor config (<meta_table_size>)");
				if(cores[i].btb_nelt != 2)
					fatal("bad btb config (<num_sets> <associativity>)");

				cores[i].pred.push_back(new bpred_bpred_comb(cores[i].bimod_config[0],cores[i].twolev_config[0],cores[i].twolev_config[1],cores[i].comb_config[0],cores[i].twolev_config[2],cores[i].twolev_config[3],
					cores[i].btb_config[0],cores[i].btb_config[1],cores[i].ras_size));
				//Type, bimod table size, 2lev l1 size, 2lev l2 size, meta table size
				//history reg size, history xor address, btb sets, btb assoc, ret-addr stack size
			}
#endif
			else
				fatal("cannot parse predictor type `%s'", cores[i].pred_type);

			if(!cores[i].bpred_spec_opt)
				cores[i].bpred_spec_update = core_t::spec_CT;
			else if(!mystricmp(cores[i].bpred_spec_opt, "ID"))
				cores[i].bpred_spec_update = core_t::spec_ID;
			else if(!mystricmp(cores[i].bpred_spec_opt, "WB"))
				cores[i].bpred_spec_update = core_t::spec_WB;
			else
				fatal("bad speculative update stage specifier, use {ID|WB}");

			/**************************** load hit/miss predictor ***********************/
			// bimodal predictor, bpred_create() checks BTB_SIZE
			if(!mystricmp(cores[i].cpred_type, "perfect"))
			{
				//perfect predictor
				cores[i].load_lat_pred.push_back(NULL);
				cores[i].pred_perfect = TRUE;
			}
#ifdef BPRED_TAKEN_H
			else if(!mystricmp(cores[i].cpred_type, "taken"))
			{
				//static predictor, taken
				cores[i].load_lat_pred.push_back(new bpred_bpred_taken());
			}
#endif
#ifdef BPRED_NOT_TAKEN_H
			else if(!mystricmp(cores[i].cpred_type, "nottaken"))
			{
				//static predictor, not taken
				cores[i].load_lat_pred.push_back(new bpred_bpred_not_taken());
			}
#endif
#ifdef BPRED_BIMODAL_H
			else if(!mystricmp(cores[i].cpred_type, "bimod"))
			{
				//bimodal predictor, bpred_create checks BTB_SIZE
				if(cores[i].cbimod_nelt!=1)
					fatal("bad cbimod predictor config (<table_size>)");
				if(cores[i].cbtb_nelt!=2)
					fatal("bad cbtb config (<num_sets> <associativity)");
				cores[i].load_lat_pred.push_back(new bpred_bpred_2bit(cores[i].cbimod_config[0],cores[i].cbtb_config[0],cores[i].cbtb_config[1],cores[i].cras_size));
				//Type, bimod table size, 2lev l1 size, 2lev l2 size, meta table size
				//history reg size, history xor address, btb sets, btb assoc, ret-addr stack size
			}
#endif
#ifdef BPRED_TWO_LEVEL_H
			else if(!mystricmp(cores[i].cpred_type, "2lev"))
			{
				//2-level adaptive predictor, bpred_create() checks args
				if(cores[i].ctwolev_nelt != 4)
					fatal("bad 2-level cpred config (<l1size> <l2size> <hist_size> <xor>)");
				if(cores[i].cbtb_nelt != 2)
					fatal("bad cbtb config (<num_sets> <associativity>)");
				cores[i].load_lat_pred.push_back(new bpred_bpred_2Level(cores[i].ctwolev_config[0],cores[i].ctwolev_config[1],cores[i].ctwolev_config[2],cores[i].ctwolev_config[3],cores[i].cbtb_config[0],cores[i].cbtb_config[1],cores[i].cras_size));
				//Type, bimod table size, 2lev l1 size, 2lev l2 size, meta table size
				//history reg size, history xor address, btb sets, btb assoc, ret-addr stack size
			}
#endif
#ifdef BPRED_COMBINING_H
			else if(!mystricmp(cores[i].cpred_type, "comb"))
			{
				//combining predictor, bpred_create() checks args
				if(cores[i].ctwolev_nelt != 4)
					fatal("bad 2-level cpred config (<l1size> <l2size> <hist_size> <xor>)");
				if(cores[i].cbimod_nelt != 1)
					fatal("bad cbimod predictor config (<table_size>)");
				if(cores[i].ccomb_nelt != 1)
					fatal("bad combining cpredictor config (<meta_table_size>)");
				if(cores[i].cbtb_nelt != 2)
					fatal("bad cbtb config (<num_sets> <associativity>)");

				cores[i].load_lat_pred.push_back(new bpred_bpred_comb(cores[i].cbimod_config[0],cores[i].ctwolev_config[0],cores[i].ctwolev_config[1],cores[i].ccomb_config[0],cores[i].ctwolev_config[2],
					cores[i].ctwolev_config[3],cores[i].cbtb_config[0],cores[i].cbtb_config[1],cores[i].cras_size));
				//Type, bimod table size, 2lev l1 size, 2lev l2 size, meta table size
				//history reg size, history xor address, btb sets, btb assoc, ret-addr stack size
			}
#endif
			else
				fatal("cannot parse load_latency predictor type `%s'", cores[i].cpred_type);
		}
	}

	if(cache_dl3_lat < 1)
		fatal("l3 data cache latency must be greater than zero");

	if(cache_il3_lat < 1)
		fatal("l3 instruction cache latency must be greater than zero");

	//Note: cache_dl3 and cache_il3 belong to no core
	std::string prepend = "Core_0_";
	int nsets, bsize, assoc;
	char name[128], c;

	//is the level 3 D-cache defined?
	if (!mystricmp(cache_dl3_opt, "none"))
		cache_dl3 = NULL;
	else
	{
		if(sscanf(cache_dl3_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
			fatal("bad l3 D-cache parms: " "<name>:<nsets>:<bsize>:<assoc>:<repl>");
		cache_dl3 = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */0, assoc, cache_char2policy(c),
			dl3_access_fn, /* hit lat */cache_dl3_lat);
	}
	//is the level 3 D-cache defined?
	if(!mystricmp(cache_il3_opt, "none"))
		cache_il3 = NULL;
	else if(!mystricmp(cache_il3_opt, "dl3"))
	{
		if(!cache_dl3)
			fatal("I-cache l3 cannot access D-cache l3 as it's undefined");
		cache_il3 = cache_dl3;
	}
	else
	{
		if(sscanf(cache_il3_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
			fatal("bad l3 I-cache parms: <name>:<nsets>:<bsize>:<assoc>:<repl>");
		cache_il3 = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */0, assoc, cache_char2policy(c),
			il3_access_fn, /* hit lat */cache_il3_lat);
	}

	for(unsigned int i=0;i<num_cores;i++)
	{
		std::string prepend = "Core_";
		std::stringstream in;
		in << i;
		std::string temp;
		in >> temp;
		prepend += (temp + "_");

		//use a level 1 D-cache?
		if(!mystricmp(cores[i].cache_dl1_opt, "none"))
		{
			cores[i].cache_dl1 = cache_dl3;

			//the level 2 D-cache cannot be defined
			if(strcmp(cores[i].cache_dl2_opt, "none"))
				fatal("the l1 data cache must defined if the l2 cache is defined");
			cores[i].cache_dl2 = cache_dl3;
		}
		else //dl1 is defined
		{
			if(sscanf(cores[i].cache_dl1_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
				fatal("bad l1 D-cache parms: <name>:<nsets>:<bsize>:<assoc>:<repl>");

			cores[i].cache_dl1 = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */0, assoc, cache_char2policy(c),
				dl1_access_fn, /* hit lat */cores[i].cache_dl1_lat);

			//is the level 2 D-cache defined?
			if(!mystricmp(cores[i].cache_dl2_opt, "none"))
				cores[i].cache_dl2 = cache_dl3;
			else
			{
				if(sscanf(cores[i].cache_dl2_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
					fatal("bad l2 D-cache parms: " "<name>:<nsets>:<bsize>:<assoc>:<repl>");
				cores[i].cache_dl2 = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */0, assoc, cache_char2policy(c),
					dl2_access_fn, /* hit lat */cores[i].cache_dl2_lat);
			}
		}

		//use a level 1 I-cache?
		if(!mystricmp(cores[i].cache_il1_opt, "none"))
		{
			cores[i].cache_il1 = cache_il3;

			//the level 2 I-cache cannot be defined
			if(strcmp(cores[i].cache_il2_opt, "none"))
				fatal("the l1 inst cache must defined if the l2 cache is defined");
			cores[i].cache_il2 = cache_il3;
		}
		else if(!mystricmp(cores[i].cache_il1_opt, "dl1"))
		{
			if (!cores[i].cache_dl1)
				fatal("I-cache l1 cannot access D-cache l1 as it's undefined");
			cores[i].cache_il1 = cores[i].cache_dl1;

			//the level 2 I-cache cannot be defined
			if(strcmp(cores[i].cache_il2_opt, "none"))
				fatal("the l1 inst cache must defined if the l2 cache is defined");
			cores[i].cache_il2 = cache_il3;
		}
		else if(!mystricmp(cores[i].cache_il1_opt, "dl2"))
		{
			if(!cores[i].cache_dl2)
				fatal("I-cache l1 cannot access D-cache l2 as it's undefined");
			cores[i].cache_il1 = cores[i].cache_dl2;

			//the level 2 I-cache cannot be defined
			if(strcmp(cores[i].cache_il2_opt, "none"))
				fatal("the l1 inst cache must defined if the l2 cache is defined");
			cores[i].cache_il2 = cache_il3;
		}
		else //il1 is defined
		{
			if(sscanf(cores[i].cache_il1_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
				fatal("bad l1 I-cache parms: <name>:<nsets>:<bsize>:<assoc>:<repl>");
			cores[i].cache_il1 = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */0, assoc, cache_char2policy(c),
				il1_access_fn, /* hit lat */cores[i].cache_il1_lat);
			//is the level 2 D-cache defined?
			if(!mystricmp(cores[i].cache_il2_opt, "none"))
				cores[i].cache_il2 = cache_il3;
			else if(!mystricmp(cores[i].cache_il2_opt, "dl2"))
			{
				if(!cores[i].cache_dl2)
					fatal("I-cache l2 cannot access D-cache l2 as it's undefined");
				cores[i].cache_il2 = cores[i].cache_dl2;
			}
			else
			{
				if(sscanf(cores[i].cache_il2_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
					fatal("bad l2 I-cache parms: <name>:<nsets>:<bsize>:<assoc>:<repl>");
				cores[i].cache_il2 = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */0, assoc, cache_char2policy(c),
					il2_access_fn, /* hit lat */cores[i].cache_il2_lat);
			}
		}

	 	//use an I-TLB?
		if(!mystricmp(cores[i].itlb_opt, "none"))
			cores[i].itlb = NULL;
		else
		{
			if(sscanf(cores[i].itlb_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
				fatal("bad TLB parms: <name>:<nsets>:<page_size>:<assoc>:<repl>");
			cores[i].itlb = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */sizeof(md_addr_t), assoc,
				cache_char2policy(c), itlb_access_fn, /* hit latency */1);
		}

		//use a D-TLB?
		if(!mystricmp(cores[i].dtlb_opt, "none"))
			cores[i].dtlb = NULL;
		else
		{
			if(sscanf(cores[i].dtlb_opt, "%[^:]:%d:%d:%d:%c", name, &nsets, &bsize, &assoc, &c) != 5)
				fatal("bad TLB parms: <name>:<nsets>:<page_size>:<assoc>:<repl>");
			cores[i].dtlb = new cache_t(prepend + name, nsets, bsize, /* balloc */FALSE, /* usize */sizeof(md_addr_t), assoc,
				cache_char2policy(c), dtlb_access_fn, /* hit latency */1);
		}
	}

	assert(main_mem = dram_parser(main_mem_config));
	for(unsigned int i=0;i<num_cores;i++)
	{
		cores[i].main_mem = main_mem;
		if(cores[i].pred_perfect)
		{
			std::cerr << "Warning: Perfect Predictor affects an entire core (both branch and load predictors)!" << std::endl;
		}

		cores[i].fu_pool = new res_pool("fu-pool", &cores[i].fu_CMP[0], cores[i].fu_CMP.size());

		//register power stats
		cores[i].power.core_id = i;
		cores[i].power.contexts_on_core = max_contexts_per_core;
		cores[i].power.decode_width = cores[i].decode_width;
		cores[i].power.issue_width = cores[i].issue_width;
		cores[i].power.commit_width = cores[i].commit_width;
		cores[i].power.ROB_size = cores[i].ROB_size;
		cores[i].power.rf_size = cores[i].rf_size;
		cores[i].power.iq_size = cores[i].iq_size;
		cores[i].power.LSQ_size = cores[i].LSQ_size;
		cores[i].power.data_width = data_width;
		cores[i].power.res_ialu = cores[i].res_ialu;
		cores[i].power.res_fpalu = cores[i].res_fpalu;
		cores[i].power.res_memport = cores[i].res_memport;
		cores[i].power.bimod_config = &cores[i].bimod_config[0];
		cores[i].power.twolev_config = &cores[i].twolev_config[0];
		cores[i].power.comb_config = &cores[i].comb_config[0];
		cores[i].power.btb_config = &cores[i].btb_config[0];
		cores[i].power.ras_size = cores[i].ras_size;
		cores[i].power.cache_dl1 = cores[i].cache_dl1;
		cores[i].power.cache_il1 = cores[i].cache_il1;
		cores[i].power.cache_dl2 = cores[i].cache_dl2;

		//FIXME: this might record L3 cache usage too many times
		cores[i].power.cache_dl3 = cache_dl3;
		cores[i].power.dtlb = cores[i].dtlb;
		cores[i].power.itlb = cores[i].itlb;

		//compute static power estimates
		if(print_power_stats)
		{
			cores[i].power.calculate_power(stderr);
		}
		else
		{
			cores[i].power.calculate_power(NULL);
		}
	}
}

//print simulator-specific configuration information
void sim_aux_config(FILE *stream)
{
	//nada
}

//register simulator-specific statistics
void sim_reg_stats(stat_sdb_t *sdb)
{
	stat_reg_counter(sdb, "sim_num_insn",
		"total number of instructions executed",
		&sim_num_insn, sim_num_insn, NULL);

	stat_reg_int(sdb, "sim_elapsed_time",
		"total simulation time in seconds",
		&sim_elapsed_time, 0, NULL);

	//register performance stats
	stat_reg_counter(sdb, "sim_cycle",
		"total simulation time in cycles",
		&sim_cycle, /* initial value */0, /* format */NULL);
	stat_reg_formula(sdb, "sim_inst_rate",
		"simulation speed (in insts/sec)",
		std::string("sim_num_insn / sim_elapsed_time").c_str(), NULL);

	//debug variable(s)
	stat_reg_counter(sdb, "sim_invalid_addrs",
		"total non-speculative bogus addresses seen (debug var)",
		&sim_invalid_addrs, /* initial value */0, /* format */NULL);

	//register predictor (branch and load-latency) stats
	for(unsigned int i = 0; i < num_cores; i++)
	{
		for(int j=0;j<max_contexts_per_core;j++)
		{
			if(cores[i].pred[j])
			{
				char name_buf[30];
				std::string name = "bpred_" + cores[i].pred[j]->name;

				sprintf(name_buf, "Core_%d_pred_%d_%s", i, j, name.c_str());
				name = name_buf;
				cores[i].pred[j]->bpred_reg_stats(sdb, name.c_str());
			}
		}
	}
	for(unsigned int i = 0; i < num_cores; i++)
	{
		for(int j=0;j<max_contexts_per_core;j++)
		{
			if(cores[i].load_lat_pred[j])
			{
				char name_buf[30];
				std::string name = "cbpred_" + cores[i].load_lat_pred[j]->name;
				sprintf(name_buf, "Core_%d_pred_%d_%s", i, j, name.c_str());
				name = name_buf;
				cores[i].load_lat_pred[j]->bpred_reg_stats(sdb, name.c_str());
			}
		}
	}

	for(int i=0; i<pcstat_nelt; i++)
	{
		char buf[512], buf1[512];

		//track the named statistical variable by text address

		//find it...
		stat_stat_t *stat = stat_find_stat(sdb, pcstat_vars[i]);
		if(!stat)
			fatal("cannot locate any statistic named `%s'", pcstat_vars[i]);

		//stat must be an integral type
		if(stat->sc != sc_int && stat->sc != sc_uint && stat->sc != sc_counter)
			fatal("`-pcstat' statistical variable `%s' is not an integral type",stat->name.c_str());

		//register this stat
		pcstat_stats[i] = stat;
		pcstat_lastvals[i] = STATVAL(stat);

		//declare the sparce text distribution
		sprintf(buf, "%s_by_pc", stat->name.c_str());
		sprintf(buf1, "%s (by text address)", stat->desc.c_str());
		pcstat_sdists[i] = stat_reg_sdist(sdb, buf, buf1,
			/* initial value */0,
			/* print format */(PF_COUNT|PF_PDF),
			/* format */"0x%lx %lu %.2f",
			/* print fn */NULL);
	}

	//register power stats
	if(print_power_stats)
	{
		for(unsigned int i=0;i<num_cores;i++)
		{
			cores[i].power.power_reg_stats(sdb);
		}
	}
}

//initialize the simulator
void sim_init()
{}

//load program into simulated state
void sim_load_prog(std::string fname,			//program to load
	std::vector<std::string> argv,			//program arguments
	std::vector<std::string> envp)			//program environment
{
	//load the program into the next available context
	regs_t* regs = &contexts[num_contexts].regs;

	//Distributes programs into cores via round_robin
	static int core_num = -1;
	core_num++;

	//Contexts are put into cores in a round_robin fashion and initialized
	int targetcore = core_num%num_cores;
	contexts[num_contexts].init_context(num_contexts);
	if(!cores[targetcore].addcontext(contexts[num_contexts]))
	{
		std::cout << "Could not add: " << contexts[num_contexts].filename << " (context #" << num_contexts << ") to core: " << targetcore << std::endl;
		assert(0);
	}

	//load program text and data, set up environment, memory, and regs
	if(loader.ld_load_prog(fname, argv, envp, regs, contexts[num_contexts].mem, TRUE))
	{
		std::cerr << "Can't load context " << fname << " at start time" << std::endl;
		exit(1);
	}

	//Initialize the process id
	contexts[num_contexts].pid = pid_handler.get_new_pid();
	contexts[num_contexts].gpid = 15;
	contexts[num_contexts].gid = 15;

	//initalize the Program Counter and context_id
	regs->regs_PC = contexts[num_contexts].mem->ld_prog_entry;
	regs->regs_NPC = contexts[num_contexts].mem->ld_prog_entry + 4;
	regs->context_id = num_contexts;

	//initialize here, so symbols can be loaded
	if(ptrace_nelt == 2)
	{
		//generate a pipeline trace
		ptrace_open(/* fname */ptrace_opts[0], /* range */ptrace_opts[1], contexts[num_contexts].mem);
	}
	else if(ptrace_nelt == 0)
	{
		//no pipetracing
	}
	else
	{
		std::cout << "bad pipetrace args, use: <fname|stdout|stderr> <range>" << std::endl;
		assert(0);
	}

	//initialize the DLite debugger
	contexts[num_contexts].dlite_evaluator = new dlite_t(simoo_reg_obj, simoo_mem_obj, simoo_mstate_obj, contexts[num_contexts].mem, &contexts[num_contexts].regs);

	//increment the global counter for the number of used contexts
	num_contexts++;
}

//dump simulator-specific auxiliary simulator statistics to an output stream
void sim_aux_stats(FILE *stream)
{
	for(size_t i=0;i<contexts.size();i++)
	{
		contexts[i].print_stats(stream);
	}

	if(!ejected_contexts.size())
	{
		for(size_t i=0;i<ejected_contexts.size();i++)
		{
			ejected_contexts[i].print_stats(stream);
		}
	}

	for(unsigned int i=0;i<num_cores;i++)
	{
		cores[i].print_stats(stream,sim_cycle);
	}

	for(size_t i=0;i<contexts.size();i++)
	{
		contexts[i].mem->print_stats(stream);
	}

	//register cache stats
	std::set<cache_t *> caches;
	caches.insert(cache_dl3);
	caches.insert(cache_il3);
	for(unsigned int i=0;i<num_cores;i++)
	{
		caches.insert(cores[i].cache_il1);
		caches.insert(cores[i].cache_il2);
		caches.insert(cores[i].cache_dl1);
		caches.insert(cores[i].cache_dl2);
		caches.insert(cores[i].itlb);
		caches.insert(cores[i].dtlb);
	}
	caches.erase(NULL);
	for(std::set<cache_t *>::iterator it=caches.begin();it!=caches.end();it++)
	{
		(*it)->print_stats(stream);
	}

}

//uninitialize the simulator
void sim_uninit()
{
	if(ptrace_nelt > 0)
		ptrace_close();

	delete main_mem;

	for(size_t i=0;i<contexts.size();i++)
	{
		contexts[i].file_table.closer(0);
		contexts[i].file_table.closer(1);
		contexts[i].file_table.closer(2);
		delete contexts[i].dlite_evaluator;
	}

	std::set<cache_t *> caches;
	caches.insert(cache_dl3);
	caches.insert(cache_il3);
	for(size_t i=0;i<cores.size();i++)
	{
		caches.insert(cores[i].cache_il1);
		caches.insert(cores[i].cache_il2);
		caches.insert(cores[i].cache_dl1);
		caches.insert(cores[i].cache_dl2);
		caches.insert(cores[i].itlb);
		caches.insert(cores[i].dtlb);
	}
	for(std::set<cache_t *>::iterator it=caches.begin();it!=caches.end();it++)
	{
		delete (*it);
	}

	std::set<bpred_t *> bpreds;
	for(size_t i=0;i<cores.size();i++)
	{
		for(unsigned int j=0;j<cores[i].load_lat_pred.size();j++)
		{
			bpreds.insert(cores[i].load_lat_pred[j]);
		}
		for(unsigned int j=0;j<cores[i].pred.size();j++)
		{
			bpreds.insert(cores[i].pred[j]);
		}
	}
	for(std::set<bpred_t *>::iterator it=bpreds.begin();it!=bpreds.end();it++)
	{
		delete (*it);
	}

	for(unsigned int i=0;i<cores.size();i++)
	{
		delete cores[i].fu_pool;
	}
}

//processor core definitions and declarations

//dump the contents of the ROB
void rob_dumpent(ROB_entry *rs,		//ptr to ROB entry
	int index,			//Entry index
	FILE *stream,			//output stream
	int header)			//print header?
{
	if(!stream)
		stream = stderr;

	if(header)
		fprintf(stream, "idx: %2d: opcode: %s, inst: `", index, MD_OP_NAME(rs->op));
	else
		fprintf(stream, "       opcode: %s, inst: `", MD_OP_NAME(rs->op));
	md_print_insn(rs->IR, rs->PC, stream);
	fprintf(stream, "'\n");
	myfprintf(stream, "         PC: 0x%08p, NPC: 0x%08p (pred_PC: 0x%08p)\n", rs->PC, rs->next_PC, rs->pred_PC);
	fprintf(stream,   "         in_LSQ: %d, ea_comp: %d, recover_inst: %d\n", rs->in_LSQ, rs->ea_comp, rs->recover_inst);
	myfprintf(stream, "         spec_mode: %d, addr: 0x%08p", rs->spec_mode, rs->addr);
	fprintf(stream,   "         seq: %lld, ptrace_seq: %lld\n", rs->seq, rs->ptrace_seq);
	fprintf(stream,   "         queued: %d, issued: %d, completed: %d\n", rs->queued,	rs->issued, rs->completed);
	fprintf(stream,   "         operands ready: %d\n", all_operands_ready(rs));
}

//dump the contents of the ROB
void rob_dump(FILE *stream, int context_id)
{
	if(!stream)
		stream = stderr;

	fprintf(stream, "** ROB state **\n");
	fprintf(stream, "ROB_head: %d, ROB_tail: %d\n", contexts[context_id].ROB_head, contexts[context_id].ROB_tail);
	fprintf(stream, "ROB_num: %d\n", contexts[context_id].ROB_num);

	int num = contexts[context_id].ROB_num;
	int head = contexts[context_id].ROB_head;
	while(num)
	{
		ROB_entry *rs = &contexts[context_id].ROB[head];
		rob_dumpent(rs, rs - &contexts[context_id].ROB[0], stream, /* header */TRUE);
		head = (head + 1) % contexts[context_id].ROB.size();
		num--;
	}
}

//dump the contents of the LSQ
void lsq_dump(FILE *stream, int context_id)
{
	if(!stream)
		stream = stderr;

	fprintf(stream, "** LSQ state **\n");
	fprintf(stream, "LSQ_head: %d, LSQ_tail: %d\n", contexts[context_id].LSQ_head, contexts[context_id].LSQ_tail);
	fprintf(stream, "LSQ_num: %d\n", contexts[context_id].LSQ_num);

	int num = contexts[context_id].LSQ_num;
	int head = contexts[context_id].LSQ_head;
	while(num)
	{
		ROB_entry *rs = &contexts[context_id].LSQ[head];
		rob_dumpent(rs, rs - &contexts[context_id].LSQ[0], stream, /* header */TRUE);
		head = (head + 1) % contexts[context_id].LSQ.size();
		num--;
	}
}

//the execution unit event queue implementation follows, the event queue indicates
//which instruction will complete next, the writeback handler drains this queue

//dump the contents of the event queue to output specified by stream
void eventq_dump(FILE *stream, int context_id)
{
	if(!stream)
		stream = stderr;
	fprintf(stream, "** event queue state **\n");

	std::list<RS_link>::iterator it = cores[contexts[context_id].core_id].event_queue.begin();
	while(it!=cores[contexts[context_id].core_id].event_queue.end())
	{
		ROB_entry *rs = (*it).rs;
		fprintf(stream, "idx: %2d: @ %.0f\n", (int)(rs - (rs->in_LSQ ? &contexts[context_id].LSQ[0] : &contexts[context_id].ROB[0])), (double)(*it).x.when);
		rob_dumpent(rs, rs - (rs->in_LSQ ? &contexts[context_id].LSQ[0] : &contexts[context_id].ROB[0]), stream, /* !header */FALSE);
		it++;
	}
}

void issue_exec_q_queue_event(ROB_entry *rs, tick_t when)
{
	if(rs->completed)
		panic("instruction completed");
	assert(all_operands_spec_ready(rs));
	if(when < sim_cycle)
		panic("event occurred in the past");
	RS_link n_link(rs);
	n_link.x.when = when;
	cores[contexts[rs->context_id].core_id].issue_exec_queue.inorderinsert(n_link);
}

//the ready instruction queue implementation follows, the ready instruction queue indicates which
//instruction have all of there *register* dependencies satisfied, instruction will issue when
//1) all memory dependencies for the instruction have been satisfied (see lsq_refresh() for details on how this is accomplished) and
//2) resources are available; ready queue is fully constructed each cycle before any operation
//is issued from it -- this ensures that instruction issue priorities are properly observed;
//NOTE: RS_LINK nodes are used for the event queue list so that it need not be updated during squash events

//dump the contents of the ready queue to the output specified by stream
void readyq_dump(FILE *stream, int context_id)
{
	if(!stream)
		stream = stderr;
	fprintf(stream, "** ready queue state **\n");

	std::list<RS_link>::iterator it = cores[contexts[context_id].core_id].ready_queue.begin();
	while(it!=cores[contexts[context_id].core_id].ready_queue.end())
	{
		ROB_entry *rs = (*it).rs;
		rob_dumpent(rs, rs - (rs->in_LSQ ? &contexts[context_id].LSQ[0] : &contexts[context_id].ROB[0]), stream, /* header */TRUE);
		it++;
	}
}

/* insert ready node into the ready list using ready instruction scheduling
   policy; currently the following scheduling policy is enforced:

	memory and long latency operands, and branch instructions first
then
	all other instructions, oldest instructions first

  this policy works well because branches pass through the machine quicker
  which works to reduce branch misprediction latencies, and very long latency
  instructions (such loads and multiplies) get priority since they are very
  likely on the program's critical path */
void readyq_enqueue(ROB_entry *rs)		//RS to enqueue
{
	//We queue the node here, therefore, it shouldn't be queued already
	if(rs->queued)
		panic("node is already queued");
	rs->queued = TRUE;
	assert(all_operands_spec_ready(rs));

	RS_link n_link(rs);
	n_link.x.seq = rs->seq;
	cores[contexts[rs->context_id].core_id].ready_queue.inorderinsert(n_link);

}

//COMMIT() - instruction retirement pipeline stage

//this function commits the results of the oldest completed entries from the
//ROB and LSQ to the architected reg file, stores in the LSQ will commit
//their store data to the data cache at this point as well
void commit(unsigned int core_num)
{
	std::vector<int> contexts_left(cores[core_num].context_ids);
	//Check for commit timeout
	
	for(size_t i=0;i<contexts_left.size();i++)
	{
		size_t context_id = contexts_left[i];
		assert(contexts[context_id].pid);

		//If context is waiting or stalled on select, it shouldn't generate a timeout
		if(contexts[context_id].interrupts & 0x30000)
		{
			contexts[context_id].last_commit_cycle = sim_cycle;
		}
		if((contexts[context_id].last_commit_cycle + COMMIT_TIMEOUT + 1) <= sim_cycle)
		{
			std::cerr << "No insts from " << context_id << ":" << contexts[context_id].filename;
			std::cerr << " have committed in a long time: " << contexts[context_id].last_commit_cycle << " " << sim_cycle << std::endl;
			assert(0);
		}
		if((contexts[context_id].last_commit_cycle + COMMIT_TIMEOUT) <= sim_cycle)
		{
			std::cerr << "Index of offending ROB entry: " << contexts[context_id].ROB_head << std::endl;
		}
	}

	//round robin commit
	unsigned int current_context = sim_cycle % contexts_left.size();
	unsigned int committed = 0;
	//all values must be retired to the architected reg file in program order
	while(committed < cores[core_num].commit_width)
	{
		//If there are no contexts left, we are done.
		if(contexts_left.empty())
			break;

		current_context%=contexts_left.size();
		int context_id = contexts_left[current_context];
		int events = 0;
		int lat;		//latency and default commit events

		if(contexts[context_id].ROB_num<=0)
		{
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		ROB_entry *rs = &(contexts[context_id].ROB[contexts[context_id].ROB_head]);
		if(!rs->completed)
		{
			if((contexts[context_id].last_commit_cycle + COMMIT_TIMEOUT) <= sim_cycle)
			{
				fprintf(stderr, "Instruction not completed within timeout (ROB): ");
				md_print_insn(rs->IR, rs->PC, stderr);
				fprintf(stderr, "\nrs->dispatched?: %d iq #: %d\n", rs->dispatched, rs->iq_entry_num);
			}

			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//loads and stores must retire LSQ entry as well. If the ROB is effective address computation
		if(contexts[context_id].ROB[contexts[context_id].ROB_head].ea_comp)
		{
			//ea_comp in ROB implies head of LSQ is a load or store
			if(contexts[context_id].LSQ_num <= 0 || !contexts[context_id].LSQ[contexts[context_id].LSQ_head].in_LSQ)
				panic("ROB out of sync with LSQ");

			//load or store must be complete
			if(!contexts[context_id].LSQ[contexts[context_id].LSQ_head].completed)
			{
				//load or store not complete
				if((contexts[context_id].last_commit_cycle + COMMIT_TIMEOUT) <= sim_cycle){
					fprintf(stderr, "Instruction not completed within timeout (LSQ): ");
					md_print_insn(contexts[context_id].LSQ[contexts[context_id].LSQ_head].IR, contexts[context_id].LSQ[contexts[context_id].LSQ_head].PC, stderr);
					fprintf(stderr, "\nIssued: %d ", contexts[context_id].LSQ[contexts[context_id].LSQ_head].issued);
					fprintf(stderr, "\tROB completed?: %d \n", rs->completed);
					fprintf(stderr, "Effective address computation completed but LSQ could not commit!\n");
				}
				contexts_left.erase(contexts_left.begin()+current_context);
				continue;
			}

			assert(contexts[context_id].ROB[contexts[context_id].ROB_head].context_id == contexts[context_id].LSQ[contexts[context_id].LSQ_head].context_id);

			if((MD_OP_FLAGS(contexts[context_id].LSQ[contexts[context_id].LSQ_head].op) & (F_MEM|F_STORE)) == (F_MEM|F_STORE))
			{
				if(cores[core_num].write_buf.size() == cores[core_num].write_buf_size)
				{
					while(!cores[core_num].write_buf.empty() && (*(cores[core_num].write_buf.begin()) < sim_cycle))
					{
						cores[core_num].write_buf.erase(cores[core_num].write_buf.begin());
					}
				}
				//stores must retire their store value to the cache at commit. Try to get a store port (functional unit allocation)
				res_template *fu = res_get(cores[core_num].fu_pool, MD_OP_FUCLASS(contexts[context_id].LSQ[contexts[context_id].LSQ_head].op));
				if(fu && (cores[core_num].write_buf.size() < cores[core_num].write_buf_size))
				{
					//reserve the functional unit
					if(fu->master->busy)
						panic("functional unit already in use");

					//schedule functional unit release event
					fu->master->busy = fu->issuelat;

					tick_t write_finish = sim_cycle;

					//go to the data cache
					if(cores[core_num].cache_dl1)
					{
						//Wattch -- D-cache access
						cores[core_num].power.dcache_access++;

						//commit store value to D-cache
						lat = cores[core_num].cache_dl1->cache_access(Write, (contexts[context_id].LSQ[contexts[context_id].LSQ_head].addr&~3),
							context_id, NULL, 4, sim_cycle, NULL, NULL);

						if(lat > cores[core_num].cache_dl1_lat)
							events |= PEV_CACHEMISS;

						write_finish = std::max(write_finish, sim_cycle + lat);
					}

					//all loads and stores must access D-TLB
					if(cores[core_num].dtlb)
					{
						lat = cores[core_num].dtlb->cache_access(Read, (contexts[context_id].LSQ[contexts[context_id].LSQ_head].addr & ~3),
							context_id, NULL, 4, sim_cycle, NULL, NULL);
						if(lat > 1)
							events |= PEV_TLBMISS;

						write_finish = std::max(write_finish, sim_cycle + lat);
					}
					cores[core_num].write_buf.insert(write_finish);
					assert(cores[core_num].write_buf.size() <= cores[core_num].write_buf_size);
				}
				else
				{
					//no store ports left, cannot continue to commit insts
					contexts_left.erase(contexts_left.begin()+current_context);
					continue;
				}
			}

			//invalidate load or store operation
			cores[core_num].Clear_Entry_From_Queues(&contexts[context_id].LSQ[contexts[context_id].LSQ_head]);
			cores[core_num].sim_slip += (sim_cycle - contexts[context_id].LSQ[contexts[context_id].LSQ_head].slip);

			//indicate to pipeline trace that this instruction retired
			ptrace_newstage(contexts[context_id].LSQ[contexts[context_id].LSQ_head].ptrace_seq, PST_COMMIT, events);
			ptrace_endinst(contexts[context_id].LSQ[contexts[context_id].LSQ_head].ptrace_seq);

			//commit head of LSQ (ROB will be committed later in this iteration
			contexts[context_id].LSQ_head = (contexts[context_id].LSQ_head + 1) % contexts[context_id].LSQ.size();
			contexts[context_id].LSQ_num--;
		}

		//Wattch -- committed instruction to arch reg file
		if((MD_OP_FLAGS(rs->op) & (F_ICOMP|F_FCOMP)) || ((MD_OP_FLAGS(rs->op) & (F_MEM|F_LOAD)) == (F_MEM|F_LOAD)))
		{
			cores[core_num].power.regfile_access++;
#ifdef DYNAMIC_AF
			cores[core_num].power.regfile_total_pop_count_cycle += pop_count(rs->val_rc);
			cores[core_num].power.regfile_num_pop_count_cycle++;
#endif
		}

		if(contexts[context_id].pred && cores[core_num].bpred_spec_update == core_t::spec_CT && (MD_OP_FLAGS(rs->op) & F_CTRL))
		{
			//Wattch -- bpred access
			cores[core_num].power.bpred_access++;
			contexts[context_id].pred->bpred_update(
				/* branch address */rs->PC,
				/* actual target address */rs->next_PC,
				/* taken? */rs->next_PC != (rs->PC + sizeof(md_inst_t)),
				/* pred taken? */rs->pred_PC != (rs->PC + sizeof(md_inst_t)),
				/* correct pred? */rs->pred_PC == rs->next_PC,
				/* opcode */rs->op,
				/* dir predictor update pointer */&rs->dir_update);
		}

		//invalidate ROB operation
		cores[core_num].Clear_Entry_From_Queues(&contexts[context_id].ROB[contexts[context_id].ROB_head]);
		cores[core_num].sim_slip += (sim_cycle - contexts[context_id].ROB[contexts[context_id].ROB_head].slip);

		//print retirement trace if in verbose mode
		if(verbose)
		{
			static counter_t sim_ret_insn = 0;
			sim_ret_insn++;
			myfprintf(stderr, "(%d) %10n @ 0x%08p: ", context_id, sim_ret_insn, contexts[context_id].ROB[contexts[context_id].ROB_head].PC);
			md_print_insn(contexts[context_id].ROB[contexts[context_id].ROB_head].IR, contexts[context_id].ROB[contexts[context_id].ROB_head].PC, stderr);
			if(MD_OP_FLAGS(contexts[context_id].ROB[contexts[context_id].ROB_head].op) & F_MEM)
				myfprintf(stderr, "  mem: 0x%08p", contexts[context_id].ROB[contexts[context_id].ROB_head].addr);
			fprintf(stderr, "\n");
		}

		//indicate to pipeline trace that this instruction retired
		ptrace_newstage(contexts[context_id].ROB[contexts[context_id].ROB_head].ptrace_seq, PST_COMMIT, events);
		ptrace_endinst(contexts[context_id].ROB[contexts[context_id].ROB_head].ptrace_seq);

		//update # instructions committed for this thread
		contexts[context_id].sim_num_insn++;

		//handle register deallocations
		if(contexts[context_id].ROB[contexts[context_id].ROB_head].physreg >= 0){
			//free the old physreg mapping
			assert(cores[core_num].reg_file.reg_file_access(contexts[context_id].ROB[contexts[context_id].ROB_head].old_physreg,contexts[context_id].ROB[contexts[context_id].ROB_head].dest_format).state == REG_ARCH);
			cores[core_num].reg_file.reg_file_access(contexts[context_id].ROB[contexts[context_id].ROB_head].old_physreg,contexts[context_id].ROB[contexts[context_id].ROB_head].dest_format).state = REG_FREE;
			//commit the physreg mapping to arch state
			assert(cores[core_num].reg_file.reg_file_access(contexts[context_id].ROB[contexts[context_id].ROB_head].physreg,contexts[context_id].ROB[contexts[context_id].ROB_head].dest_format).state == REG_WB);
			cores[core_num].reg_file.reg_file_access(contexts[context_id].ROB[contexts[context_id].ROB_head].physreg,contexts[context_id].ROB[contexts[context_id].ROB_head].dest_format).state = REG_ARCH;
			/******** DCRA ******/
			if(rs->dest_format == REG_INT)
			{
				contexts[context_id].DCRA_int_rf--;
				assert(contexts[context_id].DCRA_int_rf >= 0);
			}
			else
			{
				contexts[context_id].DCRA_fp_rf--;
				assert(contexts[context_id].DCRA_fp_rf >= 0);
			}
			/********************/
		}
		//commit head entry of ROB
		contexts[context_id].ROB_head = (contexts[context_id].ROB_head + 1) % contexts[context_id].ROB.size();
		contexts[context_id].ROB_num--;

		//one more instruction committed to architected state
		committed++;
		contexts[context_id].last_commit_cycle = sim_cycle;
	}
}

// WRITEBACK() - instruction result writeback pipeline stage
//writeback completed operation results from the functional units to ROB, at this point, the output
//dependency chains of completing instructions are also walked to determine if any dependent
//instruction now has all of its register operands, if so the (nearly) ready instruction is
//inserted into the ready instruction queue
void writeback(unsigned int core_num)
{
	ROB_entry *rs;
	//service all completed events
	while((rs = cores[core_num].eventq_next_event(sim_cycle)))
	{
		//RS has completed execution and (possibly) produced a result
		if(!all_operands_ready(rs) || rs->queued || !rs->issued || rs->completed)
		{
			panic("inst completed and !ready, !issued, or completed");
		}

		//operation has completed
		rs->completed = TRUE;

		// Wattch -- 	1) Writeback result to resultbus 
		//		2) Write result to phys. regs (RUU)
		//		3) Access wakeup logic
		if(!(MD_OP_FLAGS(rs->op) & F_CTRL)) 
		{
			cores[core_num].power.window_access++;
			cores[core_num].power.window_preg_access++;
			cores[core_num].power.window_wakeup_access++;
			cores[core_num].power.resultbus_access++;
#ifdef DYNAMIC_AF	
			cores[core_num].power.window_total_pop_count_cycle += pop_count(rs->val_rc);
			cores[core_num].power.window_num_pop_count_cycle++;
			cores[core_num].power.resultbus_total_pop_count_cycle += pop_count(rs->val_rc);
			cores[core_num].power.resultbus_num_pop_count_cycle++;
#endif
		}

		//mark the destination physreg as written back 
		if((rs->physreg >= 0) && (!rs->ea_comp))
		{
			if(cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).ready != sim_cycle)
			{
				md_print_insn(rs->IR, rs->PC, stderr);
				fprintf(stderr, "\n %lld %d %d %lld\n",cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).ready, rs->in_LSQ, rs->replayed, rs->ptrace_seq);
				cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).ready = sim_cycle;
				assert(FALSE);
			}
			assert(cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).state==REG_ALLOC);
			cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).state = REG_WB;
		}

		//does this operation reveal a mis-predicted branch?
		if(rs->recover_inst)
		{
			if(rs->in_LSQ)
			{
				std::cerr << "mis-predicted load or store?!?!?" << std::endl;
				assert(!rs->in_LSQ);
			}
			//recover processor state and reinitialize fetch to correct path
			assert(rs->next_PC == contexts[rs->context_id].recover_PC);

			cores[core_num].rollbackTo(contexts[rs->context_id],sim_num_insn,rs,1);
			//continue writeback of the branch/control instruction
		}

		//if we speculatively update branch-predictor, do it here
		if(contexts[rs->context_id].pred && cores[core_num].bpred_spec_update == core_t::spec_WB && !rs->in_LSQ && (MD_OP_FLAGS(rs->op) & F_CTRL))
		{
			//Wattch -- bpred access
			cores[core_num].power.bpred_access++;
			contexts[rs->context_id].pred->bpred_update(
				/* branch address */rs->PC,
				/* actual target address */rs->next_PC,
				/* taken? */rs->next_PC != (rs->PC + sizeof(md_inst_t)),
				/* pred taken? */rs->pred_PC != (rs->PC + sizeof(md_inst_t)),
				/* correct pred? */rs->pred_PC == rs->next_PC,
				/* opcode */rs->op,
				/* dir predictor update pointer */&rs->dir_update);
		}

		//entered writeback stage, indicate in pipe trace
		ptrace_newstage(rs->ptrace_seq, PST_WRITEBACK, rs->recover_inst ? PEV_MPDETECT : 0);
	}
}

//LSQ_REFRESH() - memory access dependence checker/scheduler
//this function locates ready instructions whose memory dependencies have been satisfied, this is
//accomplished by walking the LSQ for loads, looking for blocking memory dependency condition
//(e.g., earlier store with an	unknown address)
void lsq_refresh(unsigned int core_num)
{
	for(unsigned int thread=0;thread<cores[core_num].context_ids.size();thread++)
	{
		int context_id = cores[core_num].context_ids[thread];
		std::set<md_addr_t> std_unknowns;

		//scan entire queue for ready loads: scan from oldest instruction (head) until we reach
		//the tail or an unresolved store, after which no other instruction will become ready
		for(unsigned int i=0, index=contexts[context_id].LSQ_head; i < contexts[context_id].LSQ_num; i++, index=(index + 1) % contexts[context_id].LSQ.size())
		{
			//terminate search for ready loads after first unresolved store, as no later load could be resolved in its presence
			if((MD_OP_FLAGS(contexts[context_id].LSQ[index].op) & (F_MEM|F_STORE)) == (F_MEM|F_STORE))
			{
				if(!STORE_ADDR_READY(&contexts[context_id].LSQ[index]))
				{
					//FIXME: a later STD + STD known could hide the STA unknown
					//sta unknown, blocks all later loads, stop search
					break;
				}
				else if(!all_operands_spec_ready(&contexts[context_id].LSQ[index]))
				{
					//sta known, but std unknown, may block a later store, record this address
					std_unknowns.insert(contexts[context_id].LSQ[index].addr);
				}
				else	//STORE_ADDR_READY() && OPERANDS_READY()
				{
					//a later STD known hides an earlier STD unknown
					//We erase the known STA/STD from std_unknowns, nothing happens if not found
					std_unknowns.erase(contexts[context_id].LSQ[index].addr);
				}
			}

			if(((MD_OP_FLAGS(contexts[context_id].LSQ[index].op) & (F_MEM|F_LOAD)) == (F_MEM|F_LOAD))
				&& contexts[context_id].LSQ[index].dispatched
				&& /* queued? */!contexts[context_id].LSQ[index].queued
				&& /* waiting? */!contexts[context_id].LSQ[index].issued
				&& /* completed? */!contexts[context_id].LSQ[index].completed
				&& /* regs ready? */all_operands_spec_ready(&contexts[context_id].LSQ[index]))
			{
				//no STA unknown conflict (because we got to this check), check for a STD unknown conflict
				if(std_unknowns.find(contexts[context_id].LSQ[index].addr)==std_unknowns.end())
				{
					//no STA or STD unknown conflicts, put load on ready queue
					readyq_enqueue(&contexts[context_id].LSQ[index]);
				}
				//else, found a relevant STD unknown? Do nothing.

			}
		}
	}
}

//takes instructions out of the issue_exec_q and begins their execution schedules a writeback event
void execute(unsigned int core_num)
{
	ROB_entry *rs;
	//service all completed events
	while((rs = cores[core_num].issue_exec_q_next_event(sim_cycle)))
	{
		int context_id = rs->context_id;
		if(!all_operands_ready(rs))
		{
			//If operands are not ready, we incorrectly scheduled speculatively in regards
			//to a load, handle memory misprediction here
			if(cores[core_num].recovery_model_v==core_t::RECOVERY_MODEL_SQUASH)
			{
				//This is the memory mispredicted instruction. Adjust its source register ready times
				reg_set my_regs;
				cores[core_num].reg_file.get_reg_set(&my_regs, rs->op);
	    
				if((rs->src_physreg[0] > 0)&&(my_regs.src1!=REG_NONE))
				{
					cores[core_num].reg_file.reg_file_access(rs->src_physreg[0],my_regs.src1).spec_ready = cores[core_num].reg_file.reg_file_access(rs->src_physreg[0],my_regs.src1).ready - cores[core_num].ISSUE_EXEC_DELAY;
				}
				if((rs->src_physreg[1] > 0)&&(my_regs.src2!=REG_NONE))
				{
					cores[core_num].reg_file.reg_file_access(rs->src_physreg[1],my_regs.src2).spec_ready = cores[core_num].reg_file.reg_file_access(rs->src_physreg[1],my_regs.src2).ready - cores[core_num].ISSUE_EXEC_DELAY;
				}

				std::list<RS_link>::iterator it = cores[core_num].issue_exec_queue.begin();
				while(rs)
				{
					if((rs->physreg >= 0) && (!rs->ea_comp) && (rs->dest_format!=REG_NONE))
					{	//reset REG counters....
						cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).spec_ready = INF;
						cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).ready = INF;
					}
					//mark it as not issued
					rs->issued = FALSE;
					rs->replayed = TRUE;
	    
					assert(!rs->queued);
					assert(!rs->completed);

					//for LSQ entries, don't worry...
					if((MD_OP_FLAGS(rs->op) & F_LOAD) != F_LOAD)
					{
						//QUEUE IT TO WAKE UP AT THE RIGHT TIME!!
						cores[core_num].wait_q_enqueue(rs, sim_cycle);
					}

					rs = NULL;
					while((it!=cores[core_num].issue_exec_queue.end()) && !rs)
					{
						if((*it).rs->context_id == context_id)
						{
							rs = (*it).rs;
							it = cores[core_num].issue_exec_queue.erase(it);
						}
						else
						{
							it++;
						}
					}
				}

				//FLUSH THE READY QUEUE AS WELL
				//visit all ready instructions (i.e., insts whose register input
				//	dependencies have been satisfied, stop issue when no more instructions
				//	are available or issue bandwidth is exhausted
				std::list<RS_link>::iterator it2 = cores[core_num].ready_queue.begin();
				while(it2!=cores[core_num].ready_queue.end())
				{
					rs = (*it2).rs;
					if(rs->context_id!=context_id)
					{
						it2++;
						continue;
					}
					rs->queued = FALSE;
					rs->replayed = TRUE;
					if(rs->completed)
					{
						md_print_insn(rs->IR, rs->PC, stdout);
						printf("\n");
					}
					assert(!rs->completed);
					if((MD_OP_FLAGS(rs->op) & F_LOAD) != F_LOAD)
					{
						cores[core_num].wait_q_enqueue(rs, sim_cycle);
					}
					it2 = cores[core_num].ready_queue.erase(it2);
				}
				continue;
			}
			else
			{
				assert(FALSE);
			}
		}

		//Execution is valid at this point, send to writeback at execution finishes (rs->exec_lat cycles)
		assert(all_operands_ready(rs));
		cores[core_num].eventq_queue_event(rs, sim_cycle + rs->exec_lat, sim_cycle);

		//remove from IQ, if applicable
		if(rs->in_IQ)
		{
			cores[core_num].iq.free_iq_entry(rs->iq_entry_num);
			rs->in_IQ = false;
			contexts[rs->context_id].icount--;
			assert(contexts[rs->context_id].icount <= (contexts[rs->context_id].IFQ.size() + contexts[context_id].ROB.size()));
			/************ DCRA ************/
			contexts[rs->context_id].DCRA_int_iq--;
			assert(contexts[rs->context_id].DCRA_int_iq >= 0);
			/******************************/
		}
	}
}

//checks if an instructions operand is marked as ready
int operand_ready(ROB_entry *rs, int op_num){
	reg_set my_regs;
	enum reg_type src_type = REG_NONE;

	//see if it is a floating point or integer register
	cores[contexts[rs->context_id].core_id].reg_file.get_reg_set(&my_regs, rs->op);
	if(op_num == 0){
		src_type = my_regs.src1;
	}else{
		src_type = my_regs.src2;
	}

	if((rs->src_physreg[op_num] >= 0) && (src_type!=REG_NONE))
	{
		return (cores[contexts[rs->context_id].core_id].reg_file.reg_file_access(rs->src_physreg[op_num],src_type).ready <= sim_cycle);
	}
	return TRUE;
}

//checks if an instructions operand is marked as ready (speculative based on load-latency prediction)
int operand_spec_ready(ROB_entry *rs, int op_num){
	reg_set my_regs;
	enum reg_type src_type = REG_NONE;

	cores[contexts[rs->context_id].core_id].reg_file.get_reg_set(&my_regs, rs->op);

	if(op_num == 0){
		src_type = my_regs.src1;
	}else{
		src_type = my_regs.src2;
	}

	if((rs->src_physreg[op_num] >= 0) && (src_type!=REG_NONE))
	{
		return (cores[contexts[rs->context_id].core_id].reg_file.reg_file_access(rs->src_physreg[op_num],src_type).spec_ready <= sim_cycle);
	}
	return TRUE;
}

//checks if an instruction is ready to issue
int all_operands_ready(ROB_entry *rs){
	if(rs->ea_comp){
		//for address computations, we only need to check the second operand (operand 1), which determines the address
		return operand_ready(rs, 1);
	}
	//for all other instructions, check both source operands
	return (operand_ready(rs, 0) && operand_ready(rs, 1));
}

//checks if an instruction has exactly one ready operand
int one_operand_ready(ROB_entry *rs){
	return (operand_ready(rs, 0) || operand_ready(rs, 1));
}

//checks if an instruction is ready to issue (speculatively based on load-latency prediction)
int all_operands_spec_ready(ROB_entry *rs){
	if(rs->ea_comp){
		return operand_spec_ready(rs, 1);
	}
	return (operand_spec_ready(rs, 0) && operand_spec_ready(rs, 1));
}

//wakeup() - moves instructions from the waiting_queue to the ready_queue when their source operands become ready
void wakeup(unsigned int core_num)
{
	//(*it).rs refers to the instructions within the ROB/LSQ that we are trying to wakeup
	for(std::list<RS_link>::iterator it = cores[core_num].waiting_queue.begin();it!=cores[core_num].waiting_queue.end();)
	{
		if(all_operands_spec_ready((*it).rs))
		{
			//instruction ready, so move it to the ready_queue
			assert((!(*it).rs->queued)&&(!(*it).rs->completed));
			readyq_enqueue((*it).rs);
			it = cores[core_num].waiting_queue.erase(it);
		}
		else
		{
			(*it).x.when = sim_cycle;
			it++;
		}
	}
}

// selection() - instruction selection and issue to functional units
//attempt to issue all operations in the ready queue; insts in the ready instruction queue have all
//register dependencies satisfied, this function must then
//1) ensure the instructions memory dependencies have been satisfied (see lsq_refresh() for details on this process) and
//2) a function unit is available in this cycle to commence execution of the operation;
//if all goes well, the function unit is allocated, a writeback event is scheduled,
//and the instruction begins execution
void selection(unsigned int core_num)
{
	//visit all ready instructions (i.e., insts whose register input
	//	dependencies have been satisfied, stop issue when no more instructions
	//	are available or issue bandwidth is exhausted
	std::list<RS_link>::iterator it = cores[core_num].ready_queue.begin();
	for(unsigned int n_issued=0;(it!=cores[core_num].ready_queue.end()) && (n_issued < cores[core_num].issue_width);)
	{
		//only issue after the minimum # of cycles has elapsed
		//	NOTE: the current implementation assumes that the readyq is sorted by the
		//	issue_cycle. If this is changed, then it is not sufficient to simply break here!!
		int load_lat = 0, tlb_lat = 0;

		ROB_entry *rs = (*it).rs;
		mem_t* mem = contexts[rs->context_id].mem;

		//issue operation, both reg and mem deps have been satisfied
		if(!all_operands_spec_ready(rs) || !rs->queued || rs->issued || (!(MD_OP_FLAGS(rs->op) & F_STORE) && rs->completed))
		{
			std::cerr << "At cycle: " << sim_cycle << std::endl;
			std::cerr << "Core: " << core_num << " context: " << rs->context_id << ":" << contexts[rs->context_id].filename << std::endl;
			md_print_insn(rs->IR, rs->PC, stderr);
			if(!all_operands_spec_ready(rs))
				std::cerr << "\tissued inst not spec ready" << std::endl;
			if(!rs->queued)
				std::cerr << "\tissued inst not queued" << std::endl;
			if(rs->issued)
				std::cerr << "\tissued inst issued" << std::endl;
			if((!(MD_OP_FLAGS(rs->op) & F_STORE) && rs->completed))
				std::cerr << "\tissued inst completed (not a store)" << std::endl;
			assert(0);
		}
		//Wattch -- access window selection logic
		cores[core_num].power.window_selection_access++;

		//node is now un-queued
		rs->queued = FALSE;

		if(rs->in_LSQ && ((MD_OP_FLAGS(rs->op) & (F_MEM|F_STORE)) == (F_MEM|F_STORE)))
		{
			//stores complete in effectively zero time, result is written into the load/store queue, the actual store into
			//the memory system occurs when the instruction is retired (see commit())
			//This acts as if there is no write buffer
			rs->issued = TRUE;
			rs->completed = TRUE;

			//If the store has a physical register (Store Conditional), make the register as written to.
			//This is too early, but isn't checked until commit (when it would be ok anyway).
			if(rs->physreg >= 0)
			{
				//Set ready times
				cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).spec_ready = sim_cycle + rs->exec_lat;
				cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).ready = sim_cycle + rs->exec_lat + cores[core_num].ISSUE_EXEC_DELAY;

				assert(cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).state==REG_ALLOC);
				cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).state = REG_WB;
			}

			if(rs->recover_inst)
			{
				std::cerr << "mispredicted store" << std::endl;
				assert(0);
			}

			//entered execute stage, indicate in pipe trace
			ptrace_newstage(rs->ptrace_seq, PST_WRITEBACK, 0);

			//Wattch -- LSQ access -- write data into store buffer
			cores[core_num].power.lsq_access++;
			cores[core_num].power.lsq_store_data_access++;
			cores[core_num].power.lsq_preg_access++;
#ifdef DYNAMIC_AF
			cores[core_num].power.lsq_total_pop_count_cycle += pop_count(rs->val_ra);
			cores[core_num].power.lsq_num_pop_count_cycle++;
#endif
			//one more inst issued
			n_issued++;
			it = cores[core_num].ready_queue.erase(it);
		}
		else
		{
			//issue the instruction to a functional unit
			if(MD_OP_FUCLASS(rs->op) != NA)
			{
				res_template *fu = res_get(cores[core_num].fu_pool, MD_OP_FUCLASS(rs->op));
				if(fu)
				{
					//got one! issue inst to functional unit
					rs->issued = TRUE;

					//reserve the functional unit
					if(fu->master->busy)
					{
						std::cerr << "functional unit already in use" << std::endl;
						assert(0);
					}

					//schedule functional unit release event
					fu->master->busy = fu->issuelat;

					//schedule a result writeback event
					if(rs->in_LSQ && ((MD_OP_FLAGS(rs->op) & (F_MEM|F_LOAD)) == (F_MEM|F_LOAD)))
					{
						int events = 0;

						//Wattch -- LSQ access
						cores[core_num].power.lsq_access++;
						cores[core_num].power.lsq_wakeup_access++;

						//for loads, determine cache access latency: first scan LSQ to see if a store forward is
						//possible, if not, access the data cache
						load_lat = 0;
						for(unsigned int i = (rs - &contexts[rs->context_id].LSQ[0]);i!=contexts[rs->context_id].LSQ_head;)
						{
							//go to next earlier LSQ entry
							i = (i + (contexts[rs->context_id].LSQ.size()-1)) % contexts[rs->context_id].LSQ.size();

							//FIXME: not dealing with partials!
							//Aside from the destination address (which is known) we need to know how many bytes are stored
							//and how many bytes are being read
							if((MD_OP_FLAGS(contexts[rs->context_id].LSQ[i].op) & F_STORE) && (contexts[rs->context_id].LSQ[i].addr == rs->addr))
							{
								//hit in the LSQ
								load_lat = 1;
								break;
							}
				  		}

						//was the value store forwared from the LSQ?
						if(!load_lat)
						{
							int valid_addr = MD_VALID_ADDR(rs->addr);

							if(!rs->spec_mode && !valid_addr)
							{
								sim_invalid_addrs++;
							}

							//no! go to the data cache if addr is valid
							if(cores[core_num].cache_dl1 && valid_addr)
							{
								//Wattch -- D-cache access
								cores[core_num].power.dcache_access++;

								//access the cache if non-faulting
								load_lat = cores[core_num].cache_dl1->cache_access(Read,(rs->addr & ~3), rs->context_id, NULL, 4, sim_cycle, NULL, NULL);

								if(load_lat > cores[core_num].cache_dl1_lat)
								{
									events |= PEV_CACHEMISS;
									rs->L1_miss = 1;
									if(load_lat > cores[core_num].cache_dl2_lat)
									{
										rs->L2_miss = 1;
										if(cache_dl3 && (load_lat > cache_dl3_lat))
										{
											rs->L3_miss = 1;
										}
									}
								}
							}
							else
							{
								//no caches defined, just use op latency
								load_lat = fu->oplat;
							}
						}

						//all loads and stores must access D-TLB
						if(cores[core_num].dtlb && MD_VALID_ADDR(rs->addr))
						{
							//access the D-DLB, NOTE: this code will initiate speculative TLB misses
							tlb_lat = cores[core_num].dtlb->cache_access(Read, (rs->addr & ~3),rs->context_id, NULL, 4, sim_cycle, NULL, NULL);
							if(tlb_lat > 1)
							{
								events |= PEV_TLBMISS;
							}

							//D-cache/D-TLB accesses occur in parallel
							load_lat = MAX(tlb_lat, load_lat);
						}

						//use computed cache access latency
						rs->exec_lat = load_lat;
			  
						issue_exec_q_queue_event(rs, sim_cycle + cores[core_num].ISSUE_EXEC_DELAY);

						if(rs->physreg >= 0)
						{
							int pred_hit_L1 = FALSE;
							if(cores[core_num].recovery_model_v==core_t::RECOVERY_MODEL_SQUASH)
							{
								//DO LOAD LATENCY PREDICTION
								int stack_recover_idx(0);
								bpred_update_t dir_update;	//bpred direction update info
								enum md_opcode op = BNE;

								if(rs->addr)
								{
									pred_hit_L1 = contexts[rs->context_id].load_lat_pred->bpred_lookup(
										/* branch address */rs->addr, //CHANGE THIS TO CACHE TARGET???
										/* target address */ 0,
										/* opcode */ op,
										/* call? */ FALSE,
										/* return? */ FALSE,
										/* updt */&(dir_update),
										/* RSB index */&stack_recover_idx);
									assert(rs->addr);
									contexts[rs->context_id].load_lat_pred->bpred_update(
										/* branch address */rs->addr,
										/* actual target address */pred_hit_L1,
										/* taken? */ (rs->exec_lat <= cores[core_num].cache_dl1_lat),
										/* pred taken? */ pred_hit_L1,
										/* correct pred? */ ((rs->exec_lat <= cores[core_num].cache_dl1_lat) == pred_hit_L1),
										/* opcode */op,
										/* predictor update ptr */&(dir_update));
								}
								else
								{
									pred_hit_L1 = 0;
								}
							}

							/********** DCRA *************/
							if((rs->exec_lat > cores[core_num].cache_dl1_lat) && (rs->exec_lat > contexts[rs->context_id].DCRA_L1_misses))
							{
								contexts[rs->context_id].DCRA_L1_misses = rs->exec_lat;
							}
							/*****************************/
							//wakeup dependents
							if(rs->dest_format!=REG_NONE)
							{
								assert(rs->physreg >= 0);
								physreg_t *target = &(cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format));
								//LOAD PREDICTION HERE!!!!!
								//earliest cycle dependents should wakeup
								if(cores[core_num].recovery_model_v==core_t::RECOVERY_MODEL_SQUASH)
								{
									target->spec_ready = sim_cycle + (pred_hit_L1 ? cores[core_num].cache_dl1_lat : (rs->exec_lat + cores[core_num].ISSUE_EXEC_DELAY));
								}
								else
								{
									target->spec_ready = sim_cycle + rs->exec_lat;
								}
								//data will be on the bypass network when this instruction completes execution
								target->ready = sim_cycle + rs->exec_lat + cores[core_num].ISSUE_EXEC_DELAY;
							}
						}
						//entered execute stage, indicate in pipe trace
						ptrace_newstage(rs->ptrace_seq, PST_EXECUTE,((rs->ea_comp ? PEV_AGEN : 0) | events));
					}
					else	//!load && !store
					{
						//Wattch -- ALU access Wattch-FIXME
						//	(different op types)
						//	also spread out power of multi-cycle ops
						cores[core_num].power.alu_access++;

						if((MD_OP_FLAGS(rs->op) & (F_FCOMP))== (F_FCOMP))
						{
							cores[core_num].power.falu_access++;
						}
						else
						{
							cores[core_num].power.ialu_access++;
						}

						//use deterministic functional unit latency
						rs->exec_lat = fu->oplat;
						issue_exec_q_queue_event(rs, sim_cycle + cores[core_num].ISSUE_EXEC_DELAY);
			  
						//memory accesses are woken up differently via the LSQ
						if((rs->physreg >= 0) && (!rs->ea_comp))
						{
							//wakeup dependents
							if(rs->dest_format!=REG_NONE)
							{
								assert(rs->physreg >= 0);
								//earliest cycle dependents should wakeup
								cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).spec_ready = sim_cycle + rs->exec_lat;
								//data will be on the bypass network when this instruction completes execution
								cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).ready = sim_cycle + rs->exec_lat + cores[core_num].ISSUE_EXEC_DELAY;
							}
						}
						//entered execute stage, indicate in pipe trace
						ptrace_newstage(rs->ptrace_seq, PST_EXECUTE, rs->ea_comp ? PEV_AGEN : 0);
					}
					//Wattch -- window access
					cores[core_num].power.window_access++;
					//read values from window send to FUs
					cores[core_num].power.window_preg_access++;
					cores[core_num].power.window_preg_access++;
#ifdef DYNAMIC_AF	
					cores[core_num].power.window_total_pop_count_cycle += pop_count(rs->val_ra) + pop_count(rs->val_rb);
					cores[core_num].power.window_num_pop_count_cycle+=2;
#endif
					//one more inst issued
					n_issued++;
					it = cores[core_num].ready_queue.erase(it);
				}
				else	//no functional unit
				{
					//insufficient functional unit resources, leave operation in ready_queue, we'll try to issue it again next cycle
					rs->queued = TRUE;
					it++;
				}
			}
			else	//does not require a functional unit!
			{
				//FIXME: need better solution for these the instruction does not need a functional unit
				rs->issued = TRUE;
		  
				//schedule a result event
				rs->exec_lat = 1;
				issue_exec_q_queue_event(rs, sim_cycle + cores[core_num].ISSUE_EXEC_DELAY);
		  
				//wakeup dependents
				if(rs->dest_format!=REG_NONE)
				{
					assert(rs->physreg >= 0);
					//earliest cycle dependents should wakeup
					cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).spec_ready = sim_cycle + rs->exec_lat;
					//data will be on the bypass network when this instruction completes execution
					cores[core_num].reg_file.reg_file_access(rs->physreg,rs->dest_format).ready = sim_cycle + rs->exec_lat + cores[core_num].ISSUE_EXEC_DELAY;
				}
				//entered execute stage, indicate in pipe trace
				ptrace_newstage(rs->ptrace_seq, PST_EXECUTE, rs->ea_comp ? PEV_AGEN : 0);

				//Wattch -- Window access
				cores[core_num].power.window_access++;
				//read values from window send to FUs
				cores[core_num].power.window_preg_access++;
				cores[core_num].power.window_preg_access++;
#ifdef DYNAMIC_AF
				cores[core_num].power.window_total_pop_count_cycle += pop_count(rs->val_ra) + pop_count(rs->val_rb);
				cores[core_num].power.window_num_pop_count_cycle+=2;
#endif
				//one more inst issued
				n_issued++;
				it = cores[core_num].ready_queue.erase(it);
			}
		}
	}
}

//routines for generating on-the-fly instruction traces with support for control and data misspeculation modeling

//default memory state accessor, used by DLite
//Returns an error string (or NULL for no error)
const char * simoo_mem_obj(mem_t *mem,		//memory space to access
	int is_write,				//access type
	md_addr_t addr,				//address to access
	char *p,				//input/output buffer
	int nbytes)				//size of access
{
	enum mem_cmd cmd;

	if(!is_write)
		cmd = Read;
	else
		cmd = Write;
#if 0
	char *errstr = mem_valid(cmd, addr, nbytes, /* declare */FALSE);
	if(errstr)
		return errstr;
#endif
	//else, no error, access memory
	mem->mem_access(cmd, addr, p, nbytes);

	//no error
	return NULL;
}

//RENAME() - decode instructions, rename the registers, and allocate ROB and LSQ resources

//configure the instruction decode engine
#define DNA			(0)

//general register dependence decoders, $r31 maps to DNA (0)
#define DGPR(N)			(31 - (N))		//was: (((N) == 31) ? DNA : (N))

//floating point register dependence decoders
#define DFPR(N)			(63 - (N))

//miscellaneous register dependence decoders
#define DFPCR			(0+32+32)
#define DUNIQ			(1+32+32)
#define DTMP			(2+32+32)

//configure the execution engine
//next program counter
#define SET_NPC(EXPR)		(regs->regs_NPC = (EXPR))

//target program counter
#undef  SET_TPC
#define SET_TPC(EXPR)		(target_PC = (EXPR))

//current program counter
#define CPC			(regs->regs_PC)
#define SET_CPC(EXPR)		(regs->regs_PC = (EXPR))

//general purpose register accessors
#define GPR(N)			regs->regs_R[N]
#define SET_GPR(N,EXPR)		(regs->regs_R[N] = (EXPR))

//floating point register accessors
#define FPR_Q(N)		regs->regs_F[(N)].q
#define SET_FPR_Q(N,EXPR)	(regs->regs_F[(N)].q = (EXPR))
#define FPR(N)			regs->regs_F[(N)].d
#define SET_FPR(N,EXPR)		(regs->regs_F[(N)].d = (EXPR))

//miscellanous register accessors
#define FPCR			regs->regs_C.fpcr
#define SET_FPCR(EXPR)		(regs->regs_C.fpcr = (EXPR))
#define UNIQ			regs->regs_C.uniq
#define SET_UNIQ(EXPR)		(regs->regs_C.uniq = (EXPR))
#define FCC			regs->regs_C.fcc
#define SET_FCC(EXPR)		(regs->regs_C.fcc = (EXPR))

//precise architected memory state accessor macros
#define __READ_SPECMEM(SRC, SRC_V, FAULT)				\
	(addr = (SRC), ((FAULT) = mem->mem_access(Read, addr, &SRC_V, sizeof(SRC_V))), SRC_V)

#define READ_BYTE(SRC, FAULT)		__READ_SPECMEM((SRC), temp_byte, (FAULT))
#define READ_HALF(SRC, FAULT)		MD_SWAPH(__READ_SPECMEM((SRC), temp_half, (FAULT)))
#define READ_WORD(SRC, FAULT)		MD_SWAPW(__READ_SPECMEM((SRC), temp_word, (FAULT)))
#define READ_QWORD(SRC, FAULT)		MD_SWAPQ(__READ_SPECMEM((SRC), temp_qword, (FAULT)))

#define __WRITE_SPECMEM(SRC, DST, DST_V, FAULT)					\
	(DST_V = (SRC), addr = (DST),						\
	((FAULT) = mem->mem_access(Write, addr, &DST_V, sizeof(DST_V))))

#define WRITE_BYTE(SRC, DST, FAULT)	__WRITE_SPECMEM((SRC), (DST), temp_byte, (FAULT))
#define WRITE_HALF(SRC, DST, FAULT)	__WRITE_SPECMEM(MD_SWAPH(SRC), (DST), temp_half, (FAULT))
#define WRITE_WORD(SRC, DST, FAULT)	__WRITE_SPECMEM(MD_SWAPW(SRC), (DST), temp_word, (FAULT))
#define WRITE_QWORD(SRC, DST, FAULT)	__WRITE_SPECMEM(MD_SWAPQ(SRC), (DST), temp_qword, (FAULT))

//system call handler macro - only execute system calls in non-speculative mode
#define SYSCALL(INST)			((spec_mode ? panic("speculative syscall at %lld",sim_cycle) : (void) 0),	sys_syscall(regs, mem, INST))
#define PALCALL(INST)			((spec_mode ? panic("speculative palcall at %lld",sim_cycle) : (void) 0),	sys_palcall(regs, mem, INST))

//default register state accessor, used by DLite
//Returns an error string (or NULL for no error)
const char * simoo_reg_obj(regs_t *regs,	//registers to access
	int is_write,				//access type
	enum md_reg_type rt,			//reg bank to probe
	int reg,				//register number
	eval_value_t *val)			//input, output
{
	switch(rt)
	{
	case rt_gpr:
		if(reg < 0 || reg >= MD_NUM_IREGS)
			return "register number out of range";
		if(!is_write)
		{
			val->type = et_uint;
			val->value.as_uint = GPR(reg);
		}
		else
			SET_GPR(reg, eval_as<unsigned int>(*val));
		break;
	case rt_lpr:
		if(reg < 0 || reg >= MD_NUM_FREGS)
			return "register number out of range";
		//FIXME: this is not portable...
		abort();
#if 0
		if(!is_write)
		{
			val->type = et_uint;
			val->value.as_uint = FPR_L(reg);
		}
		else
			SET_FPR_L(reg, eval_as<unsigned int>(*val));
#endif
		break;
	case rt_fpr:
		//FIXME: this is not portable...
		abort();
#if 0
		if(!is_write)
			val->value.as_float = FPR_F(reg);
		else
			SET_FPR_F(reg, val->value.as_float);
#endif
		break;
	case rt_dpr:
		//FIXME: this is not portable...
		abort();
#if 0
		//1/2 as many regs in this mode
		if(reg < 0 || reg >= MD_NUM_REGS/2)
			return "register number out of range";
		if(at == at_read)
			val->as_double = FPR_D(reg * 2);
		else
			SET_FPR_D(reg * 2, val->as_double);
#endif
		break;
		//FIXME: this is not portable...
#if 0
		abort();
	case rt_hi:
		if(at == at_read)
			val->as_word = HI;
		else
			SET_HI(val->as_word);
		break;
	case rt_lo:
		if(at == at_read)
			val->as_word = LO;
		else
			SET_LO(val->as_word);
		break;
	case rt_FCC:
		if(at == at_read)
			val->as_condition = FCC;
		else
			SET_FCC(val->as_condition);
		break;
#endif
	case rt_PC:
		if(!is_write)
		{
			val->type = et_addr;
			val->value.as_addr = regs->regs_PC;
		}
		else
			regs->regs_PC = eval_as<md_addr_t>(*val);
		break;
	case rt_NPC:
		if(!is_write)
		{
			val->type = et_addr;
			val->value.as_addr = regs->regs_NPC;
		}
		else{
			printf("eval as addr...\n");
			regs->regs_NPC = eval_as<md_addr_t>(*val);
		}
		break;
	default:
		panic("bogus register bank");
	}
	//no error
	return NULL;
}

//counts the number of instructions in the rename->dispatch pipeline for a particular thread
unsigned int not_dispatched_count(int context_id)
{
	unsigned int count = 0, num_searched = 0;

	//walk ROB, and count non-dispatched functions
	unsigned int ROB_index = contexts[context_id].ROB_head;
	while(num_searched<contexts[context_id].ROB_num)
	{
		if(!contexts[context_id].ROB[ROB_index].dispatched)
		{
			count++;
		}

		ROB_index = (ROB_index + 1) % contexts[context_id].ROB.size();
		num_searched++;
	}
	return count;
}

double last_line()
{
  const std::string filename = "action.csv";
  std::ifstream fs;
  fs.open(filename.c_str(), std::fstream::in);
  if(fs.is_open())
  {
    //Got to the last character before EOF
    fs.seekg(-1, std::ios_base::end);
    if(fs.peek() == '\n')
    {
      //Start searching for \n occurrences
      fs.seekg(-1, std::ios_base::cur);
      int i = fs.tellg();
      for(i;i > 0; i--)
      {
        if(fs.peek() == '\n')
        {
          //Found
          fs.get();
          break;
        }
        //Move one character back
        fs.seekg(i, std::ios_base::beg);
      }
    }
    std::string lastline;
    getline(fs, lastline);
    std::cout << lastline << std::endl;
	double lastLine = atof(lastline.c_str());
	return lastLine;
  }
  else
  {
    std::cout << "Could not find end line character" << std::endl;
  }

}

//rename instructions from the IFETCH -> RENAME queue: instructions are
//	first decoded, then they allocated ROB (and LSQ for load/stores) resources
//	and then their registers are renamed
std::tuple<unsigned int, unsigned int,unsigned int, unsigned int, unsigned int> register_rename(unsigned int core_num, unsigned int j)
{
	int made_check(FALSE);				//used to ensure DLite entry

	//Threads left to rename from
	std::vector<int> contexts_left(cores[core_num].context_ids);
	//std::cout << "Number of renamed register " << contexts_left(cores[core_num].context_ids) << std::endl;
	//Current context to rename from (by index). Utilizing round robin.
	unsigned int current_context = sim_cycle % contexts_left.size();

	//How many insts have been renameed? Can we rename (decode bandwidth) any more?
	unsigned int n_renamed(0);
	unsigned int a;
	unsigned int state[5] = {0,0,0,0,0};
	unsigned int n_renamed_threads_0 = 0, n_renamed_threads_1 = 0, n_renamed_threads_2 = 0, n_renamed_threads_3 = 0;
	//unsigned int j = 0;
	//std::cout << "Number of renamed register " << n_renamed << std::endl;
	if (j%1000==0)
		{cores[core_num].decode_width = (rand()%(32-0+1))+0;}
	if (j%1000==0)
		{cores[core_num].decode_width = 10*int(last_line());}
	//std::cout << "Number of decode_width " << cores[core_num].decode_width << std::endl;
	//std::cout << "Current context: " << n_renamed_threads[0]<<n_renamed_threads[1]<<n_renamed_threads[2]<<n_renamed_threads[3] << std::endl;
	while(n_renamed < cores[core_num].decode_width)
	{
		//These are the major variables used here. They are not defined here but the code is left here to explain the variables
		//md_inst_t inst;				//actual instruction bits
		enum md_opcode op(MD_NOP_OP);			//decoded opcode enum
		int out1(0), out2(0), in1(0), in2(0), in3(0);	//output/input register names
		
		md_addr_t target_PC(0);				//actual next/target PC address
		//md_addr_t addr(0);				//effective address, if load/store
		ROB_entry *rs(NULL);				//ROB entry being allocated
		ROB_entry *lsq(NULL);				//LSQ entry for ld/st's
		//unsigned long long pseq;			//pipetrace sequence number
		//int is_write(0);				//store?
		//int br_taken, br_pred_taken;			//if br, taken?  predicted taken?
		//enum md_fault_type fault(md_fault_none);	//indicates if a fault has occured while decoding an instruction
		//mem_t* mem(NULL);				//the memory space for the thread currently being examined
		//regs_t* regs(NULL);				//the arch register set for the thread currently being examined
		//quad_t val_ra, val_rb, val_rc, val_ra_result;	//Wattch:  Added for pop count generation (AFs)

		//temp variables for spec mem access
		byte_t temp_byte(0);
		half_t temp_half(0);
		word_t temp_word(0);
		qword_t temp_qword(0);

		//If there are no contexts left, we are done.
		if(contexts_left.empty())
			break;

		//Get the context_id for a thread to rename from. Ensure that current_context is within bounds.
		current_context%=contexts_left.size();
		int disp_context_id = contexts_left[current_context];
		
		if(contexts[current_context].interrupts)
		{
			//Executable is waiting (wait4) for someone
			if(contexts[current_context].interrupts & 0x10000)
			{
				assert(contexts[current_context].waiting_for);
				if(!pid_handler.is_retval_avail(contexts[current_context].pid, contexts[current_context].waiting_for))
				{
					contexts_left.erase(contexts_left.begin()+current_context);
					continue;
				}
				contexts[current_context].waiting_for = 0;
				contexts[current_context].interrupts &= ~0x10000;
			}

			//Executable has a select pending
			if(contexts[current_context].interrupts & 0x20000)
			{
				contexts[current_context].next_check--;
				if(contexts[current_context].next_check>0)
				{
					contexts_left.erase(contexts_left.begin()+current_context);
					continue;
				}
				contexts[current_context].interrupts &= ~0x20000;
			}
		}

		//on an acceptable trace path?
		int spec_mode = contexts[disp_context_id].spec_mode;
		if(!cores[core_num].include_spec && contexts[disp_context_id].spec_mode)
		{
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//see if it is possible to rename from this thread
		if((contexts[disp_context_id].fetch_num == 0)
			|| (contexts[disp_context_id].ROB_num >= contexts[disp_context_id].ROB.size())		//check if the ROB is full
			|| (contexts[disp_context_id].LSQ_num >= contexts[disp_context_id].LSQ.size())		//check if the LSQ is full
			|| ((contexts[disp_context_id].IFQ[contexts[disp_context_id].fetch_head].fetched_cycle + cores[core_num].FETCH_RENAME_DELAY) > sim_cycle))	//enforce the fetch to rename delay
			{
				//Not possible, this thread is not eligible this cycle
				contexts_left.erase(contexts_left.begin()+current_context);
				continue;
			}

		//only permit decode_width*RENAME_DISPATCH_DELAY instructions in the rename->dispatch pipeline
		if(not_dispatched_count(disp_context_id) >= (cores[core_num].RENAME_DISPATCH_DELAY ? cores[core_num].decode_width*cores[core_num].RENAME_DISPATCH_DELAY : cores[core_num].decode_width))
		{
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//if issuing in-order, block until last op issues if inorder issue
		if(cores[core_num].inorder_issue && (contexts[disp_context_id].last_op.rs && !all_operands_ready(contexts[disp_context_id].last_op.rs)))
		{
			//If last_op is no longer valid, ignore this
			if(contexts[disp_context_id].ROB_num != 0)
			{
				unsigned int index = (contexts[disp_context_id].last_op.rs - &contexts[disp_context_id].ROB[0]);
				unsigned int upper = contexts[disp_context_id].ROB_head + contexts[disp_context_id].ROB_num - 1;
				if(index<contexts[disp_context_id].ROB_head)
				{
					index+=contexts[disp_context_id].ROB.size();
				}
				if((index >= contexts[disp_context_id].ROB_head) && (index <= upper))
				{
					//stall until last operation is ready to issue
					contexts_left.erase(contexts_left.begin()+current_context);
					continue;
				}
			}
			contexts[disp_context_id].last_op = RS_link((ROB_entry *)NULL);
		}

		//get the next instruction from the IFETCH -> RENAME queue
		mem_t *mem = contexts[disp_context_id].mem;
		regs_t *regs = &contexts[disp_context_id].regs;
		md_inst_t inst = contexts[disp_context_id].IFQ[contexts[disp_context_id].fetch_head].IR;
		regs->regs_PC = contexts[disp_context_id].IFQ[contexts[disp_context_id].fetch_head].regs_PC;
		contexts[disp_context_id].pred_PC = contexts[disp_context_id].IFQ[contexts[disp_context_id].fetch_head].pred_PC;

		//decode the inst
		MD_SET_OPCODE(op, inst);

		reg_set my_regs;
		cores[core_num].reg_file.get_reg_set(&my_regs, op);
		//make sure we have a free physical register for the destination
		if(my_regs.dest != REG_NONE)
		{
			if(cores[core_num].reg_file.find_free_physreg(my_regs.dest) < 0)
			{
				//stall because there are no physical registers free
				contexts_left.erase(contexts_left.begin()+current_context);
				continue;
			}
		}

		//compute default next PC
		regs->regs_NPC = regs->regs_PC + sizeof(md_inst_t);

		//drain ROB for TRAPs and system calls
		if(MD_OP_FLAGS(op) & F_TRAP)
		{
			//Must stall until all prior instructions are guaranteed not to generate exceptions
			if(contexts[disp_context_id].ROB_num != 0)
			{
				contexts_left.erase(contexts_left.begin()+current_context);
				continue;
			}
		}

		//maintain $r0 semantics
		regs->regs_R[MD_REG_ZERO] = 0;
		regs->regs_F[MD_REG_ZERO].d = 0.0;

		//one more instruction executed
		sim_num_insn++;
		cores[core_num].sim_num_insn_core++;

		//default effective address (0). Maintain a flag to determine if this is a store.
		md_addr_t addr = 0; 
		bool is_write = FALSE;

		//Wattch: Get values of source operands
		quad_t val_ra = GPR(RA);
		quad_t val_rb = GPR(RB);

		//Get information needed for rollback
		//Just copy it all, during rollback we can decide to use regs_R or regs_F.
		int Rlist[3] = {RA,RB,RC};
		qword_t regs_R[3] = {GPR(RA), GPR(RB), GPR(RC)};
		md_fpr_t regs_F[3] = {regs->regs_F[RA], regs->regs_F[RB], regs->regs_F[RC]};
		md_ctrl_t regs_C = regs->regs_C;
		size_t data_size = 0;

		//set default fault - none
		enum md_fault_type fault = md_fault_none;

		//If this is a store, retain the old memory data so that we can rollback.
		//This must be done before the "switch" below.
		//We only need data sizes for stores, loads always replace the full register on rollback
		qword_t previous_mem(0);
		if((MD_OP_FLAGS(op) & F_STORE) == F_STORE)
		{
			md_addr_t addr = (GPR(RB) + SEXT(OFS));

			//Should probably consider mem_access_direct for all previous_mem creations
			switch(op)
			{
			case STB:
				data_size = 1;
				mem->mem_access_direct(Read,addr,&previous_mem,1);
				break;
			case STW:
				data_size = 2;
				mem->mem_access_direct(Read,addr,&previous_mem,2);
				break;
			case STS:
			case STL:
			case STL_C:
				data_size = 4;
				mem->mem_access_direct(Read,addr,&previous_mem,4);
				break;
			case STQ_U:
				addr &= ~7;
			case STT:
			case STQ:
			case STQ_C:
				data_size = 8;
				mem->mem_access_direct(Read,addr,&previous_mem,data_size);
				break;
			default:
				fatal("Store without matching opcode!");
			}
		}

		//more decoding and execution
		switch(op)
		{
#define DEFINST(OP,MSK,NAME,OPFORM,RES,CLASS,O1,O2,I1,I2,I3)				\
		case OP:								\
			/* compute output/input dependencies to out1-2 and in1-3 */	\
			out1 = O1; out2 = O2;						\
			in1 = I1; in2 = I2; in3 = I3;					\
			/* execute the instruction */					\
			SYMCAT(OP,_IMPL);						\
			break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)							\
		case OP:								\
			/* could speculatively decode a bogus inst, convert to NOP */	\
			op = MD_NOP_OP;							\
			/* compute output/input dependencies to out1-2 and in1-3 */	\
			out1 = NA; out2 = NA;						\
			in1 = NA; in2 = NA; in3 = NA;					\
			/* no EXPR */							\
			break;
#define CONNECT(OP)	/* nada... */
			/* the following macro wraps the instruction fault declaration macro
				with a test to see if the trace generator is in non-speculative
				mode, if so the instruction fault is declared, otherwise, the
				error is shunted because instruction faults need to be masked on
				the mis-speculated instruction paths */
#define DECLARE_FAULT(FAULT)								\
			{								\
				if(!contexts[disp_context_id].spec_mode)		\
					fault = (FAULT);				\
				/* else, spec fault, ignore it, always terminate exec.. */	\
				break;							\
			}
#include "machine.def"
		default:
			//can speculatively decode a bogus inst, convert to a NOP
			op = MD_NOP_OP;
			//compute output/input dependencies to out1-2 and in1-3
			out1 = NA; out2 = NA;
			in1 = NA; in2 = NA; in3 = NA;
			//no EXPR
		}
		//operation sets next PC

		//If the instruction was fork, pointers within the context are no longer guaranteed. Fix them here
		mem = contexts[disp_context_id].mem;
		regs = &contexts[disp_context_id].regs;
		unsigned long long pseq = contexts[disp_context_id].IFQ[contexts[disp_context_id].fetch_head].ptrace_seq;

		//print retirement trace if in verbose mode
		if(!contexts[disp_context_id].spec_mode && verbose)
		{
			myfprintf(stderr, "+%d+ %10n [xor: 0x%08x] @ 0x%08p: ", disp_context_id, sim_num_insn, md_xor_regs(regs), regs->regs_PC);
			md_print_insn(inst, regs->regs_PC, stderr);
			std::cerr << std::endl;
		}

		if(fault != md_fault_none)
		{
			std::cerr << std::endl << "non-speculative fault (" << fault << ") detected" << std::endl;
			std::cerr << "Context: " << disp_context_id << "\tregs->regs_PC: 0x" << std::hex << regs->regs_PC << std::dec;
			std::cerr << "\tSpec_mode: " << spec_mode << "\tcxt.spec_mode: " << contexts[disp_context_id].spec_mode << std::endl;
			std::cerr << "Pre-execution values: " << std::endl;
			for(int i=0;i<3;i++)
			{
				std::cerr << "Register: 0x" << std::hex << Rlist[i] << "\t0x" << regs_R[i] << std::dec << "\tFloat (d/q)" << regs_F[i].d << "/" << regs_F[i].q << std::endl;
			}
			md_print_insn(inst, regs->regs_PC, stderr);
			std::cerr << "\nPost-execution values: " << std::endl;
			for(int i=0;i<3;i++)
			{
				std::cerr << "Register: 0x" << std::hex << Rlist[i] << "\t0x" << GPR(Rlist[i]) << std::dec << "\tFloat (d/q)" << regs->regs_F[Rlist[i]].d << "/" << regs->regs_F[Rlist[i]].q << std::endl;
			}
			std::cerr << "Target address: " << std::hex << addr << std::dec << std::endl;
			assert(0);
		}

		//Wattch: Get result values
#if defined(TARGET_ALPHA)
		quad_t val_rc = GPR(RC);
		quad_t val_ra_result = GPR(RA);
#endif

		//Did the instruction generate a pipe-flush/abort
		if(contexts[disp_context_id].interrupts & 0x10000000)
		{
			std::cerr << "Abort detected, clearing IFQ" << std::endl;
			//clear IFQ
			while(contexts[disp_context_id].fetch_num)
			{
				contexts[disp_context_id].icount--;
				contexts[disp_context_id].fetch_head = (contexts[disp_context_id].fetch_head+1) & (contexts[disp_context_id].IFQ.size()-1);
				contexts[disp_context_id].fetch_num--;
			}
			assert(contexts[disp_context_id].icount <= (contexts[disp_context_id].IFQ.size() + contexts[disp_context_id].ROB.size()));
			contexts[disp_context_id].fetch_regs_PC = contexts[disp_context_id].fetch_pred_PC = regs->regs_PC += 4;

			contexts[disp_context_id].interrupts &= ~0x10000000;
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//update memory access stats
		if(MD_OP_FLAGS(op) & F_MEM)
		{
			cores[core_num].sim_total_refs++;
			if(!contexts[disp_context_id].spec_mode)
				cores[core_num].sim_num_refs++;
			if (MD_OP_FLAGS(op) & F_STORE)
				is_write = TRUE;
			else
			{
				cores[core_num].sim_total_loads++;
				if(!contexts[disp_context_id].spec_mode)
					cores[core_num].sim_num_loads++;
			}
		}

		//Was this branch predicted incorrectly and we are using a perfect predictor?
		//	or, the target address of the branch doesn't match
		//If so, we may need to redirect the fetch to the correct instructions
		//In this case, speculatively fetched instructions are tainted and must be flushed from the IFQ
		//bool br_taken = (regs->regs_NPC != (regs->regs_PC + sizeof(md_inst_t)));
		bool br_pred_taken = (contexts[disp_context_id].pred_PC != (regs->regs_PC + sizeof(md_inst_t)));
		bool fetch_redirected(FALSE);

		if((contexts[disp_context_id].pred_PC != regs->regs_NPC && cores[core_num].pred_perfect)
			|| ((MD_OP_FLAGS(op) & (F_CTRL|F_DIRJMP)) == (F_CTRL|F_DIRJMP)
			&& target_PC != contexts[disp_context_id].pred_PC && br_pred_taken))
		{
			//Either 1) we're simulating perfect prediction and are in a
			//	mis-predict state and need to patch up, or 2) We're not simulating
			//	perfect prediction, we've predicted the branch taken, but our
			//	predicted target doesn't match the computed target (i.e.,
			//	mis-fetch).  Just update the PC values and do a fetch squash.
			//	This is just like calling fetch_squash() except we pre-anticipate
			//	the updates to the fetch values at the end of this function.  If
			//	case #2, also charge a mispredict penalty for redirecting fetch
			contexts[disp_context_id].fetch_pred_PC = contexts[disp_context_id].fetch_regs_PC = regs->regs_NPC;
			if(cores[core_num].pred_perfect)
				contexts[disp_context_id].pred_PC = regs->regs_NPC;
			fetch_redirected = TRUE;
		}

		if(op != MD_NOP_OP)
		{
			/* for load/stores:
				idep #0     - store operand (value that is store'ed)
				idep #1, #2 - eff addr computation inputs (addr of access)
			resulting ROB/LSQ operation pair:
			ROB (effective address computation operation):
				idep #0, #1 - eff addr computation inputs (addr of access)
			LSQ (memory access operation):
				idep #0     - operand input (value that is store'd)
				idep #1     - eff addr computation result (from ROB op)
			effective address computation is transfered via the reserved name DTMP */

			//Wattch -- Dispatch + RAT lookup stage
			cores[core_num].power.rename_access++;
			//fill in ROB entry
			rs = &contexts[disp_context_id].ROB[contexts[disp_context_id].ROB_tail];
			rs->slip = sim_cycle - 1;
			rs->IR = inst;
			rs->op = op;
			rs->PC = regs->regs_PC;
			rs->next_PC = regs->regs_NPC; rs->pred_PC = contexts[disp_context_id].pred_PC;
			rs->in_LSQ = FALSE;
			rs->LSQ_index = -1;
			rs->ea_comp = FALSE;
			rs->recover_inst = FALSE;
			rs->dir_update = contexts[disp_context_id].IFQ[contexts[disp_context_id].fetch_head].dir_update;
			rs->stack_recover_idx = contexts[disp_context_id].IFQ[contexts[disp_context_id].fetch_head].stack_recover_idx;
			rs->spec_mode = contexts[disp_context_id].spec_mode;
			rs->addr = 0;
			rs->replayed = FALSE;
			rs->seq = ++inst_seq;
			rs->in_IQ = rs->dispatched = rs->queued = rs->issued = rs->completed = FALSE;
			rs->ptrace_seq = pseq;
			rs->context_id = disp_context_id;
			rs->rename_cycle = sim_cycle;
			rs->disp_cycle = 0;
			//store the architectural registers in the ROB entry
			rs->archreg = out1;
			rs->src_archreg[0] = in1;
			rs->src_archreg[1] = in2;
			//store the physical source registers
			rs->src_physreg[0] = contexts[disp_context_id].rename_table[in1];
			rs->src_physreg[1] = contexts[disp_context_id].rename_table[in2];
			rs->regs_C = regs_C;
			rs->regs_index = -1;
			rs->L1_miss = rs->L2_miss = rs->L3_miss = 0;
			rs->iq_entry_num = -1;
			contexts[disp_context_id].last_op = RS_link(rs);

			if((out1 != 0) && (out1 != 32))
			{
				//Register renaming support
				reg_set my_regs;
				cores[core_num].reg_file.get_reg_set(&my_regs, op);

				//allocate and store the physical register for the destination
				if(my_regs.dest==REG_NONE)
				{
					rs->physreg = -1;
					rs->old_physreg = -1;
				}
				else
				{
					if(my_regs.dest==REG_INT)
					{
						/******* DCRA *********/
						contexts[disp_context_id].DCRA_int_rf++;
						assert(contexts[disp_context_id].DCRA_int_rf <= cores[core_num].reg_file.size());
						/**********************/
						rs->regs_index = DGPR(out1);
						if(rs->regs_index==Rlist[0])
						{
							rs->regs_R = regs_R[0];
						} else if(rs->regs_index==Rlist[1])
						{
							rs->regs_R = regs_R[1];
						}
						else
						{
							assert(rs->regs_index==Rlist[2]);
							rs->regs_R = regs_R[2];
						}
					}
					else
					{
						/******* DCRA *********/
						contexts[disp_context_id].DCRA_activity_fp = 256;
						contexts[disp_context_id].DCRA_fp_rf++;
						assert(contexts[disp_context_id].DCRA_fp_rf <= cores[core_num].reg_file.size());
						/**********************/
						rs->regs_index = DFPR(out1);
						if(rs->regs_index==Rlist[0])
						{
							rs->regs_F = regs_F[0];
						} else if(rs->regs_index==Rlist[1])
						{
							rs->regs_F = regs_F[1];
						}
						else
						{
							//Special case, if op==MT_FPCR (mt_fpcr $fX -> fpcr = $fX)
							//The regs_index is -1 (fpcr is 64) which doesn't match any expected destinations
							if(out1==DFPCR)
							{
								//FPCR is already preserved by regs_C. We want to nop the rollback here
								rs->regs_index = Rlist[2];
								//This register didn't change anyway. Now it matches and we don't have to alter the rollback code
							}
							assert(rs->regs_index==Rlist[2]);
							rs->regs_F = regs_F[2];
						}
						//This lets us keep track of FP via regs_index.
						rs->regs_index+=32;
					}
					rs->physreg = cores[core_num].reg_file.alloc_physreg(rs,sim_cycle,contexts[disp_context_id].rename_table);
					assert(rs->physreg >= 0);
				}
				rs->dest_format = my_regs.dest;
			}
			else
			{
				//DON'T RENAME REGISTER 0
				rs->physreg = -1;
				rs->old_physreg = -1;
				rs->dest_format = REG_NONE;
			}

			//Wattch: Maintain values through core for AFs
			rs->val_ra = val_ra;
			rs->val_rb = val_rb;
			rs->val_rc = val_rc;
			rs->val_ra_result = val_ra_result;

			//split ld/st's into two operations: eff addr comp + mem access
			if(MD_OP_FLAGS(op) & F_MEM)
			{
				//convert ROB operation from ld/st to an add (eff addr comp)
				rs->op = MD_AGEN_OP;
				rs->ea_comp = TRUE;
				//fill in LSQ entry
				lsq = &contexts[disp_context_id].LSQ[contexts[disp_context_id].LSQ_tail];
				lsq->slip = sim_cycle - 1;
				lsq->IR = inst;
				lsq->op = op;
				lsq->PC = regs->regs_PC;
				lsq->next_PC = regs->regs_NPC; lsq->pred_PC = contexts[disp_context_id].pred_PC;
				rs->LSQ_index = contexts[disp_context_id].LSQ_tail;
				lsq->in_LSQ = TRUE;
				lsq->in_IQ = FALSE;
				lsq->ea_comp = FALSE;
				lsq->dispatched = FALSE;
				lsq->recover_inst = FALSE;
				lsq->dir_update.pdir1 = lsq->dir_update.pdir2 = NULL;
				lsq->dir_update.pmeta = NULL;
				lsq->stack_recover_idx = 0;
				lsq->spec_mode = contexts[disp_context_id].spec_mode;
				lsq->addr = addr;
				lsq->seq = ++inst_seq;
				lsq->replayed = FALSE;
				lsq->queued = lsq->issued = lsq->completed = FALSE;
				lsq->ptrace_seq = pseq;
				lsq->context_id = disp_context_id;
				lsq->disp_cycle = 0;
				lsq->rename_cycle = sim_cycle;
				lsq->archreg = out1;
				lsq->src_archreg[0] = in1;
				lsq->src_archreg[1] = in2;
				lsq->src_physreg[0] = rs->src_physreg[0];
				lsq->src_physreg[1] = rs->src_physreg[1];
				lsq->physreg = rs->physreg;
				lsq->old_physreg = -1;
				lsq->dest_format = rs->dest_format;
				//Wattch: Maintain values through core for AFs
				lsq->val_ra = val_ra;
				lsq->val_rb = val_rb;
				lsq->val_rc = val_rc;
				lsq->val_ra_result = val_ra_result;
				lsq->previous_mem = previous_mem;
				lsq->data_size = data_size;
				lsq->is_store = ((MD_OP_FLAGS(op) & (F_MEM|F_STORE)) == (F_MEM|F_STORE));
				lsq->iq_entry_num = -1;

				//pipetrace this uop
				ptrace_newuop(lsq->ptrace_seq, "internal ld/st", lsq->PC, 0);
				ptrace_newstage(lsq->ptrace_seq, PST_DISPATCH, 0);

				//install operation in the ROB and LSQ
				n_renamed++;
				contexts[disp_context_id].ROB_tail = (contexts[disp_context_id].ROB_tail + 1) % contexts[disp_context_id].ROB.size();
				contexts[disp_context_id].ROB_num++;
				contexts[disp_context_id].LSQ_tail = (contexts[disp_context_id].LSQ_tail + 1) % contexts[disp_context_id].LSQ.size();
				contexts[disp_context_id].LSQ_num++;
				//issue may continue when the load/store is issued
				contexts[disp_context_id].last_op = RS_link(lsq);
			}
			else	//!(MD_OP_FLAGS(op) & F_MEM)
			{
				//Wattch: Regfile writes taken care of inside ruu_link_idep
				//install operation in the ROB
				n_renamed++;
				contexts[disp_context_id].ROB_tail = (contexts[disp_context_id].ROB_tail + 1) % contexts[disp_context_id].ROB.size();
				contexts[disp_context_id].ROB_num++;
			}
		}
		else
		{
			//this is a NOP, no need to update ROB/LSQ state
			contexts[disp_context_id].icount--;
			assert(contexts[disp_context_id].icount <= (contexts[disp_context_id].IFQ.size() + contexts[disp_context_id].ROB.size()));
			rs = NULL;
		}

		//one more instruction executed, speculative or otherwise
		cores[core_num].sim_total_insn++;
		if(MD_OP_FLAGS(op) & F_CTRL)
			cores[core_num].sim_total_branches++;

		if(!contexts[disp_context_id].spec_mode && rs)
		{
			//if this is a branching instruction update BTB, i.e., only non-speculative state is committed into the BTB
			if(MD_OP_FLAGS(op) & F_CTRL)
			{
				cores[core_num].sim_num_branches++;
				if(contexts[disp_context_id].pred && cores[core_num].bpred_spec_update == core_t::spec_ID)
				{
					contexts[disp_context_id].pred->bpred_update(
					/* branch address */regs->regs_PC,
					/* actual target address */regs->regs_NPC,
					/* taken? */regs->regs_NPC != (regs->regs_PC + sizeof(md_inst_t)),
					/* pred taken? */contexts[disp_context_id].pred_PC != (regs->regs_PC + sizeof(md_inst_t)),
					/* correct pred? */contexts[disp_context_id].pred_PC == regs->regs_NPC,
					/* opcode */op,
					/* predictor update ptr */&rs->dir_update);
				}
			}
			assert(regs->regs_NPC == rs->next_PC);

			//is the trace generator transitioning into mis-speculation mode?
			if(contexts[disp_context_id].pred_PC != regs->regs_NPC)
			{
				//entering mis-speculation mode, indicate this and save PC
				contexts[disp_context_id].spec_mode = TRUE;
				rs->recover_inst = TRUE;
				contexts[disp_context_id].recover_PC = regs->regs_NPC;
			}
		}

		//entered decode/allocate stage, indicate in pipe trace
		ptrace_newstage(pseq, PST_DISPATCH,(contexts[disp_context_id].pred_PC != regs->regs_NPC) ? PEV_MPOCCURED : 0);
		if(op == MD_NOP_OP)
		{
			//end of the line
			ptrace_endinst(pseq);
		}

		//update any stats tracked by PC
		for(int i=0; i<pcstat_nelt; i++)
		{
			//check if any tracked stats changed
			counter_t newval = STATVAL(pcstat_stats[i]);
			int delta = newval - pcstat_lastvals[i];
			if(delta != 0)
			{
				stat_add_samples(pcstat_sdists[i], regs->regs_PC, delta);
				pcstat_lastvals[i] = newval;
			}
		}

		//consume instruction from IFETCH -> RENAME queue
		contexts[disp_context_id].fetch_head = (contexts[disp_context_id].fetch_head+1) & (contexts[disp_context_id].IFQ.size() - 1);
		contexts[disp_context_id].fetch_num--;

		//If a prediction is changed (by perfect prediction) then the IFQ is tainted. Drain it here.
		if(fetch_redirected)
		{
			//consume instruction from IFETCH -> RENAME queue
			while(contexts[disp_context_id].fetch_num)
			{
				contexts[disp_context_id].icount--;
				contexts[disp_context_id].fetch_head = (contexts[disp_context_id].fetch_head+1) & (contexts[disp_context_id].IFQ.size()-1);
				contexts[disp_context_id].fetch_num--;
			}
			assert(contexts[disp_context_id].icount <= (contexts[disp_context_id].IFQ.size() + contexts[disp_context_id].ROB.size()));
		}

		//check for DLite debugger entry condition
		made_check = TRUE;
		if(dlite_check_break(contexts[disp_context_id].dlite_evaluator,contexts[disp_context_id].pred_PC, is_write ? ACCESS_WRITE : ACCESS_READ, addr, sim_num_insn, sim_cycle))
			contexts[disp_context_id].dlite_evaluator->dlite_main(regs->regs_PC, contexts[disp_context_id].pred_PC, sim_cycle);

		//need to enter DLite at least once per cycle - this should be outside of this loop... (it has been moved inside in order to track some uninitialized values down
		if(!made_check)
		{
			if(dlite_check_break(contexts[disp_context_id].dlite_evaluator, 0, is_write ? ACCESS_WRITE : ACCESS_READ, addr, sim_num_insn, sim_cycle))
				contexts[disp_context_id].dlite_evaluator->dlite_main(regs->regs_PC, 0, sim_cycle);
		}
		a = n_renamed;
		if (n_renamed > a)
		{a = n_renamed;}
		//std::cout << "Current context: " << current_context<< std::endl;
	//return n_renamed_threads;
	//return cores[core_num].decode_width;
		if (current_context == 0)
			{n_renamed_threads_0 = a;}
		else if (current_context == 1)
			{n_renamed_threads_1 = a;}
		else if (current_context == 2)
			{n_renamed_threads_2 = a;}
		else 
			{n_renamed_threads_3 = a;}
		//std::cout << "Current renamed register: " << n_renamed_threads[0]<<n_renamed_threads[1]<<n_renamed_threads[2]<< std::endl;
	}
	
	//std::cout << "Number of renamed register a " << a << std::endl;
	
		//std::cout << "Number of renamed register " << n_renamed << std::endl;
	//std::cout << "j " << j << std::endl;
	//j = j+1;
	state[0] = cores[core_num].decode_width;
	state[1] = n_renamed_threads_0;
	state[2] = n_renamed_threads_1;
	state[3] = n_renamed_threads_2;
	state[4] = n_renamed_threads_3;
	//return state;
	return std::make_tuple(cores[core_num].decode_width,n_renamed_threads_0,n_renamed_threads_1,n_renamed_threads_2,n_renamed_threads_3);
	//return cores[core_num].decode_width,n_renamed_threads_0,n_renamed_threads_1,n_renamed_threads_2,n_renamed_threads_3;
	
}

//dispatches instruction into the IQ
double dispatch(unsigned int core_num)
{
	//double iq_percentage = 0;
	//round robin dispatch
	std::vector<int> contexts_left(cores[core_num].context_ids);
	unsigned int current_context = sim_cycle % contexts_left.size();

	//initalize ROB_index - starting location for dispatch
	unsigned int ROB_index[contexts_at_init_time];
	for(size_t i=0;i<contexts.size();i++)
	{
		ROB_index[i] = contexts[i].ROB_head;
	}

	unsigned int num_dispatched(0);		//total insts dispatched
	while((num_dispatched < cores[core_num].decode_width)&&(!contexts_left.empty()))
	{
		current_context%=contexts_left.size();
		int disp_context_id = contexts_left[current_context];		//ID of the context to dispatch from

		if(contexts[disp_context_id].ROB_num == 0)
		{
			//if there are no more instructions waiting dispatch for this thread, try another thread
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//find the next instruction awaiting dispatch
		bool found = FALSE;
		do
		{
			if(!contexts[disp_context_id].ROB[ROB_index[disp_context_id]].dispatched)
			{
				found = TRUE;
				break;
			}
			ROB_index[disp_context_id] = (ROB_index[disp_context_id] + 1) % contexts[disp_context_id].ROB.size();
		} while(ROB_index[disp_context_id] != contexts[disp_context_id].ROB_tail);

		//If ROB index is ROB_tail, we are at the end of the ROB
		//except if ROB is full, then tail==head.
		if(!found)
		{
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//We have an instruction now
		ROB_entry *rs = &contexts[disp_context_id].ROB[ROB_index[disp_context_id]];

		//enforce the rename-to-dispatch delay
		if(rs->rename_cycle + cores[core_num].RENAME_DISPATCH_DELAY > sim_cycle)
		{
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//rs now refers to a valid instruction for dispatch

		//if IQ is full, block this thread
		int my_iq_num = cores[core_num].iq.alloc_iq_entry();
		//std::cout << "My iq number is at cycle: " << my_iq_num << std::endl;
		double iq_percentage = (double) my_iq_num/cores[core_num].iq.size();
		//std::cout << "IQ percentage: " << (double) my_iq_num/cores[core_num].iq.size() << std::endl;
		if(my_iq_num < 0)
		{
			contexts_left.erase(contexts_left.begin()+current_context);
			continue;
		}

		//IQ entry obtained, instruction will be dispatched now

		/************* DCRA ************/
		contexts[disp_context_id].DCRA_int_iq++;
		assert(contexts[disp_context_id].DCRA_int_iq <= cores[core_num].iq.size());
		/*******************************/

		//update the ROB entry, dispatch time, IQ status
		rs->disp_cycle = sim_cycle;
		rs->dispatched = TRUE;
		rs->iq_entry_num = my_iq_num;
		rs->in_IQ = TRUE;
		assert(cores[core_num].iq[my_iq_num] != IQ_ENTRY_FREE);
    
		if(all_operands_spec_ready(rs))
		{
			// Wattch -- both speculative operands ready, 2 window write accesses

//Old code: Replaced with check to see results come from bypass (if not ready, speculatively ready and came from bypass)
			//Wattch -- FIXME: currently being read from arch.
			//	spec_ready and ready can't tell bypass ready from architecturally ready
			//	However, this defaults to architecturally ready */
			cores[core_num].power.window_access++;
			cores[core_num].power.window_access++;
			cores[core_num].power.window_preg_access++;
			cores[core_num].power.window_preg_access++;
#ifdef DYNAMIC_AF
			cores[core_num].power.regfile_total_pop_count_cycle += pop_count(rs->val_ra);
			cores[core_num].power.regfile_total_pop_count_cycle += pop_count(rs->val_rb);
			cores[core_num].power.regfile_num_pop_count_cycle+=2;
#endif
	
			//effective address computation ready, queue it on ready list
			assert(!rs->queued);
			readyq_enqueue(rs);
		}
		else if(one_operand_ready(rs))
		{
			//Wattch -- one operand ready, 1 window write accesses
			cores[core_num].power.window_access++;
			cores[core_num].power.window_preg_access++;
#ifdef DYNAMIC_AF
			if(operand_ready(rs,0))
				cores[core_num].power.regfile_total_pop_count_cycle += pop_count(rs->val_ra);
			else
				cores[core_num].power.regfile_total_pop_count_cycle += pop_count(rs->val_rb);
			cores[core_num].power.regfile_num_pop_count_cycle++;
#endif
		}

		if(rs->LSQ_index != -1)
		{
			//This is a load/store, rs is effective address computation
			//The load/store itself is in the LSQ and needs to be handled here
			ROB_entry *lsq = &contexts[disp_context_id].LSQ[rs->LSQ_index];
			lsq->disp_cycle = sim_cycle;
			lsq->dispatched = TRUE;

			//issue stores only, loads are issued by lsq_refresh()
			if(MD_OP_FLAGS(lsq->op) & F_STORE)
			{
				if(all_operands_spec_ready(lsq))
				{
					//Wattch -- store operand ready, 1 LSQ access
					cores[core_num].power.lsq_store_data_access++;
	  
					//panic("store immediately ready");
					//put operation on ready list, selection() issue it later
					assert(!lsq->queued);
					readyq_enqueue(lsq);
				}
				else
				{
					assert(!lsq->queued);
					cores[core_num].wait_q_enqueue(lsq, sim_cycle);
				}
			}
		}

		//If instruction was not ready (not put into readyq_enqueue) then it is put into waiting queue here
		if(!rs->queued)
		{
			cores[core_num].wait_q_enqueue(rs, sim_cycle);
		}
		num_dispatched++;
	return iq_percentage;
	}
	
}

//FETCH() - instruction fetch pipeline stage(s)
//fetch up as many instruction as one branch prediction and one cache line
//access will support without overflowing the instruction fetch queue (IFQ)
//contexts_left comes from the core's fetching logic and refers to the order
//of eligible contexts for fetching
void fetch(std::vector<int> contexts_left)
{
	std::vector<int>::iterator it = contexts_left.begin();
	while(it != contexts_left.end())
	{
		//If there thread is done (no pid) or if it has an interrupt other than dynamically loading, skip it.
		if((!contexts[*it].pid) || (contexts[*it].interrupts & ~1))
		{
			it = contexts_left.erase(it);
		}
		else
		{
			it++;
		}
	}
	if(contexts_left.empty())
	{
		return;
	}
	unsigned int core_num = contexts[contexts_left[0]].core_id;

	std::set<unsigned int> fetchedfrom;
	bool discontinuous = FALSE;

	//Fetch up to as many instructions as fetch_width equivalent allows ahd there are contexts left to fetch from
	for(unsigned int i=0;(i < (cores[core_num].decode_width * cores[core_num].fetch_speed))&&(!contexts_left.empty());i++)
	{
		int context_id = contexts_left[0];

		if(contexts[context_id].interrupts)
		{
			//Executable has begun dynamic loading, fast-forward to program start
			if(contexts[context_id].interrupts & 0x1)
			{
				contexts[context_id].spec_mode = 0;
				contexts[context_id].regs.regs_PC = contexts[context_id].mem->ld_prog_entry + 4;
				contexts[context_id].regs.regs_NPC = contexts[context_id].mem->ld_prog_entry + 8;
				assert(contexts[context_id].entry_point);
				while(contexts[context_id].entry_point)
				{
					ff_context(context_id,1,NO_WARMUP);
					if(contexts[context_id].regs.regs_PC == contexts[context_id].entry_point)
					{
						contexts[context_id].entry_point = 0;
						contexts[context_id].interrupts &= ~0x1;
//						std::cerr << "Context(" << context_id << ") has reached its entry point" << std::endl;
					}
				}
				contexts[context_id].fetch_pred_PC = contexts[context_id].fetch_regs_PC = contexts[context_id].regs.regs_PC;
//				std::cerr << "Entry point at: " << std::hex << contexts[context_id].regs.regs_PC << std::dec << std::endl;
			}
		}

		if(discontinuous)
		{	//Fetch was discontinuous, remove
			discontinuous = FALSE;
			contexts_left.erase(contexts_left.begin());
			i--;
			continue;
		}

		//fetch until IFETCH -> RENAME queue fills
		if(contexts[context_id].fetch_num == contexts[context_id].IFQ.size())
		{
			contexts_left.erase(contexts_left.begin());
			i--;
			continue;
		}

		//enforce the fetch-issue delay counters for each thread these counters are used for modeling the minimum branch misprediction penalty
		//This is for the fetcher (i-caches).
		if(contexts[context_id].fetch_issue_delay > 0)
		{
			contexts_left.erase(contexts_left.begin());
			i--;
			continue;
		}

		//enforce fetch from 2 contexts limit
		if(fetchedfrom.size()==2)
		{
			if(fetchedfrom.find(context_id)==fetchedfrom.end())
			{
				break;
			}
		}

		int stack_recover_idx = contexts[context_id].pred->retstack.TOS();
		md_inst_t inst;

		mem_t* mem = contexts[context_id].mem;

		//fetch an instruction at the next predicted fetch address
		contexts[context_id].fetch_regs_PC = contexts[context_id].fetch_pred_PC;

		assert(context_id < num_contexts);

		//Wattch: add power for i-fetch stage
		cores[core_num].power.icache_access++;

		//is this a bogus text address? (can happen on mis-spec path)
		md_addr_t ld_text_bound = mem->ld_text_base + mem->ld_text_size;
		bool do_fetch = false;
		if((mem->ld_text_base <= contexts[context_id].fetch_regs_PC) && (contexts[context_id].fetch_regs_PC < ld_text_bound))
		{
			if(!(contexts[context_id].fetch_regs_PC & (sizeof(md_inst_t)-1)))
			{
				do_fetch = true;
			}
		}
		else if(contexts[context_id].fetch_regs_PC >= ld_text_bound)
		{
			if(!mem->mem_translate(contexts[context_id].fetch_regs_PC))
			{
				if(!contexts[context_id].spec_mode)
				{
					std::cerr << "Could not find memory page associated with: " << std::hex << contexts[context_id].fetch_regs_PC << std::dec << std::endl;
					assert(!contexts[context_id].spec_mode);
				}
			}
			do_fetch = true;
		}

		int lat = 1;
		int last_inst_missed = false, last_inst_tmissed = false;
		if(do_fetch)
		{
			//read instruction from memory
			MD_FETCH_INST(inst, mem, contexts[context_id].fetch_regs_PC);

			//Then access Level 1 Instruction cache and Instruction TLB in parallel
			lat = cores[core_num].cache_il1_lat;
			if(cores[core_num].cache_il1)
			{
				//access the I-cache
				lat = cores[core_num].cache_il1->cache_access(Read, IACOMPRESS(contexts[context_id].fetch_regs_PC), context_id, NULL, ISCOMPRESS(sizeof(md_inst_t)), sim_cycle, NULL, NULL);
				last_inst_missed = (lat > cores[core_num].cache_il1_lat);
			}

			if(cores[core_num].itlb)
			{
				//access the I-TLB, NOTE: this code will initiate speculative TLB misses
				int tlb_lat = cores[core_num].itlb->cache_access(Read, IACOMPRESS(contexts[context_id].fetch_regs_PC), context_id, NULL, ISCOMPRESS(sizeof(md_inst_t)), sim_cycle, NULL, NULL);
				last_inst_tmissed = (tlb_lat > 1);
				lat = MAX(tlb_lat, lat);
			}

			//I-cache/I-TLB miss? assumes I-cache hit >= I-TLB hit (assuming 1 cycle)
			if(lat != cores[core_num].cache_il1_lat)
			{
				//I-cache/I-TLB miss, block fetch until it is resolved
				contexts[context_id].fetch_issue_delay += lat - 1;
			}
		}
		else
		{
			if(contexts[context_id].fetch_regs_PC > 0x1000)
			{
				if(!mem->mem_translate(contexts[context_id].fetch_regs_PC))
				{
					std::cerr << "Could not find memory page associated with: " << std::hex << contexts[context_id].fetch_regs_PC << std::dec << std::endl;
					assert(0);
				}
			}
			//fetch PC is bogus, send a NOP down the pipeline
			inst = MD_NOP_INST;
		}
		//This inst is valid at this point

		//possibly use the BTB target
		if(contexts[context_id].pred)
		{
			enum md_opcode op;

			//pre-decode instruction, used for bpred stats recording
			MD_SET_OPCODE(op, inst);

			//get the next predicted fetch address; only use branch predictor
			//result for branches (assumes pre-decode bits);
			//NOTE: returned value may be 1 if bpred can only predict a direction
			if(MD_OP_FLAGS(op) & F_CTRL)
			{
				contexts[context_id].fetch_pred_PC = contexts[context_id].pred->bpred_lookup(
					/* branch address */contexts[context_id].fetch_regs_PC, /* target address */0,
					/* opcode */op, /* call? */MD_IS_CALL(op), /* return? */MD_IS_RETURN(op),
					/* updt */&(contexts[context_id].IFQ[contexts[context_id].fetch_tail].dir_update), /* RSB index */&stack_recover_idx);
			}
			else
			{
				contexts[context_id].fetch_pred_PC = 0;
			}
			//valid address returned from branch predictor?
			if(!contexts[context_id].fetch_pred_PC)
			{
				//no predicted taken target, attempt not taken target
				contexts[context_id].fetch_pred_PC = contexts[context_id].fetch_regs_PC + sizeof(md_inst_t);
			}
			else
			{
				//go with target, NOTE: discontinuous fetch, so fetch from another context
				discontinuous = TRUE;
			}
		}
		else
		{
			//no predictor, just default to predict not taken, and continue fetching instructions linearly
			contexts[context_id].fetch_pred_PC = contexts[context_id].fetch_regs_PC + sizeof(md_inst_t);
		}
		fetchedfrom.insert(context_id);

		//commit this instruction to the IFETCH -> RENAME queue
		contexts[context_id].IFQ[contexts[context_id].fetch_tail].IR = inst;
		contexts[context_id].IFQ[contexts[context_id].fetch_tail].regs_PC = contexts[context_id].fetch_regs_PC;
		contexts[context_id].IFQ[contexts[context_id].fetch_tail].pred_PC = contexts[context_id].fetch_pred_PC;
		contexts[context_id].IFQ[contexts[context_id].fetch_tail].stack_recover_idx = stack_recover_idx;
		contexts[context_id].IFQ[contexts[context_id].fetch_tail].ptrace_seq = contexts[context_id].ptrace_seq++;
		contexts[context_id].IFQ[contexts[context_id].fetch_tail].fetched_cycle = sim_cycle + (lat - 1);

		//for pipe trace
		ptrace_newinst(contexts[context_id].IFQ[contexts[context_id].fetch_tail].ptrace_seq, inst, contexts[context_id].IFQ[contexts[context_id].fetch_tail].regs_PC, 0);
		ptrace_newstage(contexts[context_id].IFQ[contexts[context_id].fetch_tail].ptrace_seq, PST_IFETCH, ((last_inst_missed ? PEV_CACHEMISS : 0) | (last_inst_tmissed ? PEV_TLBMISS : 0)));

		//adjust instruction fetch queue
		contexts[context_id].fetch_tail = (contexts[context_id].fetch_tail + 1) & (contexts[context_id].IFQ.size() - 1);
		contexts[context_id].fetch_num++;
		contexts[context_id].icount++;

		assert(contexts[context_id].icount <= (contexts[context_id].IFQ.size() + contexts[context_id].ROB.size()));

		//If we used half of the fetch width, go to next context (if available)
		if(i == (cores[core_num].decode_width * cores[core_num].fetch_speed)/2)
		{
			if(contexts_left.size()>1)
			{
				contexts_left.erase(contexts_left.begin());
				discontinuous = FALSE;	//clear discontinuous flag if it were set.
			}
		}
	}
}

//used for ICOUNT and DCRA fetch
int my_comparator(const int & a1, const int & a2)
{
	return (contexts[a1].icount < contexts[a2].icount);
}

std::vector<int> icount_fetch(unsigned int core_num){
	std::vector<int> sorted_contexts(cores[core_num].context_ids);

	for(unsigned int i=0;i<sorted_contexts.size();i++){
		assert(contexts[sorted_contexts[i]].icount >= 0);
		assert(contexts[sorted_contexts[i]].fetch_num <= contexts[sorted_contexts[i]].IFQ.size());
		assert(contexts[sorted_contexts[i]].icount <= (contexts[sorted_contexts[i]].IFQ.size()+contexts[sorted_contexts[i]].ROB.size()));
	}
	sort(sorted_contexts.begin(),sorted_contexts.end(),my_comparator);

	return sorted_contexts;
}

std::vector<int> RR_fetch(unsigned int core_num){
	std::vector<int> sorted_contexts(cores[core_num].context_ids);

	for(unsigned int i=0;i<cores[core_num].context_ids.size();i++){
		sorted_contexts[i]= cores[core_num].context_ids[(sim_cycle + i) % cores[core_num].context_ids.size()];
	}

	return sorted_contexts;
}

std::vector<int> dcra_fetch(unsigned int core_num)
{
	std::vector<int> sorted_contexts(cores[core_num].context_ids);
	int num_fa = 0, num_sa = 0;
	std::vector<int> fast(sorted_contexts.size(),0);
	int num_contexts = sorted_contexts.size();
	for(unsigned int i=0;i<sorted_contexts.size();i++)
	{
		if(contexts[sorted_contexts[i]].DCRA_activity_fp > 0)
		{
			contexts[sorted_contexts[i]].DCRA_activity_fp--;
		}
		if(contexts[sorted_contexts[i]].DCRA_L1_misses > 0)
		{
			contexts[sorted_contexts[i]].DCRA_L1_misses--;
		}
		fast[i] = !(contexts[sorted_contexts[i]].DCRA_L1_misses > 0);
		if(!(contexts[sorted_contexts[i]].DCRA_activity_fp == 0))
		{
			num_fa += fast[i];
			num_sa += (!fast[i]);
		}
	}
	for(unsigned int i=0;i<sorted_contexts.size();i++)
	{
		//fast threads won't be blocked, so we don't consider them here
		if(!fast[i])
		{
			int context_id = sorted_contexts[i];
			if(contexts[context_id].DCRA_int_iq >= cores[core_num].iq.size() / num_contexts * (1 + (1/num_contexts) * num_fa))
			{
				sorted_contexts.erase(sorted_contexts.begin()+i);
				fast.erase(fast.begin()+i);
				i--;
				continue;
			}
			if(contexts[context_id].DCRA_int_rf >= cores[core_num].reg_file.size() / num_contexts * (1 + (1/num_contexts) * num_fa))
			{
				sorted_contexts.erase(sorted_contexts.begin()+i);
				fast.erase(fast.begin()+i);
				i--;
				continue;
			}
			if(num_fa + num_sa)
			{
				if(contexts[context_id].DCRA_fp_rf >= cores[core_num].reg_file.size() / (num_fa + num_sa) * (1 + (1/(num_fa + num_sa)) * num_fa))
				{
					sorted_contexts.erase(sorted_contexts.begin()+i);
					fast.erase(fast.begin()+i);
					i--;
					continue;
				}
			}
		}
	}
	if(sorted_contexts.empty())
	{
		return std::vector<int>();
	}
	sort(sorted_contexts.begin(),sorted_contexts.end(),my_comparator);
	return sorted_contexts;
}

//default machine state accessor, used by DLite
//Returns an error string (or NULL for no error)
const char * simoo_mstate_obj(FILE *stream,		//output stream
	char *cmd,					//optional command string
	regs_t *regs,					//registers to access
	mem_t *mem)					//memory space to access
{
	if(!cmd || !strcmp(cmd, "help"))
		fprintf(stream,
			"mstate commands:\n"
			"\n"
			"    mstate help   - show all machine-specific commands (this list)\n"
			"    mstate stats  - dump all statistical variables\n"
			"    mstate res    - dump current functional unit resource states\n"
			"    mstate ruu    - dump contents of the register update unit\n"
			"    mstate lsq    - dump contents of the load/store queue\n"
			"    mstate eventq - dump contents of event queue\n"
			"    mstate readyq - dump contents of ready instruction queue\n"
			"    mstate fetch  - dump contents of fetch stage registers and fetch queue\n"
			"\n"
			);
	else if(!strcmp(cmd, "stats"))
	{
		//just dump intermediate stats
		sim_print_stats(stream);
	}
	else if(!strcmp(cmd, "res"))
	{
		for(unsigned int i=0;i<cores.size();i++)
		{
			//dump resource state
			res_dump(cores[i].fu_pool, stream);
		}
	}
	else if(!strcmp(cmd, "ruu"))
	{
		//dump ROB contents
		rob_dump(stream,mem->context_id);
	}
	else if(!strcmp(cmd, "lsq"))
	{
		//dump LSQ contents
		lsq_dump(stream, regs->context_id);
	}
	else if(!strcmp(cmd, "eventq"))
	{
		//dump event queue contents
		eventq_dump(stream, regs->context_id);
	}
	else if(!strcmp(cmd, "readyq"))
	{
		//dump event queue contents
		readyq_dump(stream, regs->context_id);
	}
	else if(!strcmp(cmd, "fetch"))
	{
		//dump event queue contents
		contexts[regs->context_id].fetch_dump(stream);
	}
	else
		return "unknown mstate command";

	//no error
	return NULL;
}

//Segmentation fault handler, since these errors are rather common when editing
//What information should we put here?
void segfault_handler(int sig_type)
{
	std::cout << "Segmentation fault occurred at cycle: " << sim_cycle << std::endl;
	for(size_t i=0;i<contexts.size();i++)
	{
		std::cerr << "Context " << i << ":\t";
		eventq_dump(stderr,i);
	}
	signal(SIGSEGV, SIG_DFL);
	raise(SIGSEGV);
}

//Fast forward handler
//int ff_context(unsigned int context_id, long long insts, int mode)
int ff_context(unsigned int current_context, long long insts_count, ff_mode_t mode)
{
//	md_inst_t inst(0);		//actual instruction bits
//	enum md_opcode op(MD_NOP_OP);	//decoded opcode enum
//	md_addr_t target_PC;		//actual next/target PC address
//	md_addr_t addr;			//effective address, if load/store
//	int is_write;			//store?

	if(contexts[current_context].sleep)
	{
		contexts[current_context].sleep--;
		return 0;
	}

	mem_t* mem = contexts[current_context].mem;
	regs_t* regs = &contexts[current_context].regs;

	//access the I-cache and I-TLB (if it exists)
	if(!(mode & NO_WARMUP))
	{
		cores[contexts[current_context].core_id].cache_il1->cache_access(Read, IACOMPRESS(regs->regs_PC),
			current_context, NULL, ISCOMPRESS(sizeof(md_inst_t)), sim_cycle, NULL, NULL);
		if(cores[contexts[current_context].core_id].itlb)
		{
			cores[contexts[current_context].core_id].itlb->cache_access(Read, IACOMPRESS(regs->regs_PC),
				current_context, NULL, ISCOMPRESS(sizeof(md_inst_t)), sim_cycle, NULL, NULL);
		}
	}

	//maintain $r0 semantics
	regs->regs_R[MD_REG_ZERO] = regs->regs_F[MD_REG_ZERO].d = 0.0;

	//get the next instruction to execute
	md_inst_t inst(0);
	MD_FETCH_INST(inst, mem, regs->regs_PC);

	//set default reference address, set default fault to none
	md_addr_t addr = 0;
	bool is_write = FALSE;
	enum md_fault_type fault = md_fault_none;

	//decode the instruction
	enum md_opcode op;
	MD_SET_OPCODE(op, inst);

	md_addr_t target_PC(0);			//actual next/target PC address
	int core_num = contexts[current_context].core_id;
	int spec_mode = contexts[current_context].spec_mode;
	//temp variable for spec mem access
	byte_t temp_byte = 0;
	half_t temp_half = 0;
	word_t temp_word = 0;
	qword_t temp_qword = 0;

	//execute the instruction
	switch(op)
	{
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3)		\
	case OP:							\
		SYMCAT(OP,_IMPL);					\
		break;
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT)					\
	case OP:							\
		panic("attempted to execute a linking opcode");
#define CONNECT(OP)
#undef DECLARE_FAULT
#define DECLARE_FAULT(FAULT)	{ fault = (FAULT); break; }
#include "machine.def"
	default:
		//Only bogus if the inst is not a NOP
		panic("attempted to execute a bogus opcode (%x) at %llx at fastfwd: %lld",inst,regs->regs_PC,contexts[current_context].fastfwd_cnt - contexts[current_context].fastfwd_left);
	}

	if(fault != md_fault_none)
	{
		fprintf(stderr,"(%d) Tried to access: %llx\n",current_context,addr);
		fprintf(stderr,"Instruction: ");
		md_print_insn(inst,regs->regs_PC,stderr);
		fatal(" -> fault (%d) detected @ 0x%08p at fastfwd: %lld", fault, regs->regs_PC,contexts[current_context].fastfwd_cnt - contexts[current_context].fastfwd_left);
	}

	//update memory access stats
	if(MD_OP_FLAGS(op) & F_MEM)
	{
		is_write = (MD_OP_FLAGS(op) & F_STORE);
		if(MD_OP_FLAGS(op) & F_STORE)
		{
			if(!(mode & NO_WARMUP))
			{
				cores[core_num].cache_dl1->cache_access(Write, (addr&~3), current_context, NULL, 4, sim_cycle, NULL, NULL);
			}
		}
		else if(addr)
		{
			int pred_hit_L1 = FALSE;
			int stack_recover_idx(0);
			bpred_update_t dir_update;		//bpred direction update info

			int latency = cores[core_num].cache_dl1->cache_access(Read, (addr&~3), current_context, NULL, 4, sim_cycle, NULL, NULL);

			if((cores[core_num].recovery_model_v==core_t::RECOVERY_MODEL_SQUASH) && (!(mode & NO_WARMUP)))
			{
				//DO LOAD LATENCY PREDICTION
				pred_hit_L1 = contexts[current_context].load_lat_pred->bpred_lookup(
					/* branch address */addr,
					/* target address */ 0,
					/* opcode */ op,
					/* call? */ FALSE,
					/* return? */ FALSE,
					/* updt */&(dir_update),
					/* RSB index */&stack_recover_idx);

				contexts[current_context].load_lat_pred->bpred_update(
					/* branch address */addr,
					/* actual target address */pred_hit_L1,
					/* taken? */latency <= cores[core_num].cache_dl1_lat,
					/* pred taken? */pred_hit_L1,
					/* correct pred? */(latency <= cores[core_num].cache_dl1_lat) == pred_hit_L1,
					/* opcode */op,
					/* predictor update ptr */&dir_update);
			}
		}
		//all loads and stores must access D-TLB
		if((cores[core_num].dtlb) && (!(mode & NO_WARMUP)))
		{
			cores[core_num].dtlb->cache_access(Read, (addr & ~3), current_context, NULL, 4, sim_cycle, NULL, NULL);
		}
	}

	bpred_update_t dir_update;		//bpred direction update info
	int stack_recover_idx(0);

	//update branch predictor
	if((MD_OP_FLAGS(op) & F_CTRL) && (!(mode & NO_WARMUP)))
	{
		md_addr_t pred_PC = contexts[current_context].pred->bpred_lookup(
			/* branch address */regs->regs_PC,
			/* target address */0,
			/* opcode */op,
			/* call? */MD_IS_CALL(op),
			/* return? */MD_IS_RETURN(op),
			/* updt */&dir_update,
			/* RSB index */&stack_recover_idx);

		contexts[current_context].pred->bpred_update(
			/* branch address */regs->regs_PC,
			/* actual target address */regs->regs_NPC,
			/* taken? */regs->regs_NPC != (regs->regs_PC +	sizeof(md_inst_t)),
			/* pred taken? */pred_PC != (regs->regs_PC + sizeof(md_inst_t)),
			/* correct pred? */pred_PC == regs->regs_NPC,
			/* opcode */op,
			/* predictor update ptr */&dir_update);
	}

	//check for DLite debugger entry condition
	if(dlite_check_break(contexts[current_context].dlite_evaluator, regs->regs_NPC, is_write ? ACCESS_WRITE : ACCESS_READ, addr, sim_num_insn, sim_num_insn))
		contexts[current_context].dlite_evaluator->dlite_main(regs->regs_PC, regs->regs_NPC, sim_num_insn);

	//go to the next instruction
	contexts[current_context].regs.regs_PC = contexts[current_context].regs.regs_NPC;
	contexts[current_context].regs.regs_NPC += sizeof(md_inst_t);

	//one more instruction has been executed (1 has already been counted)
	//This portion is unclear at the prototype and should be clarified or fixed.
	if(!(mode & NO_WARMUP))
	{
		contexts[current_context].fastfwd_left--;
	}
	return 0;
}

bool continue_fastfwd(std::vector<unsigned int> & contexts_left)
{
	bool retval = false;
	for(size_t i=0;i<contexts_left.size();i++)
	{
		if(contexts[contexts_left[i]].fastfwd_left && contexts[contexts_left[i]].pid)
		{
			return true;
			continue;
		}
		contexts_left.erase(contexts_left.begin()+i);
		i--;
	}
	return retval;
}

//start simulation, program loaded, processor precise state initialized
void sim_main()
{
unsigned int j =0;
unsigned int action = 0;
double reward = 0;
double iq_percentage = 0;
unsigned int state_0 = 0, state_1 =0, state_2 =0, state_3=0;
unsigned int states[5] ={0,0,0,0,0};
//FIXME: Don't do this here, do this at cache creation time:
#ifdef BUS_CONTENTION
	for(size_t i=0;i<cores.size();i++)
	{
		cores[i].cache_il1->next_cache = cores[i].cache_dl2;
		cores[i].cache_dl1->next_cache = cores[i].cache_dl2;
	}
#endif

	//ignore any floating point exceptions, they may occur on mis-speculated execution paths
	signal(SIGFPE, SIG_IGN);
	signal(SIGSEGV, segfault_handler);

	//Assert that contexts on each core is valid
	for(unsigned int i = 0; i<cores.size();i++)
	{	//max_contexts_per_core better be non-negative by now!
		assert(cores[i].context_ids.size() <= static_cast<unsigned int>(max_contexts_per_core));
	}

	//load the first context...
	int current_context = num_contexts - 1;

	//check for DLite debugger entry condition
	if(dlite_check_break(contexts[0].dlite_evaluator, contexts[0].regs.regs_PC, /* no access */0, /* addr */0, 0, 0))
	{
		contexts[0].dlite_evaluator->dlite_main(contexts[0].regs.regs_PC, contexts[0].regs.regs_PC + sizeof(md_inst_t), sim_cycle);
	}

	//if fastfwd_count is 1, then use the fastfwd numbers in the .arg files
	//otherwise use the specified fastfwd_count from each thread
	if(fastfwd_count != 1)
	{
		for(int i=0;i<num_contexts;i++)
		{
			contexts[i].fastfwd_left = fastfwd_count;
			contexts[i].fastfwd_cnt = fastfwd_count;
		}
	}

	for(size_t i=0;i<contexts.size();i++)
	{
		assert(i == (size_t)contexts[i].mem->context_id);
	}

	//fast forward simulation loop, performs functional simulation for "fastfwd_count" insts, then turns on performance (timing) simulation
	std::vector<unsigned int> contexts_left(num_contexts);
	int start_contexts = num_contexts;
	for(size_t i=0;i<contexts_left.size();i++)
	{
		contexts_left[i] = i;
	}
	fprintf(stderr, "sim: ** fast forwarding insts **\n");
	while(continue_fastfwd(contexts_left))
	{
		for(size_t j=0;j<contexts_left.size();j++)
		{
			size_t i = contexts_left[j];
			if(contexts[i].interrupts)
			{
				//Change these conditions to named values

				//Executable has begun dynamic loading, fast-forward to program start
				if(contexts[i].interrupts & 0x1)
				{
					assert(contexts[i].entry_point);
					while(contexts[i].entry_point)
					{
						ff_context(i,1,NO_WARMUP);
						if(contexts[i].regs.regs_PC == contexts[i].entry_point)
						{
								contexts[i].entry_point = 0;
								contexts[i].interrupts &= ~0x1;
//								std::cerr << "Context(" << i << ") has reached its entry point" << std::endl;
						}
					}
				}

				//Executable is waiting (wait4) for someone
				if(contexts[i].interrupts & 0x10000)
				{
					assert(contexts[i].waiting_for);
					if(!pid_handler.is_retval_avail(contexts[i].pid, contexts[i].waiting_for))
					{
						contexts[i].fastfwd_left--;
						continue;
					}
					contexts[i].waiting_for = 0;
					contexts[i].interrupts &= ~0x10000;
				}

				//Executable has a select pending
				if(contexts[i].interrupts & 0x20000)
				{
					contexts[i].next_check--;
					if(contexts[i].next_check>0)
					{
						contexts[i].fastfwd_left--;
						continue;
					}
					contexts[i].interrupts &= ~0x20000;
				}
			}
			//Fast forward the current context by 1 instruction (no special handling)
			ff_context(i,1,NORMAL);
		}
		while(start_contexts != num_contexts)
		{
			contexts_left.push_back(start_contexts);
			start_contexts++;
		}
	}

	//set up timing simulation entry state
	for(int i=0;i<num_contexts;i++)
	{
		contexts[i].fetch_regs_PC = contexts[i].fetch_pred_PC = contexts[i].regs.regs_PC;
	}

	//init pipeline structs for full simulation
	for(unsigned int i=0;i<cores.size();i++)
	{
		//resets cache stats after fast forwarding
		if(cores[i].cache_il1){
			cores[i].cache_il1->reset_cache_stats();
		}
		if(cores[i].cache_dl1){
			cores[i].cache_dl1->reset_cache_stats();
		}
		if(cores[i].cache_il2){
			cores[i].cache_il2->reset_cache_stats();
		}
		if(cores[i].cache_dl2){
			cores[i].cache_dl2->reset_cache_stats();
		}
		if(cores[i].itlb){
			cores[i].itlb->reset_cache_stats();
		}
		if(cores[i].dtlb){
			cores[i].dtlb->reset_cache_stats();
		}
		cores[i].main_mem->reset();
	}
	if(cache_dl3)
		cache_dl3->reset_cache_stats();
	if(cache_il3)
		cache_il3->reset_cache_stats();

	//reset bpred stats
	for(int i=0;i<num_contexts;i++){
		if(contexts[i].pred)
		{
			contexts[i].pred->reset();
		}
		if(contexts[i].load_lat_pred)
		{
			contexts[i].load_lat_pred->reset();
		}
	}

	if(eio_name && std::string(eio_name)!="none")
	{
		eio_write_chkpt(&contexts[0].regs,contexts[0].mem,eio_name,contexts[0].sim_num_insn);
		std::cerr << "Checkpoint created from " << contexts[0].filename << " at location: " << eio_name << std::endl;
	}

	std::cerr << "sim: ** starting performance simulation **" << std::endl;
	current_context = 0;

	if(num_contexts==0)
	{	//This is probably not needed, however, it was checked for in the main loop every cycle and doesn't need to be
		return;
	}
	//main simulator loop, NOTE: the pipe stages are traverse in reverse order
	//to eliminate this/next state synchronization and relaxation problems
	ofstream outFile;
	//outFile.open("data.csv");
	for(;;)
	{
		for(int i=0;i<num_contexts;i++)
		{
			//ROB/LSQ sanity checks
			if(contexts[i].ROB_num < contexts[i].LSQ_num)
				panic("ROB_num < LSQ_num");
			if(((contexts[i].ROB_head + contexts[i].ROB_num) % contexts[i].ROB.size()) != contexts[i].ROB_tail)
				panic("ROB_head/ROB_tail wedged");
			if(((contexts[i].LSQ_head + contexts[i].LSQ_num) % contexts[i].LSQ.size()) != contexts[i].LSQ_tail)
				panic("LSQ_head/LSQ_tail wedged");
		}
		//added for Wattch to clear hardware access counters
		for(unsigned int i=0;i<cores.size();i++)
		{
			cores[i].power.clear_access_stats();
		}

		//check if pipetracing is still active - DO NOT replicate this, only once!
		ptrace_check_active(contexts[current_context].regs.regs_PC, sim_num_insn, sim_cycle);

		//indicate new cycle in pipetrace
		ptrace_newcycle(sim_cycle);

		size_t empty_cores = 0;
		//Note, cores go one at a time, this should probably be changed
		for(size_t i=0;i<cores.size();i++)
		{
			if(cores[i].context_ids.empty())
			{	//The core has no contexts, nothing to do
				empty_cores++;
				continue;
			}
			
			//commit entries from ROB/LSQ to architected register file
			//commit COMMIT_WIDTH intsructions from each context each cycle
			commit(i);

			//Reduce busy time of in-use functional units by 1 cycle
			cores[i].update_fu();

			//==> may have ready queue entries carried over from previous cycles
			//service result completions, also readies dependent operations
			//==> inserts operations into ready queue --> register deps resolved
			writeback(i);

			//try to locate memory operations that are ready to execute
			//==> inserts operations into ready queue --> mem deps resolved
			//refresh each core, which refreshes each thread
			lsq_refresh(i);

			//issue operations ready to execute from a previous cycle
			//<== drains ready queue <-- ready operations commence execution
			//scheduling occurs in two phases: instruction wakeup and instruction selection
			//wakeup instructions once their source opearnds are ready (speculative on loads)
			wakeup(i);

			//select among the ready instructions for functional units and issue them to begin RF access
			selection(i);

			//actually begin the execution of instructions on the functional units
			execute(i);

			//dispatch instructions to the IQ
			iq_percentage = dispatch(i);

			//decode and rename new operations
			//==> insert ops w/ no deps or all regs ready --> reg deps resolved
			auto ss = register_rename(i, j);
			states[0] = std::get<0>(ss);
			states[1] = std::get<1>(ss);
			states[2] = std::get<2>(ss);
			states[3] = std::get<3>(ss);
			states[4] = std::get<4>(ss);
			fetch(cores[i].fetcher(i));
			j++;
			//std::cout << "j " << j << std::endl;
			if (j%1000 ==0)
				{reward = smt_print_stats();
				outFile.open("data.csv", std::ios_base::app);
				std::cout << "state/action/reward"<<'\t'<< states[1]<<'\t'<<states[2]<<'\t'<<states[3]<<'\t'<<states[4] << '\t' << iq_percentage<<'\t'<< states[0]<< '\t' << 2.3*reward << std::endl;
				outFile << states[1]<<','<<states[2]<<','<<states[3]<<','<<states[4] << ',' << iq_percentage<<','<< states[0]<< ',' << 2.3*reward << std::endl;
				outFile.close();
				}
		}
		

		//decrement the fetch-issue delay counters (used for min. branch mispred. penalty)
		for(int i=0;i<num_contexts;i++)
		{
			if(contexts[i].fetch_issue_delay)
			{
				contexts[i].fetch_issue_delay--;
			}
		}

		//Added for Wattch to update per-cycle power statistics
		for(unsigned int i=0;i<cores.size();i++)
		{
			cores[i].power.update_power_stats();
		}

		//go to next cycle
		sim_cycle++;

		//finished early? execute until the first thread reaches max_insts
		for(int i=0;i<num_contexts;i++)
		{
			if(contexts[i].sim_num_insn >= max_insts)
			{
				return;
			}
		}

		//If we reached the desired number of cycles
		if(sim_cycle==max_cycles)
		{
			return;
		}

		if(empty_cores == cores.size())
		{
			return;
		}
	}
	//outFile.close();
}

// Prints statistics at the end of simulation.
// This is the easiest place to add your own output.
double smt_print_stats()
{
	for(int i=0;i<num_contexts;i++)
	{
		//std::cerr << "Fast Forwarded: " << i << " (" << contexts[i].filename << "): " << contexts[i].fastfwd_cnt-contexts[i].fastfwd_left << std::endl;
	}

	//print my STATS
	std::cerr << "\n******* SMT STATS *******" << std::endl;
	std::cerr << "THROUGHPUT IPC: " << 2.3*sim_num_insn/(static_cast<double>(sim_cycle)) << "\n\n";
	std::cerr << "SIM CYCLE: " << static_cast<double>(sim_cycle) << "\n\n";
	for(int i=0;i<num_contexts;i++)
	{
		//std::cerr << "IPC " << i << " (" << contexts[i].filename << "): " << contexts[i].sim_num_insn/static_cast<double>(sim_cycle) << std::endl;
	}

	//FIXME: IPCs are horribly off for threads that were forked. This is because they run for a portion of the time but use sim_cycle as a divisor.
	//FIXME: Should we account for wait cycles in this?

	//std::cerr << "\n******* CMP STATS *******" << std::endl;
 	for(unsigned int i=0;i<cores.size();i++)
	{
		//fprintf(stderr,"\nCore %d IPC: %1.4f\n\n",i,cores[i].sim_num_insn_core / (double)sim_cycle);
		for(unsigned int j=0;j<cores[i].context_ids.size();j++)
		{
			//fprintf(stderr,"\tIPC %d (%s):\t%1.4f\n",j,contexts[cores[i].context_ids[j]].filename.c_str(),contexts[cores[i].context_ids[j]].sim_num_insn/static_cast<double>(sim_cycle));
		}
	}

	for(size_t i=0;i<contexts.size();i++)
	{
		//contexts[i].file_table.prettyprint(std::cerr);
		//std::cerr << std::endl;
	}
	return sim_num_insn/static_cast<double>(sim_cycle);
}
