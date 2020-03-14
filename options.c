/* options.c - options package routines */

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
#ifndef _MSC_VER
#include <unistd.h>
#else /* _MSC_VER */
#define chdir	_chdir
#define getcwd	_getcwd
#endif
#include<string>
#include<vector>
#include<ctype.h>
#include<float.h>
#include<fstream>

#include "host.h"
#include "misc.h"
#include "options.h"

opt_odb_t::opt_odb_t()
: options(NULL), orphan_fn(NULL), header(NULL), notes(NULL)
{}

opt_odb_t::opt_odb_t(orphan_fn_t orphan_fn)
: options(NULL), orphan_fn(orphan_fn), header(NULL), notes(NULL)
{}

opt_note_t::opt_note_t()
: next(NULL), note(NULL)
{}

//free an option database
void opt_delete(opt_odb_t *odb)
{
	opt_opt_t *opt, *opt_next;
	opt_note_t *note, *note_next;

	//free all options
	for(opt=odb->options; opt; opt=opt_next)
	{
		opt_next = opt->next;
		opt->next = NULL;
		free(opt);
	}

	//free all notes
	for(note = odb->notes; note != NULL; note = note_next)
	{
		note_next = note->next;
		note->next = NULL;
		free(note);
	}
	odb->notes = NULL;
	free(odb);
}

//add option OPT to option database ODB
void add_option(opt_odb_t *odb,			//option database
	opt_opt_t *opt)				//option variable
{
	opt_opt_t *elt, *prev;

	//sanity checks on option name
	if(!opt->name.empty() && opt->name[0] != '-')
	{
		panic("option `%s' must start with a `-'", opt->name.c_str());
	}

	//add to end of option list
	for(prev=NULL, elt=odb->options; elt != NULL; prev=elt, elt=elt->next)
	{
		//sanity checks on option name
		//Efficiency isn't really that important here for potentially badly indexed code
		if(elt->name == opt->name)
		{
			panic("option `%s' is multiply defined", opt->name.c_str());
		}
	}

	if(prev != NULL)
	{
		prev->next = opt;
	}
	else	//prev == NULL
	{
		odb->options = opt;
	}
	opt->next = NULL;
}

//register an integer option variable
void opt_reg_int(opt_odb_t *odb,		//option data base
	std::string name, std::string offset,	//option name
	const char *desc,			//option description
	int *var,				//target variable
	int def_val,				//default variable value
	int print,				//print during `-dumpconfig'?
	char *format)				//optional value print format/
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12d");
	opt->oc = oc_int;
	opt->variant.for_int.var = var;
	opt->print = print;
	opt->accrue = FALSE;

	//place on ODB's option list
	opt->next = NULL;
	add_option(odb, opt);

	//set default value
	*var = def_val;
}

//register an long long integer option variable
void opt_reg_long_long(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,			/* option name */
	const char *desc,			/* option description */
	long long *var,			/* target variable */
	long long def_val,		/* default variable value */
	int print,			/* print during `-dumpconfig'? */
	char *format)		/* optional value print format */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12lld");
	opt->oc = oc_long_long;
	opt->variant.for_long_long.var = var;
	opt->print = print;
	opt->accrue = FALSE;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	*var = def_val;
}

/* register an integer option list */
void
opt_reg_int_list(opt_odb_t *odb,/* option database */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	int *vars,		/* pointer to option array */
	int nvars,		/* total entries in option array */
	int *nelt,		/* number of entries parsed */
	int *def_val,		/* default value of option array */
	int print,		/* print during `-dumpconfig'? */
	char *format,		/* optional user print format */
	int accrue)		/* accrue list across uses */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = nvars;
	opt->nelt = nelt;
	opt->format = (char *)(format ? format : "%12d");
	opt->oc = oc_int;
	opt->variant.for_int.var = vars;
	opt->print = print;
	opt->accrue = accrue;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	for(int i=0; i < *nelt; i++)
		vars[i] = def_val[i];
}

/* register an unsigned integer option variable */
void
opt_reg_uint(opt_odb_t *odb,	/* option database */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	unsigned int *var,		/* pointer to option variable */
	unsigned int def_val,	/* default value of option variable */
	int print,			/* print during `-dumpconfig'? */
	char *format)		/* optional user print format */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12u");
	opt->oc = oc_uint;
	opt->variant.for_uint.var = var;
	opt->print = print;
	opt->accrue = FALSE;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	*var = def_val;
}

/* register an unsigned integer option list */
void
opt_reg_uint_list(opt_odb_t *odb,/* option database */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	unsigned int *vars,	/* pointer to option array */
	int nvars,		/* total entries in option array */
	int *nelt,		/* number of elements parsed */
	unsigned int *def_val,/* default value of option array */
	int print,		/* print opt during `-dumpconfig'? */
	char *format,		/* optional user print format */
	int accrue)		/* accrue list across uses */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = nvars;
	opt->nelt = nelt;
	opt->format = (char *)(format ? format : "%u");
	opt->oc = oc_uint;
	opt->variant.for_uint.var = vars;
	opt->print = print;
	opt->accrue = accrue;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	for(int i=0; i < *nelt; i++)
		vars[i] = def_val[i];
}

/* register a single-precision floating point option variable */
void
opt_reg_float(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	float *var,		/* target option variable */
	float def_val,		/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format)		/* optional value print format */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12.4f");
	opt->oc = oc_float;
	opt->variant.for_float.var = var;
	opt->print = print;
	opt->accrue = FALSE;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	*var = def_val;
}

