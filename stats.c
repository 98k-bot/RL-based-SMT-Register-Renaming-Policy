/* stats.c - statistical package routines */

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
#include<climits>
#include<cmath>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "eval.h"
#include "stats.h"

//evaluate a stat as an expression
eval_value_t stat_eval_ident(eval_state_t *es)
{
	stat_sdb_t *sdb = (stat_sdb_t *)es->user_ptr;
	stat_stat_t *stat;
	static eval_value_t err_value = { et_int, { 0 } };
	eval_value_t val;
	mem_t* my_mem = es->mem;

	//locate the stat variable
	for(stat = sdb->stats; stat != NULL; stat = stat->next)
	{
		if(!strcmp(stat->name.c_str(), es->tok_buf))
		{
			//found it!
			break;
		}
	}
	if(!stat)
	{
		//could not find stat variable
		eval_error = ERR_UNDEFVAR;
		return err_value;
	}
	//else, return the value of stat

	//convert the stat variable value to a typed expression value
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
		//FIXME: cast to double, eval package doesn't support long long's
		val.type = et_double;
		val.value.as_double = (double)*stat->variant.for_qword.var;
		break;
	case sc_sqword:
		//FIXME: cast to double, eval package doesn't support long long's
		val.type = et_double;
		val.value.as_double = (double)*stat->variant.for_sqword.var;
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
		fatal("stat distributions not allowed in formula expressions");
		break;
	case sc_formula:
		{
			//instantiate a new evaluator to avoid recursion problems */
			eval_state_t *es = new eval_state_t(stat_eval_ident, sdb, my_mem);
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

stat_sdb_t::stat_sdb_t()
: stats(NULL)
{
	evaluator = new eval_state_t(stat_eval_ident, this, NULL);
}

stat_sdb_t::~stat_sdb_t()
{
	stat_stat_t *stat_next;

	//free all individual stat variables
	for(stat_stat_t *stat = stats; stat != NULL; stat = stat_next)
	{
		stat_next = stat->next;
		stat->next = NULL;

		switch (stat->sc)
		{
		case sc_int:
		case sc_uint:
		case sc_qword:
		case sc_sqword:
		case sc_float:
		case sc_double:
		case sc_formula:
			//no other storage to deallocate
			break;
		case sc_dist:
			//free distribution array
			free(stat->variant.for_dist.arr);
			stat->variant.for_dist.arr = NULL;
			break;
		case sc_sdist:
			//free all hash table buckets
			for(int i=0; i<HTAB_SZ; i++)
			{
				bucket_t *bucket_next;
				for(bucket_t *bucket = stat->variant.for_sdist.sarr[i]; bucket != NULL; bucket = bucket_next)
				{
					bucket_next = bucket->next;
					bucket->next = NULL;
					delete bucket;
				}
				stat->variant.for_sdist.sarr[i] = NULL;
			}
			//free hash table array
			free(stat->variant.for_sdist.sarr);
			stat->variant.for_sdist.sarr = NULL;
			break;
		default:
			panic("bogus stat class");
		}
		//free stat variable record
		delete stat;
	}
	stats = NULL;
	delete evaluator;
	evaluator = NULL;
}

//add stat variable STAT to stat database SDB
void add_stat(stat_sdb_t *sdb,				//stat database
	stat_stat_t *stat)				//stat variable
{
	stat_stat_t *elt, *prev;

	//append at end of stat database list, this moves us to the end
	for(prev=NULL, elt=sdb->stats; elt != NULL; prev=elt, elt=elt->next);

	//append stat to stats chain
	if(prev != NULL)
	{
		prev->next = stat;
	}
	else	//prev == NULL
	{
		sdb->stats = stat;
	}
	stat->next = NULL;
}

//register an integer statistical variable
stat_stat_t * stat_reg_int(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	int *var,					//stat variable
	int init_val,					//stat variable initial value
	const char *format)				//optional variable output format
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = (char *)(format ? format : "%12d");
	stat->sc = sc_int;
	stat->variant.for_int.var = var;
	stat->variant.for_int.init_val = init_val;

	//link onto SDB chain
	add_stat(sdb, stat);

	//initialize stat
	*var = init_val;

	return stat;
}

//register an unsigned integer statistical variable
stat_stat_t * stat_reg_uint(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	unsigned int *var,				//stat variable
	unsigned int init_val,				//stat variable initial value
	const char *format)				//optional variable output format
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = (char *)(format ? format : "%12u");
	stat->sc = sc_uint;
	stat->variant.for_uint.var = var;
	stat->variant.for_uint.init_val = init_val;

	//link onto SDB chain
	add_stat(sdb, stat);

	//initialize stat
	*var = init_val;

	return stat;
}

