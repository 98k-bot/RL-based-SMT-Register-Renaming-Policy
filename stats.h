/* stats.h - statistical package interfaces */

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


#ifndef STAT_H
#define STAT_H

#include<cstdio>
#include<string>
#include<sstream>

#include "host.h"
#include "machine.h"
#include "eval.h"

//The stats package is a uniform module for handling statistical variables,
//including counters, distributions, and expressions.  The user must first
//create a stats database using new stat_sdb_t, then statical counters are added
//to the database using the *_reg_*() functions.  Interfaces are included to
//allocate and manipulate distributions (histograms) and general expression
//of other statistical variables constants.  Statistical variables can be
//located by name using stat_find_stat().  And, statistics can be print in
//a highly standardized and stylized fashion using stat_print_stats().

//stat variable classes
enum stat_class_t
{
	sc_int = 0,			//integer stat
	sc_uint,			//unsigned integer stat
	sc_qword,			//qword integer stat
	sc_sqword,			//signed qword integer stat
	sc_float,			//single-precision FP stat
	sc_double,			//double-precision FP stat
	sc_dist,			//array distribution stat
	sc_sdist,			//sparse array distribution stat
	sc_formula,			//stat expression formula
	sc_NUM
};

//sparse array distributions are implemented with a hash table
#define HTAB_SZ			1024
#define HTAB_HASH(I)		((((I) >> 8) ^ (I)) & (HTAB_SZ - 1))

//hash table bucket definition
class bucket_t
{
	public:
		bucket_t()
		: next(NULL), index(0), count(0)
		{};
		~bucket_t()
		{};
		bucket_t *next;			//pointer to the next bucket
		md_addr_t index;		//bucket index - as large as an addr
		unsigned int count;		//bucket count
};

//enable distribution components:  index, count, probability, cumulative
#define PF_COUNT		0x0001
#define PF_PDF			0x0002
#define PF_CDF			0x0004
#define PF_ALL			(PF_COUNT|PF_PDF|PF_CDF)

//user-defined print function, if this option is selected, a function of this
//form is called for each bucket in the distribution, in ascending index order
//forward declaration needed
class stat_stat_t;
typedef void (*print_fn_t)(stat_stat_t *stat,	//the stat variable being printed
	md_addr_t index,			//entry index to print
	int count,				//entry count
	double sum,				//cumulative sum
	double total);				//total count for distribution

//statistical variable definition
class stat_stat_t
{
public:
	~stat_stat_t();
	stat_stat_t *next;				//pointer to next stat in database list
	std::string name;				//stat name
	std::string desc;				//stat description
	const char * format;				//stat output print format
	stat_class_t sc;				//stat class
	union stat_variant_t
	{
		//sc == sc_int
		class stat_for_int_t
		{
			public:
				int *var;		//integer stat variable
				int init_val;		//initial integer value
		} for_int;

		//sc == sc_uint
		class stat_for_uint_t
		{
			public:
				unsigned int *var;	//unsigned integer stat variable
				unsigned int init_val;	//initial unsigned integer value
		} for_uint;

		//sc == sc_qword
		class stat_for_qword_t
		{
			public:
				qword_t *var;		//qword integer stat variable
				qword_t init_val;	//qword integer value
		} for_qword;

		//sc == sc_sqword
		class stat_for_sqword_t
		{
			public:
				sqword_t *var;		//signed qword integer stat variable
				sqword_t init_val;	//signed qword integer value
		} for_sqword;

		//sc == sc_float
		class stat_for_float_t
		{
			public:
				float *var;		//float stat variable
				float init_val;		//initial float value
		} for_float;

		//sc == sc_double
		class stat_for_double_t
		{
			public:
				double *var;		//double stat variable
				double init_val;	//initial double value
		} for_double;

		//sc == sc_dist
		class stat_for_dist_t
		{
			public:
				unsigned int init_val;	//initial dist value
				unsigned int *arr;	//non-sparse array pointer
				unsigned int arr_sz;	//array size
				unsigned int bucket_sz;	//array bucket size
				int pf;			//printables
				char **imap;		//index -> string map
				print_fn_t print_fn;	//optional user-specified print fn
				unsigned int overflows;	//total overflows in stat_add_samples()
		} for_dist;

		//sc == sc_sdist
		class stat_for_sdist_t
		{
			public:
				unsigned int init_val;	//initial dist value
				bucket_t **sarr;	//sparse array pointer
				int pf;			//printables
				print_fn_t print_fn;	//optional user-specified print fn
		} for_sdist;

