/* dlite.h - DLite, the lite debugger, interfaces */

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


/*
 * This module implements DLite, the lite debugger.  DLite is a very light
 * weight semi-symbolic debugger that can interface to any simulator with
 * only a few function calls.  
 *
 * The following commands are supported by DLite: 
 *

 *
 * help			 - print command reference
 * version		 - print DLite version information
 * terminate		 - terminate the simulation with statistics
 * quit			 - exit the simulator
 * cont {<addr>}	 - continue program execution (optionally at <addr>)
 * step			 - step program one instruction
 * next			 - step program one instruction in current procedure
 * print <expr>		 - print the value of <expr>
 * regs			 - print register contents
 * mstate		 - print machine specific state (simulator dependent)
 * display/<mods> <addr> - display the value at <addr> using format <modifiers>
 * dump {<addr>} {<cnt>} - dump memory at <addr> (optionally for <cnt> words)
 * dis <addr> {<cnt>}	 - disassemble instructions at <addr> (for <cnt> insts)
 * break <addr>		 - set breakpoint at <addr>, returns <id> of breakpoint
 * dbreak <addr> {r|w|x} - set data breakpoint at <addr> (for (r)ead, (w)rite,
 *			   and/or e(x)ecute, returns <id> of breakpoint
 * breaks		 - list active code and data breakpoints
 * delete <id>		 - delete breakpoint <id>
 * clear		 - clear all breakpoints (code and data)
 *
 * ** command args <addr>, <cnt>, <expr>, and <id> are any legal expression:
 *
 * <expr>		<- <factor> +|- <expr>
 * <factor>		<- <term> *|/ <factor>
 * <term>		<- ( <expr> )
 *			   | - <term>
 *			   | <const>
 *			   | <symbol>
 *			   | <file:loc>
 *
 * ** command modifiers <mods> are any of the following:
 *
 * b - print a byte
 * h - print a half (short)
 * w - print a word
 * q - print a qword
 * t - print in decimal format
 * u - print in unsigned decimal format
 * o - print in octal format
 * x - print in hex format
 * 1 - print in binary format
 * f - print a float
 * d - print a double
 * c - print a character
 * s - print a string
 */

#ifndef DLITE_H
#define DLITE_H

#include<cstdio>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "eval.h"
#include "range.h"

#include<vector>

//Forward declarations for private classes
class dlite_break_t;
class dlite_cmd_t;
class dlite_refs_t;

//DLite register access function, the debugger uses this function to access simulator register state
//Returns error str, NULL if no error
typedef const char * (*dlite_reg_obj_t)(regs_t *regs,	//registers to access
	int is_write,					//access type
	enum md_reg_type rt,				//reg bank to access
	int reg,					//register number
	eval_value_t *val);				//input, output

//DLite memory access function, the debugger uses this function to access simulator memory state
//Returns error str, NULL if no error
typedef const char * (*dlite_mem_obj_t)(mem_t *mem,	//memory space to access
	int is_write,					//access type
	md_addr_t addr,					//address to access
	char *p,					//input/output buffer
	int nbytes);					//size of access

//DLite memory access function, the debugger uses this function to display the state of machine-specific state
//Returns error str, NULL if no error
typedef const char * (*dlite_mstate_obj_t)(FILE *stream,	//output stream
	char *cmd,						//optional command string
	regs_t *regs,						//registers to access
	mem_t *mem);						//memory space to access

class dlite_t
{
	public:
		//initialize the DLite debugger with a register state object, memory state object, machine state object and pointer to context's memory and registers
		dlite_t(dlite_reg_obj_t reg_obj, dlite_mem_obj_t mem_obj, dlite_mstate_obj_t mstate_obj, mem_t * my_mem, regs_t * my_reg);
		~dlite_t();

		//DLite debugger main loop
		void dlite_main(md_addr_t regs_PC,			//addr of last inst to exec
			md_addr_t next_PC,				//addr of next inst to exec
			counter_t cycle);				//current processor cycle

		//internal break check interface, returns non-zero if breakpoint is hit/
		int __check_break(md_addr_t next_PC,			//address of next inst
			int access,					//mem access of last inst
			md_addr_t addr,					//mem addr of last inst
			counter_t icount,				//instruction count
			counter_t cycle);				//cycle count

		//Use after fork to repair the pointers that may have changed during fork
		//This fixes the parent thread and returns a pointer to a dlite_t for the child
		dlite_t * repair_ptrs(regs_t * parent_regs, regs_t * child_regs, mem_t * parent_mem, mem_t * child_mem);

	private:
		dlite_reg_obj_t f_dlite_reg_obj;
		dlite_mem_obj_t f_dlite_mem_obj;
		dlite_mstate_obj_t f_dlite_mstate_obj;
		dlite_refs_t * dlite_refs;		//Structure to provide pointers to dlite_evaluator
		eval_state_t * dlite_evaluator;

		bool dlite_fastbreak;			//set if one breakpoint is set, for fast break check
	public:
		bool dlite_active;			//set to enter DLite after next instruction
		bool dlite_check;			//set to force a check for a break
	private:
		bool dlite_return;			//set to exit DLite command loop
		int break_access;			//this variable clues dlite_main() into why it was called

