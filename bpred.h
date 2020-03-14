/* bpred.h - branch predictor interfaces */

/* SimpleScalar(TM) Tool Suite
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 * All Rights Reserved.
 *
 * THIS IS A LEGAL DOCUMENT, BY USING SIMPLESCALAR,
 * YOU ARE AGREEING TO THESE TERMS AND CONDITIONS.
 *
 * No portion of this work may be used by any commercial entity, or for any
 * commercial purpose, without the prior, written permission of SimpleScalar,
 * LLC (info@simplescalar.com). Nonprofit and noncommercial use is permitted
 * as described below.
 *
 * 1. SimpleScalar is provided AS IS, with no warranty of any kind, express
 * or implied. The user of the program accepts full responsibility for the
 * application of the program and the use of any results.
 *
 * 2. Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 * downloaded, compiled, executed, copied, and modified solely for nonprofit,
 * educational, noncommercial research, and noncommercial scholarship
 * purposes provided that this notice in its entirety accompanies all copies.
 * Copies of the modified software can be delivered to persons who use it
 * solely for nonprofit, educational, noncommercial research, and
 * noncommercial scholarship purposes provided that this notice in its
 * entirety accompanies all copies.
 *
 * 3. ALL COMMERCIAL USE, AND ALL USE BY FOR PROFIT ENTITIES, IS EXPRESSLY
 * PROHIBITED WITHOUT A LICENSE FROM SIMPLESCALAR, LLC (info@simplescalar.com).
 *
 * 4. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 *
 * 5. Noncommercial and nonprofit users may distribute copies of SimpleScalar
 * in compiled or executable form as set forth in Section 2, provided that
 * either: (A) it is accompanied by the corresponding machine-readable source
 * code, or (B) it is accompanied by a written offer, with no time limit, to
 * give anyone a machine-readable copy of the corresponding source code in
 * return for reimbursement of the cost of distribution. This written offer
 * must permit verbatim duplication by anyone, or (C) it is distributed by
 * someone who received only the executable form, and is accompanied by a
 * copy of the written offer of source code.
 *
 * 6. SimpleScalar was developed by Todd M. Austin, Ph.D. The tool suite is
 * currently maintained by SimpleScalar LLC (info@simplescalar.com). US Mail:
 * 2395 Timbercrest Court, Ann Arbor, MI 48105.
 *
 * Copyright (C) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.
 */


#ifndef BPRED_H
#define BPRED_H

#include"host.h"
#include"stats.h"
#include"retstack.h"

//branch predictor update information
class bpred_update_t
{
	public:
		bpred_update_t()
		: pdir1(NULL), pdir2(NULL), pmeta(NULL)
		{}

		char *pdir1;					//direction-1 predictor counter
		char *pdir2;					//direction-2 predictor counter
		char *pmeta;					//meta predictor counter
		class dir_t 
		{
			public:
				//Default values for this constructor don't make sense
				dir_t()
				: ras(false), bimod(false), twolev(false), meta(false)
				{}
				//predicted directions
				bool ras;		//RAS used
				bool bimod;		//bimodal predictor
				bool twolev;		//2-level predictor
				bool meta;		//meta predictor (0..bimod / 1..2lev)
		} dir;
};

//branch predictor definition
class bpred_t
{
	public:
		counter_t addr_hits;			//num correct addr-predictions
		counter_t dir_hits;			//num correct dir-predictions (incl addr)
		counter_t used_ras;			//num RAS predictions used
		counter_t jr_hits;			//num correct addr-predictions for JR's
		counter_t jr_seen;			//num JR's seen
		counter_t jr_non_ras_hits;		//num correct addr-preds for non-RAS JR's
		counter_t jr_non_ras_seen;		//num non-RAS JR's seen
		counter_t misses;			//num incorrect predictions
		counter_t lookups;			//num lookups
		counter_t ras_hits;			//num correct return-address predictions

		std::string name;			//Indicates the type of branch predictor
		retstack_t retstack;			//Return address stack

		//create/destroy a branch predictor
		bpred_t();
		bpred_t(std::string name, unsigned int retstack_size = 0);
		virtual ~bpred_t();

		//probe a predictor for a next fetch address, the predictor is probed with branch address BADDR,
		//the branch target is BTARGET (used for static predictors), and OP is the instruction
		//opcode (used to simulate predecode bits; a pointer to the predictor state entry (or null
		//for jumps) is returned in *DIR_UPDATE_PTR (used for updating predictor state), and the
		//non-speculative top-of-stack is returned in stack_recover_idx (used for recovering
		//ret-addr stack after mispredict). Returns predicted branch target address
		virtual md_addr_t bpred_lookup(md_addr_t baddr,	//branch address
	     		md_addr_t btarget,			//branch target if taken
	     		md_opcode op,				//opcode of instruction
	     		bool is_call,				//non-zero if inst is fn call
	     		bool is_return,				//non-zero if inst is fn return
	     		bpred_update_t *dir_update_ptr, 	//pred state pointer
	     		int *stack_recover_idx) = 0;		//Non-speculative top-of-stack; used on mispredict recovery

		//Comment needs updating, BTBs are not required.
		//update the branch predictor, only useful for stateful predictors; updates entry for instruction
		//type OP at address BADDR. BTB only gets updated for branches which are taken. Inst was
		//determined to jump to address BTARGET and was taken if TAKEN is non-zero. Predictor
		//statistics are updated with result of prediction, indicated by CORRECT and PRED_TAKEN,
		//predictor state to be updated is indicated by *DIR_UPDATE_PTR (may be NULL for jumps,
		//which shouldn't modify state bits). Note if bpred_update is done speculatively, branch-prediction may get polluted.
		virtual void bpred_update(md_addr_t baddr,	//branch address
	     		md_addr_t btarget,			//resolved branch target
	     		bool taken,				//non-zero if branch was taken
	     		bool pred_taken,			//non-zero if branch was pred taken
	     		bool correct,				//was earlier prediction correct?
	     		md_opcode op,				//opcode of instruction
	     		bpred_update_t *dir_update_ptr)	= 0;	//pred state pointer

		//reset stats after priming, if appropriate
		void reset();

		//register branch predictor stats with sdb (stat database) using name as an identifier
		void bpred_reg_stats(stat_sdb_t *sdb, const char * name);
};

#endif
