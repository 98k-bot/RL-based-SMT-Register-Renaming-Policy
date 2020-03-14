/*
 * cmp.c: Contains CMP definitions
 *
 * Author: Jason Loew <jloew@cs.binghamton.edu>, January 2009
 *
 */

#ifndef CMP_C
#define CMP_C

#include "cmp.h"

#include<iostream>
#include<algorithm>

core_t::core_t()
: max_contexts(1),
	cache_dl1(NULL), cache_il1(NULL), cache_dl2(NULL), cache_il2(NULL),
	itlb(NULL), dtlb(NULL),
	sim_slip(0), sim_num_insn_core(0), sim_total_insn(0), sim_num_refs(0), sim_total_refs(0), sim_num_loads(0), sim_total_loads(0), sim_num_branches(0), sim_total_branches(0),
	pred_perfect(FALSE),
	write_buf_size(16),
	main_mem(NULL),
	bimod_nelt(1),cbimod_nelt(1),twolev_nelt(4),ctwolev_nelt(4),
	comb_nelt(1),ccomb_nelt(1),btb_nelt(2),cbtb_nelt(2),
	lock_flag(FALSE),locked_physical_address(0)
{
	bimod_config[0] = 2048;
	twolev_config[0] = 1;
	twolev_config[1] = 1024;
	twolev_config[2] = 12;
	twolev_config[3] = 0;
	comb_config[0] = 1024;
	btb_config[0] = 512;
	btb_config[1] = 4;
	cbimod_config[0] = 2048;
	ctwolev_config[0] = 1;
	ctwolev_config[1] = 1024;
	ctwolev_config[2] = 12;
	ctwolev_config[3] = 0;
	ccomb_config[0] = 1024;
	cbtb_config[0] = 512;
	cbtb_config[1] = 4;
};