/* register a single-precision floating point option array */
void
opt_reg_float_list(opt_odb_t *odb,/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	float *vars,		/* target array */
	int nvars,		/* target array size */
	int *nelt,		/* number of args parsed goes here */
	float *def_val,	/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format,	/* optional value print format */
	int accrue)		/* accrue list across uses */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = nvars;
	opt->nelt = nelt;
	opt->format = (char *)(format ? format : "%.4f");
	opt->oc = oc_float;
	opt->variant.for_float.var = vars;
	opt->print = print;
	opt->accrue = accrue;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	for(int i=0; i < *nelt; i++)
		vars[i] = def_val[i];
}

/* register a double-precision floating point option variable */
void
opt_reg_double(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	double *var,		/* target variable */
	double def_val,		/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format)		/* option value print format */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12.4f");
	opt->oc = oc_double;
	opt->variant.for_double.var = var;
	opt->print = print;
	opt->accrue = FALSE;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	*var = def_val;
}

/* register a double-precision floating point option array */
void
opt_reg_double_list(opt_odb_t *odb, /* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	double *vars,	/* target array */
	int nvars,		/* target array size */
	int *nelt,		/* number of args parsed goes here */
	double *def_val,	/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format,	/* option value print format */
	int accrue)		/* accrue list across uses */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = nvars;
	opt->nelt = nelt;
	opt->format = (char *)(format ? format : "%.4f");
	opt->oc = oc_double;
	opt->variant.for_double.var = vars;
	opt->print = print;
	opt->accrue = accrue;

	//place on ODB's option list
	opt->next = NULL;
	add_option(odb, opt);

	//set default value
	for(int i=0; i < *nelt; i++)
	{
		vars[i] = def_val[i];
	}
}

//bind an enumeration string to an enumeration value
int bind_to_enum(std::string str,		//string to bind to an enum
	const char **emap,			//enumeration string map
	int *eval,				//enumeration value map, optional
	int emap_sz,				//size of maps
	int *res)				//enumeration string value result
{
	//string enumeration string map
	for(int i=0; i<emap_sz; i++)
	{
		if(str==emap[i])
		{
			if(eval)
			{
				//bind to eval value
				*res = eval[i];
			}
			else
			{
				//bind to string index
				*res = i;
			}
			return TRUE;
		}
	}

	//not found
	*res = -1;
	return FALSE;
}

//bind a enumeration value to an enumeration string
const char * bind_to_str(int val,		//enumeration value
	const char **emap,			//enumeration string map
	int *eval,				//enumeration value map, optional
	int emap_sz)				//size of maps
{
	if(eval)
	{
		//bind to first matching eval value */
		for(int i=0; i<emap_sz; i++)
		{
			if(eval[i] == val)
			{	//found
				return emap[i];
			}
		}
		//not found
		return NULL;
	}
	else
	{
		//bind to string at index
		if(val >= emap_sz)
		{
			//invalid index
			return NULL;
		}
		//else, index is in range
		return emap[val];
	}
}

//register an enumeration option variable, NOTE: all enumeration option
//variables must be of type `int', since true enum variables may be allocated
//with variable sizes by some compilers
void opt_reg_enum(opt_odb_t *odb,		//option data base
	std::string name, std::string offset,	//option name
	const char *desc,			//option description
	int *var,				//target variable
	char *def_val,				//default variable value
	const char **emap,			//enumeration string map
	int *eval,				//enumeration value map, optional
	int emap_sz,				//size of maps
	int print,				//print during `-dumpconfig'?
	char *format)				//option value print format
{
	int enum_val;
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12s");
	opt->oc = oc_enum;
	opt->variant.for_enum.var = var;
	opt->variant.for_enum.emap = emap;
	opt->variant.for_enum.eval = eval;
	opt->variant.for_enum.emap_sz = emap_sz;
	if(def_val)
	{
		if(!bind_to_enum(def_val, emap, eval, emap_sz, &enum_val))
		{
			fatal("could not bind default value for option `%s'", name.c_str());
		}
	}
	else
	{
		enum_val = 0;
	}
	opt->print = print;
	opt->accrue = FALSE;

	//place on ODB's option list
	opt->next = NULL;
	add_option(odb, opt);

	//set default value
	*var = enum_val;
}

//register an enumeration option array, NOTE: all enumeration option variables
//must be of type `int', since true enum variables may be allocated with
//variable sizes by some compilers
void opt_reg_enum_list(opt_odb_t *odb,		//option data base
	std::string name, std::string offset,	//option name
	const char *desc,			//option description
	int *vars,				//target array
	int nvars,				//target array size
	int *nelt,				//number of args parsed goes here
	char *def_val,				//default variable value
	const char **emap,			//enumeration string map
	int *eval,				//enumeration value map, optional
	int emap_sz,				//size of maps
	int print,				//print during `-dumpconfig'?
	char *format,				//option value print format
	int accrue)				//accrue list across uses
{
	int enum_val;
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = nvars;
	opt->nelt = nelt;
	opt->format = (char *)(format ? format : "%s");
	opt->oc = oc_enum;
	opt->variant.for_enum.var = vars;
	opt->variant.for_enum.emap = emap;
	opt->variant.for_enum.eval = eval;
	opt->variant.for_enum.emap_sz = emap_sz;
	if(def_val)
	{
		if(!bind_to_enum(def_val, emap, eval, emap_sz, &enum_val))
		{
			fatal("could not bind default value for option `%s'", name.c_str());
		}
	}
	else
	{
		enum_val = 0;
	}
	opt->print = print;
	opt->accrue = accrue;

	//place on ODB's option list
	opt->next = NULL;
	add_option(odb, opt);

	//set default value
	for(int i=0; i < *nelt; i++)
	{
		vars[i] = enum_val;
	}
}

