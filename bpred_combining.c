#include"bpred_combining.h"

//turn this on to enable the SimpleScalar 2.0 RAS bug
//#define RAS_BUG_COMPATIBLE

bpred_bpred_comb::bpred_bpred_comb(
	unsigned int bimod_size,		//bimod table size
	unsigned int l1size,                    //2lev l1 table size
	unsigned int l2size,                    //2lev l2 table size
	unsigned int meta_size,			//meta table size
	unsigned int shift_width,               //history register width
	unsigned int XOR,                       //history xor address flag
	unsigned int btb_sets,                  //number of sets in BTB
	unsigned int btb_assoc,                 //BTB associativity
	unsigned int retstack_size)             //num entries in ret-addr stack
: bpred_t("comb",retstack_size), btb(btb_sets,btb_assoc), bimod(bimod_size,0,0,0), meta(meta_size,0,0,0), twolev(l1size,l2size,shift_width,XOR,0,0,0)
{}

md_addr_t bpred_bpred_comb::bpred_lookup(md_addr_t baddr,		//branch address
	md_addr_t btarget,			//branch target if taken
	md_opcode op,				//opcode of instruction
	bool is_call,				//non-zero if inst is fn call
	bool is_return,				//non-zero if inst is fn return
	bpred_update_t *dir_update_ptr, 	//pred state pointer
	int *stack_recover_idx)			//Non-speculative top-of-stack; used on mispredict recovery
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

	//Except for jumps, get a pointer to direction-prediction bits
	if((MD_OP_FLAGS(op) & (F_CTRL|F_UNCOND)) != (F_CTRL|F_UNCOND))
	{
		//get a pointer to prediction state information
		char *pbimod = (char *)bimod.get_index(baddr);
		char *pmeta = (char *)meta.get_index(baddr);
		char *ptwolev = (char *)twolev.get_index(baddr);

		dir_update_ptr->pmeta = pmeta;
		dir_update_ptr->dir.meta  = (*pmeta >= 2);
		dir_update_ptr->dir.bimod = (*pbimod >= 2);
		dir_update_ptr->dir.twolev  = (*ptwolev >= 2);
		if(*pmeta >= 2)
		{
			dir_update_ptr->pdir1 = ptwolev;
			dir_update_ptr->pdir2 = pbimod;
		}
		else
		{
			dir_update_ptr->pdir1 = pbimod;
			dir_update_ptr->pdir2 = ptwolev;
		}
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
	//not a return. Get a pointer into the BTB
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

void bpred_bpred_comb::update_state(char *p, bool taken)
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

void bpred_bpred_comb::bpred_reg_stats(stat_sdb_t *sdb, const char *name)	/* stats database */
{
	char buf[512];

	bpred_t::bpred_reg_stats(sdb,name);

	sprintf(buf, "%s.used_bimod", name);
	stat_reg_counter(sdb, buf, "total number of bimodal predictions used", &used_bimod, 0, NULL);
	sprintf(buf, "%s.used_2lev", name);
	stat_reg_counter(sdb, buf, "total number of 2-level predictions used", &used_2lev, 0, NULL);
}

void bpred_bpred_comb::reset()
{
	used_bimod = used_2lev = 0;
	bpred_t::reset();
}

void bpred_bpred_comb::bpred_update(md_addr_t baddr,		//branch address
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
	else
	{
		if(dir_update_ptr->dir.meta)
		{
			used_2lev++;
		}
		else
		{
			used_bimod++;
		}
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
	//L1 table is updated unconditionally for combining predictor too
	twolev.update_table(baddr,taken);

	bpred_btb_ent_t *pbtb = btb.update_pbtb(taken,baddr);

	update_state(dir_update_ptr->pdir1, taken);
	update_state(dir_update_ptr->pdir2, taken);
	//meta predictor
	if(dir_update_ptr->pmeta)
	{
		if(dir_update_ptr->dir.bimod != dir_update_ptr->dir.twolev)
		{
			//we only update meta predictor if directions were different
			if(dir_update_ptr->dir.twolev == taken)
			{
				//2-level predictor was correct
				if(*dir_update_ptr->pmeta < 3)
					++*dir_update_ptr->pmeta;
			}
			else
			{
				//bimodal predictor was correct
				if(*dir_update_ptr->pmeta > 0)
					--*dir_update_ptr->pmeta;
			}
		}
	}
	btb.update(pbtb, taken, baddr, correct, op, btarget);
}

//print branch predictor configuration to output strean
void bpred_bpred_comb::bpred_config(FILE *stream)
{
	bimod.bpred_config(stream);
	twolev.bpred_config(stream);
	meta.bpred_config(stream);
	fprintf(stream, "btb: %ld sets x %ld associativity",  btb.sets, btb.assoc);
	fprintf(stream, "ret_stack: %ld entries", retstack.stack.size());
}