#if 1
//check processor_power
//should rebuild caches?
//contexts may be damaged when we change contexts
//bpreds?
//res_pool * fu_pool
//fetch policy, recovery model
core_t::core_t(const core_t & rhs)
: id(rhs.id), max_contexts(rhs.max_contexts), power(rhs.power),
	context_ids(rhs.context_ids),
	cache_dl1(rhs.cache_dl1), cache_il1(rhs.cache_il1), cache_dl2(rhs.cache_dl2), cache_il2(rhs.cache_il2),
	itlb(rhs.itlb), dtlb(rhs.dtlb),

	cache_dl1_lat(rhs.cache_dl1_lat), cache_il1_lat(rhs.cache_il1_lat), cache_dl2_lat(rhs.cache_dl2_lat), cache_il2_lat(rhs.cache_il2_lat),
	tlb_miss_lat(rhs.tlb_miss_lat),

	bpred_misprediction_penalty(rhs.bpred_misprediction_penalty),

	pred(rhs.pred), load_lat_pred(rhs.load_lat_pred), ROB(rhs.ROB), LSQ(rhs.LSQ), IFQ(rhs.IFQ),
	pred_list(rhs.pred_list), load_lat_pred_list(rhs.load_lat_pred_list), ROB_list(rhs.ROB_list), LSQ_list(rhs.LSQ_list), IFQ_list(rhs.IFQ_list),

	fetcher(rhs.fetcher),
	reg_file(rhs.reg_file),
	ready_queue(rhs.ready_queue), event_queue(rhs.event_queue), waiting_queue(rhs.waiting_queue), issue_exec_queue(rhs.issue_exec_queue),
	iq(rhs.iq), fu_CMP(rhs.fu_CMP), fu_pool(rhs.fu_pool),

	sim_slip(rhs.sim_slip), sim_num_insn_core(rhs.sim_num_insn_core), sim_total_insn(rhs.sim_total_insn), sim_num_refs(rhs.sim_num_refs), sim_total_refs(rhs.sim_total_refs),
	sim_num_loads(rhs.sim_num_loads), sim_total_loads(rhs.sim_total_loads), sim_num_branches(rhs.sim_num_branches), sim_total_branches(rhs.sim_total_branches),
	int_regs(rhs.int_regs), fp_regs(rhs.fp_regs),

	commit_width(rhs.commit_width), issue_width(rhs.issue_width), decode_width(rhs.decode_width), fetch_speed(rhs.fetch_speed),
	FETCH_RENAME_DELAY(rhs.FETCH_RENAME_DELAY), RENAME_DISPATCH_DELAY(rhs.RENAME_DISPATCH_DELAY), ISSUE_EXEC_DELAY(rhs.ISSUE_EXEC_DELAY),

	inorder_issue(rhs.inorder_issue), include_spec(rhs.include_spec),

	rf_size(rhs.rf_size), ROB_size(rhs.ROB_size), LSQ_size(rhs.LSQ_size), iq_size(rhs.iq_size), fetch_policy(rhs.fetch_policy),
	recovery_model_v(rhs.recovery_model_v), recovery_model(rhs.recovery_model), res_ialu(rhs.res_ialu), res_imult(rhs.res_imult), res_memport(rhs.res_memport), res_fpalu(rhs.res_fpalu), res_fpmult(rhs.res_fpmult),

	pred_perfect(rhs.pred_perfect),

	write_buf_size(rhs.write_buf_size), write_buf(rhs.write_buf),

	cache_dl1_opt(rhs.cache_dl1_opt), cache_dl2_opt(rhs.cache_dl2_opt), cache_il1_opt(rhs.cache_il1_opt), cache_il2_opt(rhs.cache_il2_opt), itlb_opt(rhs.itlb_opt), dtlb_opt(rhs.dtlb_opt),

	pred_type(rhs.pred_type), cpred_type(rhs.cpred_type), ras_size(rhs.ras_size), cras_size(rhs.cras_size), bpred_spec_opt(rhs.bpred_spec_opt), bpred_spec_update(rhs.bpred_spec_update),
	main_mem(rhs.main_mem), main_mem_config(rhs.main_mem_config),

	bimod_nelt(rhs.bimod_nelt),cbimod_nelt(rhs.cbimod_nelt),twolev_nelt(rhs.twolev_nelt),ctwolev_nelt(rhs.ctwolev_nelt),
	comb_nelt(rhs.comb_nelt),ccomb_nelt(rhs.ccomb_nelt),btb_nelt(rhs.btb_nelt),cbtb_nelt(rhs.cbtb_nelt),
	lock_flag(rhs.lock_flag),locked_physical_address(rhs.locked_physical_address)
{
	bimod_config[0] = rhs.bimod_config[0];
	twolev_config[0] = rhs.twolev_config[0];
	twolev_config[1] = rhs.twolev_config[1];
	twolev_config[2] = rhs.twolev_config[2];
	twolev_config[3] = rhs.twolev_config[3];
	comb_config[0] = rhs.comb_config[0];
	btb_config[0] = rhs.btb_config[0];
	btb_config[1] = rhs.btb_config[1];
	cbimod_config[0] = rhs.cbimod_config[0];
	ctwolev_config[0] = rhs.ctwolev_config[0];
	ctwolev_config[1] = rhs.ctwolev_config[1];
	ctwolev_config[2] = rhs.ctwolev_config[2];
	ctwolev_config[3] = rhs.ctwolev_config[3];
	ccomb_config[0] = rhs.ccomb_config[0];
	cbtb_config[0] = rhs.cbtb_config[0];
	cbtb_config[1] = rhs.cbtb_config[1];
}
#endif

core_t::~core_t()
{};

void core_t::Clear_Entry_From_Queues(ROB_entry * entry)
{
	event_queue.remove(entry);
	ready_queue.remove(entry);
	waiting_queue.remove(entry);
	issue_exec_queue.remove(entry);
}

void core_t::reserveArch(int contexts_per_core)
{
	max_contexts=contexts_per_core;
	for(unsigned int i=0;i<(32*max_contexts);i++)
	{
		reg_file.reg_file_access(i,REG_INT).state = REG_ARCH;
		reg_file.reg_file_access(i,REG_FP).state = REG_ARCH;
                reg_file.reg_file_access(i,REG_INT).ready = 0;
                reg_file.reg_file_access(i,REG_FP).ready = 0;
                reg_file.reg_file_access(i,REG_INT).spec_ready = 0;
                reg_file.reg_file_access(i,REG_FP).spec_ready = 0;
		int_regs.push_back(i);
		fp_regs.push_back(i);
	}
}

