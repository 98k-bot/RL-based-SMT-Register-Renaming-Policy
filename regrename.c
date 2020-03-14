// Physical Register File Definitions

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


#ifndef REGRENAME_C
#define REGRENAME_C

#include<cassert>
#include"regrename.h"

reg_set::reg_set()
: src1(REG_NONE), src2(REG_NONE), dest(), load(0), store(0)
{};

physical_reg_file::physical_reg_file()
{};

void physical_reg_file::resize(int size)
{
	data.resize(size);
}

physreg_t::physreg_t()
: state(REG_FREE), ready(0), spec_ready(0), alloc_cycle(0)
{};

physreg_t & physical_reg_file::operator[](unsigned int index)
{
	return *(&data[index]);
}

const physreg_t & physical_reg_file::operator[](unsigned int index) const
{
	return *(&data[index]);
}

int physical_reg_file::find_free_physreg()
{
	for(int i = 0; i<static_cast<int>(data.size()); i++)
	{
		if(data[i].state == REG_FREE)
		{
			return i;
		}
	}
	return -1;
}

reg_file_t::reg_file_t()
{};

void reg_file_t::resize(int size)
{
	intregs.resize(size);
	fpregs.resize(size);
}

unsigned int reg_file_t::size()
{
	return intregs.data.size();
}

int reg_file_t::find_free_physreg(reg_type type)
{
	if(type==REG_INT)
	{
		return intregs.find_free_physreg();
	}
	else
	{
		return fpregs.find_free_physreg();
	}
}

physreg_t & reg_file_t::reg_file_access(int index,reg_type type)
{
	if(type==REG_INT)
	{
		return intregs[index];
	}
	else
	{
		return fpregs[index];
	}
}

// Allocates a physical register to the specified ROB entry
int reg_file_t::alloc_physreg(ROB_entry* rob_entry,tick_t sim_cycle,std::vector<int> & rename_table)
{
	reg_set my_regs;
	get_reg_set(&my_regs, rob_entry->op);

	//store the old destination register mapping
	rob_entry->old_physreg = rename_table[rob_entry->archreg];

	//find a new physreg
	rob_entry->physreg = find_free_physreg(my_regs.dest);
	assert(rob_entry->physreg >= 0);

	//mark the new physreg as allocated
	physreg_t * target = NULL;
	if(my_regs.dest==REG_INT)
	{
		target = &intregs.data[rob_entry->physreg];
	}
	else
	{
		target = &fpregs.data[rob_entry->physreg];
	}
	assert(target->state==REG_FREE);
	target->state = REG_ALLOC;
	target->alloc_cycle = sim_cycle;
	target->spec_ready = __LONG_LONG_MAX__;;
	target->ready = __LONG_LONG_MAX__;;

	//update the rename table
	rename_table[rob_entry->archreg] = rob_entry->physreg;

	//return the physical register number
	return rob_entry->physreg;
}

