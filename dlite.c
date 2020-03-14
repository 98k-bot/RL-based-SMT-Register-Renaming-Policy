/* dlite.c - DLite, the lite debugger, routines */

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

#include<cstdio>
#include<cstdlib>
#include<string>
#include<ctype.h>
#include<errno.h>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "version.h"
#include "eval.h"
#include "regs.h"
#include "memory.h"
#include "sim.h"
#include "symbol.h"
#include "loader.h"
#include "options.h"
#include "stats.h"
#include "range.h"
#include "dlite.h"

#include<string>

//Forward declaration
eval_value_t ident_evaluator(eval_state_t *es);

#define MAX_ARGS			4			//maximum number of arguments that can passed to a dlite command handler
#define MAX_STR				128			//maximum length of a dlite command string argument

//argument array entry, argument arrays are passed to command handlers
union arg_val_t
{
	int as_modifier;
	eval_value_t as_value;
	int as_access;
	char as_str[MAX_STR];
};

class dlite_refs_t
{
	public:
		dlite_refs_t(regs_t * regs, dlite_reg_obj_t f_dlite_reg_obj)
		: regs(regs), f_dlite_reg_obj(f_dlite_reg_obj)
		{}
		regs_t * regs;
		dlite_reg_obj_t f_dlite_reg_obj;
};

dlite_t::dlite_t(dlite_reg_obj_t reg_obj, dlite_mem_obj_t mem_obj, dlite_mstate_obj_t mstate_obj, mem_t * my_mem, regs_t * my_reg)
: f_dlite_reg_obj(reg_obj), f_dlite_mem_obj(mem_obj), f_dlite_mstate_obj(mstate_obj), dlite_refs(new dlite_refs_t(my_reg, reg_obj)),
	dlite_evaluator(new eval_state_t(ident_evaluator, &dlite_refs, my_mem)),
	dlite_active(false), dlite_check(false), dlite_return(false), break_access(0),
	regs(my_reg), mem(my_mem),
	dlite_bps(NULL), break_id(1)
{}

dlite_t::~dlite_t()
{
	delete dlite_evaluator;
	delete dlite_refs;
	dlite_evaluator = NULL;
	dlite_refs = NULL;
}

//DLite debugger command parser command definitions, NOTE: optional arguments must be trailing
//arguments, otherwise the command parser will break (modifiers are an exception to this rule)
enum cmd_vals
{
	NONE = 0,
	DLITE_HELP = 1,
	DLITE_VERSION,
	DLITE_TERMINATE,
	DLITE_QUIT,
	DLITE_CONT,
	DLITE_STEP,
#if 0 //NYI
	DLITE_NEXT,
#endif
	DLITE_PRINT,
	DLITE_OPTIONS,
	DLITE_OPTION,
	DLITE_STATS,
	DLITE_STAT,
	DLITE_WHATIS,
	DLITE_REGS,
	DLITE_IREGS,
	DLITE_FPREGS,
	DLITE_CREGS,
	DLITE_MSTATE,
	DLITE_DISPLAY,
	DLITE_DUMP,
	DLITE_DIS,
	DLITE_BREAK,
	DLITE_DBREAK,
	DLITE_RBREAK,
	DLITE_BREAKS,
	DLITE_DELETE,
	DLITE_CLEAR,
	DLITE_SYMBOLS,
	DLITE_TSYMBOLS,
	DLITE_DSYMBOLS,
	DLITE_SYMBOL
};

//DLite command descriptor, fully describes a command supported by the DLite debugger command handler
class dlite_cmd_t
{
	public:
		const char *cmd_str;		//DLite command string
		const char *arg_strs[MAX_ARGS];	//NULL-terminated cmd args (? - optional):
						//	m - size/type modifiers
						//	a - address expression
						//	c - count expression
						//	e - any expression
						//	s - any string
						//	t - access type {r|w|x}
						//	i - breakpoint id
		cmd_vals cmd_fn;		//implementing function
		const char *help_str;		//DLite command help string
};

static dlite_cmd_t cmd_db[] =
{
	{ "help", { "s?", NULL }, DLITE_HELP,		"print command reference" },
	{ "version", { NULL }, DLITE_VERSION,		"print DLite version information" },
	{ "terminate", { NULL }, DLITE_TERMINATE,	"terminate the simulation with statistics" },
	{ "quit", { NULL }, DLITE_QUIT,			"exit the simulator" },
	{ "cont", { "a?", NULL }, DLITE_CONT,		"continue program execution (optionally at <addr>)" },
	{ "step", { NULL }, DLITE_STEP,			"step program one instruction" },
#if 0 //NYI
	{ "next", { NULL }, DLITE_NEXT,			"step program one instruction in current procedure" },
#endif
	{ "print", { "m?", "e", NULL }, DLITE_PRINT,	"print the value of <expr> using format <modifiers>" },
	{ "options", { NULL }, DLITE_OPTIONS,		"print the value of all options" },
	{ "option", { "s", NULL }, DLITE_OPTION,	"print the value of an option" },
	{ "stats", { NULL }, DLITE_STATS,		"print the value of all statistical variables" },
	{ "stat", { "s", NULL }, DLITE_STAT,		"print the value of a statistical variable" },
	{ "whatis", { "e", NULL }, DLITE_WHATIS,	"print the type of expression <expr>" },
	{ "---", { NULL }, NONE, NULL },
	{ "regs", { NULL }, DLITE_REGS,			"print all register contents" },
	{ "iregs", { NULL }, DLITE_IREGS,		"print integer register contents" },
	{ "fpregs", { NULL }, DLITE_FPREGS,		"print floating point register contents" },
	{ "cregs", { NULL }, DLITE_CREGS,		"print control register contents" },
	{ "mstate", { "s?", NULL }, DLITE_MSTATE,	"print machine specific state (simulator dependent)" },
	{ "display", { "m?", "a", NULL }, DLITE_DISPLAY,"display the value at memory location <addr> using format <modifiers>" },
	{ "dump", { "a?", "c?", NULL }, DLITE_DUMP,	"dump memory at <addr> (optionally for <cnt> words)" },
	{ "dis", { "a?", "c?", NULL }, DLITE_DIS,	"disassemble instructions at <addr> (for <cnt> insts)" },
	{ "break", { "a", NULL }, DLITE_BREAK,		"set breakpoint at <addr>, returns <id> of breakpoint" },
	{ "dbreak", { "a", "t?", NULL }, DLITE_DBREAK,	"set data breakpoint at <addr> (for(r)ead, (w)rite,\nand/or e(x)ecute, returns <id> of breakpoint" },
	{ "rbreak", { "s", "t?", NULL }, DLITE_RBREAK,	"set read/write/exec breakpoint at <range> (for(r)ead, (w)rite,\n\tand/or e(x)ecute, returns <id> of breakpoint" },
	{ "breaks", { NULL }, DLITE_BREAKS,		"list active code and data breakpoints" },
	{ "delete", { "i", NULL }, DLITE_DELETE,	"delete breakpoint <id>" },
	{ "clear", { NULL }, DLITE_CLEAR,		"clear all breakpoints (code and data)" },
	{ "---", { NULL }, NONE, NULL },
	{ "symbols", { NULL }, DLITE_SYMBOLS,		"print the value of all program symbols" },
	{ "tsymbols", { NULL }, DLITE_TSYMBOLS,		"print the value of all program text symbols" },
	{ "dsymbols", { NULL }, DLITE_DSYMBOLS,		"print the value of all program data symbols" },
	{ "symbol", { "s", NULL }, DLITE_SYMBOL,	"print the value of a symbol" },
	{ "---", { NULL }, NONE, NULL },
	// list terminator
	{ NULL, { NULL }, NONE, NULL }
};

