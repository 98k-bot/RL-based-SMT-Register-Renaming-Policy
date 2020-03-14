/* alpha.h - Alpha ISA definitions */

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


#ifndef ALPHA_H
#define ALPHA_H

#include<cstdio>
#include<vector>
#include "host.h"
#include "misc.h"
#include "endian.h"


//This file contains various definitions needed to decode, disassemble, and execute Alpha AXP instructions.

//build for Alpha AXP target
#define TARGET_ALPHA

//probe cross-endian execution
#if defined(BYTES_BIG_ENDIAN)
#define MD_CROSS_ENDIAN
#endif

//not applicable/available, usable in most definition contexts
#define NA		0

//target-dependent type definitions
//define MD_QWORD_ADDRS if the target requires 64-bit (qword) addresses
#define MD_QWORD_ADDRS
//address type definition
typedef qword_t md_addr_t;

//target-dependent memory module configuration
//physical memory page size (must be a power-of-two)
#define MD_PAGE_SIZE		8192
#define MD_LOG_PAGE_SIZE	13

//target-dependent instruction faults
enum md_fault_type
{
	md_fault_none = 0,		//no fault
	md_fault_access,		//storage access fault
	md_fault_alignment,		//storage alignment fault
	md_fault_overflow,		//signed arithmetic overflow fault
	md_fault_div0,			//division by zero fault
	md_fault_invalid,		//invalid arithmetic operation. Added to allow SQRT{S,T} in FIX exts
	md_fault_break,			//BREAK instruction fault
	md_fault_unimpl,		//unimplemented instruction fault
	md_fault_internal,		//internal S/W fault
	md_fault_segfault		//segmentation fault, thrown by out-of-bounds usage of memory mapped files (shared only, very limited in scope)
};


//target-dependent register file definitions, used by regs.[hc]
//number of integer, floating point and control registers
#define MD_NUM_IREGS		32
#define MD_NUM_FREGS		32
#define MD_NUM_CREGS		2

//total number of registers, excluding PC and NPC
//Int (32) + FP (32) + Misc (2) + Tmp (1) + Mem (1) + Ctrl (1) -> 69
#define MD_TOTAL_REGS		69

//general purpose (integer) register file entry type
typedef qword_t md_gpr_t;

//floating point register file entry type
union md_fpr_t
{
	qword_t q;		//integer qword view
	dfloat_t d;		//double-precision floating point view
};

//control register file contents
class md_ctrl_t {
	public:
		md_ctrl_t()
		: fpcr(0), uniq(0)
		{}
		qword_t fpcr;		//floating point condition codes
		qword_t uniq;		//process-unique register
};

//well known registers
enum md_reg_names
{
	MD_REG_V0 = 0,		//return value reg
	MD_REG_ERR = 7,
	MD_REG_FP = 15,		//frame pointer
	MD_REG_A0 = 16,		//argument regs
	MD_REG_A1 = 17,
	MD_REG_A2 = 18,
	MD_REG_A3 = 19,
	MD_REG_A4 = 20,
	MD_REG_A5 = 21,
	MD_REG_RA = 26,		//return address reg
	MD_REG_GP = 29,		//global data section pointer
	MD_REG_SP = 30,		//stack pointer
	MD_REG_ZERO = 31	//zero register
};

//target-dependent instruction format definition
//instruction formats
typedef word_t md_inst_t;

//preferred nop instruction definition
extern md_inst_t MD_NOP_INST;

//target swap support
#ifdef MD_CROSS_ENDIAN
#define MD_SWAPH(X)		SWAP_HALF(X)
#define MD_SWAPW(X)		SWAP_WORD(X)
#define MD_SWAPQ(X)		SWAP_QWORD(X)
#define MD_SWAPI(X)		SWAP_WORD(X)
#else //!MD_CROSS_ENDIAN
#define MD_SWAPH(X)		(X)
#define MD_SWAPW(X)		(X)
#define MD_SWAPQ(X)		(X)
#define MD_SWAPD(X)		(X)
#define MD_SWAPI(X)		(X)
#endif