std::vector<int> core_t::getArch()
{
	std::vector<int> retval;
	//If these assertions failed, there weren't enough registers to add another core
	assert(int_regs.size()>=32);
	assert(fp_regs.size()>=32);
	for(unsigned int i=(int_regs.size()-32);i<int_regs.size();i++)
	{
		assert(reg_file.reg_file_access(int_regs[i],REG_INT).state == REG_ARCH);
		assert(reg_file.reg_file_access(int_regs[i],REG_INT).ready == 0);
		assert(reg_file.reg_file_access(int_regs[i],REG_INT).spec_ready == 0);
		retval.push_back(int_regs[i]);
	}
	int_regs.resize(int_regs.size()-32);
	for(unsigned int i=(fp_regs.size()-32);i<fp_regs.size();i++)
	{
		assert(reg_file.reg_file_access(fp_regs[i],REG_FP).state == REG_ARCH);
		assert(reg_file.reg_file_access(fp_regs[i],REG_FP).ready == 0);
		assert(reg_file.reg_file_access(fp_regs[i],REG_FP).spec_ready == 0);
		retval.push_back(fp_regs[i]);
	}
	fp_regs.resize(fp_regs.size()-32);
	return retval;
}

void core_t::returnArch(context & thecontext)
{
	assert(thecontext.ROB_num==0);
	assert(thecontext.icount==0);
	unsigned int i;
	for(i=0;i<32;i++)
	{
		int reg = thecontext.rename_table[i];
		assert(reg_file.reg_file_access(reg,REG_INT).state == REG_ARCH);
                reg_file.reg_file_access(reg,REG_INT).ready = 0;
                reg_file.reg_file_access(reg,REG_INT).spec_ready = 0;
		int_regs.push_back(reg);
	}
	for(;i<64;i++)
	{
		int reg = thecontext.rename_table[i];
		assert(reg_file.reg_file_access(reg,REG_FP).state == REG_ARCH);
                reg_file.reg_file_access(reg,REG_FP).ready = 0;
                reg_file.reg_file_access(reg,REG_FP).spec_ready = 0;
		fp_regs.push_back(reg);
	}
}

bool core_t::addcontext(context & thecontext)
{
	assert(thecontext.ROB_num==0);
	assert(thecontext.icount==0);
	if((1+context_ids.size())>max_contexts)
	{
		return FALSE;
	}

	int location = context_ids.size();
	context_ids.push_back(thecontext.id);

	thecontext.pred = pred[location];
	thecontext.load_lat_pred = load_lat_pred[location];
	thecontext.ROB.resize(ROB[location]);
	thecontext.LSQ.resize(LSQ[location]);
	thecontext.IFQ.resize(IFQ[location]);
	
	pred_list[location] = thecontext.id;
	load_lat_pred_list[location] = thecontext.id;
	ROB_list[location] = thecontext.id;
	LSQ_list[location] = thecontext.id;
	IFQ_list[location] = thecontext.id;

	std::vector<int> newreg = getArch();
	for(int i=0;i<32;i++)
	{
		thecontext.rename_table[i] = newreg[i];
		thecontext.rename_table[i+32] = newreg[i+32];
	}

	thecontext.core_id = id;
	return TRUE;
}

bool core_t::ejectcontext(context & thecontext)
{
	assert(thecontext.ROB_num==0);
	assert(thecontext.icount==0);
	int context_id = thecontext.id;
	std::vector<int>::iterator it = find(context_ids.begin(),context_ids.end(),context_id);
	if(it==context_ids.end())
	{
		return FALSE;
	}
	int index = it - context_ids.begin();

	context_ids.erase(it);
	
	bpred_t * temp_pred = pred[index];
	pred.erase(pred.begin()+index);
	pred.push_back(temp_pred);
	pred_list.erase(pred_list.begin()+index);
	pred_list.push_back(-1);						

	bpred_t * temp_load_lat_pred = load_lat_pred[index];
	load_lat_pred.erase(load_lat_pred.begin()+index);
	load_lat_pred.push_back(temp_load_lat_pred);
	load_lat_pred_list.erase(load_lat_pred_list.begin()+index);
	load_lat_pred_list.push_back(-1);						
	
	int temp_ROB = ROB[index];
	ROB.erase(ROB.begin()+index);
	ROB.push_back(temp_ROB);
	ROB_list.erase(ROB_list.begin()+index);
	ROB_list.push_back(-1);

	int temp_LSQ = LSQ[index];
	LSQ.erase(LSQ.begin()+index);
	LSQ.push_back(temp_LSQ);
	LSQ_list.erase(LSQ_list.begin()+index);
	LSQ_list.push_back(-1);

	int temp_IFQ = IFQ[index];
	IFQ.erase(IFQ.begin()+index);
	IFQ.push_back(temp_IFQ);
	IFQ_list.erase(IFQ_list.begin()+index);
	IFQ_list.push_back(-1);

	thecontext.core_id = -1;

	returnArch(thecontext);

	return TRUE;
}