//pre-defined boolean flag operands
#define NUM_FLAGS		28
const char *flag_emap[NUM_FLAGS] =
{
	"true", "t", "T", "True", "TRUE", "1", "y", "Y", "yes", "Yes", "YES",
	"on", "On", "ON",
	"false", "f", "F", "False", "FALSE", "0", "n", "N", "no", "No", "NO",
	"off", "Off", "OFF"
};

int flag_eval[NUM_FLAGS] =
{
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//register a boolean flag option variable
void opt_reg_flag(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	int *var,			/* target variable */
	int def_val,		/* default variable value */
	int print,			/* print during `-dumpconfig'? */
	char *format)		/* optional value print format */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;

	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12s");
	opt->oc = oc_flag;
	opt->variant.for_enum.var = var;
	opt->variant.for_enum.emap = flag_emap;
	opt->variant.for_enum.eval = flag_eval;
	opt->variant.for_enum.emap_sz = NUM_FLAGS;
	opt->print = print;
	opt->accrue = FALSE;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	/* set default value */
	*var = def_val;
}

//register a boolean flag option array
void opt_reg_flag_list(opt_odb_t *odb,/* option database */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	int *vars,		/* pointer to option array */
	int nvars,		/* total entries in option array */
	int *nelt,		/* number of elements parsed */
	int *def_val,		/* default array value */
	int print,		/* print during `-dumpconfig'? */
	char *format,		/* optional value print format */
	int accrue)		/* accrue list across uses */
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = nvars;
	opt->nelt = nelt;
	opt->format = (char *)(format ? format : "%s");
	opt->oc = oc_flag;
	opt->variant.for_enum.var = vars;
	opt->variant.for_enum.emap = flag_emap;
	opt->variant.for_enum.eval = flag_eval;
	opt->variant.for_enum.emap_sz = NUM_FLAGS;
	opt->print = print;
	opt->accrue = accrue;

	/* place on ODB's option list */
	opt->next = NULL;
	add_option(odb, opt);

	//set default value
	for(int i=0; i < *nelt; i++)
	{
		vars[i] = def_val[i];
	}
}

//register a string option variable
void opt_reg_string(opt_odb_t *odb,		//option data base
	std::string name, std::string offset,	//option name
	const char *desc,			//option description
	char **var,				//pointer to string option variable
	const char *def_val,			//default variable value
	int print,				//print during `-dumpconfig'?
	char *format)				//optional value print format
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = 1;
	opt->nelt = NULL;
	opt->format = (char *)(format ? format : "%12s");
	opt->oc = oc_string;
	opt->variant.for_string.var = const_cast<const char **>(var);
	opt->print = print;
	opt->accrue = FALSE;

	//place on ODB's option list
	opt->next = NULL;
	add_option(odb, opt);

	//set default value
	//FIXME, the cast below is not safe, although, within constraints of the simulator is probably fine
	*var = const_cast<char *>(def_val);
	if(def_val)
	{
		opt->var.push_back(def_val);
	}
}

//register a string option array
void opt_reg_string_list(opt_odb_t *odb,	//option data base
	std::string name, std::string offset,	//option name
	const char *desc,			//option description
	char **vars,				//pointer to option string array
	int nvars,				//target array size
	int *nelt,				//number of args parsed goes here
	char **def_val,				//default variable value
	int print,				//print during `-dumpconfig'?
	char *format,				//optional value print format
	int accrue)				//accrue list across uses
{
	opt_opt_t *opt = new opt_opt_t;

	opt->name = name + offset;
	opt->desc = desc;
	opt->nvars = nvars;
	opt->nelt = nelt;
	opt->format = (char *)(format ? format : "%s");
	opt->oc = oc_string;
	opt->variant.for_string.var = const_cast<const char **>(vars);
	opt->print = print;
	opt->accrue = accrue;

	//place on ODB's option list
	opt->next = NULL;
	add_option(odb, opt);

	//set default value
	for(int i=0; i < *nelt; i++)
	{
		vars[i] = def_val[i];
		if(def_val[i])
		{
			opt->var.push_back(def_val[i]);
		}
	}
}