//register a qword integer statistical variable
stat_stat_t * stat_reg_qword(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	qword_t *var,					//stat variable
	qword_t init_val,				//stat variable initial value
	const char *format)				//optional variable output format
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = (char *)(format ? format : "%12lu");
	stat->sc = sc_qword;
	stat->variant.for_qword.var = var;
	stat->variant.for_qword.init_val = init_val;

	//link onto SDB chain
	add_stat(sdb, stat);

	//initialize stat
	*var = init_val;

	return stat;
}

//register a signed qword integer statistical variable
stat_stat_t * stat_reg_sqword(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	sqword_t *var,					//stat variable
	sqword_t init_val,				//stat variable initial value
	const char *format)				//optional variable output format
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = (char *)(format ? format : "%12ld");
	stat->sc = sc_sqword;
	stat->variant.for_sqword.var = var;
	stat->variant.for_sqword.init_val = init_val;

	//link onto SDB chain
	add_stat(sdb, stat);

	//initialize stat
	*var = init_val;

	return stat;
}

//register a float statistical variable
stat_stat_t * stat_reg_float(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	float *var,					//stat variable
	float init_val,					//stat variable initial value
	const char *format)				//optional variable output format
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = (char *)(format ? format : "%12.4f");
	stat->sc = sc_float;
	stat->variant.for_float.var = var;
	stat->variant.for_float.init_val = init_val;

	//link onto SDB chain
	add_stat(sdb, stat);

	//initialize stat
	*var = init_val;

	return stat;
}

//register a double statistical variable
stat_stat_t * stat_reg_double(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	double *var,					//stat variable
	double init_val,				//stat variable initial value
	const char *format)				//optional variable output format
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = (char *)(format ? format : "%12.4f");
	stat->sc = sc_double;
	stat->variant.for_double.var = var;
	stat->variant.for_double.init_val = init_val;

	//link onto SDB chain
	add_stat(sdb, stat);

	//initialize stat
	*var = init_val;

	return stat;
}

//create an array distribution (w/ fixed size buckets) in stat database SDB,
//the array distribution has ARR_SZ buckets with BUCKET_SZ indicies in each
//bucked, PF specifies the distribution components to print for optional
//format FORMAT; the indicies may be optionally replaced with the strings from
//IMAP, or the entire distribution can be printed with the optional
//user-specified print function PRINT_FN */
stat_stat_t * stat_reg_dist(stat_sdb_t *sdb,		//stat database
	const char *name,				//stat variable name
	const char *desc,				//stat variable description
	unsigned int init_val,				//dist initial value
	unsigned int arr_sz,				//array size
	unsigned int bucket_sz,				//array bucket size
	int pf,						//print format, use PF_* defs
	const char *format,				//optional variable output format
	char **imap,					//optional index -> string map
	print_fn_t print_fn)				//optional user print function
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = format ? format : NULL;
	stat->sc = sc_dist;
	stat->variant.for_dist.init_val = init_val;
	stat->variant.for_dist.arr_sz = arr_sz;
	stat->variant.for_dist.bucket_sz = bucket_sz;
	stat->variant.for_dist.pf = pf;
	stat->variant.for_dist.imap = imap;
	stat->variant.for_dist.print_fn = print_fn;
	stat->variant.for_dist.overflows = 0;

	stat->variant.for_dist.arr = new unsigned int[arr_sz];

	//link onto SDB chain
	add_stat(sdb, stat);

	//initialize stat
	for(unsigned int i=0; i < arr_sz; i++)
	{
		stat->variant.for_dist.arr[i] = init_val;
	}
	return stat;
}

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
	print_fn_t print_fn)				//optional user print function
{
	stat_stat_t *stat = new stat_stat_t;

	stat->name = name;
	stat->desc = desc;
	stat->format = format ? format : NULL;
	stat->sc = sc_sdist;
	stat->variant.for_sdist.init_val = init_val;
	stat->variant.for_sdist.pf = pf;
	stat->variant.for_sdist.print_fn = print_fn;

	//allocate hash table
	stat->variant.for_sdist.sarr = new bucket_t*[HTAB_SZ];

	//link onto SDB chain
	add_stat(sdb, stat);

	return stat;
}