//size modifier mask bit definitions
#define MOD_BYTE	0x0001		//b - print a byte
#define MOD_HALF	0x0002		//h - print a half (short)
#define MOD_WORD	0x0004		//w - print a word
#define MOD_QWORD	0x0008		//q - print a qword
#define MOD_FLOAT	0x0010		//F - print a float
#define MOD_DOUBLE	0x0020		//f - print a double
#define MOD_CHAR	0x0040		//c - print a character
#define MOD_STRING	0x0080		//s - print a string

#define MOD_SIZES	(MOD_BYTE|MOD_HALF|MOD_WORD|MOD_QWORD|MOD_FLOAT|MOD_DOUBLE|MOD_CHAR|MOD_STRING)

//format modifier mask bit definitions
#define MOD_DECIMAL	0x0100		//d - print in decimal format
#define MOD_UNSIGNED	0x0200		//u - print in unsigned format
#define MOD_OCTAL	0x0400		//o - print in octal format
#define MOD_HEX		0x0800		//x - print in hex format
#define MOD_BINARY	0x1000		//1 - print in binary format

#define MOD_FORMATS	(MOD_DECIMAL|MOD_UNSIGNED|MOD_OCTAL|MOD_HEX|MOD_BINARY)

#define INSTS_PER_SCREEN		16			//disassembler print format

//"dump" command print format
#define BYTES_PER_LINE			16 			//must be a power of two
#define LINES_PER_SCREEN		4
#define DLITE_PROMPT			"DLite! > "		//DLite command line prompt

//break instance descriptor, one allocated for each breakpoint set
class dlite_break_t
{
	public:
		dlite_break_t *next;		//next active breakpoint
		int id;				//break id
		int bpclass;			//break class
		range_range_t range;		//break range
};

//DLite modifier parser, transforms /<mods> strings to modifier mask
//returns error string, NULL if no error
const char * modifier_parser(char *p,		//ptr to /<mods> string
	char **endp,				//ptr to first byte not consumed
	int *pmod)				//modifier mask written to *PMOD
{
	int modifiers = 0;

	//default modifiers
	*pmod = 0;

	//is this a valid modifier?
	if(*p == '/')
	{
		p++;
		//parse modifiers until end-of-string or whitespace is found
		while(*p != '\0' && *p != '\n' && *p != ' ' && *p != '\t')
		{
			switch(*p)
			{
				case 'b':
					modifiers |= MOD_BYTE;
					break;
				case 'h':
					modifiers |= MOD_HALF;
					break;
				case 'w':
					modifiers |= MOD_WORD;
					break;
				case 'q':
					modifiers |= MOD_QWORD;
					break;
				case 'd':
					modifiers |= MOD_DECIMAL;
					break;
				case 'u':
					modifiers |= MOD_UNSIGNED;
					break;
				case 'o':
					modifiers |= MOD_OCTAL;
					break;
				case 'x':
					modifiers |= MOD_HEX;
					break;
				case '1':
					modifiers |= MOD_BINARY;
					break;
				case 'F':
					modifiers |= MOD_FLOAT;
					break;
				case 'f':
					modifiers |= MOD_DOUBLE;
					break;
				case 'c':
					modifiers |= MOD_CHAR;
					break;
				case 's':
					modifiers |= MOD_STRING;
					break;
				default:
					return "bad modifier (use one or more of /bhwqduox1fdcs)";
			}
			p++;
		}
	}

	//no error, return end of string and modifier mask
	*endp = p;
	*pmod = modifiers;
	return NULL;
}

//DLite identifier evaluator, used by the expression evaluator, returns the value of the
//ident in ES->TOK_BUF, sets eval_error to value other than ERR_NOERR if an error is encountered
eval_value_t ident_evaluator(eval_state_t *es)			//expression evaluator
{
	const char *err_str;
	eval_value_t val;
	sym_sym_t *sym;
	static eval_value_t err_value = { et_int, { 0 } };
	mem_t *my_mem = es->mem;
	dlite_refs_t * ptrs = (dlite_refs_t *)es->user_ptr;
	regs_t * regs = ptrs->regs;
	dlite_reg_obj_t f_dlite_reg_obj = ptrs->f_dlite_reg_obj;

	//is this a builtin register definition?
	for(int i=0; md_reg_names[i].str != NULL; i++)
	{
		if(!mystricmp(es->tok_buf, md_reg_names[i].str))
		{
			err_str = f_dlite_reg_obj(regs, /* !is_write */FALSE, md_reg_names[i].file, md_reg_names[i].reg, &val);
			if(err_str)
			{
				eval_error = ERR_UNDEFVAR;
				val = err_value;
			}
			return val;
		}
	}

	//else, try to locate a program symbol
	sym_loadsyms(es->mem->ld_prog_fname.c_str(), /* load locals */TRUE, es->mem);
	sym = sym_bind_name(es->tok_buf, NULL, sdb_any);
	if(sym)
	{
		//found a symbol with this name, return it's (address) value
		val.type = et_addr;
		val.value.as_addr = sym->addr;
		return val;
	}

	//else, try to locate a statistical value symbol
	stat_stat_t *stat = stat_find_stat(sim_sdb, es->tok_buf);
	if(stat)
	{
		//found it, convert stat value to an eval_value_t value
		switch(stat->sc)
		{
		case sc_int:
			val.type = et_int;
			val.value.as_int = *stat->variant.for_int.var;
			break;
		case sc_uint:
			val.type = et_uint;
			val.value.as_uint = *stat->variant.for_uint.var;
			break;
		case sc_qword:
			val.type = et_qword;
			val.value.as_qword = *stat->variant.for_qword.var;
			break;
		case sc_float:
			val.type = et_float;
			val.value.as_float = *stat->variant.for_float.var;
			break;
		case sc_double:
			val.type = et_double;
			val.value.as_double = *stat->variant.for_double.var;
			break;
		case sc_dist:
		case sc_sdist:
			eval_error = ERR_BADEXPR;
			val = err_value;
			break;
		case sc_formula:
			{
				//instantiate a new evaluator to avoid recursion problems
				eval_state_t *es = new eval_state_t(ident_evaluator, sim_sdb, my_mem);
				char *endp;

				val = eval_expr(es, stat->variant.for_formula.formula, &endp);
				if(eval_error != ERR_NOERR || *endp != '\0')
				{
					//pass through eval_error
					val = err_value;
				}
				//else, use value returned
				delete es;
			}
			break;
		default:
			panic("bogus stat class");
		}
		return val;
	}
	//else, not found, this is a bogus symbol
	eval_error = ERR_UNDEFVAR;
	val = err_value;
	return val;
}