//process command line arguments, returns index of next argument to parse
int process_option(opt_odb_t *odb,	//option database
	unsigned int index,		//index of the first arg to parse
	std::vector<std::string> argv)	//argument string array
{
	int cnt, ent, nvars;
	char *endp;
	double tmp;

	opt_opt_t * opt = NULL;
	//locate the option in the option database
	for(opt=odb->options; opt != NULL; opt=opt->next)
	{
		if(opt->name==argv[index])
		{
			break;
		}
	}
	if(!opt)
	{
		//no one registered this option
		fatal("option `%s' is undefined", argv[index].c_str());
	}
	index++;

	//process option arguments
	switch(opt->oc)
	{
	case oc_int:
		//this option needs at least one argument
		if(index >= argv.size() || (argv[index][0] == '-' && !isdigit((int)argv[index][1])))
		{
			//no arguments available
			fatal("option `%s' requires an argument", opt->name.c_str());
		}
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
			{
				fatal("too many invocations of option `%s'", opt->name.c_str());
			}
		}
		else
		{
			ent = 0;
			if(opt->nelt)
			{
				*opt->nelt = 0;
			}
			nvars = opt->nvars;
		}
		//parse all arguments
		while(index < argv.size() && cnt < nvars
			&& (argv[index][0] != '-' || isdigit((int)argv[index][1])))
		{
			opt->variant.for_int.var[ent] = strtol(argv[index].c_str(), &endp, 0);
			if(*endp)
			{
				//could not parse entire argument
				fatal("could not parse argument `%s' of option `%s'", argv[index].c_str(), opt->name.c_str());
			}
			//else, argument converted correctly
			if(opt->nelt)
			{
				(*opt->nelt)++;
			}
			cnt++; index++; ent++;
		}
		break;

	case oc_long_long:
		//this option needs at least one argument
		if(index >= argv.size() || (argv[index][0] == '-' && !isdigit((int)argv[index][1])))
		{
			//no arguments available
			fatal("option `%s' requires an argument", opt->name.c_str());
		}
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
				fatal("too many invocations of option `%s'", opt->name.c_str());
		}
		else
		{
			ent = 0;
			if(opt->nelt)
				*opt->nelt = 0;
			nvars = opt->nvars;
		}
		//parse all arguments
		while(index < argv.size() && cnt < nvars && (argv[index][0] != '-' || isdigit((int)argv[index][1])))
		{
			opt->variant.for_long_long.var[ent] = strtol(argv[index].c_str(), &endp, 0);
			if(*endp)
			{
				//could not parse entire argument
				fatal("could not parse argument `%s' of option `%s'", argv[index].c_str(), opt->name.c_str());
			}
			//else, argument converted correctly
			if(opt->nelt)
				(*opt->nelt)++;
			cnt++; index++; ent++;
		}
		break;

	case oc_uint:
		/* this option needs at least one argument */
		if(index >= argv.size() || argv[index][0] == '-')
		{
			/* no arguments available */
			fatal("option `%s' requires an argument", opt->name.c_str());
		}
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
				fatal("too many invocations of option `%s'", opt->name.c_str());
		}
		else
		{
			ent = 0;
			if(opt->nelt)
				*opt->nelt = 0;
			nvars = opt->nvars;
		}
		/* parse all arguments */
		while(index < argv.size() && cnt < nvars && argv[index][0] != '-')
		{
			opt->variant.for_uint.var[ent] = strtoul(argv[index].c_str(), &endp, 0);
			if(*endp)
			{
				/* could not parse entire argument */
				fatal("could not parse argument `%s' of option `%s'", argv[index].c_str(), opt->name.c_str());
			}
			/* else, argument converted correctly */
			if(opt->nelt)
				(*opt->nelt)++;
			cnt++; index++; ent++;
		}
		break;
	case oc_float:
		/* this option needs at least one argument */
		if(index >= argv.size() || (argv[index][0] == '-' && !isdigit((int)argv[index][1])))
		{
			/* no arguments available */
			fatal("option `%s' requires an argument", opt->name.c_str());
		}
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
				fatal("too many invocations of option `%s'", opt->name.c_str());
		}
		else
		{
			ent = 0;
			if(opt->nelt)
				*opt->nelt = 0;
			nvars = opt->nvars;
		}
		/* parse all arguments */
		while(index < argv.size() && cnt < nvars && (argv[index][0] != '-' || isdigit((int)argv[index][1])))
		{
			tmp = strtod(argv[index].c_str(), &endp);
			if(*endp)
			{
				/* could not parse entire argument */
				fatal("could not parse argument `%s' of option `%s'", argv[index].c_str(), opt->name.c_str());
			}
			if(tmp < FLT_MIN || tmp > FLT_MAX)
			{
				/* over/underflow */
				fatal("FP over/underflow for argument `%s' of option `%s'", argv[index].c_str(), opt->name.c_str());
			}
			/* else, argument converted correctly */
			opt->variant.for_float.var[ent] = (float)tmp;
			if(opt->nelt)
				(*opt->nelt)++;
			cnt++; index++; ent++;
		}
		break;
	case oc_double:
		/* this option needs at least one argument */
		if(index >= argv.size() || (argv[index][0] == '-' && !isdigit((int)argv[index][1])))
		{
			/* no arguments available */
			fatal("option `%s' requires an argument", opt->name.c_str());
		}
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
				fatal("too many invocations of option `%s'", opt->name.c_str());
		}
		else
		{
			ent = 0;
			if(opt->nelt)
				*opt->nelt = 0;
			nvars = opt->nvars;
		}
		/* parse all arguments */
		while(index < argv.size() && cnt < nvars && (argv[index][0] != '-' || isdigit((int)argv[index][1])))
		{
			opt->variant.for_double.var[ent] = strtod(argv[index].c_str(), &endp);
			if(*endp)
			{
				/* could not parse entire argument */
				fatal("could not parse argument `%s' of option `%s'", argv[index].c_str(), opt->name.c_str());
			}
			/* else, argument converted correctly */
			if(opt->nelt)
				(*opt->nelt)++;
			cnt++; index++; ent++;
		}
		break;
	case oc_enum:
		/* this option needs at least one argument */
		if(index >= argv.size() || argv[index][0] == '-')
		{
			/* no arguments available */
			fatal("option `%s' requires an argument", opt->name.c_str());
		}
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
				fatal("too many invocations of option `%s'", opt->name.c_str());
		}
		else
		{
			ent = 0;
			if(opt->nelt)
				*opt->nelt = 0;
			nvars = opt->nvars;
		}
		/* parse all arguments */
		while(index < argv.size() && cnt < nvars && argv[index][0] != '-')
		{
			if(!bind_to_enum(argv[index], opt->variant.for_enum.emap, opt->variant.for_enum.eval,
				opt->variant.for_enum.emap_sz, &opt->variant.for_enum.var[ent]))
			{
				/* could not parse entire argument */
				fatal("could not parse argument `%s' of option `%s'", argv[index].c_str(), opt->name.c_str());
			}
			/* else, argument converted correctly */
			if(opt->nelt)
				(*opt->nelt)++;
			cnt++; index++; ent++;
		}
		break;
	case oc_flag:
		/* check if this option has at least one argument */
		if(index >= argv.size() || argv[index][0] == '-')
		{
			/* no arguments available, default value for flag is true */
			opt->variant.for_enum.var[0] = TRUE;
			break;
		}
		/* else, parse boolean argument(s) */
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
				fatal("too many invocations of option `%s'", opt->name.c_str());
		}
		else
		{
			ent = 0;
			if(opt->nelt)
				*opt->nelt = 0;
			nvars = opt->nvars;
		}
		while(index < argv.size() && cnt < nvars && argv[index][0] != '-')
		{
			if(!bind_to_enum(argv[index], opt->variant.for_enum.emap, opt->variant.for_enum.eval,
				opt->variant.for_enum.emap_sz, &opt->variant.for_enum.var[ent]))
			{
				/* could not parse entire argument, default to true */
				opt->variant.for_enum.var[ent] = TRUE;
				break;
			}
			else
			{
				/* else, argument converted correctly */
				if(opt->nelt)
					(*opt->nelt)++;
				cnt++; index++; ent++;
			}
		}
		break;
	case oc_string:
		//this option needs at least one argument
		if(index >= argv.size() || argv[index][0] == '-')
		{
			//no arguments available
			fatal("option `%s' requires an argument", opt->name.c_str());
		}
		cnt = 0;
		if(opt->accrue)
		{
			ent = opt->nelt ? *opt->nelt : 0;
			nvars = 1;
			if(ent >= opt->nvars)
			{
				fatal("too many invocations of option `%s'", opt->name.c_str());
			}
		}
		else
		{
			ent = 0;
			if(opt->nelt)
			{
				*opt->nelt = 0;
			}
			nvars = opt->nvars;
		}

		opt->var.clear();
		//parse all arguments
		while(index < argv.size() && cnt < nvars && argv[index][0] != '-')
		{
			opt->var.push_back(argv[index]);
			opt->variant.for_string.var[ent] = &(opt->var.back()[0]);
			if(opt->nelt)
			{
				(*opt->nelt)++;
			}
			cnt++; index++; ent++;
		}
		break;
	default:
		panic("bogus option class");
	}
	return index;
}

