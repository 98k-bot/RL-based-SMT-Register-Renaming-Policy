// ReOrder Buffer Prototype and RS_Link Prototype (a wrapper for ROB/LSQ entries in the inflight queues)

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


#ifndef ROB_H
#define ROB_H

#include "regs.h"
#include "bpreds.h"
#include<list>

//inst sequence type, used to order instructions in the ready list, if
//	this rolls over the ready list order temporarily will get messed up,
//	but execution will continue and complete correctly
typedef unsigned long long INST_SEQ_TYPE;

//total output dependencies possible, only 1 of the 2 used by Alpha
#define MAX_ODEPS               2

//A re-order buffer (ROB) entry, this record is contained in the program order.
//	NOTE: the ROB and LSQ share the same structure, this is useful because
//	loads and stores are split into two operations: an effective address
//	add and a load/store, the add is inserted into the ROB and the load/store
//	inserted into the LSQ, allowing the add to wake up the load/store when
//	effective address computation has finished
class ROB_entry {
public:
	ROB_entry();

	//instruction info
	md_inst_t IR;					//instruction bits
	enum md_opcode op;				//decoded instruction opcode
	md_addr_t PC, next_PC, pred_PC;			//inst PC, next PC, predicted PC
	int in_LSQ;
	int LSQ_index;					//non-zero if op is in LSQ
	int ea_comp;					//non-zero if op is an addr comp
	int recover_inst;				//start of mis-speculation?
	int stack_recover_idx;				//non-speculative TOS for RSB pred
	bpred_update_t dir_update;			//bpred direction update info
	int spec_mode;					//non-zero if issued in spec_mode
	md_addr_t addr;					//effective address for ld/st's
	INST_SEQ_TYPE seq;				//instruction sequence, used to sort the ready list and tag inst
	unsigned long long ptrace_seq;			//pipetrace sequence number

	//Wattch: values of source operands and result operand used for AF generation
	quad_t val_ra, val_rb, val_rc, val_ra_result;

	int slip;
	int exec_lat;					//execution latency

	//instruction status
	int dispatched;
	int queued;					//operands ready and queued
	int issued;					//operation is/was executing
	int completed;					//operation has completed execution
	int replayed;					//operation has been replayed due to load speculation

	int context_id;					//the id of the context this entry belongs to
	int iq_entry_num;				//the IQ entry number allocated to this entry (or -1 if no entry is allocated)
	int in_IQ;					//flag - is the instruction currently in the IQ?
	long long disp_cycle;				//the cycle this instruction was dispatched
	long long rename_cycle;				//the cycle this instruction was renameded

	int physreg;					//the physical register assigned for the instructions destination (-1 if NA)
	int old_physreg;				//the physical register assigned for the instructions destination (-1 if NA)
	int src_physreg[2];				//physical register sources for the inst. (-1 if NA)

	enum reg_type dest_format;			//the type of destination register used (none, int, or fp)

	int archreg;					//the architectural register destination (0 if NA)
	int src_archreg[2];				//the architectural source registers (0 if NA)

	//useful instruction state bits
	int L1_miss,L2_miss,L3_miss;			//Did this instruction miss into one of the D-caches?

	//For walk-through rollback, this should ultimately replace spec_mode
	//These should represent the old values, and must be retained before instruction execution
	qword_t regs_R;					//Integer register data
	md_fpr_t regs_F;				//Floating point register data
	md_ctrl_t regs_C;				//Control flags
	int regs_index;					//index for the register (if >32, then -32 and use fp)
	//These are for precise state rollback
	qword_t previous_mem;				//Data before store occurred
	size_t data_size;
	bool is_store;					//Is this a store? (Did it write to memory?)
};

//RS_LINK defintions and declarations

//an ROB Link: this structure links elements of an ROB entry;
//	used for ready instruction queue, event queue, and
//	output dependency lists; each RS_LINK node contains a pointer to the ROB
//	entry it references along with an instance tag, the RS_LINK is only valid if
//	the instruction instance tag matches the instruction ROB entry instance tag;
//	this strategy allows entries in the ROB can be squashed and reused without
//	updating the lists that point to it, which significantly improves the
//	performance of (all to frequent) squash events
class RS_link
{
        public:
                RS_link();
                RS_link(const RS_link & rhs);
                RS_link(ROB_entry *rhs);

                ROB_entry *rs;
                union
                {
                        tick_t when;
                        INST_SEQ_TYPE seq;
                        int opnum;
                } x;

		//Less than operator provided for x.when
		//see seq_sort for sort by x.seq (used by ready_queue)
                bool operator<(const RS_link & rhs) const;
                bool operator==(const RS_link & rhs) const;
};

class seq_sort : public std::binary_function<RS_link,RS_link,bool>
{
	public:
	        bool operator()(const RS_link & left, const RS_link & right)
	        {
	                return left.x.seq < right.x.seq;
	        }
};

#endif