//help command trailing text
const char *dlite_help_tail =
	"Arguments <addr>, <cnt>, <expr>, and <id> are any legal expression:\n"
	"  <expr>    <-  <factor> +|- <expr>\n"
	"  <factor>  <-  <term> *|/ <factor>\n"
	"  <term>    <-  ( <expr> )\n"
	"                | - <term>\n"
	"                | <const>\n"
	"                | <symbol>\n"
	"                | <file:loc>\n"
	"\n"
	"Command modifiers <mods> are any of the following:\n"
	"\n"
	"  b - print a byte\n"
	"  h - print a half (short)\n"
	"  w - print a word (default)\n"
	"  q - print a qword\n"
	"  F - print a float\n"
	"  f - print a double\n"
	"  c - print a character\n"
	"  s - print a string\n"
	"  d - print in decimal format (default)\n"
	"  u - print in unsigned decimal format\n"
	"  o - print in octal format\n"
	"  x - print in hex format\n"
	"  1 - print in binary format\n";

//Use after fork to repair the pointers that may have changed during fork
//This fixes the parent thread and returns a pointer to a dlite_t for the child
dlite_t * dlite_t::repair_ptrs(regs_t * parent_regs, regs_t * child_regs, mem_t * parent_mem, mem_t * child_mem)
{
	regs = parent_regs;
	mem = parent_mem;
	dlite_refs->regs = regs;
	return new dlite_t(f_dlite_reg_obj, f_dlite_mem_obj, f_dlite_mstate_obj, child_mem, child_regs);
}

//execute DLite command string CMD, returns error string, NULL if no error
const char * dlite_t::dlite_exec(char *cmd_str)		//command string
{
	dlite_cmd_t *cmd;
	char cmd_buf[512];
	char *endp;
	union arg_val_t args[MAX_ARGS];

	char *p = cmd_str;
	char *q = cmd_buf;

	//skip any whitespace before argument
	while(*p == ' ' || *p == '\t' || *p == '\n')
		p++;

	//anything left?
	if(*p == '\0')
	{
		//NOP, no error
		return NULL;
	}

	//copy out command name string
	while(*p != '\0' && *p != '\n' && *p != ' ' && *p != '\t' && *p != '/')
		*q++ = *p++;
	*q = '\0';

	//find matching command
	for(cmd=cmd_db; cmd->cmd_str != NULL; cmd++)
	{
		if(!strcmp(cmd->cmd_str, cmd_buf))
			break;
	}
	if(cmd->cmd_str == NULL)
		return "unknown command";

	//match arguments for *CMD
	int i, arg_cnt;
	for(i=0, arg_cnt=0; i<MAX_ARGS && cmd->arg_strs[i] != NULL; i++, arg_cnt++)
	{
		int access, modifiers;
		const char *err_str;
		eval_value_t val;

		//skip any whitespace before argument
		while(*p == ' ' || *p == '\t' || *p == '\n')
			p++;

		const char *arg = cmd->arg_strs[i];
		char arg_type = arg[0];
		bool optional = (arg[1] == '?');

		if(*p == '\0')
		{
			if(optional)
			{
				//all arguments parsed
				break;
			}
			else
				return "missing an argument";
		}

		endp = p;
		switch(arg_type)
		{
		case 'm':
			err_str = modifier_parser(p, &endp, &modifiers);
			if(err_str)
				return err_str;
			args[arg_cnt].as_modifier = modifiers;
			break;
		case 'a':
			val = eval_expr(dlite_evaluator, p, &endp);
			if(eval_error)
				return eval_err_str[eval_error];
			args[arg_cnt].as_value = val;
			break;
		case 'c':
			val = eval_expr(dlite_evaluator, p, &endp);
			if(eval_error)
				return eval_err_str[eval_error];
			args[arg_cnt].as_value = val;
			break;
		case 'e':
			val = eval_expr(dlite_evaluator, p, &endp);
			if(eval_error)
				return eval_err_str[eval_error];
			args[arg_cnt].as_value = val;
			break;
		case 't':
			access = 0;
			while(*p != '\0' && *p != '\n' && *p != ' ' && *p != '\t')
			{
				switch(*p)
				{
				case 'r':
					access |= ACCESS_READ;
					break;
				case 'w':
					access |= ACCESS_WRITE;
					break;
				case 'x':
					access |= ACCESS_EXEC;
					break;
				default:
					return "bad access type specifier (use r|w|x)";
				}
				p++;
			}
			endp = p;
			args[arg_cnt].as_access = access;
			break;
		case 'i':
			val = eval_expr(dlite_evaluator, p, &endp);
			if(eval_error)
				return eval_err_str[eval_error];
			args[arg_cnt].as_value = val;
			break;
		case 's':
			q = args[arg_cnt].as_str;
			while(*p != ' ' && *p != '\t' && *p != '\0')
				*q++ = *p++;
			*q = '\0';
			endp = p;
			break;
		default:
			panic("bogus argument type: `%c'", arg_type);
		}
		p = endp;
	}

	//skip any whitespace before any trailing argument
	while(*p == ' ' || *p == '\t' || *p == '\n')
		p++;

	//check for any orphan arguments
	if(*p != '\0')
		return "too many arguments";

	//if we reach here, all arguments were parsed correctly, call handler
	switch(cmd->cmd_fn)
	{
	case DLITE_HELP:	return dlite_help(arg_cnt, args);
	case DLITE_VERSION:	return dlite_version(arg_cnt, args);
	case DLITE_TERMINATE:	return dlite_terminate(arg_cnt, args);
	case DLITE_QUIT:	return dlite_quit(arg_cnt, args);
	case DLITE_CONT:	return dlite_cont(arg_cnt, args);
	case DLITE_STEP:	return dlite_step(arg_cnt, args);
#if 0 //NYI
	case DLITE_NEXT:	return dlite_next(arg_cnt, args);
#endif
	case DLITE_PRINT:	return dlite_print(arg_cnt, args);
	case DLITE_OPTIONS:	return dlite_options(arg_cnt, args);
	case DLITE_OPTION:	return dlite_option(arg_cnt, args);
	case DLITE_STATS:	return dlite_stats(arg_cnt, args);
	case DLITE_STAT:	return dlite_stat(arg_cnt, args);
	case DLITE_WHATIS:	return dlite_whatis(arg_cnt, args);
	case DLITE_REGS:	return dlite_regs(arg_cnt, args);
	case DLITE_IREGS:	return dlite_iregs(arg_cnt, args);
	case DLITE_FPREGS:	return dlite_fpregs(arg_cnt, args);
	case DLITE_CREGS:	return dlite_cregs(arg_cnt, args);
	case DLITE_MSTATE:	return dlite_mstate(arg_cnt, args);
	case DLITE_DISPLAY:	return dlite_display(arg_cnt, args);
	case DLITE_DUMP:	return dlite_dump(arg_cnt, args);
	case DLITE_DIS:		return dlite_dis(arg_cnt, args);
	case DLITE_BREAK:	return dlite_break(arg_cnt, args);
	case DLITE_DBREAK:	return dlite_dbreak(arg_cnt, args);
	case DLITE_RBREAK:	return dlite_rbreak(arg_cnt, args);
	case DLITE_BREAKS:	return dlite_breaks(arg_cnt, args);
	case DLITE_DELETE:	return dlite_delete(arg_cnt, args);
	case DLITE_CLEAR:	return dlite_clear(arg_cnt, args);
	case DLITE_SYMBOLS:	return dlite_symbols(arg_cnt, args);
	case DLITE_TSYMBOLS:	return dlite_tsymbols(arg_cnt, args);
	case DLITE_DSYMBOLS:	return dlite_dsymbols(arg_cnt, args);
	case DLITE_SYMBOL:	return dlite_symbol(arg_cnt, args);
	default:		return "Invalid command";
	}
}