//fetch an instruction
#define MD_FETCH_INST(INST, MEM, PC)		{ (INST) = MEM_READ_WORD((MEM), (PC)); }

//target-dependent loader module configuration
//maximum size of argc+argv+envp environment
#define MD_MAX_ENVIRON		16384

//machine.def specific definitions

//inst -> enum md_opcode mapping, use this macro to decode insts
#define MD_TOP_OP(INST)		(((INST) >> 26) & 0x3f)
#define MD_SET_OPCODE(OP, INST)							\
	{ OP = md_mask2op[MD_TOP_OP(INST)];					\
	while (md_opmask[OP])							\
		OP = md_mask2op[((INST >> md_opshift[OP]) & md_opmask[OP])	\
		+ md_opoffset[OP]]; }

//largest opcode field value (currently upper 8-bit are used for pre/post-
//incr/decr operation specifiers
#define MD_MAX_MASK		2048

//internal decoder state
extern unsigned int md_opoffset[];
extern unsigned int md_opmask[];
extern unsigned int md_opshift[];

//global opcode names, these are returned by the decoder (MD_OP_ENUM())
extern enum md_opcode
{
	OP_NA = 0,	/* NA */
#define DEFINST(OP,MSK,NAME,OPFORM,RES,FLAGS,O1,O2,I1,I2,I3) OP,
#define DEFLINK(OP,MSK,NAME,MASK,SHIFT) OP,
#define CONNECT(OP)
#include "machine.def"
	OP_MAX	/* number of opcodes + NA */
} md_mask2op[];

//enum md_opcode -> description string
#define MD_OP_NAME(OP)		(md_op2name[OP])
extern const char *md_op2name[];

//enum md_opcode -> opcode operand format, used by disassembler
#define MD_OP_FORMAT(OP)	(md_op2format[OP])
extern const char *md_op2format[];

//function unit classes, update md_fu2name if you update this definition
enum md_fu_class
{
	FUClamd_NA = 0,		//inst does not use a functional unit
	IntALU,			//integer ALU
	IntMULT,		//integer multiplier
	IntDIV,			//integer divider
	FloatADD,		//floating point adder/subtractor
	FloatCMP,		//floating point comparator
	FloatCVT,		//floating point<->integer converter
	FloatMULT,		//floating point multiplier
	FloatDIV,		//floating point divider
	FloatSQRT,		//floating point square root
	RdPort,			//memory read port
	WrPort,			//memory write port
	NUM_FU_CLASSES		//total functional unit classes
};

//Added for C to c++ conversion, replaces badly used enum
extern int md_op2fu[];

//enum md_opcode -> enum md_fu_class, used by performance simulators
#define MD_OP_FUCLASS(OP)	(md_op2fu[OP])

//enum md_fu_class -> description string
#define MD_FU_NAME(FU)		(md_fu2name[FU])
extern const char *md_fu2name[];

//instruction flags
#define F_ICOMP		0x00000001	//integer computation
#define F_FCOMP		0x00000002	//FP computation
#define F_CTRL		0x00000004	//control inst
#define F_UNCOND	0x00000008	//	unconditional change
#define F_COND		0x00000010	//	conditional change
#define F_MEM		0x00000020	//memory access inst
#define F_LOAD		0x00000040	//	load inst
#define F_STORE		0x00000080	//	store inst
#define F_DISP		0x00000100	//	displaced (R+C) addr mode
#define F_RR		0x00000200	//	R+R addr mode
#define F_DIRECT	0x00000400	//	direct addressing mode
#define F_TRAP		0x00000800	//traping inst
#define F_LONGLAT	0x00001000	//long latency inst (for sched)
#define F_DIRJMP	0x00002000	//direct jump
#define F_INDIRJMP	0x00004000	//indirect jump
#define F_CALL		0x00008000	//function call
#define F_FPCOND	0x00010000	//FP conditional branch
#define F_IMM		0x00020000	//instruction has immediate operand

