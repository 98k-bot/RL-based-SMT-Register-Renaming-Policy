#include"bpred_taken.h"

bpred_bpred_taken::bpred_bpred_taken()
: bpred_t("taken")
{}

md_addr_t bpred_bpred_taken::bpred_lookup(md_addr_t,
	md_addr_t btarget,					//branch target, if taken
	md_opcode op,						//opcode of instruction
	bool, bool,
	bpred_update_t *dir_update_ptr, 			//pred state pointer
	int*)
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

	return btarget;
}

void bpred_bpred_taken::bpred_update(md_addr_t,
	md_addr_t,
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
}

//print branch predictor configuration to output strean
void bpred_bpred_taken::bpred_config(FILE *stream)
{
	fprintf(stream, "pred_dir: %s: predict taken\n", name.c_str());
}