//print expression value VAL using modifiers MODIFIERS, returns error string, NULL if no error
const char * print_val(int modifiers,		//print modifiers
	eval_value_t val)			//expr value to print
{
	const char *format = "", *prefix = "";
	char radix, buf[512];

	//fill in any default size
	if((modifiers & MOD_SIZES) == 0)
	{
		//compute default size
		switch(val.type)
		{
			case et_int:	modifiers |= MOD_WORD;		break;
			case et_uint:	modifiers |= MOD_WORD;		break;
			case et_addr:
				if(sizeof(md_addr_t) > 4)
					modifiers |= MOD_QWORD;
				else
					modifiers |= MOD_WORD;
				break;
			case et_qword:	modifiers |= MOD_QWORD;		break;
			case et_sqword:	modifiers |= MOD_QWORD;		break;
			case et_float:	modifiers |= MOD_FLOAT;		break;
			case et_double:	modifiers |= MOD_DOUBLE;	break;
			case et_symbol:
			default:	return "bad print value";
		}
	}
	if(((modifiers & MOD_SIZES) & ((modifiers & MOD_SIZES) - 1)) != 0)
		return "multiple size specifiers";

	//fill in any default format
	if((modifiers & MOD_FORMATS) == 0)
	{
		//compute default size
		switch(val.type)
		{
			case et_int:	modifiers |= MOD_DECIMAL;	break;
			case et_uint:	modifiers |= MOD_UNSIGNED;	break;
			case et_addr:	modifiers |= MOD_HEX;		break;
			case et_qword:	modifiers |= MOD_UNSIGNED;	break;
			case et_sqword:	modifiers |= MOD_DECIMAL;	break;
			case et_float:					break;	//use default format
			case et_double:					break;	//use default format
			case et_symbol:
			default:	return "bad print value";
		}
	}
	if(((modifiers & MOD_FORMATS) & ((modifiers & MOD_FORMATS) - 1)) != 0)
		return "multiple format specifiers";

	//decode modifiers
	if(modifiers & (MOD_BYTE|MOD_HALF|MOD_WORD|MOD_QWORD))
	{
		if(modifiers & MOD_DECIMAL)
			radix = 'd';
		else if(modifiers & MOD_UNSIGNED)
			radix = 'u';
		else if(modifiers & MOD_OCTAL)
			radix = 'o';
		else if(modifiers & MOD_HEX)
			radix = 'x';
		else if(modifiers & MOD_BINARY)
			return "binary format not yet implemented";
		else
			panic("no default integer format");

		if(modifiers & MOD_BYTE)
		{
			if(modifiers & MOD_OCTAL)
			{
				prefix = "0";
				format = "03";
			}
			else if(modifiers & MOD_HEX)
			{
				prefix = "0x";
				format = "02";
			}
			else
			{
				prefix = "";
				format = "";
			}

			sprintf(buf, "%s%%%s%c", prefix, format, radix);
			myfprintf(stdout, buf, eval_as<unsigned int>(val));
		}
		else if(modifiers & MOD_HALF)
		{
			if(modifiers & MOD_OCTAL)
			{
				prefix = "0";
				format = "06";
			}
			else if(modifiers & MOD_HEX)
			{
				prefix = "0x";
				format = "04";
			}
			else
			{
				prefix = "";
				format = "";
			}

			sprintf(buf, "%s%%%s%c", prefix, format, radix);
			myfprintf(stdout, buf, eval_as<unsigned int>(val));
		}
		else if(modifiers & MOD_WORD)
		{
			if(modifiers & MOD_OCTAL)
			{
				prefix = "0";
				format = "011";
			}
			else if(modifiers & MOD_HEX)
			{
				prefix = "0x";
				format = "08";
			}
			else
			{
				prefix = "";
				format = "";
			}

			sprintf(buf, "%s%%%s%c", prefix, format, radix);
			myfprintf(stdout, buf, eval_as<unsigned int>(val));
		}
		else if(modifiers & MOD_QWORD)
		{
			if(modifiers & MOD_OCTAL)
			{
				prefix = "0";
				format = "022";
			}
			else if(modifiers & MOD_HEX)
			{
				prefix = "0x";
				format = "016";
			}
			else
			{
				prefix = "";
				format = "";
			}

			sprintf(buf, "%s%%%sl%c", prefix, format, radix);
			myfprintf(stdout, buf, eval_as<qword_t>(val));
		}
	}
	else if(modifiers & MOD_FLOAT)
		fprintf(stdout, "%f", (double)eval_as<float>(val));
	else if(modifiers & MOD_DOUBLE)
		fprintf(stdout, "%f", eval_as<double>(val));
	else if(modifiers & MOD_CHAR)
		fprintf(stdout, "`%c'", eval_as<unsigned int>(val));
	else if(modifiers & MOD_STRING)
		return "string format not yet implemented";
	else	//no format specified, default to value type format
		panic("no default format");
	return NULL;
}

//default memory state accessor, returns error string, NULL if no error
const char * dlite_mem_obj(mem_t *mem,		//memory space to access
	int is_write,				//access type
	md_addr_t addr,				//address to access
	char *p,				//input/output buffer
	int nbytes)				//size of access
{
	mem_cmd cmd;
	if(!is_write)
		cmd = Read;
	else
		cmd = Write;
#if 0
	char *errstr;
	errstr = mem_valid(cmd, addr, nbytes, /* !declare */FALSE);
	if(errstr)
		return errstr;
#endif
	//else, no error, access memory
	mem->mem_access(cmd, addr, p, nbytes);
	return NULL;
}

//default machine state accessor, returns error string, NULL if no error
const char * dlite_mstate_obj(FILE *stream,	//output stream
	char *cmd,				//optional command string
	regs_t *regs,				//registers to access
	mem_t *mem)				//memory to access
{
	fprintf(stream, "No machine state.\n");
	return NULL;
}

//scroll terminator, wait for user to press return
void dlite_pause(void)
{
	fprintf(stdout, "Press <return> to continue...");
	fflush(stdout);
	std::string temp;
	getline(std::cin,temp);
}

//print help information for DLite command CMD
void print_help(dlite_cmd_t *cmd)
{
	//print command name
	fprintf(stdout, "  %s ", cmd->cmd_str);

	//print arguments of command
	for(int i=0; i < MAX_ARGS && cmd->arg_strs[i] != NULL; i++)
	{
		const char * arg = cmd->arg_strs[i];
		char arg_type = arg[0];
		bool optional = (arg[1] == '?');

		if(optional)
			fprintf(stdout, "{");
		else
			fprintf(stdout, "<");

		switch(arg_type)
		{
		case 'm':
			fprintf(stdout, "/modifiers");
			break;
		case 'a':
			fprintf(stdout, "addr");
			break;
		case 'c':
			fprintf(stdout, "count");
			break;
		case 'e':
			fprintf(stdout, "expr");
			break;
		case 't':
			fprintf(stdout, "r|w|x");
			break;
		case 'i':
			fprintf(stdout, "id");
			break;
		case 's':
			fprintf(stdout, "string");
			break;
		default:
			panic("bogus argument type: `%c'", arg_type);
		}

		if(optional)
			fprintf(stdout, "}");
		else
			fprintf(stdout, ">");

		fprintf(stdout, " ");
	}
	fprintf(stdout, "\n");

	//print command description
	fprintf(stdout, "    %s\n", cmd->help_str);
}