//enum md_opcode -> opcode flags, used by simulators
#define MD_OP_FLAGS(OP)		(md_op2flags[OP])
extern unsigned int md_op2flags[];

//integer register specifiers
#define RA		((inst >> 21) & 0x1f)		//reg source #1
#define RB		((inst >> 16) & 0x1f)		//reg source #2
#define RC		(inst & 0x1f)			//reg dest

//returns 8-bit unsigned immediate field value
#define IMM		((qword_t)((inst >> 13) & 0xff))

//returns 21-bit unsigned absolute jump target field value
#define TARG		(inst & 0x1fffff)

//load/store 16-bit unsigned offset field value
#define OFS		(inst & 0xffff)

//sign-extend operands
#define SEXT(X)		(((X) & 0x8000) ? ((sqword_t)(X) | LL(0xffffffffffff0000)) : (sqword_t)(X))

#define SEXT21(X)	(((X) & 0x100000) ? ((sqword_t)(X) | LL(0xffffffffffe00000)) : (sqword_t)(X))

#define SEXT32(X)	(((X) & 0x80000000) ? ((sqword_t)(X)|LL(0xffffffff00000000)) : (sqword_t)(X))

//test for arithmetic overflow
#define ARITH_OVFL(RESULT, OP1, OP2) ((RESULT) < (OP1) || (RESULT) < (OP2))

//test for NaN
#define IEEEFP_DBL_SIGN(Q)	((Q) >> 63)
#define IEEEFP_DBL_EXPONENT(Q)	(((Q) >> 52) & 0x7ff)
#define IEEEFP_DBL_FRACTION(Q)	((Q) & ULL(0xfffffffffffff))
#define IS_IEEEFP_DBL_NAN(Q)	((IEEEFP_DBL_EXPONENT(Q) == 0x7ff) && (IEEEFP_DBL_FRACTION(Q)))

//default target PC handling
#ifndef SET_TPC
#define SET_TPC(PC)	(void)0
#endif

//various other helper macros/functions

//non-zero if system call is an exit()
#define OSF_SYS_exit		1
#define MD_EXIT_SYSCALL(REGS)	((REGS)->regs_R[MD_REG_V0] == OSF_SYS_exit)

//returns stream of an output system call, translated to host
#define MD_STREAM_FILENO(REGS)		((REGS)->regs_R[MD_REG_A0])

//returns non-zero if instruction is a function call
#define MD_IS_CALL(OP)			((OP) == JSR || (OP) == BSR)

/* returns non-zero if instruction is a function return */
#define MD_IS_RETURN(OP)		((OP) == RETN)

//returns non-zero if instruction is an indirect jump
#define MD_IS_INDIR(OP)			((OP) == JMP || (OP) == JSR || (OP) == RETN || (OP) == JSR_COROUTINE)

//addressing mode probe, enums and strings
enum md_amode_type
{
	md_amode_imm,		//immediate addressing mode
	md_amode_gp,		//global data access through global pointer
	md_amode_sp,		//stack access through stack pointer
	md_amode_fp,		//stack access through frame pointer
	md_amode_disp,		//(reg + const) addressing
	md_amode_rr,		//(reg + reg) addressing
	md_amode_NUM
};
extern const char *md_amode_str[md_amode_NUM];

//addressing mode pre-probe FSM, must see all instructions
#define MD_AMODE_PREPROBE(OP, FSM)		{ (FSM) = 0; }