		//sc == sc_formula
		class stat_for_formula_t
		{
			public:
				char *formula;		//stat formula, see eval.h for format
		} for_formula;
	} variant;
};


//statistical database
class stat_sdb_t
{
	public:
		stat_sdb_t();
		~stat_sdb_t();
		stat_stat_t *stats;			//list of stats in database
		eval_state_t *evaluator;		//an expression evaluator
};

//evaluate a stat as an expression
eval_value_t stat_eval_ident(eval_state_t *es);

//register an integer statistical variable
stat_stat_t * stat_reg_int(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	int *var,					//stat variable
	int init_val,					//stat variable initial value
	const char *format);				//optional variable output format

//register an unsigned integer statistical variable
stat_stat_t * stat_reg_uint(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	unsigned int *var,				//stat variable
	unsigned int init_val,				//stat variable initial value
	const char *format);				//optional variable output format

//register a qword integer statistical variable
stat_stat_t * stat_reg_qword(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	qword_t *var,					//stat variable
	qword_t init_val,				//stat variable initial value
	const char *format);				//optional variable output format

//register a signed qword integer statistical variable
stat_stat_t * stat_reg_sqword(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	sqword_t *var,					//stat variable
	sqword_t init_val,				//stat variable initial value
	const char *format);				//optional variable output format

//register a float statistical variable
stat_stat_t * stat_reg_float(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	float *var,					//stat variable
	float init_val,					//stat variable initial value
	const char *format);				//optional variable output format

//register a double statistical variable
stat_stat_t * stat_reg_double(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	double *var,					//stat variable
	double init_val,				//stat variable initial value
	const char *format);				//optional variable output format

//create an array distribution (w/ fixed size buckets) in stat database SDB,
//the array distribution has ARR_SZ buckets with BUCKET_SZ indicies in each
//bucked, PF specifies the distribution components to print for optional
//format FORMAT; the indicies may be optionally replaced with the strings
//from IMAP, or the entire distribution can be printed with the optional
//user-specified print function PRINT_FN
stat_stat_t * stat_reg_dist(stat_sdb_t *sdb,		//stat database
	const char *name,				//stat variable name
	const char *desc,				//stat variable description
	unsigned int init_val,				//dist initial value
	unsigned int arr_sz,				//array size
	unsigned int bucket_sz,				//array bucket size
	int pf,						//print format, use PF_* defs
	const char *format,				//optional variable output format
	char **imap,					//optional index -> string map
	print_fn_t print_fn);				//optional user print function

//create a sparse array distribution in stat database SDB, while the sparse
//array consumes more memory per bucket than an array distribution, it can
//efficiently map any number of indicies from 0 to 2^32-1, PF specifies the
//distribution components to print for optional format FORMAT; the indicies
//may be optionally replaced with the strings from IMAP, or the entire
//distribution can be printed with the optional user-specified print function PRINT_FN
stat_stat_t * stat_reg_sdist(stat_sdb_t *sdb,		//stat database
	const char *name,				//stat variable name
	const char *desc,				//stat variable description
	unsigned int init_val,				//dist initial value
	int pf,						//print format, use PF_* defs
	const char *format,				//optional variable output format
	print_fn_t print_fn);				//optional user print function

//add NSAMPLES to array or sparse array distribution STAT
void stat_add_samples(stat_stat_t *stat,		//stat database
	md_addr_t index,				//distribution index of samples
	int nsamples);					//number of samples to add to dist

//add a single sample to array or sparse array distribution STAT
void stat_add_sample(stat_stat_t *stat,			//stat variable
	md_addr_t index);				//index of sample

//register a double statistical formula, the formula is evaluated when the
//statistic is printed, the formula expression may reference any registered
//statistical variable and, in addition, the standard operators '(', ')', '+',
//'-', '*', and '/', and literal (i.e., C-format decimal, hexidecimal, and
//octal) constants are also supported; NOTE: all terms are immediately
//converted to double values and the result is a double value, see eval.h
//for more information on formulas
stat_stat_t * stat_reg_formula(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	std::string formula,				//formula expression
	const char *format);				//optional variable output format

//print the value of stat variable STAT
void stat_print_stat(stat_sdb_t *sdb,			//stat database
	stat_stat_t *stat,				//stat variable
	FILE *fd);					//output stream

//print the value of all stat variables in stat database SDB
void stat_print_stats(stat_sdb_t *sdb,			//stat database
	FILE *fd);					//output stream

//find a stat variable, returns NULL if it is not found
stat_stat_t * stat_find_stat(stat_sdb_t *sdb,		//stat database
	std::string stat_name);				//stat name

#endif /* STAT_H */
