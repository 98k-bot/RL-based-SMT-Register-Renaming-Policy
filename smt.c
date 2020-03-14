/*
 * smt.h: Contains SMT definitions
 *
 * Author: Jason Loew <jloew@cs.binghamton.edu>, January 2009
 *
 * Author: Joseph J. Sharkey <jsharke@cs.binghamton.edu>, September 2005
 *
 */

#ifndef SMT_C
#define SMT_C

#include "smt.h"
#include<stdio.h>
#include<fcntl.h>

context::context()
: mem(NULL),
fetch_num(0), fetch_tail(0), fetch_head(0),
fetch_issue_delay(0), ptrace_seq(0), icount(0), sim_num_insn(0), rename_table(MD_TOTAL_REGS),
ROB_head(0),
ROB_tail(0), ROB_num(0), LSQ_head(0), LSQ_tail(0),
LSQ_num(0),
last_op((ROB_entry *)NULL),
spec_mode(FALSE),
pid(0), gpid(0), gid(0),
last_commit_cycle(0),
DCRA_int_iq(0), DCRA_int_rf(0), DCRA_fp_rf(0), DCRA_activity_fp(256), DCRA_L1_misses(0),
dlite_evaluator(NULL), 
sleep(0), interrupts(0), entry_point(0), waiting_for(0), nfds(0), next_check(0)
{
};

context::context(const context & source)
{
	if(source.mem)
	{
		mem = new mem_t(*(source.mem));
	}
	else
	{
		mem = NULL;
	}

	regs = source.regs;
	pred_PC = source.pred_PC;
	recover_PC = source.recover_PC;
	fetch_regs_PC = source.fetch_regs_PC;
	fetch_pred_PC = source.fetch_pred_PC;

	pred = source.pred;
	load_lat_pred = source.load_lat_pred;

	sim_num_insn = source.sim_num_insn;

	//Must be provided by the core
	IFQ = source.IFQ;
	rename_table = source.rename_table;
	ROB = source.ROB;
	LSQ = source.LSQ;

	//Unsafe to copy contexts with active IFQ entries
	fetch_num = source.fetch_num;
	fetch_tail = source.fetch_tail;
	fetch_head = source.fetch_head;
	ROB_head = source.ROB_head;
	ROB_tail = source.ROB_tail;
	ROB_num = source.ROB_num;
	LSQ_head = source.LSQ_head;
	LSQ_tail = source.LSQ_tail;
	LSQ_num = source.LSQ_num;

	fetch_issue_delay = source.fetch_issue_delay;
	fastfwd_cnt = source.fastfwd_cnt;
	fastfwd_left = source.fastfwd_left;

	ptrace_seq = source.ptrace_seq;
	icount = source.icount;

	//No last op
	last_op = source.last_op;

	spec_mode = source.spec_mode;

	//Provide an invalid id for now
	id = source.id;
	core_id = source.core_id;

	//This could be replaced with the fork/exec call
	filename = source.filename;

	last_commit_cycle = source.last_commit_cycle;

	DCRA_int_iq = source.DCRA_int_iq;
	DCRA_int_rf = source.DCRA_int_rf;
	DCRA_fp_rf = source.DCRA_fp_rf;
	DCRA_activity_fp = source.DCRA_activity_fp;
	DCRA_L1_misses = source.DCRA_L1_misses;

	//Get a new one?
	dlite_evaluator = source.dlite_evaluator;

	file_table = source.file_table;

	pid = source.pid;
	gpid = source.gpid;
	gid = source.gid;

	sleep = source.sleep;
	interrupts = source.interrupts;

	entry_point = source.entry_point;

	waiting_for = source.waiting_for;
	nfds = source.nfds;
	readfd = source.readfd;
	writefd = source.writefd;
	exceptfd = source.exceptfd;
	timeout = source.timeout;
	next_check = source.next_check;
}

context::~context()
{
	delete mem;
}

//initalizes the context
void context::init_context(int context_id)
{
	//core_id will be assigned by the core
	//Architectural register assignment is handled by the core

	//allocate and initialize memory space
	char str[20];
	sprintf(str,"Thread_%d_mem",context_id);
	mem = new mem_t(str);
	mem->ld_text_base = 0;
	mem->ld_text_size = 0;
	mem->ld_data_base = 0;
	mem->ld_brk_point = 0;
	mem->ld_data_size = 0;
	mem->ld_stack_base = 0;
	mem->ld_stack_size = 0;
	mem->ld_stack_min = -1;
	mem->ld_prog_fname = "";
	mem->ld_prog_entry = 0;
	mem->ld_environ_base = 0;
	mem->ld_target_big_endian = 0;
	mem->context_id = context_id;
	id = context_id;

	//This line seems useless
	regs.context_id = context_id;
}

int context::pendingLoadMisses(int cacheLevel)
{
	int retval = 0;
	for(unsigned int i=LSQ_head;i!=LSQ_tail;i=(i+1)%LSQ.size())
	{
		switch(cacheLevel)
		{
		case 1:
			if(LSQ[i].L1_miss)
				retval++;
			break;
		case 2:
			if(LSQ[i].L2_miss)
				retval++;
			break;
		case 3:
			if(LSQ[i].L3_miss)
				retval++;
			break;
		default:	//invalid cache level
			assert(0);
			break;
		};
	}
	return retval;
}


//dump contents of fetch stage registers and fetch queue to output stream
void context::fetch_dump(FILE *stream)
{
	if(!stream)
		stream = stderr;

	fprintf(stream, "** fetch stage state **\n");

	myfprintf(stream, "pred_PC: 0x%08p, recover_PC: 0x%08p\n", pred_PC, recover_PC);
	myfprintf(stream, "fetch_regs_PC: 0x%08p, fetch_pred_PC: 0x%08p\n", fetch_regs_PC, fetch_pred_PC);
	fprintf(stream, "\n");

	fprintf(stream, "** fetch queue contents **\n");
	fprintf(stream, "fetch_num: %d\n", fetch_num);
	fprintf(stream, "fetch_head: %d, fetch_tail: %d\n", fetch_head, fetch_tail);

	int num = fetch_num;
	int head = fetch_head;
	while(num)
	{
		fprintf(stream, "idx: %2d: inst: `", head);
		md_print_insn(IFQ[head].IR, IFQ[head].regs_PC, stream);
		fprintf(stream, "'\n");
		myfprintf(stream, "         regs_PC: 0x%08p, pred_PC: 0x%08p\n", IFQ[head].regs_PC, IFQ[head].pred_PC);
		head = (head + 1) & (IFQ.size() - 1);
		num--;
	}
}

void context::print_stats(FILE * stream)
{
	fprintf(stream,"sim_num_insn_%d                %lld # total number of instructions commited for this thread\n",      id, sim_num_insn);
}

#endif