//forward declarations
void process_file(opt_odb_t *odb, std::string fname, int depth);
void dump_config(opt_odb_t *odb, std::string fname);

//process a command line, internal version that tracks '-config' depth
void __opt_process_options(opt_odb_t *odb,		//option data base
	std::vector<std::string> argv,			//argument array
	int depth)					//'-config' option depth
{
	unsigned int index = 0;
	bool do_dumpconfig = FALSE;
	std::string dumpconfig_name;

	//visit all command line arguments
	while(index < argv.size())
	{
		//process any encountered orphans
		while(index < argv.size() && argv[index][0] != '-')
		{
			if(depth > 0)
			{
				//orphans are not allowed during config file processing
				fatal("orphan `%s' encountered during config file processing", argv[index].c_str());
			}
			//else, call the user-stalled orphan handler
			if(odb->orphan_fn)
			{
				if(!odb->orphan_fn(index+1, argv.size(), argv))
				{
					//done processing command line
					if(do_dumpconfig)
					{
						dump_config(odb, dumpconfig_name);
					}
					return;
				}
			}
			else
			{
				//no one claimed this option
				fatal("orphan argument `%s' was unclaimed", argv[index].c_str());
			}
			//go to next option
		}
		//if not done with command line?
		if(index < argv.size())
		{
			//finished, argv[index] is an option to parse

			//process builtin options
			if(argv[index]== "-config")
			{
				//handle `-config' builtin option
				index++;
				if(index >= argv.size() || argv[index][0] == '-')
				{
					//no arguments available
					fatal("option `-config' requires an argument");
				}
				process_file(odb, argv[index], depth);
				index++;
			}
			else if(argv[index]=="-dumpconfig")
			{
				//this is performed last
				do_dumpconfig = TRUE;
				//handle `-dumpconfig' builtin option
				index++;
				if(index >= argv.size() || (argv[index][0] == '-' && argv[index][1] != '\0'))
				{
					//no arguments available
					fatal("option `-dumpconfig' requires an argument");
				}
				dumpconfig_name = argv[index];
				index++;
			}
			else
			{
				//process user-installed option
				index = process_option(odb, index, argv);
			}
		}
	}

	if(do_dumpconfig)
	{
		dump_config(odb, dumpconfig_name);
	}
}

//process command line arguments
void opt_process_options(opt_odb_t *odb,	//option data base
	std::vector<std::string> argv)		//argument array
{
	//need at least two entries to have an option
	if(argv.size() < 2)
	{
	    return;
	}
	//Remove <executable> from the argument vector.
	argv.erase(argv.begin());
	//process the command, starting at `-config' depth 0 (top level)
	__opt_process_options(odb, argv, 0);
}

