/* options.h - options package interfaces */

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


#ifndef OPTIONS_H
#define OPTIONS_H

#include<string>
#include<vector>

//This options package allows the user to specify the name, description,
//location, and default values of program option variables.  In addition,
//two builtin options are supported:
//	-config <filename>		# load options from <filename>
//	-dumpconfig <filename>		# save current option into <filename>
//NOTE: all user-installed option names must begin with a `-', e.g., `-debug'

//option variable classes
enum opt_class_t
{
	oc_int = 0,		//integer option
	oc_uint,		//unsigned integer option
	oc_float,		//float option
	oc_double,		//double option
	oc_enum,		//enumeration option
	oc_flag,		//boolean option
	oc_string,		//string option
	oc_long_long,		//long long option
	oc_NUM
};

//user-specified option definition
class opt_opt_t
{
	public:
		opt_opt_t *next;			//next option
		std::string name; std::string offset;	//option name, e.g., "-foo:bar"
		const char *desc;			//option description
		int nvars;				// nvars > 1 if var represents a list
		int *nelt;				//number of elements parsed
		const char *format;			//option value print format
		int print;				//print option during `-dumpconfig'?
		int accrue;				//accrue list across uses
		opt_class_t oc;				//class of this option
		union opt_variant_t
		{
			//oc == oc_int
			struct opt_for_int_t
			{
				int *var;		// pointer to integer option
			} for_int;

			//oc == oc_uint
			struct opt_for_uint_t
			{
				unsigned int *var;	//pointer to unsigned integer option
			} for_uint;

			//oc == oc_float
			struct opt_for_float_t
			{
				float *var;		//pointer to float option
			} for_float;

			//oc == oc_double
			struct opt_for_double_t
			{
				double *var;		//pointer to double option
			} for_double;

			//oc == oc_enum, oc_flag
			struct opt_for_enum_t
			{
				int *var;		//ptr to *int* enum option, NOTE: AN INT
				const char **emap;	//array of enum strings
				int *eval;		//optional array of enum values
				int emap_sz;		//number of enum's in arrays
			} for_enum;

			//oc == oc_string
			struct opt_for_string_t
			{
				const char **var;		//pointer to string pointer option
//				std::vector<std::string> var;	//pointer to string pointer option
			} for_string;

			struct opt_for_long_long_t
			{
				long long *var;		//pointer to long long integer option
			} for_long_long;
		} variant;

		//For string handler, this supercedes variant.for_string,
		std::vector<std::string> var;
};

//user-specified argument orphan parser, called when an argument is
//encountered that is not claimed by a user-installed option
typedef int (*orphan_fn_t)(int i,			//index of the orphan'ed argument
	int argc,					//number of program arguments
	std::vector<std::string> argv);		//program arguments

//an option note, these trail the option list when help or option state is printed
class opt_note_t
{
	public:
		opt_note_t();
		opt_note_t *next;			//next option note
		const char *note;			//option note
};

//option database definition
class opt_odb_t
{
	public:
		opt_odb_t();
		~opt_odb_t();
		opt_odb_t(orphan_fn_t orphan_fn);
		opt_opt_t *options;			//user-installed options in option database
		orphan_fn_t orphan_fn;			//user-specified orphan parser
		const char *header;			//options header
		opt_note_t *notes;			//option notes
};

//free an option database
void opt_delete(opt_odb_t *odb);			//option database

//register an integer option variable
void opt_reg_int(opt_odb_t *odb,			//option database
	std::string name, std::string offset,		//option name
	const char *desc,				//option description
	int *var,					//pointer to option variable
	int def_val,					//default value of option variable
	int print,					//print during `-dumpconfig'
	char *format);					//optional user print format

//register an integer option list
void opt_reg_int_list(opt_odb_t *odb,			//option database
	std::string name, std::string offset,		//option name
	const char *desc,				//option description
	int *vars,					//pointer to option array
	int nvars,					//total entries in option array
	int *nelt,					//number of entries parsed
	int *def_val,					//default value of option array
	int print,					//print during `-dumpconfig'?
	char *format,					//optional user print format
	int accrue);					//accrue list across uses

//register an unsigned integer option variable
void opt_reg_uint(opt_odb_t *odb,			//option database
	std::string name, std::string offset,		//option name
	const char *desc,				//option description
	unsigned int *var,				//pointer to option variable
	unsigned int def_val,				//default value of option variable
	int print,					//print during `-dumpconfig'?
	char *format);					//optional user print format

//register an long long integer option variable
void opt_reg_long_long(opt_odb_t *odb,			//option database
	std::string name, std::string offset,		//option name
	const char *desc,				//option description
	long long *var,					//pointer to option variable
	long long def_val,				//default value of option variable
	int print,					//print during `-dumpconfig'
	char *format);					//optional user print format

