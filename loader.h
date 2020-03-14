/* loader.h - program loader interfaces */

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


#ifndef LOADER_H
#define LOADER_H

#include<cstdio>
#include<vector>
#include<string>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"

/*
 * This module implements program loading.  The program text (code) and
 * initialized data are first read from the program executable.  Next, the
 * program uninitialized data segment is initialized to all zero's.  Finally,
 * the program stack is initialized with command line arguments and
 * environment variables.  The format of the top of stack when the program
 * starts execution is as follows:
 *
 * 0x7fffffff    +----------+
 *               | unused   |
 * 0x7fffc000    +----------+
 *               | envp     |
 *               | strings  |
 *               +----------+
 *               | argv     |
 *               | strings  |
 *               +----------+
 *               | envp     |
 *               | array    |
 *               +----------+
 *               | argv     |
 *               | array    |
 *               +----------+
 *               | argc     |
 * regs_R[29]    +----------+
 * (stack ptr)
 *
 * NOTE: the start of envp is computed in crt0.o (C startup code) using the
 * value of argc and the computed size of the argv array, the envp array size
 * is not specified, but rather it is NULL terminated, so the startup code
 * has to scan memory for the end of the string.
 */

class loader_t
{
	public:
		loader_t(counter_t & sim_num_insn);		//Provides sim_num_insn to loader_t

		//load program text and initialized data into simulated virtual memory
		//space and initialize program segment range variables
		int ld_load_prog(std::string fname,		//program to load
			std::vector<std::string> argv,		//simulated program cmd line args
			std::vector<std::string> envp,		//simulated program environment
			regs_t *regs,				//registers to initialize for load
			mem_t *mem,				//memory space to load prog into
			int zero_bss_segs);			//zero uninit data segment?

		counter_t & sim_num_insn;
};
#endif /* LOADER_H */