//handle `-config' builtin option
#define MAX_LINE_ARGS		65536
#define MAX_FILENAME_LEN	1024
void process_file(opt_odb_t *odb, std::string fname, int depth)
{
	std::vector<std::string> largv;
	std::string line;
	char cwd[MAX_FILENAME_LEN];
	char *header = NULL;

	std::ifstream infile(fname.c_str());
	if(!infile.is_open())
	{
		fatal("could not open configuration file `%s'", fname.c_str());
	}

	if(!getcwd(cwd, MAX_FILENAME_LEN))
	{
		fatal("can't get cwd");
	}

	size_t slash_offset = fname.rfind('/');
	if(slash_offset != std::string::npos)
	{
		header = &fname[0] + slash_offset;
	}
	if(header != NULL)
	{
		//filename is path - get header
		*header = '\0';
		if(chdir(fname.c_str()) == -1)
		{
			fatal("can't chdir to %s\n", fname.c_str());
		}
		*header = '/';
	}

	while(!infile.eof())
	{
#if 0
		fprintf(stderr, "!infile.eof(): %d, strlen(line): %d, line: %s\n", !infile.eof(), line.size(), line.c_str());
#endif
		line = "\n";
		//read a line from the file and chop
		getline(infile,line);
		if(line[0] == '\0' || line[0] == '\n')
		{
			continue;
		}

		//process one line from the file
		char *q = NULL;
		char *p = &line[0];

		while(*p)
		{
			//skip whitespace, '[' and ']'
			while(*p != '\0' && (*p == ' ' || *p == '\t' || *p == '[' || *p == ']'))
			{
				p++;
			}

			//ignore empty lines and comments
			if(*p == '\0' || *p == '#')
			{
				break;
			}
			//skip to the end of the argument
			q = p;
			while(*q != '\0' && *q != ' ' && *q != '\t')
			{
				q++;
			}
			if(*q)
			{
				*q = '\0';
				q++;
			}

			//marshall an option array
			largv.push_back(p);

			if(largv.size() == MAX_LINE_ARGS)
			{
				if(chdir(cwd) == -1)
				{
					fatal("can't chdir back to %s\n", cwd);
				}
				fatal("option line too complex in file `%s'", fname.c_str());
			}

			//go to next argument
			p = q;
		}

		//process the line
		if(!largv.empty())
		{
			__opt_process_options(odb, largv, depth+1);
		}
		//else, empty line
	}
	infile.close();
	if(chdir(cwd) == -1)
	{
		fatal("can't chdir back to %s\n", cwd);
	}
}

//print the value of an option
void opt_print_option(opt_opt_t *opt,		//option variable
	FILE *fd)				//output stream
{
	unsigned int nelt = 1;
	if(opt->nelt)
	{
		nelt = *(opt->nelt);
		if(nelt>1)
		{
			fprintf(fd,"[");
		}
	}

	switch(opt->oc)
	{
	case oc_int:
		for(unsigned int i=0; i<nelt; i++)
		{
			fprintf(fd, opt->format, opt->variant.for_int.var[i]);
			fprintf(fd, " ");
		}
		break;
	case oc_long_long:
		for(unsigned int i=0; i<nelt; i++)
		{
			fprintf(fd, opt->format, opt->variant.for_long_long.var[i]);
			fprintf(fd, " ");
		}
		break;
	 case oc_uint:
		for(unsigned int i=0; i<nelt; i++)
		{
			fprintf(fd, opt->format, opt->variant.for_uint.var[i]);
			fprintf(fd, " ");
		}
		break;
	case oc_float:
		for(unsigned int i=0; i<nelt; i++)
		{
			fprintf(fd, opt->format, (double)opt->variant.for_float.var[i]);
			fprintf(fd, " ");
		}
		break;
	case oc_double:
		for(unsigned int i=0; i<nelt; i++)
		{
			fprintf(fd, opt->format, opt->variant.for_double.var[i]);
			fprintf(fd, " ");
		}
		break;
	case oc_enum:
		for(unsigned int i=0; i<nelt; i++)
		{
			const char *estr = bind_to_str(opt->variant.for_enum.var[i],
				opt->variant.for_enum.emap,
				opt->variant.for_enum.eval,
				opt->variant.for_enum.emap_sz);
			if(!estr)
			{
				panic("could not bind enum `%d' for option `%s'", opt->variant.for_enum.var[i], opt->name.c_str());
			}
			fprintf(fd, opt->format, estr);
			fprintf(fd, " ");
		}
		break;
	case oc_flag:
		for(unsigned int i=0; i<nelt; i++)
		{
			const char *estr = bind_to_str(opt->variant.for_enum.var[i],
				opt->variant.for_enum.emap,
				opt->variant.for_enum.eval,
				opt->variant.for_enum.emap_sz);
			if(!estr)
			{
				panic("could not bind boolean `%d' for option `%s'", opt->variant.for_enum.var[i], opt->name.c_str());
			}
			fprintf(fd, opt->format, estr);
			fprintf(fd, " ");
		}
		break;
	case oc_string:
		//FIXME: This can be cleaned up
		if(!opt->nvars)
		{
			fprintf(fd, "%12s ", "<null>");
			break;
		}
		if(nelt == 0)
		{
			fprintf(fd, "%12s ", "<null>");
			break;
		}
		else
		{
			for(unsigned int i=0; i<nelt; i++)
			{
				//FIXME: The first if can probably be removed and this simplifed to the contents of else
				if(i>=opt->var.size())
				{
					fprintf(fd, opt->format, "<null>");
				}
				else
				{
					if(opt->var[i].empty())
					{
						fprintf(fd, opt->format, "<null>");
					}
					else
					{
						fprintf(fd, opt->format, opt->var[i].c_str());
					}
				}
				fprintf(fd, " ");
			}
		}
		break;
	default:
		panic("bogus option class");
	}
	if(nelt>1)
	{
		fprintf(fd,"]");
	}
}

//print any options header
void print_option_header(opt_odb_t *odb,	//options database
	FILE *fd)				//output stream
{
	if(!odb->header)
	{
		return;
	}
	fprintf(fd, "\n%s\n", odb->header);
}

//print option notes
void print_option_notes(opt_odb_t *odb,		//options database
	FILE *fd)				//output stream
{
	if(!odb->notes)
	{
		return;
	}

	fprintf(fd, "\n");
	for(opt_note_t *note=odb->notes; note != NULL; note=note->next)
	{
		fprintf(fd, "%s\n", note->note);
	}
	fprintf(fd, "\n");
}

//builtin options
opt_opt_t dumpconfig_opt =
{
	NULL, std::string("-dumpconfig"), std::string(""), "dump configuration to a file",
	0, NULL, NULL, FALSE, FALSE, oc_string
};
opt_opt_t config_opt =
{
	&dumpconfig_opt, std::string("-config"), std::string(""), "load configuration from a file",
	0, NULL, NULL, FALSE, FALSE, oc_string
};
opt_opt_t *builtin_options = &config_opt;

