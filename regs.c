/* regs.c - architected registers state routines */

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

#include<iostream>
#include<iomanip>
#include"host.h"
#include"misc.h"
#include"machine.h"
#include"loader.h"
#include"regs.h"

//initialize architected register state
//FIXME: assuming all entries should be zero... (probably don't need to clear both items to zero either)
regs_t::regs_t()
: regs_R(MD_NUM_IREGS,0), regs_F(MD_NUM_FREGS), regs_PC(0), regs_NPC(0), context_id(0)
{
	for(unsigned int i=0;i<MD_NUM_FREGS;i++)
	{
		regs_F[i].d = 0;
		regs_F[i].q = 0;
	}
}

std::ostream & operator << (std::ostream & out, const regs_t & source)
{
	out << "/* misc regs icnt, PC, NPC, etc... */" << std::endl;

	//sim_num_insn is always zero... instructions counts can't be stored here.
	out << "(" << 0 << ", ";

	//regs_PC, regs_NPC, regs_C.fpcr, regs_C.uniq
	out << "0x" << std::hex << source.regs_PC << ", ";
	out << "0x" << source.regs_NPC << ", ";
	out << std::dec << source.regs_C.fpcr << ", ";
	out << source.regs_C.uniq << ")" << std::endl;

	out << std::endl;
	out << "/* integer regs */" << std::endl;
	out << "(";
	for(unsigned int i = 0;i<source.regs_R.size();i++)
	{
		out << "0x" << std::hex << source.regs_R[i];
		if((i+1)<source.regs_R.size())
		{
			out << ", ";
		}
	}
	out << ")" << std::endl;

	out << std::endl;
	out << "/* FP regs (integer format) */" << std::endl;
	out << "(";
	for(unsigned int i = 0;i<source.regs_F.size();i++)
	{
		out << "0x" << std::hex << source.regs_F[i].q;
		if((i+1)<source.regs_F.size())
		{
			out << ", ";
		}
	}
	out << ")" << std::endl;

	out << std::dec << std::endl;
	return out;
}

std::istream & operator >> (std::istream & in, regs_t & target)
{
	std::string misc_header("/* misc regs icnt, PC, NPC, etc... */");
	std::string int_header("/* integer regs */");
	std::string fp_header("/* FP regs (integer format) */");

	std::string buf;
	char c_buf;
	std::getline(in,buf);

	if(buf!=misc_header)
	{
		std::cout << "Malformed EIO file, could not read initial header, read: " << buf << std::endl;
		std::cout << "Needed to read: " << misc_header << std::endl;
		exit(-1);
	}

	in >> buf;
	if(buf!="(0,")
	{
		std::cout << "Failed reading sim_num_insn from control register data" << std::endl;
		exit(-1);
	}

	in >> std::hex >> target.regs_PC >> c_buf;
	in >> std::hex >> target.regs_NPC >> c_buf;
	in >> std::dec >> target.regs_C.fpcr >> c_buf;
	in >> std::dec >> target.regs_C.uniq;
	std::getline(in,buf);
	std::getline(in,buf);
	std::getline(in,buf);

	if(buf!=int_header)
	{
		std::cout << "Malformed EIO file, could not read int reg header, read: " << buf << std::endl;
		std::cout << "Needed to read: " << int_header << std::endl;
		exit(-1);
	}
	in >> c_buf;

	for(unsigned int i = 0;i<target.regs_R.size();i++)
	{
		in >> std::hex >> target.regs_R[i];
		if((i+1)<target.regs_R.size())
		{
			in >> c_buf;
		}
	}
	in >> std::dec;
	std::getline(in,buf);
	std::getline(in,buf);
	std::getline(in,buf);

	if(buf!=fp_header)
	{
		std::cout << "Malformed EIO file, could not read fp reg header, read: " << buf << std::endl;
		std::cout << "Needed to read: " << fp_header << std::endl;
		exit(-1);
	}
	in >> c_buf;

	for(unsigned int i = 0;i<target.regs_F.size();i++)
	{
		in >> std::hex >> target.regs_F[i].q;
		if((i+1)<target.regs_F.size())
		{
			in >> c_buf;
		}
	}
	in >> std::dec;
	std::getline(in,buf);
	std::getline(in,buf);
	return in;
}

regs_t::regs_t(const regs_t & target)
: regs_R(target.regs_R), regs_F(target.regs_F), regs_C(target.regs_C), regs_PC(target.regs_PC), regs_NPC(target.regs_NPC),
	context_id(target.context_id)
{}

const regs_t & regs_t::operator=(const regs_t & target)
{
	if(this==&target)
	{
		return *this;
	}
	regs_F = target.regs_F;
	regs_R = target.regs_R;
	regs_C = target.regs_C;
	regs_PC = target.regs_PC;
	regs_NPC = target.regs_NPC;
	context_id = target.context_id;

	return *this;
}