bool core_t::TransferContext(context & thecontext, counter_t & sim_num_insn, core_t & target)
{
	flushcontext(thecontext,sim_num_insn);
	if(!ejectcontext(thecontext))
	{
		return FALSE;
	}
	if(!target.addcontext(thecontext))
	{
		return FALSE;
	}
	return TRUE;
}

void core_t::flushcontext(context & target, counter_t & sim_num_insn)
{
	//Turn off spec_mode for the context, it is being flushed, spec_mode needs to be removed if applied.
	target.spec_mode = 0;

	int ROB_index = target.ROB_tail, LSQ_index = target.LSQ_tail;
	int ROB_prev_tail = target.ROB_tail, LSQ_prev_tail = target.LSQ_tail;
	assert(static_cast<int>(id)==target.core_id);

	md_addr_t recover_PC = 0;

	ROB_index = (ROB_index + (target.ROB.size()-1)) % target.ROB.size();
	LSQ_index = (LSQ_index + (target.LSQ.size()-1)) % target.LSQ.size();

	if(target.ROB_num>0)
	{
		recover_PC = target.ROB[target.ROB_head].PC;
		if(target.pred)
		{
			target.pred->retstack.recover(target.ROB[target.ROB_head].stack_recover_idx);
		}
	}

	//Clean out the ROB
	while(target.ROB_num>0)
	{
		sim_num_insn--;
		sim_num_insn_core--;

		//is this operation an effective address calculation for a load or store?
		if(target.ROB[ROB_index].ea_comp)
		{
			//should be at least one load or store in the LSQ
			if(!target.LSQ_num)
				panic("ROB and LSQ out of sync");

			assert(target.LSQ[LSQ_index].physreg == target.ROB[ROB_index].physreg);

			//Here is where we perform memory rollback
			if(target.LSQ[LSQ_index].is_store)
			{	
				qword_t temp = MD_SWAPQ(target.LSQ[LSQ_index].previous_mem);
				md_addr_t addr = target.LSQ[LSQ_index].addr;

				switch(target.LSQ[LSQ_index].data_size)
				{
					case 8:
						target.mem->mem_access_direct(Write,addr,&temp,8);
						break;
					case 4:
						target.mem->mem_access_direct(Write,addr,&temp,4);
						break;
					case 2:
						target.mem->mem_access_direct(Write,addr,&temp,2);
						break;
					case 1:
						target.mem->mem_access_direct(Write,addr,&temp,1);
						break;
					default:
						fatal("Bad data_size value!");
				}
			}

			//squash this LSQ entry and indicate this in pipetrace
			Clear_Entry_From_Queues(&target.LSQ[LSQ_index]);
			ptrace_endinst(target.LSQ[LSQ_index].ptrace_seq);

			//go to next earlier LSQ slot
			LSQ_prev_tail = LSQ_index;
			LSQ_index = (LSQ_index + (target.LSQ.size()-1)) % target.LSQ.size();
			target.LSQ_num--;
		}

		//squash this ROB entry and indicate this in pipetrace
		Clear_Entry_From_Queues(&target.ROB[ROB_index]);
		ptrace_endinst(target.ROB[ROB_index].ptrace_seq);

		//if the instruction is in the IQ, then free the IQ entry
		if(target.ROB[ROB_index].in_IQ >= 1){
			iq.free_iq_entry(target.ROB[ROB_index].iq_entry_num);
			target.ROB[ROB_index].in_IQ = FALSE;
			target.icount--;
			assert(target.icount <= (target.IFQ.size() + target.ROB.size()));
			/************ DCRA ***************/
			target.DCRA_int_iq--;
			assert(target.DCRA_int_iq >= 0);
			/*********************************/
		} 
		else if(!target.ROB[ROB_index].dispatched)
		{
			target.icount--;
			assert(target.icount <= (target.IFQ.size() + target.ROB.size()));
		}

		//release physical registers
		if(target.ROB[ROB_index].physreg >= 0){
			assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_FREE);
			assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_ARCH);
			reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state=REG_FREE;

			//rollback the rename table
			target.rename_table[target.ROB[ROB_index].archreg] = target.ROB[ROB_index].old_physreg;
			target.ROB[ROB_index].physreg = -1;

			/*********** DCRA ************/
			if(target.ROB[ROB_index].dest_format == REG_INT)
			{
				target.DCRA_int_rf--;
				assert(target.DCRA_int_rf >= 0);
			}
			else
			{
				target.DCRA_fp_rf--;
				assert(target.DCRA_fp_rf >= 0);
			}
			/*****************************/

			int index = target.ROB[ROB_index].regs_index;
			if(index>=32)
			{
				target.regs.regs_F[index-32] = target.ROB[ROB_index].regs_F;
			}	
			else
			{
				target.regs.regs_R[index] = target.ROB[ROB_index].regs_R;
			}
		}

		target.regs.regs_C = target.ROB[ROB_index].regs_C;

		//go to next earlier slot in the ROB
		ROB_prev_tail = ROB_index;
		ROB_index = (ROB_index + (target.ROB.size()-1)) % target.ROB.size();
		target.ROB_num--;
	}
	assert(target.ROB_num==0);
	assert(target.LSQ_num==0);
	target.ROB_tail = target.ROB_head = 0;
	target.LSQ_tail = target.LSQ_head = 0;

	target.icount -= target.fetch_num;
	assert(target.icount <= (target.IFQ.size() + target.ROB.size()));
	while(target.fetch_num)
	{
		//if pipetracing, indicate squash of instructions in the inst fetch queue
		if(ptrace_active)
		{
			//squash the next instruction from the IFETCH -> RENAME queue
			ptrace_endinst(target.IFQ[target.fetch_head].ptrace_seq);
		}
		if(recover_PC==0)
		{
			recover_PC = target.IFQ[target.fetch_head].regs_PC;
		}
		//consume instruction from IFETCH -> RENAME queue
		target.fetch_head = (target.fetch_head+1) & (target.IFQ.size() - 1);
		target.fetch_num--;
	}

	target.fetch_head = target.fetch_tail = 0;

	if(recover_PC==0)
	{
		recover_PC = target.fetch_pred_PC;
	}

	target.fetch_pred_PC = target.fetch_regs_PC = target.recover_PC = recover_PC;
}

