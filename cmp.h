/*
 * cmp.c: Contains CMP prototypes
 *
 * Author: Jason Loew <jloew@cs.binghamton.edu>, January 2009
 *
 */

#ifndef CMP_H
#define CMP_H

#include"smt.h"
#include"iq.h"
#include"power.h"
#include"inflightq.h"
#include"resource.h"
#include"ptrace.h"
#include"rob.h"
#include"dram.h"
#include<vector>
#include<list>
#include<set>

class core_t
{
	public:
		core_t();
		core_t(const core_t & source);
		~core_t();

		unsigned int id;				//The core's id number

		unsigned int max_contexts;			//The maximum number of contexts that this core can handle

		processor_power power;				//Wattch power model

		std::vector<int> context_ids;			//The "context_id"s associated with the core

		//Caches: i->instruction, d->data followed by type (l1, l2, tlb)
		cache_t *cache_dl1,*cache_il1;
		cache_t *cache_dl2,*cache_il2;
		cache_t *itlb,*dtlb;

		//Cache Latencies: In cycles. TLB latency is shared amongst both TLBs
		int cache_dl1_lat, cache_il1_lat;
		int cache_dl2_lat, cache_il2_lat;
		int tlb_miss_lat;

		//Branch misprediction penalty
		unsigned int bpred_misprediction_penalty;

		//memory access latency, assumed to not cross a page boundary
		unsigned int mem_access_latency(md_addr_t addr, int size, tick_t when, int context_id);

		//address space?

	//Per core objects, NOTE: ROB, LSQ and IFQ are just sizes used to initialize a context's resources
		std::vector<bpred_t *> pred;
		std::vector<bpred_t *> load_lat_pred;
		std::vector<int> ROB;
		std::vector<int> LSQ;
		std::vector<int> IFQ;

		//For each "Per core object", this is the list of who is using that resource
		std::vector<int> pred_list;
		std::vector<int> load_lat_pred_list;
		std::vector<int> ROB_list;
		std::vector<int> LSQ_list;
		std::vector<int> IFQ_list;

	//Shared objects
		//Fetch function pointer for the core, all contexts use this
		std::vector<int> (*fetcher)(unsigned int);

		reg_file_t reg_file;				//The integer and floating-point physical register file (rename tables are in the context)

		//The Issue Queue, shared structure, all associated data structures are here
		//Note: ready_queue uses a different sorting mechanism (as per templated constructor)
		inflight_queue_t<RS_link,seq_sort> ready_queue;
		inflight_queue_t<RS_link> event_queue,waiting_queue,issue_exec_queue;
		issue_queue_t iq;

		//Issue Queue functionality
		//waiting instruction queue - instructions wait here until all their source operands are marked as ready
		void wait_q_enqueue(ROB_entry *rs, tick_t when);

		//Execution queue - these instructions are going to execute
		ROB_entry * issue_exec_q_next_event(tick_t sim_cycle);

		//Writeback queue - instructions that have completed execution go here to wait for in-order writeback
		//Priority Queue: Instructions leave at time "when".
		void eventq_queue_event(ROB_entry *rs, tick_t when, tick_t sim_cycle);

		//Writeback queue - any instruction that has completed execution is here
		//When these instructions can writeback, they exit this queue. Otherwise, it returns NULL.
		ROB_entry * eventq_next_event(tick_t sim_cycle);

	//Resource Pool: These are the configurations of the functional units
		std::vector<res_desc> fu_CMP;

		res_pool* fu_pool;				//These are the actual functional units

		void update_fu();				//Releases functional units if they are no longer busy, called each cycle

	//Simulator statistic variables
		counter_t sim_slip;				//SLIP variable
		counter_t sim_num_insn_core;			//Number of instructions committed per core
		counter_t sim_total_insn;			//total number of instructions executed
		counter_t sim_num_refs;				//total number of memory references committed
		counter_t sim_total_refs;			//total number of memory references executed
		counter_t sim_num_loads;			//total number of loads committed
		counter_t sim_total_loads;			//total number of loads executed
		counter_t sim_num_branches;			//total number of branches committed
		counter_t sim_total_branches;			//total number of branches executed
		void print_stats(FILE * stream, counter_t sim_cycle);	//prints statistics to output stream

	//Architectural Register State
		//This determines how many contexts a core can handle as well as creates the pool of architectural registers
		//Call this at core initialization time
		void reserveArch(int contexts_per_core);

		//These are lists of the archtectural registers
		std::vector<int> int_regs, fp_regs;

		//Returns a vector with 32 int Arch registers and 32 fp Arch registers
		//These represent a set of architectural registers for a context to use
		//These must be given to the context, this function allocates those registers for use!
		std::vector<int> getArch();

