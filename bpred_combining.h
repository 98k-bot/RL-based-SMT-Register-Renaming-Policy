#ifndef BPRED_COMBINING_H
#define BPRED_COMBINING_H

#include"bpred.h"
#include"bpred_two_level.h"
#include"bpred_bimodal.h"

class bpred_bpred_comb : public bpred_t
{
	public:
		btb_t btb;

		bpred_bpred_2bit bimod, meta;
		bpred_bpred_2Level twolev;

		counter_t used_bimod;			//num bimodal predictions used
		counter_t used_2lev;			//num 2-level predictions used

		bpred_bpred_comb(
			unsigned int bimod_size,		//bimod table size
			unsigned int l1size,                    //2lev l1 table size
			unsigned int l2size,                    //2lev l2 table size
			unsigned int meta_size,			//meta table size
			unsigned int shift_width,               //history register width
			unsigned int XOR,                       //history xor address flag
			unsigned int btb_sets,                  //number of sets in BTB
			unsigned int btb_assoc,                 //BTB associativity
			unsigned int retstack_size);             //num entries in ret-addr stack

		md_addr_t bpred_lookup(md_addr_t baddr,		//branch address
			md_addr_t btarget,			//branch target if taken
			md_opcode op,				//opcode of instruction
			bool is_call,				//non-zero if inst is fn call
			bool is_return,				//non-zero if inst is fn return
			bpred_update_t *dir_update_ptr, 	//pred state pointer
			int *stack_recover_idx);			//Non-speculative top-of-stack; used on mispredict recovery

		void bpred_update(md_addr_t baddr,		//branch address
	     		md_addr_t btarget,			//resolved branch target
	     		bool taken,				//non-zero if branch was taken
	     		bool pred_taken,			//non-zero if branch was pred taken
	     		bool correct,				//was earlier prediction correct?
	     		md_opcode op,				//opcode of instruction
	     		bpred_update_t *dir_update_ptr);	//pred state pointer

		void bpred_config(FILE *stream);

		//Update a predictor entry - pointer to the entry (NULL if none)
		void update_state(char *p, bool taken);

		void bpred_reg_stats(stat_sdb_t *sdb, const char *name);
		void reset();
};

#endif