//Returns the set of register types used by a specific operation
void reg_file_t::get_reg_set(reg_set* my_regs, md_opcode op)
{
	my_regs->src1 = REG_NONE;
	my_regs->src2 = REG_NONE;
	my_regs->dest = REG_NONE;
	my_regs->load = 0;
	my_regs->store = 0;

	switch(op)
	{
	case LDA:
	case LDAH:
	case LDBU:
	case LDQ_U:
	case LDWU:
	case LDL:
	case LDQ:
	case LDL_L:
	case LDQ_L:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_INT;
		my_regs->dest = REG_INT;
		my_regs->load = 1;
		break;

	case STW:
	case STB:
	case STQ_U:
	case STL:
	case STQ:
		my_regs->src1 = REG_INT;
		my_regs->src2 = REG_INT;
		my_regs->dest = REG_NONE;
		my_regs->store = 1;
		break;

	case STL_C:
	case STQ_C:
		my_regs->src1 = REG_INT;
		my_regs->src2 = REG_INT;
		my_regs->dest = REG_INT;
		my_regs->store = 1;
		break;

	case FLTV:
	case LDG:
	case STF:
	case STG:
	case PAL_CALLSYS:
	case SQRTF:
	case ITOFF:
	case SQRTG:
	case TRAPB:
	case EXCB:
	case MB:
	case WMB:
	case FETCH:
	case FETCH_M:
	case _RC:
	case ECB:
	case _RS:
	case WH64:
	case OP_NA:
	case CALL_PAL:
	case LDF:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_NONE;
		break;

	case LDS:
	case LDT:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_INT;
		my_regs->dest = REG_FP;
		my_regs->load = 1;
		break;

	case STS:
	case STT:
		my_regs->src1 = REG_FP;
		my_regs->src2 = REG_INT;
		my_regs->dest = REG_NONE;
		break;

	case BR:
	case BSR:
	case IMPLVER:
	case RPCC:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_INT;
		break;

	case FBEQ:
	case FBLT:
	case FBLE:
	case FBNE:
	case FBGE:
	case FBGT:
		my_regs->src1 = REG_FP;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_NONE;
		break;

	case BLBC:
	case BEQ:
	case BLT:
	case BLE:
	case BLBS:
	case BNE:
	case BGE:
	case BGT:
		my_regs->src1 = REG_INT;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_NONE;
		break;

	case ADDL:
	case S4ADDL:
	case SUBL:
	case S4SUBL:
	case CMPBGE:
	case S8ADDL:
	case S8SUBL:
	case CMPULT:
	case ADDQ:
	case S4ADDQ:
	case SUBQ:
	case S4SUBQ:
	case CMPEQ:
	case S8ADDQ:
	case S8SUBQ:
	case CMPULE:
	case ADDLV:
	case SUBLV:
	case CMPLT:
	case ADDQV:
	case SUBQV:
	case CMPLE:
	case AND:
	case BIC:
	case CMOVLBS:
	case CMOVLBC:
	case BIS:
	case CMOVEQ:
	case CMOVNE:
	case ORNOT:
	case XOR:
	case CMOVLT:
	case CMOVGE:
	case EQV:
	case CMOVLE:
	case CMOVGT:
	case MSKBL:
	case EXTBL:
	case INSBL:
	case MSKWL:
	case EXTWL:
	case INSWL:
	case MSKLL:
	case EXTLL:
	case INSLL:
	case ZAP:
	case ZAPNOT:
	case MSKQL:
	case SRL:
	case EXTQL:
	case SLL:
	case INSQL:
	case SRA:
	case MSKWH:
	case INSWH:
	case EXTWH:
	case MSKLH:
	case INSLH:
	case EXTLH:
	case MSKQH:
	case INSQH:
	case EXTQH:
	case MULL:
	case MULQ:
	case UMULH:
	case PERR:
	case MINSB8:
	case MINSW4:
	case MINUB8:
	case MINUW4:
	case MAXUB8:
	case MAXUW4:
	case MAXSB8:
	case MAXSW4:
		my_regs->src1 = REG_INT;
		my_regs->src2 = REG_INT;
		my_regs->dest = REG_INT;
		break;

	case ADDLI:
	case S4ADDLI:
	case SUBLI:
	case S4SUBLI:
	case CMPBGEI:
	case S8ADDLI:
	case S8SUBLI:
	case CMPULTI:
	case ADDQI:
	case S4ADDQI:
	case SUBQI:
	case S4SUBQI:
	case CMPEQI:
	case S8ADDQI:
	case S8SUBQI:
	case CMPULEI:
	case ADDLVI:
	case SUBLVI:
	case CMPLTI:
	case ADDQVI:
	case SUBQVI:
	case CMPLEI:
	case ANDI:
	case BICI:
	case CMOVLBSI:
	case CMOVLBCI:
	case BISI:
	case CMOVEQI:
	case CMOVNEI:
	case ORNOTI:
	case XORI:
	case CMOVLTI:
	case CMOVGEI:
	case EQVI:
	case AMASK:
	case CMOVLEI:
	case CMOVGTI:
	case MSKBLI:
	case EXTBLI:
	case INSBLI:
	case MSKWLI:
	case EXTWLI:
	case INSWLI:
	case MSKLLI:
	case EXTLLI:
	case INSLLI:
	case ZAPI:
	case ZAPNOTI:
	case MSKQLI:
	case SRLI:
	case EXTQLI:
	case SLLI:
	case INSQLI:
	case SRAI:
	case MSKWHI:
	case INSWHI:
	case EXTWHI:
	case MSKLHI:
	case INSLHI:
	case EXTLHI:
	case MSKQHI:
	case INSQHI:
	case EXTQHI:
	case MULLI:
	case MULQI:
	case UMULHI:
	case JMP:
	case JSR:
	case RETN:
	case JSR_COROUTINE:
	case CTPOP:
	case CTLZ:
	case CTTZ:
	case UNPKBW:
	case UNPKBL:
	case PKWB:
	case PKLB:
	case MINSB8I:
	case MINSW4I:
	case MINUB8I:
	case MINUW4I:
	case MAXUB8I:
	case MAXUW4I:
	case MAXSB8I:
	case MAXSW4I:
	case SEXTB:
	case SEXTW:
		my_regs->src1 = REG_INT;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_INT;
		break;

	case AMASKI:
	case SEXTBI:
	case SEXTWI:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_INT;
		break;

	case ITOFS:
	case ITOFT:
		my_regs->src1 = REG_INT;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_FP;
		break;

	case SQRTS:
	case SQRTT:
	case CVTTS:
	case CVTTQ:
	case CVTQS:
	case CVTQT:
	case CVTLQ:
	case CVTQL:
		my_regs->src1 = REG_FP;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_FP;
		break;

	case ADDS:
	case SUBS:
	case MULS:
	case DIVS:
	case ADDT:
	case SUBT:
	case MULT:
	case DIVT:
	case CMPTUN:
	case CMPTEQ:
	case CMPTLT:
	case CMPTLE:
	case CPYS:
	case CPYSN:
	case CPYSE:
	case FCMOVEQ:
	case FCMOVNE:
	case FCMOVLT:
	case FCMOVGE:
	case FCMOVLE:
	case FCMOVGT:
		my_regs->src1 = REG_FP;
		my_regs->src2 = REG_FP;
		my_regs->dest = REG_FP;
		break;

	case FTOIT:
	case FTOIS:
		my_regs->src1 = REG_FP;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_INT;
		break;

	case MT_FPCR:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_FP;
		break;

	case PAL_RDUNIQ:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_INT;
		break;

	case PAL_WRUNIQ:
		my_regs->src1 = REG_INT;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_NONE;
		break;

	default:
		my_regs->src1 = REG_NONE;
		my_regs->src2 = REG_NONE;
		my_regs->dest = REG_NONE;
		break;
	}
}
#endif