//print help messages for all (or single) DLite debugger commands, returns error string, NULL if no error
const char * dlite_t::dlite_help(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0 && nargs != 1)
		return "too many arguments";

	dlite_cmd_t *cmd;
	if(nargs == 1)
	{
		//print help for specified commands
		for(cmd=cmd_db; cmd->cmd_str != NULL; cmd++)
		{
			if(!strcmp(cmd->cmd_str, args[0].as_str))
				break;
		}
		if(!cmd->cmd_str)
			return "command unknown";
		print_help(cmd);
	}
	else
	{
		//print help for all commands
		for(cmd=cmd_db; cmd->cmd_str != NULL; cmd++)
		{
			// "---" specifies a good point for a scroll pause
			if(!strcmp(cmd->cmd_str, "---"))
				dlite_pause();
			else
				print_help(cmd);
		}
		fprintf(stdout, "\n");
		if(dlite_help_tail)
			fprintf(stdout, "%s\n", dlite_help_tail);
	}
	return NULL;
}

//print version information for simulator, returns error string, NULL if no error
const char * dlite_t::dlite_version(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "too many arguments";

	//print simulator version info
	fprintf(stdout, "The SimpleScalar/%s Tool Set, version %d.%d of %s.\n", VER_TARGET, VER_MAJOR, VER_MINOR, VER_UPDATE);
	fprintf(stdout, "Copyright (c) 1994-1998 by Todd M. Austin.  All Rights Reserved.\n");
	return NULL;
}

//terminate simulation with statistics, returns error string, NULL if no error
const char * dlite_t::dlite_terminate(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "too many arguments";

	fprintf(stdout, "DLite: terminating simulation...\n");
	exit(1);
	return NULL;
}

//quit the simulator, omit any stats dump, returns error string, NULL if no error
const char * dlite_t::dlite_quit(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "too many arguments";

	fprintf(stdout, "DLite: exiting simulator...\n");
	exit(1);
	return NULL;
}

//continue executing program (possibly at specified address), return error string, NULL if no error
const char * dlite_t::dlite_cont(int nargs, union arg_val_t args[])	//command arguments
{
	if(!f_dlite_reg_obj || !f_dlite_mem_obj)
		panic("DLite is not configured");

	if(nargs != 0 && nargs != 1)
		return "too many arguments";

	eval_value_t val;
	if(nargs == 1)
	{
		//continue from specified address, check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset PC
		val.type = et_addr;
		val.value.as_addr = eval_as<md_addr_t>(args[0].as_value);
		f_dlite_reg_obj(regs, /* is_write */TRUE, rt_PC, 0, &val);

		myfprintf(stdout, "DLite: continuing execution @ 0x%08p...\n", val.value.as_addr);
	}

	//signal end of main debugger loop, and continuation of prog execution
	dlite_active = FALSE;
	dlite_return = TRUE;
	return NULL;
}

//step program one instruction, returns error string, NULL if no error
const char * dlite_t::dlite_step(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "too many arguments";

	//signal on instruction step
	dlite_active = TRUE;
	dlite_return = TRUE;
	return NULL;
}

#if 0 //NYI
//step program one instruction in current procedure, returns error string, NULL if no error
const char * dlite_t::dlite_next(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "too many arguments";

	//signal on instruction step
	dlite_step_cnt = 1;
	dlite_step_into = FALSE;
	return NULL;
}
#endif

//print the value of <expr> using format <modifiers>, returns error string, NULL if no error
const char * dlite_t::dlite_print(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1 && nargs != 2)
		return "wrong number of arguments";

	int modifiers = 0;
	eval_value_t val;

	if(nargs == 2)
	{
		//arguments include modifiers and expression value
		modifiers = args[0].as_modifier;
		val = args[1].as_value;
	}
	else
	{
		//arguments include only expression value
		val = args[0].as_value;
	}

	//print expression value
	const char *err_str = print_val(modifiers, val);
	if(err_str)
		return err_str;
	fprintf(stdout, "\n");
	return NULL;
}

//print the value of all command line options, returns error string, NULL if no error
const char * dlite_t::dlite_options(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "wrong number of arguments";

	//print all options
	opt_print_options(sim_odb, stdout, /* terse */TRUE, /* !notes */FALSE);
	return NULL;
}

//print the value of all (or single) command line options, returns error string, NULL if no error
const char * dlite_t::dlite_option(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1)
		return "wrong number of arguments";

	//print a single option, specified by argument
	opt_opt_t *opt = opt_find_option(sim_odb, args[0].as_str);
	if(!opt)
		return "option is not defined";

	//else, print this option's value
	fprintf(stdout, "%-16s  ", opt->name.c_str());
	opt_print_option(opt, stdout);
	if(opt->desc)
		fprintf(stdout, " # %s", opt->desc);
	fprintf(stdout, "\n");
	return NULL;
}

//print the value of all statistical variables, returns error string, NULL if no error
const char * dlite_t::dlite_stats(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "wrong number of arguments";

	//print all options
	stat_print_stats(sim_sdb, stdout);
	sim_aux_stats(stdout);
	return NULL;
}

//print the value of a statistical variable, returns error string, NULL if no error
const char * dlite_t::dlite_stat(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1)
		return "wrong number of arguments";

	//print a single option, specified by argument
	stat_stat_t *stat = stat_find_stat(sim_sdb, args[0].as_str);
	if(!stat)
		return "statistical variable is not defined";

	//else, print this option's value
	stat_print_stat(sim_sdb, stat, stdout);

	return NULL;
}

//print the type of expression <expr>, returns error string, NULL if no error
const char * dlite_t::dlite_whatis(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1)
		return "wrong number of arguments";
	fprintf(stdout, "type == `%s'\n", eval_type_str[args[0].as_value.type]);
	return NULL;
}

//print integer register contents, returns error string, NULL if no error
const char * dlite_t::dlite_iregs(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "too many arguments";

	const char *err_str;

	//print integer registers
	myfprintf(stdout, "PC: 0x%08p   NPC: 0x%08p\n", regs->regs_PC, regs->regs_NPC);
	if((err_str = dlite_cregs(nargs, args)) != NULL)
		return err_str;
	md_print_iregs(regs->regs_R, stdout);
	return NULL;
}

//print floating point register contents, returns error string, NULL if no error
const char * dlite_t::dlite_fpregs(int nargs, union arg_val_t args[])	//command arguments
{
	//print floating point registers
	md_print_fpregs(regs->regs_F, stdout);
	return NULL;
}