//add NSAMPLES to array or sparse array distribution STAT
void stat_add_samples(stat_stat_t *stat,		//stat database
	md_addr_t index,				//distribution index of samples
	int nsamples)					//number of samples to add to dist
{
	switch (stat->sc)
	{
	case sc_dist:
		{
			//compute array index
			unsigned int i = index / stat->variant.for_dist.bucket_sz;

			//check for overflow
			if(i >= stat->variant.for_dist.arr_sz)
			{
				stat->variant.for_dist.overflows += nsamples;
			}
			else
			{
				stat->variant.for_dist.arr[i] += nsamples;
			}
		}
		break;
	case sc_sdist:
		{
			int hash = HTAB_HASH(index);

			if(hash < 0 || hash >= HTAB_SZ)
			{
				panic("hash table index overflow");
			}

			//find bucket
			bucket_t *bucket;
			for(bucket = stat->variant.for_sdist.sarr[hash]; bucket != NULL; bucket = bucket->next)
			{
				if(bucket->index == index)
				{
					break;
				}
			}
			if(!bucket)
			{
				//add a new sample bucket
				bucket = new bucket_t;
				bucket->next = stat->variant.for_sdist.sarr[hash];
				stat->variant.for_sdist.sarr[hash] = bucket;
				bucket->index = index;
				bucket->count = stat->variant.for_sdist.init_val;
			}
			bucket->count += nsamples;
		}
		break;
	default:
		panic("stat variable is not an array distribution");
	}
}

//add a single sample to array or sparse array distribution STAT
void stat_add_sample(stat_stat_t *stat,				//stat variable
	md_addr_t index)					//index of sample
{
	stat_add_samples(stat, index, 1);
}

stat_stat_t::~stat_stat_t()
{
	if(sc==sc_formula)
	{
		delete [] variant.for_formula.formula;
	}
}

//register a double statistical formula, the formula is evaluated when the
//	statistic is printed, the formula expression may reference any registered
//	statistical variable and, in addition, the standard operators '(', ')', '+',
//	'-', '*', and '/', and literal (i.e., C-format decimal, hexidecimal, and
//	octal) constants are also supported; NOTE: all terms are immediately
//	converted to double values and the result is a double value, see eval.h
//	for more information on formulas
stat_stat_t * stat_reg_formula(stat_sdb_t *sdb,		//stat database
	std::string name,				//stat variable name
	std::string desc,				//stat variable description
	std::string formula,				//formula expression
	const char *format)				//optional variable output format
{
	stat_stat_t *stat = new stat_stat_t;

	stat->variant.for_formula.formula = new char[strlen(formula.c_str())+1];
	strcpy(stat->variant.for_formula.formula,formula.c_str());
	stat->name = name;
	stat->desc = desc;
	stat->format = (char *)(format ? format : "%12.4f");
	stat->sc = sc_formula;

	//link onto SDB chain
	add_stat(sdb, stat);

	return stat;
}


//compare two indicies in a sparse array hash table, used by qsort()
int compare_fn(void *p1, void *p2)
{
	bucket_t **pb1 = (bucket_t **)p1, **pb2 = (bucket_t **)p2;

	//compare indices
	if((*pb1)->index < (*pb2)->index)
	{
		return -1;
	}
	else if ((*pb1)->index > (*pb2)->index)
	{
		return 1;
	}
	//else, ((*pb1)->index == (*pb2)->index)
	return 0;
}