		regs_t * regs;				//Dlite default expression evaluator
		mem_t * mem;				//memory to access

		dlite_break_t * dlite_bps;		//all active breakpoints, int a list
		int break_id;				//unique id of the next breakpoint

		//DLite debugger commands, all return an error string (NULL if no error)
		const char * dlite_help(int nargs, union arg_val_t args[]);		//print help messages for all (or single) DLite debugger commands
		const char * dlite_version(int nargs, union arg_val_t args[]);		//print version information for simulator
		const char * dlite_terminate(int nargs, union arg_val_t args[]);	//terminate simulation with statistics
		const char * dlite_quit(int nargs, union arg_val_t args[]);		//quit the simulator, omit any stats dump
		const char * dlite_cont(int nargs, union arg_val_t args[]);		//continue executing program (possibly at specified address)
		const char * dlite_step(int nargs, union arg_val_t args[]);		//step program one instruction
#if 0 /* NYI */
		const char * lite_next(int nargs, union arg_val_t args[]);		//step program one instruction in current procedure
#endif
		const char * dlite_print(int nargs, union arg_val_t args[]);		//print the value of <expr> using format <modifiers>
		const char * dlite_options(int nargs, union arg_val_t args[]);		//print the value of all command line options
		const char * dlite_option(int nargs, union arg_val_t args[]);		//print the value of all (or single) command line options
		const char * dlite_stats(int nargs, union arg_val_t args[]);		//print the value of all statistical variables
		const char * dlite_stat(int nargs, union arg_val_t args[]);		//print the value of a statistical variable
		const char * dlite_whatis(int nargs, union arg_val_t args[]);		//print the type of expression <expr>
		const char * dlite_regs(int nargs, union arg_val_t args[]);		//print all register contents
		const char * dlite_iregs(int nargs, union arg_val_t args[]);		//print integer register contents
		const char * dlite_fpregs(int nargs, union arg_val_t args[]);		//print floating point register contents
		const char * dlite_cregs(int nargs, union arg_val_t args[]);		//print floating point register contents
		const char * dlite_mstate(int nargs, union arg_val_t args[]);		//print machine specific state (simulator dependent)
		const char * dlite_display(int nargs, union arg_val_t args[]);		//display the value at memory location <addr> using format <modifiers>
		const char * dlite_dump(int nargs, union arg_val_t args[]);		//dump the contents of memory to screen
		const char * dlite_dis(int nargs, union arg_val_t args[]);		//disassemble instructions at specified address
		const char * dlite_break(int nargs, union arg_val_t args[]);		//set a text breakpoint
		const char * dlite_dbreak(int nargs, union arg_val_t args[]);		//set a data breakpoint at specified address
		const char * dlite_rbreak(int nargs, union arg_val_t args[]);		//set a breakpoint at specified range
		const char * dlite_breaks(int nargs, union arg_val_t args[]);		//list all outstanding breakpoints
		const char * dlite_delete(int nargs, union arg_val_t args[]);		//delete specified breakpoint
		const char * dlite_clear(int nargs, union arg_val_t args[]);		//clear all breakpoints
		const char * dlite_symbols(int nargs, union arg_val_t args[]);		//print the value of all program symbols
		const char * dlite_tsymbols(int nargs, union arg_val_t args[]);		//print the value of all text symbols
		const char * dlite_dsymbols(int nargs, union arg_val_t args[]);		//print the value of all text symbols
		const char * dlite_symbol(int nargs, union arg_val_t args[]);		//print the value of all (or single) command line options

		const char * delete_break(int id);					//delete breakpoint with id ID
		const char * set_break(int bpclass,					//break class, use ACCESS_*
			range_range_t * range);						//range breakpoint

		//print a mini-state header
		void dlite_status(md_addr_t regs_PC,					//PC of just completed inst
			md_addr_t next_PC,						//PC of next inst to exec
			counter_t cycle,						//current cycle
			int dbreak);							//last break a data break?

		//execite DLite command string CMD, returns error string (NULL if no error)
		const char * dlite_exec(char *cmd_str);					//command string
};

//Default architected/machine state accessors

//Default architected memory state accessor
//Returns error str, NULL if no error
const char * dlite_mem_obj(mem_t *mem,			//memory space to access
	int is_write,					//access type
	md_addr_t addr,					//address to access
	char *p,					//input, output
	int nbytes);					//size of access

//default architected machine-specific state accessor
//Returns error str, NULL if no error
const char * dlite_mstate_obj(FILE *stream,		//output stream
	char *cmd,					//optional command string
	regs_t *regs,					//registers to access
	mem_t *mem);					//memory space to access

//state access masks
#define ACCESS_READ	0x01				//read access allowed
#define ACCESS_WRITE	0x02				//write access allowed
#define ACCESS_EXEC	0x04				//execute access allowed

//check for a break condition
#define dlite_check_break(DLITE, NPC, ACCESS, ADDR, ICNT, CYCLE)		\
	(((DLITE)->dlite_check || (DLITE)->dlite_active)			\
	? (DLITE)->__check_break((NPC), (ACCESS), (ADDR), (ICNT), (CYCLE))	\
	: FALSE)

#endif
