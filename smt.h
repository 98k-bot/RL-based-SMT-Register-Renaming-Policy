/*
 * smt.h: Contains SMT prototypes
 *
 * Author: Jason Loew <jloew@cs.binghamton.edu>, January 2009
 *
 * Author: Joseph J. Sharkey <jsharke@cs.binghamton.edu>, September 2005
 *
 */

#ifndef SMT_H
#define SMT_H

#include "regs.h"
#include "host.h"
#include "misc.h"
#include "machine.h"
#include "loader.h"
#include "regs.h"
#include "rob.h"
#include "bpreds.h"
#include "fetchtorename.h"
#include "regrename.h"
#include "file_table.h"
#include "dlite.h"

#include<vector>
#include<fstream>

class context
{
public:
	context();
	context(const context & source);
	~context();

	//initializes the context, provides context_id
	void init_context(int context_id);

	mem_t *mem;				//the address space of this context
	regs_t regs;				//the architectural registers for this context

	md_addr_t pred_PC, recover_PC;		//program counter (and recovery PC)

	md_addr_t fetch_regs_PC, fetch_pred_PC;	//fetch unit next fetch address (and predicted)
  
	bpred_t *pred;				//branch predictor - seperate for each thread

	bpred_t *load_lat_pred;			//load latency predictor - seperate for each thread
  
	std::vector<fetch_rec> IFQ;		// Instruction Fetch Queue
	unsigned int fetch_num;			// num entries in IFQ
	unsigned int fetch_tail, fetch_head;	// head and tail pointers of queue

	unsigned int fetch_issue_delay;		//cycles until fetch issue resumes due to I-Cache/TLB/Branch Misprediction

	long long fastfwd_cnt, fastfwd_left;	//the number of cycles to fast foward this thread, instructions left to fast forward

	unsigned long long ptrace_seq;		//pipetrace sequence number

	unsigned int icount;			//instruction count for the ICOUNT fetch policy

	long long sim_num_insn;			//total number of instructions commited for this thread

	std::vector<int> rename_table;		//the rename table for this context (size == MD_TOTAL_REGS)
  
	//Re-Order Buffer - organized as a circular queue
	std::vector<ROB_entry> ROB;		//Re-order buffer
	unsigned int ROB_head, ROB_tail;	//ROB head and tail pointers
	unsigned int ROB_num;			//num entries currently in ROB

	/* load/store queue (LSQ): holds loads and stores in program order, indicating
	* status of load/store access:
	*
	*   - issued: address computation complete, memory access in progress
	*   - completed: memory access has completed, stored value available
	*   - squashed: memory access was squashed, ignore this entry
	*
	* loads may execute when:
	*   1) register operands are ready, and
	*   2) memory operands are ready (no earlier unresolved store)
	*
	* loads are serviced by:
	*   1) previous store at same address in LSQ (hit latency), or
	*   2) data cache (hit latency + miss latency)
	*
	* stores may execute when:
	*   1) register operands are ready
	*
	* stores are serviced by:
	*   1) depositing store value into the load/store queue
	*   2) writing store value to the store buffer (plus tag check) at commit
	*   3) writing store buffer entry to data cache when cache is free
	*
	* NOTE: the load/store queue can bypass a store value to a load in the same
	*   cycle the store executes (using a bypass network), thus stores complete
	*   in effective zero time after their effective address is known
	*/
	std::vector<ROB_entry> LSQ;		//load/store queue
	unsigned int LSQ_head, LSQ_tail;	//LSQ head and tail pointers
	unsigned int LSQ_num;			//num entries currently in LSQ

	RS_link last_op;			//last_op for inorder issue
  
	bool spec_mode;				//is this context in "speculative" mode

	unsigned int id;			//context id
	int core_id;				//id of core this context is on
	std::string filename;			//the filename of the benchmark
	unsigned long long pid, gpid, gid;	//process, group and group process ids (as seen by the context)

	tick_t last_commit_cycle;		//last cycle an inst committed for this context

	//dump contents of fetch stage registers and fetch queue
	void fetch_dump(FILE *stream);		//output stream

	//returns the number of ROB insts that missed into a particular D-cache
	int pendingLoadMisses(int cacheLevel);

	//DCRA Counters
	unsigned int DCRA_int_iq, DCRA_int_rf, DCRA_fp_rf, DCRA_activity_fp;
	counter_t DCRA_L1_misses;

	dlite_t *dlite_evaluator;		//dlite expression evaluator

	file_table_t file_table;


	unsigned long long sleep;		//Number of cycles to sleep for usleep_thread

	long long interrupts;			//Non-zero if the context needs something handled.
						//0x0000 0000 0000 0000 0000 0000 0000 0001	Dynamically Loaded - fast forward to entry point
						//0x0000 0000 0000 0000 0000 0000 0001 0000	Syscall Wait - check wait parameter
						//0x0000 0000 0000 0000 0000 0000 0002 0000	Syscall Select - check select parameter
						//0x0000 0000 0000 0000 0000 0001 0000 0000	Abort current instruction (generated at register_rename by Syscall execve)

	//Non-null for Dynamic executables
	md_addr_t entry_point;

	//Wait: Who are we waiting for?
	unsigned long long waiting_for;

	//Select: 
	int nfds;
	fd_set readfd, writefd, exceptfd;
	timeval timeout;
	long long next_check;

	void print_stats(FILE * stream);
};

#endif
