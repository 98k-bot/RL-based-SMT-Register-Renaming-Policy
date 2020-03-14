#ifndef RETSTACK_H
#define RETSTACK_H

#include"machine.h"
#include"host.h"
#include"misc.h"
#include"stats.h"
#include"btb.h"
#include<vector>

class retstack_t
{
	public:
		retstack_t(size_t retstack_size);

		size_t size;			//return-address stack size
		size_t tos;			//top-of-stack
		std::vector<bpred_btb_ent_t> stack;	//return-address stack

		counter_t pops;			//number of times a value was popped
		counter_t pushes;		//number of times a value was pushed

		//returns top of return address stack - for rollback
		size_t TOS();

		//Speculative execution can corrupt the ret-addr stack. So for each lookup we return the
		//top-of-stack (TOS) at that point; a mispredicted branch, as part of its recovery,
		//restores the TOS using this value -- hopefully this uncorrupts the stack.
		//Non-speculative top-of-stack; used on mispredict recovery
		void recover(int stack_recover_idx);

		md_addr_t pop();
		void push(md_addr_t baddr);

		void reg_stats(stat_sdb_t *sdb, const char *name);

		//Resets statistics
		void reset();

		//Resets stack and top of stack
		void clear();
};

#endif