//compute addressing mode, only for loads/stores
#define MD_AMODE_PROBE(AM, OP, FSM)			\
	{						\
		if(MD_OP_FLAGS(OP) & F_DISP)		\
		{					\
			if((RB) == MD_REG_GP)		\
				(AM) = md_amode_gp;	\
			else if ((RB) == MD_REG_SP)	\
				(AM) = md_amode_sp;	\
			else if ((RB) == MD_REG_FP) /* && bind_to_seg(addr) == seg_stack */\
				(AM) = md_amode_fp;	\
			else				\
				(AM) = md_amode_disp;	\
		}					\
		else if (MD_OP_FLAGS(OP) & F_RR)	\
			(AM) = md_amode_rr;		\
		else					\
			panic("cannot decode addressing mode");		\
	}

//addressing mode pre-probe FSM, after all loads and stores
#define MD_AMODE_POSTPROBE(FSM)			//nada...

//EIO package configuration/macros

//expected EIO file format
#define MD_EIO_FILE_FORMAT		EIO_ALPHA_FORMAT

//configure the stats package
//counter stats
#define stat_reg_counter		stat_reg_sqword
#define sc_counter			sc_sqword
#define for_counter			for_sqword
//address stats
#define stat_reg_addr			stat_reg_qword


//configure the DLite! debugger

//register bank specifier
enum md_reg_type
{
	rt_gpr,		//general purpose register
	rt_lpr,		//integer-precision floating pointer register
	rt_fpr,		//single-precision floating pointer register
	rt_dpr,		//double-precision floating pointer register
	rt_ctrl,	//control register
	rt_PC,		//program counter
	rt_NPC,		//next program counter
	rt_NUM
};

//register name specifier
class md_reg_names_t
{
	public:
		const char *str;		//register name
		enum md_reg_type file;		//register file
		int reg;			//register index
};

//symbolic register names, parser is case-insensitive
extern const md_reg_names_t md_reg_names[];

//returns a register name string
const char *md_reg_name(enum md_reg_type rt, int reg);

//default register accessor object, forward declarations needed. Returns NULL on success
class eval_value_t;
class regs_t;
const char * md_reg_obj(regs_t *regs,		//registers to access
	int is_write,				//access type
	enum md_reg_type rt,			//reg bank to probe
	int reg,				//register number
	eval_value_t *val);			//input, output

//print integer REG(S) to STREAM
void md_print_ireg(std::vector<md_gpr_t> & regs, int reg, FILE *stream);
void md_print_iregs(std::vector<md_gpr_t> & regs, FILE *stream);

//print floating point REG(S) to STREAM
void md_print_fpreg(std::vector<md_fpr_t> & regs, int reg, FILE *stream);
void md_print_fpregs(std::vector<md_fpr_t> & regs, FILE *stream);

//print control REG(S) to STREAM
void md_print_creg(md_ctrl_t regs, int reg, FILE *stream);
void md_print_cregs(md_ctrl_t regs, FILE *stream);

//xor checksum registers
word_t md_xor_regs(regs_t *regs);

//configure sim-outorder specifics

//primitive operation used to compute addresses within pipeline
#define MD_AGEN_OP		ADDQ

//NOP operation when injected into the pipeline
#define MD_NOP_OP		OP_NA

//non-zero for a valid address, used to determine if speculative accesses
//should access the DL1 data cache
#define MD_VALID_ADDR(ADDR)						\
	(((ADDR) >= mem->ld_text_base && (ADDR) < (mem->ld_text_base + mem->ld_text_size))	\
		|| ((ADDR) >= mem->ld_data_base && (ADDR) < mem->ld_brk_point)			\
		|| ((ADDR) >= (mem->ld_stack_base - 16*1024*1024) && (ADDR) < mem->ld_stack_base))

//configure branch predictors

//shift used to ignore branch address least significant bits, usually log2(sizeof(md_inst_t))
#define MD_BR_SHIFT		2	//log2(4)

//target-dependent routines

//intialize the inst decoder, this function builds the ISA decode tables
void md_init_decoder(void);

//disassemble a SimpleScalar instruction
void md_print_insn(md_inst_t inst,		//instruction to disassemble
	md_addr_t pc,				//addr of inst, used for PC-rels
	FILE *stream);				//output stream

#endif // ALPHA_H