//print an array distribution
void print_dist(stat_stat_t *stat,			//stat variable
	FILE *fd)					//output stream
{
	//count and sum entries
	unsigned int i, bcount=0;
	//unsigned int imax=0, imin=UINT_MAX;
	double btotal=0.0, bsum, bvar=0.0, bavg, bsqsum=0.0;
	int pf = stat->variant.for_dist.pf;

	for(i=0; i<stat->variant.for_dist.arr_sz; i++)
	{
		bcount++;
		btotal += stat->variant.for_dist.arr[i];
		//on-line variance computation, tres cool, no!?!
		bsqsum += ((double)stat->variant.for_dist.arr[i] * (double)stat->variant.for_dist.arr[i]);
		bavg = btotal / MAX((double)bcount, 1.0);
		bvar = (bsqsum - ((double)bcount * bavg * bavg)) / (double)(((bcount - 1) > 0) ? (bcount - 1) : 1);
	}

	//print header
	fprintf(fd, "\n");
	fprintf(fd, "%-22s # %s\n", stat->name.c_str(), stat->desc.c_str());
	fprintf(fd, "%s.array_size = %u\n", stat->name.c_str(), stat->variant.for_dist.arr_sz);
	fprintf(fd, "%s.bucket_size = %u\n", stat->name.c_str(), stat->variant.for_dist.bucket_sz);

	fprintf(fd, "%s.count = %u\n", stat->name.c_str(), bcount);
	fprintf(fd, "%s.total = %.0f\n", stat->name.c_str(), btotal);
	if(bcount > 0)
	{
		fprintf(fd, "%s.imin = %u\n", stat->name.c_str(), 0U);
		fprintf(fd, "%s.imax = %u\n", stat->name.c_str(), bcount);
	}
	else
	{
		fprintf(fd, "%s.imin = %d\n", stat->name.c_str(), -1);
		fprintf(fd, "%s.imax = %d\n", stat->name.c_str(), -1);
	}
	fprintf(fd, "%s.average = %8.4f\n", stat->name.c_str(), btotal/MAX(bcount, 1.0));
	fprintf(fd, "%s.std_dev = %8.4f\n", stat->name.c_str(), sqrt(bvar));
	fprintf(fd, "%s.overflows = %u\n", stat->name.c_str(), stat->variant.for_dist.overflows);

	fprintf(fd, "# pdf == prob dist fn, cdf == cumulative dist fn\n");
	fprintf(fd, "# %14s ", "index");
	if(pf & PF_COUNT)
	{
		fprintf(fd, "%10s ", "count");
	}
	if(pf & PF_PDF)
	{
		fprintf(fd, "%6s ", "pdf");
	}
	if(pf & PF_CDF)
	{
		fprintf(fd, "%6s ", "cdf");
	}
	fprintf(fd, "\n");

	fprintf(fd, "%s.start_dist\n", stat->name.c_str());

	if(bcount > 0)
	{
		//print the array
		bsum = 0.0;
		for(i=0; i<bcount; i++)
		{
			bsum += (double)stat->variant.for_dist.arr[i];
			if(stat->variant.for_dist.print_fn)
			{
				stat->variant.for_dist.print_fn(stat, i, stat->variant.for_dist.arr[i], bsum, btotal);
			}
			else
			{
				if(stat->format == std::string(""))
				{
					if(stat->variant.for_dist.imap)
					{
						fprintf(fd, "%-16s ", stat->variant.for_dist.imap[i]);
					}
					else
					{
						fprintf(fd, "%16u ", i * stat->variant.for_dist.bucket_sz);
					}
					if(pf & PF_COUNT)
					{
						fprintf(fd, "%10u ", stat->variant.for_dist.arr[i]);
					}
					if(pf & PF_PDF)
					{
						fprintf(fd, "%6.2f ", (double)stat->variant.for_dist.arr[i] / MAX(btotal, 1.0) * 100.0);
					}
					if(pf & PF_CDF)
					{
						fprintf(fd, "%6.2f ", bsum/MAX(btotal, 1.0) * 100.0);
					}
				}
				else
				{
					if(pf == (PF_COUNT|PF_PDF|PF_CDF))
					{
						if(stat->variant.for_dist.imap)
						{
							fprintf(fd, stat->format, stat->variant.for_dist.imap[i], stat->variant.for_dist.arr[i],
								(double)stat->variant.for_dist.arr[i] / MAX(btotal, 1.0) * 100.0, bsum/MAX(btotal, 1.0) * 100.0);
						}
						else
						{
							fprintf(fd, stat->format, i * stat->variant.for_dist.bucket_sz, stat->variant.for_dist.arr[i],
								(double)stat->variant.for_dist.arr[i] / MAX(btotal, 1.0) * 100.0, bsum/MAX(btotal, 1.0) * 100.0);
						}
					}
					else
					{
						fatal("distribution format not yet implemented");
					}
				}
				fprintf(fd, "\n");
			}
		}
	}
	fprintf(fd, "%s.end_dist\n", stat->name.c_str());
}

