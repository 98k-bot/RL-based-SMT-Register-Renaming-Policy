#include"bpred_two_level.h"

//turn this on to enable the SimpleScalar 2.0 RAS bug
//#define RAS_BUG_COMPATIBLE

bpred_bpred_2Level::bpred_bpred_2Level(
	unsigned int l1size,                    //2lev l1 table size
	unsigned int l2size,                    //2lev l2 table size
	unsigned int shift_width,               //history register width
	unsigned int XOR,                       //history xor address flag
	unsigned int btb_sets,                  //number of sets in BTB
	unsigned int btb_assoc,                 //BTB associativity
	unsigned int retstack_size)             //num entries in ret-addr stack
: bpred_t("2lev",retstack_size), btb(btb_sets,btb_assoc), l1size(l1size), l2size(l2size), shift_width(shift_width), XOR(XOR)
{
	if(!l1size || (l1size & (l1size-1)) != 0)
	{
		fatal("level-1 size, `%d', must be non-zero and a power of two", l1size);
	}
	if(!l2size || (l2size & (l2size-1)) != 0)
	{
		fatal("level-2 size, `%d', must be non-zero and a power of two", l2size);
	}
	if(!shift_width || shift_width > 30)
	{
		fatal("shift register width, `%d', must be non-zero and positive", shift_width);
	}
	shiftregs.resize(l1size);

	l2table.resize(l2size);

	//initialize counters to weakly this-or-that
	int flipflop = 1;
	for(unsigned int cnt = 0; cnt < l2size; cnt++)
	{
		l2table[cnt] = flipflop;
		flipflop = 3 - flipflop;
	}
}

bpred_bpred_2Level::~bpred_bpred_2Level()
{}

char * bpred_bpred_2Level::get_index(md_addr_t baddr)
{
	int l1index = (baddr >> MD_BR_SHIFT) & (l1size - 1);
	int l2index = shiftregs[l1index];
	if(XOR)
	{
		//this L2 index computation is more "compatible" to McFarling's version of it, i.e., if the PC xor address component is only
		//part of the index, take the lower order address bits for theother part of the index, rather than the higher order ones
		l2index = (((l2index ^ (baddr >> MD_BR_SHIFT)) & ((1 << shift_width) - 1)) | ((baddr >> MD_BR_SHIFT) << shift_width));
		//l2index = l2index ^ (baddr >> MD_BR_SHIFT);
	}
	else
	{
		l2index = l2index | ((baddr >> MD_BR_SHIFT) << shift_width);
	}
	l2index = l2index & (l2size - 1);
	return (char *)&l2table[l2index];
}

md_addr_t bpred_bpred_2Level::bpred_lookup(md_addr_t baddr,		//branch address
	md_addr_t btarget,						//branch target if taken
	md_opcode op,							//opcode of instruction
	bool is_call,							//non-zero if inst is fn call
	bool is_return,							//non-zero if inst is fn return
	bpred_update_t *dir_update_ptr, 				//pred state pointer
	int *stack_recover_idx)						//Non-speculative top-of-stack; used on mispredict recovery
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

void bpred_bpred_2Level::update_state(char *p, bool taken)
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

void bpred_bpred_2Level::bpred_update(md_addr_t baddr,		//branch address
	md_addr_t btarget,					//resolved branch target
	bool taken,						//non-zero if branch was taken
	bool pred_taken,					//non-zero if branch was pred taken
	bool correct,						//was earlier prediction correct?
	md_opcode op,						//opcode of instruction
	bpred_update_t *dir_update_ptr)				//pred state pointer
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

	//update L1 table if appropriate
	if((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
		update_table(baddr,taken);
	}

	bpred_btb_ent_t *pbtb = btb.update_pbtb(taken,baddr);

	update_state(dir_update_ptr->pdir1, taken);

	btb.update(pbtb, taken, baddr, correct, op, btarget);
}

void bpred_bpred_2Level::update_table(md_addr_t baddr, bool taken)
{
	int l1index = (baddr >> MD_BR_SHIFT) & (l1size - 1);
	int shift_reg = (shiftregs[l1index] << 1) | taken;
	shiftregs[l1index] = shift_reg & ((1 << shift_width) - 1);
}

//print branch predictor configuration to output strean
void bpred_bpred_2Level::bpred_config(FILE *stream)
{
	fprintf(stream, "pred_dir: %s: 2-lvl: %d l1-sz, %d bits/ent, %s XOR, %d l2-sz, direct-mapped\n", name.c_str(), l1size, shift_width, XOR ? "" : "no", l2size);
	fprintf(stream, "btb: %ld sets x %ld associativity", btb.sets, btb.assoc);
	fprintf(stream, "ret_stack: %ld entries", retstack.stack.size());
}

