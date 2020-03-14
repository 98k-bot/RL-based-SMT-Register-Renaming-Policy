#ifndef BPRED_NOT_TAKEN_H
#define BPRED_NOT_TAKEN_H

#include"bpred.h"

class bpred_bpred_not_taken : public bpred_t
{
	public:
		bpred_bpred_not_taken();

		md_addr_t bpred_lookup(md_addr_t baddr,		//branch address
			md_addr_t btarget,			//branch target if taken
			md_opcode op,				//opcode of instruction
			bool, bool,
			bpred_update_t *dir_update_ptr, 	//pred state pointer
			int*);

		void bpred_update(md_addr_t,
	     		md_addr_t,
	     		bool taken,				//non-zero if branch was taken
	     		bool pred_taken,			//non-zero if branch was pred taken
	     		bool correct,				//was earlier prediction correct?
	     		md_opcode op,				//opcode of instruction
	     		bpred_update_t *dir_update_ptr);	//pred state pointer

		void bpred_config(FILE *stream);		//print configuration to FILE*
};


#endif