//print a sparse array distribution
void print_sdist(stat_stat_t *stat,				//stat variable
	FILE *fd)						//output stream
{
	//count and sum entries
	unsigned int i, bcount=0;
	md_addr_t imax=0, imin=UINT_MAX;
	double btotal=0.0, bsum, bvar=0.0, bavg, bsqsum=0.0;
	bucket_t *bucket;
	int pf = stat->variant.for_sdist.pf;

	for(i=0; i<HTAB_SZ; i++)
	{
		for(bucket = stat->variant.for_sdist.sarr[i]; bucket != NULL; bucket = bucket->next)
		{
			bcount++;
			btotal += bucket->count;

			//on-line variance computation, tres cool, no!?!
			bsqsum += ((double)bucket->count * (double)bucket->count);
			bavg = btotal / (double)bcount;
			bvar = (bsqsum - ((double)bcount * bavg * bavg)) / (double)(((bcount - 1) > 0) ? (bcount - 1) : 1);
			if(bucket->index < imin)
			{
				imin = bucket->index;
			}
			if(bucket->index > imax)
			{
				imax = bucket->index;
			}
		}
	}

	//print header
	fprintf(fd, "\n");
	fprintf(fd, "%-22s # %s\n", stat->name.c_str(), stat->desc.c_str());
	fprintf(fd, "%s.count = %u\n", stat->name.c_str(), bcount);
	fprintf(fd, "%s.total = %.0f\n", stat->name.c_str(), btotal);
	if(bcount > 0)
	{
		myfprintf(fd, "%s.imin = 0x%p\n", stat->name.c_str(), imin);
		myfprintf(fd, "%s.imax = 0x%p\n", stat->name.c_str(), imax);
	}
	else
	{
		fprintf(fd, "%s.imin = %d\n", stat->name.c_str(), -1);
		fprintf(fd, "%s.imax = %d\n", stat->name.c_str(), -1);
	}
	fprintf(fd, "%s.average = %8.4f\n", stat->name.c_str(), btotal/bcount);
	fprintf(fd, "%s.std_dev = %8.4f\n", stat->name.c_str(), sqrt(bvar));
	fprintf(fd, "%s.overflows = 0\n", stat->name.c_str());

	fprintf(fd, "# pdf == prob dist fn, cdf == cumulative dist fn\n");
	fprintf(fd, "# %14s ", "index");
	if(pf & PF_COUNT)
	{
		fprintf(fd, "%10s ", "count");
	}
	if(pf & PF_PDF)
	{
		fprintf(fd, "%6s ", "pdf");
	}
	if(pf & PF_CDF)
	{
		fprintf(fd, "%6s ", "cdf");
	}

	fprintf(fd, "\n");
	fprintf(fd, "%s.start_dist\n", stat->name.c_str());

	if(bcount > 0)
	{
		unsigned int bindex;

		//collect all buckets
		bucket_t **barr = new bucket_t*[bcount];

		for(bindex=0,i=0; i<HTAB_SZ; i++)
		{
			for(bucket = stat->variant.for_sdist.sarr[i]; bucket != NULL; bucket = bucket->next)
			{
				barr[bindex++] = bucket;
			}
		}

		//sort the array by index
		qsort(barr, bcount, sizeof(bucket_t *), (int (*)(const void*, const void*))compare_fn);

		//print the array
		bsum = 0.0;
		for(i=0; i<bcount; i++)
		{
			bsum += (double)barr[i]->count;
			if(stat->variant.for_sdist.print_fn)
			{
				stat->variant.for_sdist.print_fn(stat, barr[i]->index, barr[i]->count, bsum, btotal);
			}
			else
			{
				if(stat->format == std::string(""))
				{
					myfprintf(fd, "0x%p ", barr[i]->index);
					if(pf & PF_COUNT)
					{
						fprintf(fd, "%10u ", barr[i]->count);
					}
					if(pf & PF_PDF)
					{
						fprintf(fd, "%6.2f ", (double)barr[i]->count/MAX(btotal, 1.0) * 100.0);
					}
					if(pf & PF_CDF)
					{
						fprintf(fd, "%6.2f ", bsum/MAX(btotal, 1.0) * 100.0);
					}
				}
				else
				{
					if(pf == (PF_COUNT|PF_PDF|PF_CDF))
					{
						myfprintf(fd, stat->format, barr[i]->index, barr[i]->count, (double)barr[i]->count
							/ MAX(btotal, 1.0)*100.0, bsum/MAX(btotal, 1.0) * 100.0);
					}
					else if(pf == (PF_COUNT|PF_PDF))
					{
						myfprintf(fd, stat->format, barr[i]->index, barr[i]->count,
							(double)barr[i]->count/MAX(btotal, 1.0)*100.0);
					}
					else if(pf == PF_COUNT)
					{
						myfprintf(fd, stat->format, barr[i]->index, barr[i]->count);
					}
					else
					{
						fatal("distribution format not yet implemented");
					}
				}
				fprintf(fd, "\n");
			}
		}
		//all done, release bucket pointer array
		free(barr);
	}
	fprintf(fd, "%s.end_dist\n", stat->name.c_str());
}

