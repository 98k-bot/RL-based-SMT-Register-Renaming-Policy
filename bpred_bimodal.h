#ifndef BPRED_BIMODAL_H
#define BPRED_BIMODAL_H

#include"bpred.h"

class bpred_bpred_2bit : public bpred_t
{
	public:
		btb_t btb;

		int l1size;				//level-1 size, number of history regs
		unsigned int size;			//number of entries in direct-mapped table
		std::vector<unsigned char> table;	//prediction state table

		~bpred_bpred_2bit();

		bpred_bpred_2bit(
			unsigned int bimod_size,		//bimod table size
			unsigned int btb_sets,			//number of sets in BTB
			unsigned int btb_assoc,			//BTB associativity
			unsigned int retstack_size);		//num entries in ret-addr stack

		char * get_index(md_addr_t baddr);

		md_addr_t bpred_lookup(md_addr_t baddr,		//branch address
			md_addr_t btarget,			//branch target if taken
			md_opcode op,				//opcode of instruction
			bool is_call,				//non-zero if inst is fn call
			bool is_return,				//non-zero if inst is fn return
			bpred_update_t *dir_update_ptr, 	//pred state pointer
			int *stack_recover_idx);		//Non-speculative top-of-stack; used on mispredict recovery

		void bpred_update(md_addr_t baddr,		//branch address
	     		md_addr_t btarget,			//resolved branch target
	     		bool taken,				//non-zero if branch was taken
	     		bool pred_taken,			//non-zero if branch was pred taken
	     		bool correct,				//was earlier prediction correct?
	     		md_opcode op,				//opcode of instruction
	     		bpred_update_t *dir_update_ptr);	//pred state pointer

		void bpred_config(FILE *stream);		//print configuration to FILE*

		//Update a predictor entry - pointer to the entry (NULL if none)
		void update_state(char *p, bool taken);
};


#endif