//return non-zero if the option is a NULL-valued string option
int opt_null_string(opt_opt_t *opt)
{
	if(opt==NULL)
	{
		return 0;
	}
	if(opt->oc != oc_string)
	{
		return 0;
	}
	if(opt->nvars==0)
	{
		return 1;
	}
	if(opt->nelt != NULL && *opt->nelt == 0)
	{
		return 1;
	}
	if(opt->nelt != NULL)
	{
		return 0;
	}
	else
	{
		if(opt->var.empty())
		{
			return 1;
		}
		return 0;
	}
}

//print all options and current values
void opt_print_options(opt_odb_t *odb,		//option data base
	FILE *fd,				//output stream
	int terse,				//print terse options?
	int notes)				//include notes?
{
	if(!odb)
	{	//no options
		return;
	}

	//print any options header
	if(notes)
	{
		print_option_header(odb, fd);
	}

	//dump out builtin options
	for(opt_opt_t * opt=builtin_options; opt != NULL; opt=opt->next)
	{
		if(terse)
		{
			fprintf(fd, "# %-27s # %s\n", opt->name.c_str(), opt->desc);
		}
		else
		{
			fprintf(fd, "# %s\n", opt->desc);
			fprintf(fd, "# %-22s\n\n", opt->name.c_str());
		}
	}

	//dump out options from options database
	for(opt_opt_t *opt=odb->options; opt != NULL; opt=opt->next)
	{
		if(terse)
		{
			if(!opt->print || opt_null_string(opt))
			{
				fprintf(fd, "# %-16s ", opt->name.c_str());
			}
			else
			{
				fprintf(fd, "%-18s ", opt->name.c_str());
			}
			opt_print_option(opt, fd);
			if(opt->desc)
			{
				fprintf(fd, "# %-22s", opt->desc);
			}
			fprintf(fd, "\n");
		}
		else
		{
			if(opt->desc)
			{
				fprintf(fd, "# %s\n", opt->desc);
			}
			if(!opt->print || opt_null_string(opt))
			{
				fprintf(fd, "# %-20s ", opt->name.c_str());
			}
			else
			{
				fprintf(fd, "%-22s ", opt->name.c_str());
			}
			opt_print_option(opt, fd);
			fprintf(fd, "\n\n");
		}
	}
	//print option notes
	if(notes)
	{
		print_option_notes(odb, fd);
	}
}

//print help information for an option
void print_help(opt_opt_t *opt,		//option variable
	FILE *fd)			//output stream
{
	std::string s;

	fprintf(fd, "%-16s ", opt->name.c_str());
	switch(opt->oc)
	{
	case oc_int:
		s = "<int";
		break;
	case oc_long_long:
		s = "<long long";
		break;
	case oc_uint:
		s = "<uint";
		break;
	case oc_float:
		s = "<float";
		break;
	case oc_double:
		s = "<double";
		break;
	case oc_enum:
		s = "<enum";
		break;
	case oc_flag:
		s = "<true|false";
		break;
	case oc_string:
		s = "<string";
		break;
	default:
		panic("bogus option class");
	}
	if(opt->nvars>1)
	{
		s += " list...";
	}
	s+=">";

	fprintf(fd, "%-16s # ", s.c_str());
	opt_print_option(opt, fd);
	fprintf(fd, "# %-22s\n", opt->desc);
}

//print option help page with default values
void opt_print_help(opt_odb_t *odb,	//option data base
	FILE *fd)			//output stream
{
	opt_opt_t *opt = new opt_opt_t;

	/* print any options header */
	print_option_header(odb, fd);

	fprintf(fd, "#\n");
	fprintf(fd, "%-16s %-16s # %12s # %-22s\n", "# -option", "<args>", "<default>", "description");
	fprintf(fd, "#\n");

	/* print out help info for builtin options */
	for(opt=builtin_options; opt != NULL; opt=opt->next)
		print_help(opt, fd);

	/* print out help info for options in options database */
	for(opt=odb->options; opt != NULL; opt=opt->next)
		print_help(opt, fd);

	/* print option notes */
	print_option_notes(odb, fd);
}

//handle `-dumpconfig' builtin option, print options from file argument
void dump_config(opt_odb_t *odb,	/* option data base */
	std::string fname)		/* output file name, "-" == stdout */
{
	FILE *fd;

	//open output file stream
	if(fname=="-")
	{
		fd = stderr;
	}
	else
	{
		fd = fopen(fname.c_str(), "w");
		if(!fd)
		{
			fatal("could not open file `%s'", fname.c_str());
		}
	}

	//print current option values to output stream
	opt_print_options(odb, fd, /* long */FALSE, /* !notes */FALSE);

	//close output stream, if not a standard stream
	if(fd != stdout && fd != stderr)
	{
		fclose(fd);
	}
}

//find an option by name in the option database, returns NULL if not found
opt_opt_t * opt_find_option(opt_odb_t *odb,	/* option database */
	char *opt_name)	/* option name */
{
	opt_opt_t *opt = new opt_opt_t;

	/* search builtin options */
	for(opt = builtin_options; opt != NULL; opt = opt->next)
	{
		if(!strcmp(opt->name.c_str(), opt_name))
		{
			/* located option */
			return opt;
		}
	}

	/* search user-installed options */
	for(opt = odb->options; opt != NULL; opt = opt->next)
	{
		if(!strcmp(opt->name.c_str(), opt_name))
		{
			/* located option */
			return opt;
		}
	}
	/* not found */
	return NULL;
}

//register an options header, the header is printed before the option list
void opt_reg_header(opt_odb_t *odb,			//option database
	const char *header)				//options header string
{
	odb->header = header;
}