//print the value of stat variable STAT
void stat_print_stat(stat_sdb_t *sdb,			//stat database
	stat_stat_t *stat,				//stat variable
	FILE *fd)					//output stream
{
	eval_value_t val;

	switch(stat->sc)
	{
	case sc_int:
		fprintf(fd, "%-22s ", stat->name.c_str());
		myfprintf(fd, stat->format, *stat->variant.for_int.var);
		fprintf(fd, " # %s", stat->desc.c_str());
		break;
	case sc_uint:
		fprintf(fd, "%-22s ", stat->name.c_str());
		myfprintf(fd, stat->format, *stat->variant.for_uint.var);
		fprintf(fd, " # %s", stat->desc.c_str());
		break;
	case sc_qword:
		{
			char buf[128];
			fprintf(fd, "%-22s ", stat->name.c_str());
			mysprintf(buf, stat->format, *stat->variant.for_qword.var);
			fprintf(fd, "%s # %s", buf, stat->desc.c_str());
		}
		break;
	case sc_sqword:
		{
			char buf[128];
			fprintf(fd, "%-22s ", stat->name.c_str());
			mysprintf(buf, stat->format, *stat->variant.for_sqword.var);
			fprintf(fd, "%s # %s", buf, stat->desc.c_str());
		}
		break;
	case sc_float:
		fprintf(fd, "%-22s ", stat->name.c_str());
		myfprintf(fd, stat->format, (double)*stat->variant.for_float.var);
		fprintf(fd, " # %s", stat->desc.c_str());
		break;
	case sc_double:
		fprintf(fd, "%-22s ", stat->name.c_str());
		myfprintf(fd, stat->format, *stat->variant.for_double.var);
		fprintf(fd, " # %s", stat->desc.c_str());
		break;
	case sc_dist:
		print_dist(stat, fd);
		break;
	case sc_sdist:
		print_sdist(stat, fd);
		break;
	case sc_formula:
		{
			//instantiate a new evaluator to avoid recursion problems
			eval_state_t *es = new eval_state_t(stat_eval_ident, sdb, NULL);
			char *endp;

			fprintf(fd, "%-22s ", stat->name.c_str());
			val = eval_expr(es, stat->variant.for_formula.formula, &endp);
			if(eval_error != ERR_NOERR || *endp != '\0')
			{
				fprintf(fd, "<error: %s>", eval_err_str[eval_error]);
			}
			else
			{
				myfprintf(fd, stat->format, eval_as<double>(val));
			}
			fprintf(fd, " # %s", stat->desc.c_str());

			//done with the evaluator
			delete es;
		}
		break;
	default:
		panic("bogus stat class");
	}
	fprintf(fd, "\n");
}

//print the value of all stat variables in stat database SDB
void stat_print_stats(stat_sdb_t *sdb,			//stat database
	FILE *fd)					//output stream
{
	if(!sdb)
	{	//no stats
		return;
	}

	for(stat_stat_t *stat=sdb->stats; stat != NULL; stat=stat->next)
	{
		stat_print_stat(sdb, stat, fd);
	}
}

//find a stat variable, returns NULL if it is not found
stat_stat_t * stat_find_stat(stat_sdb_t *sdb,		//stat database
	std::string stat_name)				//stat name
{
	for(stat_stat_t *stat = sdb->stats; stat != NULL; stat = stat->next)
	{
		if(!strcmp(stat->name.c_str(), stat_name.c_str()))
		{
			return stat;
		}
	}
	return NULL;
}

#ifdef TESTIT