//register an unsigned integer option list
void opt_reg_uint_list(opt_odb_t *odb,			//option database
	std::string name, std::string offset,		//option name
	const char *desc,				//option description
	unsigned int *vars,				//pointer to option array
	int nvars,					//total entries in option array
	int *nelt,					//number of elements parsed
	unsigned int *def_val,				//default value of option array
	int print,					//print during `-dumpconfig'?
	char *format,					//optional user print format
	int accrue);					//accrue list across uses

//register a single-precision floating point option variable
void opt_reg_float(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	float *var,		/* target option variable */
	float def_val,		/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format);		/* optional value print format */

/* register a single-precision floating point option array */
void opt_reg_float_list(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	float *vars,		/* target array */
	int nvars,		/* target array size */
	int *nelt,		/* number of args parsed goes here */
	float *def_val,	/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format,	/* optional value print format */
	int accrue);		/* accrue list across uses */

/* register a double-precision floating point option variable */
void
opt_reg_double(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	double *var,		/* target variable */
	double def_val,		/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format);		/* optional value print format */

/* register a double-precision floating point option array */
void
opt_reg_double_list(opt_odb_t *odb,/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	double *vars,	/* target array */
	int nvars,		/* target array size */
	int *nelt,		/* number of args parsed goes here */
	double *def_val,	/* default variable value */
	int print,		/* print during `-dumpconfig'? */
	char *format,	/* optional value print format */
	int accrue);	/* accrue list across uses */

/* register an enumeration option variable, NOTE: all enumeration option
   variables must be of type `int', since true enum variables may be allocated
   with variable sizes by some compilers */
void
opt_reg_enum(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	int *var,			/* target variable */
	char *def_val,		/* default variable value */
	const char **emap,		/* enumeration string map */
	int *eval,			/* enumeration value map, optional */
	int emap_sz,		/* size of maps */
	int print,			/* print during `-dumpconfig'? */
	char *format);		/* optional value print format */

/* register an enumeration option array, NOTE: all enumeration option variables
   must be of type `int', since true enum variables may be allocated with
   variable sizes by some compilers */
void
opt_reg_enum_list(opt_odb_t *odb,/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	int *vars,		/* target array */
	int nvars,		/* target array size */
	int *nelt,		/* number of args parsed goes here */
	char *def_val,	/* default variable value */
	const char **emap,		/* enumeration string map */
	int *eval,		/* enumeration value map, optional */
	int emap_sz,		/* size of maps */
	int print,		/* print during `-dumpconfig'? */
	char *format,		/* optional value print format */
	int accrue);		/* accrue list across uses */

/* register a boolean flag option variable */
void
opt_reg_flag(opt_odb_t *odb,	/* option data base */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	int *var,			/* target variable */
	int def_val,		/* default variable value */
	int print,			/* print during `-dumpconfig'? */
	char *format);		/* optional value print format */

/* register a boolean flag option array */
void
opt_reg_flag_list(opt_odb_t *odb,/* option database */
	std::string name, std::string offset,		/* option name */
	const char *desc,		/* option description */
	int *vars,		/* pointer to option array */
	int nvars,		/* total entries in option array */
	int *nelt,		/* number of elements parsed */
	int *def_val,		/* default array value */
	int print,		/* print during `-dumpconfig'? */
	char *format,		/* optional value print format */
	int accrue);		/* accrue list across uses */

//register a string option variable
void opt_reg_string(opt_odb_t *odb,		//option data base
	std::string name, std::string offset,	//option name
	const char *desc,			//option description
	char **var,				//pointer to string option variable
	const char *def_val,			//default variable value
	int print,				//print during `-dumpconfig'?
	char *format);				//optional value print format

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
	int accrue);				//accrue list across uses

//process command line arguments
void opt_process_options(opt_odb_t *odb,	//option data base
	std::vector<std::string> argv);		//argument array

//print the value of an option
void opt_print_option(opt_opt_t *opt,		//option variable
	FILE *fd);				//output stream

/* print all options and current values */
void
opt_print_options(opt_odb_t *odb,/* option data base */
	FILE *fd,		/* output stream */
	int terse,		/* print terse options? */
	int notes);		/* include notes? */

/* print option help page with default values */
void
opt_print_help(opt_odb_t *odb,	/* option data base */
	FILE *fd);		/* output stream */

/* find an option by name in the option database, returns NULL if not found */
opt_opt_t *
opt_find_option(opt_odb_t *odb,	/* option database */
	char *opt_name);	/* option name */

//register an options header, the header is printed before the option list
void opt_reg_header(opt_odb_t *odb,			//option database
	const char *header);				//options header string

//register an option note, notes are printed after the list of options
void opt_reg_note(opt_odb_t *odb,			//option database
	const char *note);				//option note

#endif /* OPTIONS_H */