opt_odb_t::~opt_odb_t()
{
	while(notes!=NULL)
	{
		opt_note_t * next = notes->next;
		delete notes;
		notes = next;
	}
	while(options!=NULL)
	{
		opt_opt_t * next = options->next;
		delete options;
		options = next;
	}
}

//register an option note, notes are printed after the list of options
void opt_reg_note(opt_odb_t *odb,			//option database
	const char *note_str)				//option note
{
	opt_note_t *elt, *prev;
	opt_note_t *note = new opt_note_t;

	//record note
	note->next = NULL;
	note->note = note_str;

	//add to end of option notes list, nothing to do, just move to end
	for(prev=NULL, elt=odb->notes; elt != NULL; prev=elt, elt=elt->next);

	if(prev != NULL)
	{
		prev->next = note;
	}
	else	//prev == NULL
	{
		odb->notes = note;
	}
	note->next = NULL;
}


#ifdef TEST

int f_orphan_fn(int i, int argc, char **argv)
{
	fprintf(stdout, "orphans detected at index %d, arg = `%s'...\n", i, argv[i]);

	//done processing
	return FALSE;
}

#define MAX_VARS 4
void main(int argc, char **argv)
{
	int n_int_vars, n_uint_vars, n_float_vars, n_double_vars;
	int n_enum_vars, n_enum_eval_vars, n_flag_vars, n_string_vars;
	int int_var, int_vars[MAX_VARS];
	unsigned int uint_var, uint_vars[MAX_VARS];
	float float_var, float_vars[MAX_VARS];
	double double_var, double_vars[MAX_VARS];
	int flag_var, flag_vars[MAX_VARS];
	char *string_var, *string_vars[MAX_VARS];

	enum etest_t { enum_a, enum_b, enum_c, enum_d, enum_NUM };
	static char *enum_emap[enum_NUM] = { "enum_a", "enum_b", "enum_c", "enum_d" };
	static int enum_eval[enum_NUM] = { enum_d, enum_c, enum_b, enum_a };
	int enum_var, enum_vars[MAX_VARS];
	int enum_eval_var, enum_eval_vars[MAX_VARS];

	/* get an options processor */
	opt_odb_t *odb = new opt_odb_t(f_orphan_fn);

	opt_reg_int(odb, "-opt:int", "This is an integer option.", &int_var, 1, /* print */TRUE, /* default format */NULL);
	opt_reg_int_list(odb, "-opt:int:list", "This is an integer list option.",
		int_vars, MAX_VARS, &n_int_vars, 2,
		/* print */TRUE, /* default format */NULL);
	opt_reg_uint(odb, "-opt:uint", "This is an unsigned integer option.",
		&uint_var, 3, /* print */TRUE, /* default format */NULL);
	opt_reg_uint_list(odb, "-opt:uint:list",
		"This is an unsigned integer list option.",
		uint_vars, MAX_VARS, &n_uint_vars, 4,
		/* print */TRUE, /* default format */NULL);
	opt_reg_float(odb, "-opt:float", "This is a float option.",
		&float_var, 5.0, /* print */TRUE, /* default format */NULL);
	opt_reg_float_list(odb, "-opt:float:list", "This is a float list option.",
		float_vars, MAX_VARS, &n_float_vars, 6.0,
		/* print */TRUE, /* default format */NULL);
	opt_reg_double(odb, "-opt:double", "This is a double option.",
		&double_var, 7.0, /* print */TRUE, /* default format */NULL);
	opt_reg_double_list(odb, "-opt:double:list", "This is a double list option.",
		double_vars, MAX_VARS, &n_double_vars, 8.0,
		/* print */TRUE, /* default format */NULL);
	opt_reg_enum(odb, "-opt:enum", "This is an enumeration option.",
		&enum_var, "enum_a", enum_emap, /* index map */NULL, enum_NUM,
		/* print */TRUE, /* default format */NULL);
	opt_reg_enum_list(odb, "-opt:enum:list", "This is a enum list option.",
		enum_vars, MAX_VARS, &n_enum_vars, "enum_b",
		enum_emap, /* index map */NULL, enum_NUM,
		/* print */TRUE, /* default format */NULL);
	opt_reg_enum(odb, "-opt:enum:eval", "This is an enumeration option w/eval.",
		&enum_eval_var, "enum_b", enum_emap, enum_eval, enum_NUM,
		/* print */TRUE, /* default format */NULL);
	opt_reg_enum_list(odb, "-opt:enum:eval:list",
		"This is a enum list option w/eval.",
		enum_eval_vars, MAX_VARS, &n_enum_eval_vars, "enum_a",
		enum_emap, enum_eval, enum_NUM,
		/* print */TRUE, /* default format */NULL);
	opt_reg_flag(odb, "-opt:flag", "This is a boolean flag option.",
		&flag_var, FALSE, /* print */TRUE, /* default format */NULL);
	opt_reg_flag_list(odb, "-opt:flag:list",
		"This is a boolean flag list option.",
		flag_vars, MAX_VARS, &n_flag_vars, TRUE,
		/* print */TRUE, /* default format */NULL);
	opt_reg_string(odb, "-opt:string", "This is a string option.",
		&string_var, "a:string",
		/* print */TRUE, /* default format */NULL);
	opt_reg_string_list(odb, "-opt:string:list", "This is a string list option.",
		 string_vars, MAX_VARS, &n_string_vars, "another:string",
		/* print */TRUE, /* default format */NULL);

	/* parse options */
	opt_process_options(odb, argc, argv);

	/* print options */
	fprintf(stdout, "## Current Option Values ##\n");
	opt_print_options(odb, stdout, /* long */FALSE, /* notes */TRUE);

	/* all done */
	opt_delete(odb);
	exit(0);
}

#endif /* TEST */