//print floating point register contents, returns error string, NULL if no error
const char * dlite_t::dlite_cregs(int nargs, union arg_val_t args[])	//command arguments
{
	//print floating point registers
	md_print_cregs(regs->regs_C, stdout);
	return NULL;
}

//print all register contents, returns error string, NULL if no error
const char * dlite_t::dlite_regs(int nargs, union arg_val_t args[])	//command arguments
{
	const char *err_str;

	myfprintf(stdout, "PC: 0x%08p   NPC: 0x%08p\n", regs->regs_PC, regs->regs_NPC);
	if((err_str = dlite_cregs(nargs, args)) != NULL)
		return err_str;
	md_print_iregs(regs->regs_R, stdout);
	dlite_pause();
	if((err_str = dlite_fpregs(nargs, args)) != NULL)
		return err_str;
	return NULL;
}

//print machine specific state (simulator dependent), returns error string, NULL if no error
const char * dlite_t::dlite_mstate(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0 && nargs != 1)
		return "too many arguments";

	const char * errstr = NULL;
	if(f_dlite_mstate_obj)
	{
		if(nargs == 0)
		{
			errstr = f_dlite_mstate_obj(stdout, NULL, regs, mem);
		}
		else
		{
			errstr = f_dlite_mstate_obj(stdout, args[0].as_str, regs, mem);
		}
	}
	return errstr;
}

//display the value at memory location <addr> using format <modifiers>, returns error string, NULL if no error
const char * dlite_t::dlite_display(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1 && nargs != 2)
		return "wrong number of arguments";

	int modifiers(0), size(0);
	md_addr_t addr(0);
	unsigned char buf[512];
	eval_value_t val;

	if(nargs == 1)
	{
		//no modifiers
		modifiers = 0;

		//check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset address
		addr = eval_as<md_addr_t>(args[0].as_value);
	}
	else if(nargs == 2)
	{
		modifiers = args[0].as_modifier;

		//check address
		if(!EVAL_INTEGRAL(args[1].as_value.type))
			return "address argument must be an integral type";

		//reset address
		addr = eval_as<md_addr_t>(args[1].as_value);
	}

	//determine operand size
	if(modifiers & (MOD_BYTE|MOD_CHAR))
		size = 1;
	else if(modifiers & MOD_HALF)
		size = 2;
	else if(modifiers & (MOD_QWORD|MOD_DOUBLE))
		size = 8;
	else	//no modifiers, or MOD_WORD|MOD_FLOAT
		size = 4;

	//read memory
	const char * errstr = f_dlite_mem_obj(mem, /* !is_write */FALSE, addr, (char *)buf, size);
	if(errstr)
		return errstr;

	//marshall a value
	if(modifiers & (MOD_BYTE|MOD_CHAR))
	{
		//size == 1
		val.type = et_int;
		val.value.as_int = (int)*(unsigned char *)buf;
	}
	else if(modifiers & MOD_HALF)
	{
		//size == 2
		val.type = et_int;
		unsigned short * temp = (unsigned short *)buf;
		val.value.as_int = (int)*temp;

	}
	else if(modifiers & (MOD_QWORD|MOD_DOUBLE))
	{
		//size == 8
		val.type = et_double;
		double * temp = (double *)buf;
		val.value.as_double = *temp;
	}
	else	//no modifiers, or MOD_WORD|MOD_FLOAT
	{
		//size == 4
		val.type = et_uint;
		unsigned int * temp = (unsigned int *)buf;
		val.value.as_uint = *temp;
	}

	//print the value
	errstr = print_val(modifiers, val);
	if(errstr)
		return errstr;
	fprintf(stdout, "\n");
	return NULL;
}

//dump the contents of memory to screen, returns error string, NULL if no error
const char * dlite_t::dlite_dump(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs < 0 || nargs > 2)
		return "too many arguments";

	int i, j;
	int count = LINES_PER_SCREEN * BYTES_PER_LINE, i_count, fmt_count, fmt_lines;
	md_addr_t fmt_addr, i_addr;
	static md_addr_t addr = 0;
	unsigned char byte;
	char buf[512];
	const char *errstr;

	if(nargs == 1)
	{
		//check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset PC
		addr = eval_as<md_addr_t>(args[0].as_value);
	}
	else if(nargs == 2)
	{
		//check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset addr
		addr = eval_as<md_addr_t>(args[0].as_value);

		//check count
		if(!EVAL_INTEGRAL(args[1].as_value.type))
			return "count argument must be an integral type";

		if(eval_as<unsigned int>(args[1].as_value) > 1024)
			return "bad count argument";

		//reset count
		count = eval_as<unsigned int>(args[1].as_value);
	}
	//else, nargs == 0, use addr, count

	//normalize start address and count
	fmt_addr = addr & ~(BYTES_PER_LINE - 1);
	fmt_count = (count + (BYTES_PER_LINE - 1)) & ~(BYTES_PER_LINE - 1);
	fmt_lines = fmt_count / BYTES_PER_LINE;

	if(fmt_lines < 1)
		panic("no output lines");

	//print dump
	if(fmt_lines == 1)
	{
		//unformatted dump
		i_addr = fmt_addr;
		myfprintf(stdout, "0x%08p: ", i_addr);
		for(i=0; i < count; i++)
		{
			errstr = f_dlite_mem_obj(mem, /* !is_write */FALSE, i_addr, (char *)&byte, 1);
			if(errstr)
				return errstr;
			fprintf(stdout, "%02x ", byte);
			if(isprint(byte))
				buf[i] = byte;
			else
				buf[i] = '.';
			i_addr++;
			addr++;
		}
		buf[i] = '\0';
		//character view
		fprintf(stdout, "[%s]\n", buf);
	}
	else	//lines > 1
	{
		i_count = 0;
		i_addr = fmt_addr;
		for(i=0; i < fmt_lines; i++)
		{
			myfprintf(stdout, "0x%08p: ", i_addr);

			//byte view
			for(j=0; j < BYTES_PER_LINE; j++)
			{
				if(i_addr >= addr && i_count <= count)
				{
					errstr = f_dlite_mem_obj(mem, /* !is_write */FALSE, i_addr, (char *)&byte, 1);
					if(errstr)
						return errstr;
					fprintf(stdout, "%02x ", byte);
					if(isprint(byte))
						buf[j] = byte;
					else
						buf[j] = '.';
					i_count++;
					addr++;
				}
				else
				{
					fprintf(stdout, "   ");
					buf[j] = ' ';
				}
				i_addr++;
			}
			buf[j] = '\0';
			//character view
			fprintf(stdout, "[%s]\n", buf);
		}
	}
	return NULL;
}

