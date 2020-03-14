// ReOrder Buffer and RS_Link Definitions

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


#ifndef ROB_C
#define ROB_C

#include "rob.h"

#include<list>
#include<functional>

ROB_entry::ROB_entry()
: IR(0), op(MD_NOP_OP), PC(0), next_PC(0), pred_PC(0), in_LSQ(0), LSQ_index(0), ea_comp(0), recover_inst(0),
stack_recover_idx(0), spec_mode(0), addr(0), seq(0), ptrace_seq(0), val_ra(0), val_rb(0), val_rc(0), val_ra_result(0),
slip(0), exec_lat(0), dispatched(0), queued(0), issued(0), completed(0), replayed(0), context_id(-1), iq_entry_num(-1), in_IQ(0),
disp_cycle(-1), rename_cycle(-1), physreg(-1), old_physreg(-1), dest_format(REG_NONE), archreg(0),
L1_miss(0), L2_miss(0), L3_miss(0), regs_R(0), regs_index(0), previous_mem(0), data_size(0), is_store(0)
{
	src_physreg[0] = src_physreg[1] = -1;
	src_archreg[0] = src_archreg[1] = 0;
}

RS_link::RS_link()
:rs(NULL)
{}

RS_link::RS_link(const RS_link & rhs)
:rs(rhs.rs),x(rhs.x)
{}

RS_link::RS_link(ROB_entry *rhs)
: rs(rhs)
{}

bool RS_link::operator<(const RS_link & rhs) const
{
       return x.when < rhs.x.when;
}

bool RS_link::operator==(const RS_link & rhs) const
{
       return (rs==rhs.rs);
}

#endif
