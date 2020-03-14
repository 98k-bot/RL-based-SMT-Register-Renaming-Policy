#include"bpred_bimodal.h"

//turn this on to enable the SimpleScalar 2.0 RAS bug
//#define RAS_BUG_COMPATIBLE

bpred_bpred_2bit::bpred_bpred_2bit(
	unsigned int bimod_size,		//bimod table size
	unsigned int btb_sets,			//number of sets in BTB
	unsigned int btb_assoc,			//BTB associativity
	unsigned int retstack_size)		//num entries in ret-addr stack
: bpred_t("bimod",retstack_size), btb(btb_sets,btb_assoc), size(bimod_size)
{
	if(!size || (size & (size-1)) != 0)
	{
		fatal("2bit table size, `%d', must be non-zero and a power of two", bimod_size);
	}
	table.resize(size);

	//initialize counters to weakly this-or-that
	int flipflop = 1;
	for(unsigned int cnt = 0; cnt < size; cnt++)
	{
		table[cnt] = flipflop;
		flipflop = 3 - flipflop;
	}
}

bpred_bpred_2bit::~bpred_bpred_2bit()
{}

char * bpred_bpred_2bit::get_index(md_addr_t baddr)
{
	return (char *)&table[((baddr >> 19) ^ (baddr >> MD_BR_SHIFT)) & (table.size()-1)];
}

md_addr_t bpred_bpred_2bit::bpred_lookup(md_addr_t baddr,	//branch address
	md_addr_t btarget,					//branch target if taken
	md_opcode op,						//opcode of instruction
	bool is_call,						//non-zero if inst is fn call
	bool is_return,						//non-zero if inst is fn return
	bpred_update_t *dir_update_ptr, 			//pred state pointer
	int *stack_recover_idx)					//Non-speculative top-of-stack; used on mispredict recovery
{
	if(!dir_update_ptr)
	{
		panic("no bpred update record");
	}

	if(!(MD_OP_FLAGS(op) & F_CTRL))
	{
		return 0;
	}

	lookups++;
	dir_update_ptr->dir.ras = FALSE;
	dir_update_ptr->pdir1 = dir_update_ptr->pdir2 = dir_update_ptr->pmeta = NULL;

	//get a pointer to prediction state information
	if((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
		dir_update_ptr->pdir1 = get_index(baddr);
	}

	//Handle Retstack: set stack_recover_idx to top of retstack (or 0 if the stack is size 0)
	*stack_recover_idx = retstack.TOS();

	//if this is a return, pop return-address stack
	if(is_return && retstack.size)
	{
		md_addr_t target = retstack.pop();
		dir_update_ptr->dir.ras = TRUE; /* using RAS here */
		return target;
	}

#ifndef RAS_BUG_COMPATIBLE
	//if function call, push return-address onto return-address stack
	if(is_call && retstack.size)
	{
		retstack.push(baddr);
	}
#endif
	//Get a pointer into the BTB
	bpred_btb_ent_t *pbtb = btb.find_pbtb(baddr);

	//We now also have a pointer into the BTB for a hit, or NULL otherwise

	//if this is a jump, ignore predicted direction; we know it's taken.
	if((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) == (F_CTRL|F_UNCOND))
	{
		return (pbtb ? pbtb->target : 1);
	}

	//otherwise we have a conditional branch
	if(*(dir_update_ptr->pdir1) <= 1)
	{
		//Prediction was not taken
		return 0;
	}
	//Prediction was taken, return address (if we have it).
	return (pbtb ? pbtb->target : 1);
}

void bpred_bpred_2bit::update_state(char *p, bool taken)
{
	//update state (but not for jumps)
	if(p)
	{
		if(taken)
		{
			if(*p < 3)
				(*p)++;
		}
		else
		{
			if(*p > 0)
				(*p)--;
		}
	}
}

void bpred_bpred_2bit::bpred_update(md_addr_t baddr,		//branch address
	md_addr_t btarget,			//resolved branch target
	bool taken,				//non-zero if branch was taken
	bool pred_taken,			//non-zero if branch was pred taken
	bool correct,				//was earlier prediction correct?
	md_opcode op,				//opcode of instruction
	bpred_update_t *dir_update_ptr)		//pred state pointer
{
	if(!(MD_OP_FLAGS(op) & F_CTRL))
	{
		return;
	}

	addr_hits += correct;
	dir_hits += (pred_taken == taken);
	misses += (pred_taken != taken);

	if(dir_update_ptr->dir.ras)
	{
		used_ras++;
		ras_hits += correct;
	}

	if(MD_IS_INDIR(op))
	{
		jr_seen++;
		jr_hits += correct;

		if(!dir_update_ptr->dir.ras)
		{
			jr_non_ras_seen++;
			jr_non_ras_hits += correct;
		}
		else
		{
			//used return address stack, nothing else to do
			return;
		}
	}
#ifdef RAS_BUG_COMPATIBLE
	//if function call, push return-address onto return-address stack
	if(MD_IS_CALL(op) && retstack.size)
	{
		retstack.push(baddr);
	}
#endif
	bpred_btb_ent_t *pbtb = btb.update_pbtb(taken,baddr);

	update_state(dir_update_ptr->pdir1, taken);

	btb.update(pbtb, taken, baddr, correct, op, btarget);
}

//print branch predictor configuration to output strean
void bpred_bpred_2bit::bpred_config(FILE *stream)
{
	fprintf(stream, "pred_dir: %s: 2-bit: %d entries, direct-mapped\n", name.c_str(), size);
	fprintf(stream, "btb: %ld sets x %ld associativity",  btb.sets, btb.assoc);
	fprintf(stream, "ret_stack: %ld entries", retstack.stack.size());
}