//disassemble instructions at specified address, returns error string, NULL if no error
const char * dlite_t::dlite_dis(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs < 0 || nargs > 2)
		return "too many arguments";

	int count = INSTS_PER_SCREEN;
	static md_addr_t addr = 0;

	if(nargs == 1)
	{
		//check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset PC
		addr = eval_as<md_addr_t>(args[0].as_value);
	}
	else if(nargs == 2)
	{
		//check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset addr
		addr = eval_as<md_addr_t>(args[0].as_value);

		//check count
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "count argument must be an integral type";

		//reset count
		count = eval_as<unsigned int>(args[1].as_value);

		if(count < 0 || count > 1024)
			return "bad count argument";
	}
	//else, nargs == 0, use addr, count

	if((addr % sizeof(md_inst_t)) != 0)
		return "instruction addresses are a multiple of eight";

	//disassemble COUNT insts at ADDR
	md_inst_t inst;
	for(int i=0; i<count; i++)
	{
		//read and disassemble instruction
		myfprintf(stdout, "    0x%08p:   ", addr);
		const char * errstr  = f_dlite_mem_obj(mem, /* !is_write */FALSE, addr, (char *)&inst, sizeof(inst));
		inst = MD_SWAPI(inst);
		if(errstr)
			return errstr;
		md_print_insn(inst, addr, stdout);
		fprintf(stdout, "\n");

		//go to next instruction
		addr += sizeof(md_inst_t);
	}
	return NULL;
}

//return breakpoint class as a string
const char * bp_class_str(int bpclass)			//breakpoint class mask
{
	if(bpclass == (ACCESS_READ|ACCESS_WRITE|ACCESS_EXEC))
		return "read|write|exec";
	else if(bpclass == (ACCESS_READ|ACCESS_WRITE))
		return "read|write";
	else if(bpclass == (ACCESS_WRITE|ACCESS_EXEC))
		return "write|exec";
	else if(bpclass == (ACCESS_READ|ACCESS_EXEC))
		return "read|exec";
	else if(bpclass == ACCESS_READ)
		return "read";
	else if(bpclass == ACCESS_WRITE)
		return "write";
	else if(bpclass == ACCESS_EXEC)
		return "exec";
	else
		panic("bogus access breakpoint class");
}

//set a breakpoint of class CLASS at address ADDR, returns error string, NULL if no error
const char * dlite_t::set_break(int bpclass,		//break class, use ACCESS_*
	range_range_t *range)				//range breakpoint
{
	//add breakpoint to break list
	dlite_break_t * bp = (dlite_break_t *)calloc(1, sizeof(dlite_break_t));
	if(!bp)
		fatal("out of virtual memory");

	bp->id = break_id++;
	bp->range = *range;
	bp->bpclass = bpclass;

	bp->next = dlite_bps;
	dlite_bps = bp;

	fprintf(stdout, "breakpoint #%d set @ ", bp->id);
	range_print_range(&bp->range, stdout);
	fprintf(stdout, ", class: %s\n", bp_class_str(bpclass));

	//a breakpoint is set now, check for a breakpoint
	dlite_check = TRUE;
	return NULL;
}

//delete breakpoint with id ID, returns error string, NULL if no error
const char * dlite_t::delete_break(int id)			//id of brkpnt to delete
{
	if(!dlite_bps)
		return "no breakpoints set";

	dlite_break_t *bp, *prev;
	for(bp=dlite_bps,prev=NULL; bp != NULL; prev=bp,bp=bp->next)
	{
		if(bp->id == id)
			break;
	}
	if(!bp)
		return "breakpoint not found";

	if(!prev)
	{
		//head of list, unlink
		dlite_bps = bp->next;
	}
	else
	{
		//middle or end of list
		prev->next = bp->next;
	}

	fprintf(stdout, "breakpoint #%d deleted @ ",  bp->id);
	range_print_range(&bp->range, stdout);
	fprintf(stdout, ", class: %s\n", bp_class_str(bp->bpclass));

	bp->next = NULL;
	free(bp);

	if(!dlite_bps)
	{
		//no breakpoints set, cancel checks
		dlite_check = FALSE;
	}
	else
	{
		//breakpoints are set, do checks
		dlite_check = TRUE;
	}
	return NULL;
}

//internal break check interface, returns non-zero if breakpoint is hit.
int dlite_t::__check_break(md_addr_t next_PC,		//address of next inst
	int access,					//mem access of last inst
	md_addr_t addr,					//mem addr of last inst
	counter_t icount,				//instruction count
	counter_t cycle)				//cycle count
{
	dlite_break_t *bp;

	if(dlite_active)
	{
		//single-stepping, break always
		break_access = 0;	//single step
		return TRUE;
	}
	//else, check for a breakpoint

	for(bp=dlite_bps; bp != NULL; bp=bp->next)
	{
		switch(bp->range.start.ptype)
		{
		case pt_addr:
			if((bp->bpclass & ACCESS_EXEC) && !range_cmp_range(&bp->range, next_PC))
			{
				//hit a code breakpoint
				myfprintf(stdout, "Stopping at code breakpoint #%d @ 0x%08p...\n", bp->id, next_PC);
				break_access = ACCESS_EXEC;
				return TRUE;
			}
			if((bp->bpclass & ACCESS_READ) && ((access & ACCESS_READ) && !range_cmp_range(&bp->range, addr)))
			{
				//hit a read breakpoint
				myfprintf(stdout, "Stopping at read breakpoint #%d @ 0x%08p...\n", bp->id, addr);
				break_access = ACCESS_READ;
				return TRUE;
			}
			if((bp->bpclass & ACCESS_WRITE) && ((access & ACCESS_WRITE) && !range_cmp_range(&bp->range, addr)))
			{
				//hit a write breakpoint
				myfprintf(stdout, "Stopping at write breakpoint #%d @ 0x%08p...\n", bp->id, addr);
				break_access = ACCESS_WRITE;
				return TRUE;
			}
			break;
		case pt_inst:
			if(!range_cmp_range(&bp->range, icount))
			{
				//hit a code breakpoint
				fprintf(stdout, "Stopping at inst count breakpoint #%d @ %.0f...\n", bp->id, (double)icount);
				break_access = ACCESS_EXEC;
				return TRUE;
			}
			break;
		case pt_cycle:
			if(!range_cmp_range(&bp->range, cycle))
			{
				//hit a code breakpoint
				fprintf(stdout, "Stopping at cycle count breakpoint #%d @ %.0f...\n", bp->id, (double)cycle);
				break_access = ACCESS_EXEC;
				return TRUE;
			}
			break;
		default:
			panic("bogus range type");
		}
	}
	//no matching breakpoint found
	break_access = 0;
	return FALSE;
}

//set a text breakpoint, returns error string, NULL if no error
const char * dlite_t::dlite_break(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1)
		return "wrong number of arguments";

	//check address
	if(!EVAL_INTEGRAL(args[0].as_value.type))
		return "address argument must be an integral type";

	//reset addr
	md_addr_t addr = eval_as<md_addr_t>(args[0].as_value);

	//build the range
	range_range_t range;
	range.start.ptype = pt_addr;
	range.start.pos = addr;
	range.end.ptype = pt_addr;
#ifdef TARGET_ALPHA
	//need some extra space here, as functional have multiple entry points depending on if $GP needs to be loaded or not
	range.end.pos = addr + 9;
#else
	range.end.pos = addr + 1;
#endif
	//set a code break point
	return set_break(ACCESS_EXEC, &range);
}