//Rollback the target thread such that ROB_entry is at the head of the ROB.
//This will always leave at least 1 instruction in the ROB!
void core_t::rollbackTo(context & target, counter_t & sim_num_insn, ROB_entry * targetinst, bool branch_misprediction)
{
	if(branch_misprediction)
	{	//if a branch misprediction, reset spec_mode
		target.spec_mode = 0;
	}

	int ROB_index = target.ROB_tail, LSQ_index = target.LSQ_tail;
	int ROB_prev_tail = target.ROB_tail, LSQ_prev_tail = target.LSQ_tail;
	assert(static_cast<int>(id)==target.core_id);

	ROB_index = (ROB_index + (target.ROB.size()-1)) % target.ROB.size();
	LSQ_index = (LSQ_index + (target.LSQ.size()-1)) % target.LSQ.size();

	//Since we have an ROB entry, we know what the next_PC will be.
	//This is ok even if a branch_misprediction.
	md_addr_t recover_PC = targetinst->next_PC;

	//if there is a branch predictor, rollback the prediction return stack
	if(target.pred)
	{
		target.pred->retstack.recover(targetinst->stack_recover_idx);
	}

	//traverse the older insts until the targetinst is encountered
	while(&target.ROB[ROB_index]!=targetinst)
	{
		sim_num_insn--;
		sim_num_insn_core--;

		//is this operation an effective addr calc for a load or store?
		if(target.ROB[ROB_index].ea_comp)
		{
			//should be at least one load or store in the LSQ
			if(!target.LSQ_num)
				panic("ROB and LSQ out of sync");

			assert(target.LSQ[LSQ_index].physreg == target.ROB[ROB_index].physreg);

			//Here is where we perform memory rollback
			if(target.LSQ[LSQ_index].is_store)
			{	
				qword_t temp = MD_SWAPQ(target.LSQ[LSQ_index].previous_mem);
				md_addr_t addr = target.LSQ[LSQ_index].addr;

				switch(target.LSQ[LSQ_index].data_size)
				{
					case 8:
						target.mem->mem_access_direct(Write,addr,&temp,8);
						break;
					case 4:
						target.mem->mem_access_direct(Write,addr,&temp,4);
						break;
					case 2:
						target.mem->mem_access_direct(Write,addr,&temp,2);
						break;
					case 1:
						target.mem->mem_access_direct(Write,addr,&temp,1);
						break;
					default:
						fatal("Bad data_size value!");
				}
			}

			//squash this LSQ entry and indicate this in pipetrace
			Clear_Entry_From_Queues(&target.LSQ[LSQ_index]);
			ptrace_endinst(target.LSQ[LSQ_index].ptrace_seq);

			//go to next earlier LSQ slot, LSQ_tail will be fixed after this loop
			LSQ_prev_tail = LSQ_index;
			LSQ_index = (LSQ_index + (target.LSQ.size()-1)) % target.LSQ.size();
			target.LSQ_num--;
			target.LSQ_tail--;
		}

		//squash this ROB entry and indicate this in pipetrace
		Clear_Entry_From_Queues(&target.ROB[ROB_index]);
		ptrace_endinst(target.ROB[ROB_index].ptrace_seq);

		//if the instruction is in the IQ, then free the IQ entry
		if(target.ROB[ROB_index].in_IQ >= 1){
			iq.free_iq_entry(target.ROB[ROB_index].iq_entry_num);
			target.ROB[ROB_index].in_IQ = FALSE;
			target.icount--;
			assert(target.icount <= (target.IFQ.size() + target.ROB.size()));
			/************ DCRA ***************/
			target.DCRA_int_iq--;
			assert(target.DCRA_int_iq >= 0);
			/*********************************/
		} 
		else if(!target.ROB[ROB_index].dispatched)
		{
			target.icount--;
			assert(target.icount <= (target.IFQ.size() + target.ROB.size()));
		}

		//release physical registers
		if(target.ROB[ROB_index].physreg >= 0){
			assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_FREE);
			assert(reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state!=REG_ARCH);
			reg_file.reg_file_access(target.ROB[ROB_index].physreg,target.ROB[ROB_index].dest_format).state=REG_FREE;

			//rollback the rename table
			target.rename_table[target.ROB[ROB_index].archreg] = target.ROB[ROB_index].old_physreg;
			target.ROB[ROB_index].physreg = -1;

			/*********** DCRA ************/
			if(target.ROB[ROB_index].dest_format == REG_INT)
			{
				target.DCRA_int_rf--;
				assert(target.DCRA_int_rf >= 0);
			}
			else
			{
				target.DCRA_fp_rf--;
				assert(target.DCRA_fp_rf >= 0);
			}
			/*****************************/

			int index = target.ROB[ROB_index].regs_index;
			if(index>=32)
			{
				target.regs.regs_F[index-32] = target.ROB[ROB_index].regs_F;
			}	
			else
			{
				target.regs.regs_R[index] = target.ROB[ROB_index].regs_R;
			}
		}

		target.regs.regs_C = target.ROB[ROB_index].regs_C;

		//go to next earlier slot in the ROB, ROB_tail will be fixed after this loop
		ROB_prev_tail = ROB_index;
		ROB_index = (ROB_index + (target.ROB.size()-1)) % target.ROB.size();
		target.ROB_num--;
		target.ROB_tail--;
	}
	if(target.ROB_tail>=target.ROB.size())
	{	//ROB_tail is an unsigned int, so, we can't check negative values
		target.ROB_tail += target.ROB.size();
	}
	if(target.LSQ_tail>=target.LSQ.size())
	{	//LSQ_tail is an unsigned int, so, we can't check negative values
		target.LSQ_tail += target.LSQ.size();
	}

	target.icount -= target.fetch_num;
	assert(target.icount <= (target.IFQ.size() + target.ROB.size()));
	while(target.fetch_num)
	{
		//if pipetracing, indicate squash of instructions in the inst fetch queue
		if(ptrace_active)
		{
			//squash the next instruction from the IFETCH -> RENAME queue
			ptrace_endinst(target.IFQ[target.fetch_head].ptrace_seq);
		}
		//consume instruction from IFETCH -> RENAME queue
		target.fetch_head = (target.fetch_head+1) & (target.IFQ.size() - 1);
		target.fetch_num--;
	}

	target.fetch_head = target.fetch_tail = 0;

	target.fetch_pred_PC = target.fetch_regs_PC = target.recover_PC = recover_PC;

	//Generate branch misprediction penalty
	target.fetch_issue_delay = MAX(target.fetch_issue_delay, bpred_misprediction_penalty);
}