void
main(void)
{
  stat_sdb_t *sdb;
  stat_stat_t *stat, *stat1, *stat2, *stat3, *stat4, *stat5;
  int an_int;
  unsigned int a_uint;
  float a_float;
  double a_double;
  char *my_imap[8] = {
    "foo", "bar", "uxxe", "blah", "gaga", "dada", "mama", "googoo"
  };

  /* make stats database */
  sdb = new stat_sdb_t();

  /* register stat variables */
  stat_reg_int(sdb, "stat.an_int", "An integer stat variable.",
	       &an_int, 1, NULL);
  stat_reg_uint(sdb, "stat.a_uint", "An unsigned integer stat variable.",
		&a_uint, 2, "%u (unsigned)");
  stat_reg_float(sdb, "stat.a_float", "A float stat variable.",
		 &a_float, 3, NULL);
  stat_reg_double(sdb, "stat.a_double", "A double stat variable.",
		  &a_double, 4, NULL);
  stat_reg_formula(sdb, "stat.a_formula", "A double stat formula.",
		   "stat.a_float / stat.a_uint", NULL);
  stat_reg_formula(sdb, "stat.a_formula1", "A double stat formula #1.",
		   "2 * (stat.a_formula / (1.5 * stat.an_int))", NULL);
  stat_reg_formula(sdb, "stat.a_bad_formula", "A double stat formula w/error.",
		   "stat.a_float / (stat.a_uint - 2)", NULL);
  stat = stat_reg_dist(sdb, "stat.a_dist", "An array distribution.",
		       0, 8, 1, PF_ALL, NULL, NULL, NULL);
  stat1 = stat_reg_dist(sdb, "stat.a_dist1", "An array distribution #1.",
			0, 8, 4, PF_ALL, NULL, NULL, NULL);
  stat2 = stat_reg_dist(sdb, "stat.a_dist2", "An array distribution #2.",
			0, 8, 1, (PF_PDF|PF_CDF), NULL, NULL, NULL);
  stat3 = stat_reg_dist(sdb, "stat.a_dist3", "An array distribution #3.",
			0, 8, 1, PF_ALL, NULL, my_imap, NULL);
  stat4 = stat_reg_sdist(sdb, "stat.a_sdist", "A sparse array distribution.",
			 0, PF_ALL, NULL, NULL);
  stat5 = stat_reg_sdist(sdb, "stat.a_sdist1",
			 "A sparse array distribution #1.",
			 0, PF_ALL, "0x%08lx        %10lu %6.2f %6.2f",
			 NULL);

  /* print initial stats */
  fprintf(stdout, "** Initial stats...\n");
  stat_print_stats(sdb, stdout);

  /* adjust stats */
  an_int++;
  a_uint++;
  a_float *= 2;
  a_double *= 4;

  stat_add_sample(stat, 8);
  stat_add_sample(stat, 8);
  stat_add_sample(stat, 1);
  stat_add_sample(stat, 3);
  stat_add_sample(stat, 4);
  stat_add_sample(stat, 4);
  stat_add_sample(stat, 7);

  stat_add_sample(stat1, 32);
  stat_add_sample(stat1, 32);
  stat_add_sample(stat1, 1);
  stat_add_sample(stat1, 12);
  stat_add_sample(stat1, 17);
  stat_add_sample(stat1, 18);
  stat_add_sample(stat1, 30);

  stat_add_sample(stat2, 8);
  stat_add_sample(stat2, 8);
  stat_add_sample(stat2, 1);
  stat_add_sample(stat2, 3);
  stat_add_sample(stat2, 4);
  stat_add_sample(stat2, 4);
  stat_add_sample(stat2, 7);

  stat_add_sample(stat3, 8);
  stat_add_sample(stat3, 8);
  stat_add_sample(stat3, 1);
  stat_add_sample(stat3, 3);
  stat_add_sample(stat3, 4);
  stat_add_sample(stat3, 4);
  stat_add_sample(stat3, 7);

  stat_add_sample(stat4, 800);
  stat_add_sample(stat4, 800);
  stat_add_sample(stat4, 1123);
  stat_add_sample(stat4, 3332);
  stat_add_sample(stat4, 4000);
  stat_add_samples(stat4, 4001, 18);
  stat_add_sample(stat4, 7);

  stat_add_sample(stat5, 800);
  stat_add_sample(stat5, 800);
  stat_add_sample(stat5, 1123);
  stat_add_sample(stat5, 3332);
  stat_add_sample(stat5, 4000);
  stat_add_samples(stat5, 4001, 18);
  stat_add_sample(stat5, 7);

  /* print final stats */
  fprintf(stdout, "** Final stats...\n");
  stat_print_stats(sdb, stdout);

  /* all done */
	delete sdb;
  exit(0);
}

#endif /* TEST */