//set a data breakpoint at specified address, returns errory string, NULL if no error
const char * dlite_t::dlite_dbreak(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1 && nargs != 2)
		return "wrong number of arguments";

	int access(0);
	md_addr_t addr(0);

	if(nargs == 1)
	{
		//check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset addr
		addr = eval_as<md_addr_t>(args[0].as_value);

		//break on read or write
		access = ACCESS_READ|ACCESS_WRITE;
	}
	else if(nargs == 2)
	{
		//check address
		if(!EVAL_INTEGRAL(args[0].as_value.type))
			return "address argument must be an integral type";

		//reset addr
		addr = eval_as<md_addr_t>(args[0].as_value);

		//get access
		access = args[1].as_access;
	}

	//build the range
	range_range_t range;
	range.start.ptype = pt_addr;
	range.start.pos = addr;
	range.end.ptype = pt_addr;
	range.end.pos = addr + 1;

	//set the breakpoint
	return set_break(access, &range);
}

//set a breakpoint at specified range, returns error string, NULL if no error
const char * dlite_t::dlite_rbreak(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1 && nargs != 2)
		return "wrong number of arguments";

	int access;
	range_range_t range;

	if(nargs == 2)
	{
		//get access
		access = args[1].as_access;
	}
	else
	{
		//break on read or write or exec
		access = ACCESS_READ|ACCESS_WRITE|ACCESS_EXEC;
	}

	//check range
	const char *errstr = range_parse_range(args[0].as_str, &range, mem);
	if(errstr)
		return errstr;

	//sanity checks for ranges
	if(range.start.ptype != range.end.ptype)
		return "range endpoints are not of the same type";
	else if(range.start.pos > range.end.pos)
		return "range start is after range end";

	//set the breakpoint
	return set_break(access, &range);
}

//list all outstanding breakpoints, returns error string, NULL if no error
const char * dlite_t::dlite_breaks(int nargs, union arg_val_t args[])	//command arguments
{
	if(!dlite_bps)
	{
		fprintf(stdout, "No active breakpoints.\n");
		return NULL;
	}

	fprintf(stdout, "Active breakpoints:\n");
	for(dlite_break_t *bp=dlite_bps; bp != NULL; bp=bp->next)
	{
		fprintf(stdout, "  breakpoint #%d @ ",  bp->id);
		range_print_range(&bp->range, stdout);
		fprintf(stdout, ", class: %s\n", bp_class_str(bp->bpclass));
	}
	return NULL;
}

//delete specified breakpoint, returns error string, NULL if no error
const char * dlite_t::dlite_delete(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1)
		return "wrong number of arguments";

	//check bp id
	if(!EVAL_INTEGRAL(args[0].as_value.type))
		return "id must be an integral type";

	int id = eval_as<unsigned int>(args[0].as_value);
	return delete_break(id);
}

//clear all breakpoints, returns error string, NULL if no error
const char * dlite_t::dlite_clear(int nargs, union arg_val_t args[])	//command arguments
{
	if(!dlite_bps)
	{
		fprintf(stdout, "No active breakpoints.\n");
		return NULL;
	}

	while(dlite_bps != NULL)
	{
		//delete first breakpoint
		delete_break(dlite_bps->id);
	}
	fprintf(stdout, "All breakpoints cleared.\n");
	return NULL;
}

//print the value of all program symbols, returns error string, NULL if no error
const char * dlite_t::dlite_symbols(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "wrong number of arguments";

	//load symbols, if not already loaded
	sym_loadsyms(mem->ld_prog_fname.c_str(), /* !locals */FALSE, mem);

	//print all symbol values
	for(int i=0; i<sym_nsyms; i++)
		sym_dumpsym(sym_syms[i], stdout);
	return NULL;
}

//print the value of all text symbols, returns error string, NULL if no error
const char * dlite_t::dlite_tsymbols(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "wrong number of arguments";

	//load symbols, if not already loaded
	sym_loadsyms(mem->ld_prog_fname.c_str(), /* !locals */FALSE, mem);

	//print all symbol values
	for(int i=0; i<sym_ntextsyms; i++)
		sym_dumpsym(sym_textsyms[i], stdout);
	return NULL;
}

//print the value of all text symbols, returns error string, NULL if no error
const char * dlite_t::dlite_dsymbols(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 0)
		return "wrong number of arguments";

	//load symbols, if not already loaded
	sym_loadsyms(mem->ld_prog_fname.c_str(), /* !locals */FALSE, mem);

	//print all symbol values
	for(int i=0; i<sym_ndatasyms; i++)
		sym_dumpsym(sym_datasyms[i], stdout);
	return NULL;
}

//print the value of all (or single) command line options, returns error string, NULL if no error
const char * dlite_t::dlite_symbol(int nargs, union arg_val_t args[])	//command arguments
{
	if(nargs != 1)
		return "wrong number of arguments";

	//load symbols, if not already loaded
	sym_loadsyms(mem->ld_prog_fname.c_str(), /* !locals */FALSE, mem);

	//print a single option, specified by argument
	int index;
	sym_sym_t * sym = sym_bind_name(args[0].as_str, &index, sdb_any);
	if(!sym)
		return "symbol is not defined";
	//else, print this symbols's value
	sym_dumpsym(sym_syms_by_name[index], stdout);
	return NULL;
}

//print a mini-state header
void dlite_t::dlite_status(md_addr_t regs_PC,			//PC of just completed inst
	md_addr_t next_PC,					//PC of next inst to exec
	counter_t cycle,					//current cycle
	int dbreak)						//last break a data break?
{
	md_inst_t inst;
	const char *errstr;

	if(dbreak)
	{
		fprintf(stdout, "\n");
		fprintf(stdout, "Instruction (now finished) that caused data break:\n");
		myfprintf(stdout, "[%10n] 0x%08p:    ", cycle, regs_PC);
		errstr = f_dlite_mem_obj(mem, /* !is_write */FALSE, regs_PC, (char *)&inst, sizeof(inst));
		inst = MD_SWAPI(inst);
		if(errstr)
			fprintf(stdout, "<invalid memory>: %s", errstr);
		else
		md_print_insn(inst, regs_PC, stdout);
		fprintf(stdout, "\n\n");
	}

	//read and disassemble instruction
	myfprintf(stdout, "[%10n] 0x%08p:    ", cycle, next_PC);
	errstr = f_dlite_mem_obj(mem, /* !is_write */FALSE, next_PC, (char *)&inst, sizeof(inst));
	inst = MD_SWAPI(inst);
	if(errstr)
		fprintf(stdout, "<invalid memory>: %s", errstr);
	else
		md_print_insn(inst, next_PC, stdout);
	fprintf(stdout, "\n");
}

//DLite debugger main loop
void dlite_t::dlite_main(md_addr_t regs_PC,	//addr of last inst to exec
	md_addr_t next_PC,			//addr of next inst to exec
	counter_t cycle)			//current procesor cycle
{
	dlite_active = TRUE;
	dlite_return = FALSE;
	int dbreak = (break_access & (ACCESS_READ|ACCESS_WRITE)) != 0;
	dlite_status(regs_PC, next_PC, cycle, dbreak);

	std::string buf;
	static char cmd[512] = "";
	while(dlite_active && !dlite_return)
	{
		fprintf(stdout, DLITE_PROMPT);
		fflush(stdout);
		getline(std::cin, buf);

		if(buf[0] != '\0')
		{
			//use this command
			strcpy(cmd, buf.c_str());
		}
		//else, use last command

		const char * err_str = dlite_exec(cmd);
		if(err_str)
			fprintf(stdout, "Dlite: error: %s\n", err_str);
	}
}