void core_t::print_stats(FILE * stream, counter_t sim_cycle)
{
	fprintf(stream,"sim_num_insn_core_%d           %lld # total number of instructions executed\n",      id, sim_num_insn_core);
	fprintf(stream,"sim_num_ref_%d                 %lld # total number of loads and stores committed\n", id, sim_num_refs);
	fprintf(stream,"sim_num_loads_%d               %lld # total number of loads committed\n",            id, sim_num_loads);
	fprintf(stream,"sim_num_stores_%d              %lld # total number of stores committed\n",           id, sim_num_refs - sim_num_loads);
	fprintf(stream,"sim_num_branches_%d            %lld # total number of branches committed\n",         id, sim_num_branches);
	fprintf(stream,"sim_total_insn_%d              %lld # total number of instructions executed\n",      id, sim_total_insn);
	fprintf(stream,"sim_total_refs_%d              %lld # total number of loads and stores executed\n",  id, sim_total_refs);
	fprintf(stream,"sim_total_loads_%d             %lld # total number of loads executed\n",             id, sim_total_loads);
	fprintf(stream,"sim_total_stores_%d            %lld # total number of stores executed\n",            id, sim_total_refs - sim_total_loads);
	fprintf(stream,"sim_total_branches_%d          %lld # total number of branches executed\n",          id, sim_total_branches);
	if(sim_cycle)
	{
		fprintf(stream,"sim_IPC_%d                       %f # instructions per cycle\n",                     id, (double)sim_num_insn_core / (double)sim_cycle);
		fprintf(stream,"sim_exec_BW_%d                   %f # total instructions (mis-spec + committed)\n",  id, (double)sim_total_insn / (double)sim_cycle);
	}
	if(sim_num_insn_core)
	{
		fprintf(stream,"sim_CPI_%d                       %f # cycles per instruction\n",                     id, (double)sim_cycle / (double)sim_num_insn_core);
	}
	if(sim_num_branches)
	{
		fprintf(stream,"sim_IPB_%d                       %f # instructions per branch\n",                    id, (double)sim_num_insn_core / (double)sim_num_branches);
	}
	fprintf(stream,"sim_slip_%d                    %lld # total number of slip cycles\n",                id, sim_slip);
	if(sim_num_insn_core)
	{
		fprintf(stream,"avg_sim_slip_%d                  %f # average slip between issue and retirement\n",  id, (double)sim_slip / (double)sim_num_insn_core);
	}
}