	//Core Migration
		//Transfers a context and its instruction counter to another core
		bool TransferContext(context & thecontext, counter_t & sim_num_insn, core_t & target);

	//Context Allocation
		//Allocates private resources to a context and connects shared resources
		//The context is now part of the core, returns false is this failed
		bool addcontext(context & thecontext);

	//Misprediction Handling (and pipeline flushing/rollback)
		//Completely flushes a context, similar to rollbackTo, but this clears the ROB completely
		//FIXME: This is not needed, we should be able to replicate this behavior using rollbackTo
		void flushcontext(context & target, counter_t & sim_num_insn);

		//Rollback mechanism: Handles branch misprediction rollback
		//Must provide the instruction counter in order to properly adjust it in regard to evicted instructions
		//Rollbacks each instruction one at a time until targetinst is reached, which is not removed
		//If branch_misprediction, rolls back the RAS and fixes the PC accordingly.
		//FIXME: Can not rollback past certain syscalls, none of these have been identified yet
		void rollbackTo(context & target, counter_t & sim_num_insn, ROB_entry * targetinst, bool branch_misprediction);

		//Squashes an in-flight instruction
		void Clear_Entry_From_Queues(ROB_entry * entry);

	//Core Properties:
		//Bandwidths: (insts per cycle)
		unsigned int commit_width, issue_width, decode_width;
		//speed of front-end of machine relative to execution core (fetch_width ratio to decode)
		int fetch_speed;

		//Pipeline stage delays
		int FETCH_RENAME_DELAY, RENAME_DISPATCH_DELAY, ISSUE_EXEC_DELAY;

		int inorder_issue;				//bool: run pipeline with in-order issue?
		int include_spec;				//bool: issue instructions down wrong execution paths?

	//Size variables for initialization steps (sim_reg_options, options.[ch])
	//These are currently required to handle initialization but are later superceded (in most cases)
		unsigned int rf_size;				//size of the physical register file (INT and FP)
		unsigned int ROB_size, LSQ_size;		//Reorder buffer (ROB) and load/store queue (LSQ) sizes
		unsigned int iq_size;				//total size of the issue queue
		char * fetch_policy;				//fetch policy option string

		//ALPHA 21264 recovery string
		enum recovery_model_t {RECOVERY_MODEL_UNDEFINED, RECOVERY_MODEL_SQUASH, RECOVERY_MODEL_PERFECT} recovery_model_v;
		char *recovery_model;

		unsigned int res_ialu, res_imult;		//total number of integer FUs available: ALUs, multiplier/dividers
		unsigned int res_memport;			//total number of memory system ports available (to CPU)
		unsigned int res_fpalu,res_fpmult;		//total number of floating point FUs available: ALUs, multiplier/dividers

		bool pred_perfect;

		unsigned int write_buf_size;
		std::set<tick_t> write_buf;

		char *cache_dl1_opt, *cache_dl2_opt;		//L1 and L2 D-cache config, i.e., {<config>|none}
		char *cache_il1_opt, *cache_il2_opt;		//L1 and L2 I-cache config, i.e., {<config>|dl1|dl2|dl3|none}
		char *itlb_opt, *dtlb_opt;			//i-TLB and d-TLB config, i.e., {<config>|none}

		//branch predictor type {nottaken|taken|perfect|bimod|2lev}
		//and load-latency predictor (same variable names but prefaced with "c")
		char *pred_type, *cpred_type;

		int ras_size, cras_size;			//return address stack (RAS) size

		//speculative bpred-update-enabled string
		char * bpred_spec_opt;
		enum bpred_spec_update_t {spec_ID,spec_WB,spec_CT} bpred_spec_update;

		//Main memory model
		dram_t * main_mem;
		char * main_mem_config;

		//bimodal predictor config (<table_size>)
		int bimod_nelt, cbimod_nelt;
		int bimod_config[1], cbimod_config[1];

		//2-level predictor config (<l1size> <l2size> <hist_size> <xor>)
		int twolev_nelt, ctwolev_nelt;
		int twolev_config[4], ctwolev_config[4];

		//combining predictor config (<meta_table_size>
		int comb_nelt, ccomb_nelt;
		int comb_config[1], ccomb_config[1];

		//BTB predictor config (<num_sets> <associativity>)
		int btb_nelt, cbtb_nelt;
		int btb_config[2], cbtb_config[2];

		//Lock Registers:
		bool lock_flag;
		md_addr_t locked_physical_address;

private:
public:
		//Part of TransferContext: Removes the context from the core by deallocating its private resources
		//and disconnecting it from the shared resources
		bool ejectcontext(context & thecontext);

private:
		//Part of TransferContext: Returns a context's architectural registers to the core
		//Call after flushing a context only!
		void returnArch(context & thecontext);
};

#endif
