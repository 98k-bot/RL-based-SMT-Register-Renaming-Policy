#ifndef BPRED_TWO_LEVEL_H
#define BPRED_TWO_LEVEL_H

#include"bpred.h"

class bpred_bpred_2Level : public bpred_t
{
	public:
		btb_t btb;

		int l1size;				//level-1 size, number of history regs
		int l2size;				//level-2 size, number of pred states
		int shift_width;			//amount of history in level-1 shift regs
		int XOR;				//history xor address flag
		std::vector<int> shiftregs;		//level-1 history table
		std::vector<unsigned char> l2table;	//level-2 prediction state table

		~bpred_bpred_2Level();

		bpred_bpred_2Level(
			unsigned int l1size,			//2lev l1 table size
			unsigned int l2size,			//2lev l2 table size
			unsigned int shift_width,		//history register width
			unsigned int XOR,			//history xor address flag
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

		void update_table(md_addr_t baddr, bool taken);

		void bpred_config(FILE *stream);		//print configuration to FILE*

		//Update a predictor entry - pointer to the entry (NULL if none)
		void update_state(char *p, bool taken);
};

#endif