void core_t::wait_q_enqueue(ROB_entry *rs, tick_t when)
{
	if (rs->completed)
		panic("instruction completed");
	assert(!rs->queued);

        RS_link n_link(rs);
        n_link.x.when = when;
	waiting_queue.inorderinsert(n_link);
}

ROB_entry * core_t::issue_exec_q_next_event(tick_t sim_cycle)
{
	if(issue_exec_queue.empty())
	{
		return NULL;
	}
	RS_link event = *issue_exec_queue.begin();
	if(event.x.when <= sim_cycle)
	{
		ROB_entry *rs = event.rs;
		issue_exec_queue.erase(issue_exec_queue.begin());
		return rs;
	}
	return NULL;
}

void core_t::eventq_queue_event(ROB_entry *rs, tick_t when, tick_t sim_cycle)
{
        if(rs->completed)
        {
                assert(!rs->completed);
        }
	assert(when>sim_cycle);	//else event occurred in the past

        RS_link n_link(rs);
        n_link.x.when = when;
	event_queue.inorderinsert(n_link);
}

ROB_entry * core_t::eventq_next_event(tick_t sim_cycle)
{
        if(event_queue.empty())
        {
                return NULL;
        }
        RS_link event = *(event_queue.begin());
        if(event.x.when <= sim_cycle)
        {
                event_queue.erase(event_queue.begin());
                return event.rs;
        }
        return NULL;
}

//If a functional unit is busy this cycle (call this once a cycle), decrement the busy counter by 1.
void core_t::update_fu()
{
	//walk all resource units, decrement busy counts by one
	for(int i=0; i<fu_pool->num_resources; i++)
	{
		//resource is released when BUSY hits zero
		if(fu_pool->resources[i].busy > 0)
		{
			fu_pool->resources[i].busy--;
		}
	}
}

#endif
