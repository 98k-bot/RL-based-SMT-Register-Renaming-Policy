/* eio.c - external interfaces to external I/O files */

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

#include<iostream>
#include<fstream>
#include<cstdio>
#include<cstdlib>
#ifdef _MSC_VER
#include <io.h>
#else /* !_MSC_VER */
#include <unistd.h>
#endif

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "regs.h"
#include "memory.h"
#include "loader.h"
#include "syscall.h"
#include "sim.h"
#include "endian.h"
#include "eio.h"

#ifdef _MSC_VER
#define write		_write
#endif

#define EIO_FILE_HEADER	"/* This is a SimpleScalar EIO file - DO NOT MOVE OR EDIT THIS LINE! */\n"

//EIO transaction count, i.e., number of last transaction completed
counter_t eio_trans_icnt = -1;

int eio_open(std::string fname, std::ifstream & in)
{
	int target_big_endian = (endian_host_byte_order() == endian_big);

	in.open(fname.c_str());
	if(!in.is_open())
	{
		fatal("unable to open EIO file `%s'", fname.c_str());
	}

	std::string format_header("/* file_format: 2, file_version: 3, big_endian: 0 */");
	std::string format_header_4("/* file_format: 2, file_version: 4, big_endian: 0 */");
	std::string start_header("/* ** start checkpoint @ -1... */");
	std::string buf;

	getline(in,buf);
	buf += '\n';
	if(buf!=EIO_FILE_HEADER)
	{
		std::cerr << "Failed reading EIO file header, read: " << buf << std::endl;
		std::cerr << "Should have read: " << EIO_FILE_HEADER << std::endl;
		exit(-1);
	}
	getline(in,buf);
	getline(in,buf);
	if(buf!=format_header && buf!=format_header_4)
	{
		std::cerr << "Failed reading format header, read: " << buf << std::endl;
		std::cerr << "Should have read: " << format_header << std::endl;
		std::cerr << "or: " << format_header_4 << std::endl;
		exit(-1);
	}

	int file_format, file_version, big_endian;
	char c_buf;
	in >> c_buf >> file_format >> c_buf >> file_version >> c_buf >> big_endian;
	getline(in,buf);
	if(!in.good())
	{
		fatal("could not read EIO file header");
	}

	getline(in,buf);
	getline(in,buf);
	if(buf!=start_header)
	{
		std::cerr << "Failed reading start header, read: " << buf << std::endl;
		std::cerr << "Should have read: " << start_header << std::endl;
		exit(-1);
	}
	getline(in,buf);

	if(file_format != MD_EIO_FILE_FORMAT)
	{
		fatal("EIO file `%s' has incompatible format", fname.c_str());
	}

	if((file_version != EIO_FILE_VERSION) && (file_version != EIO_FILE_VERSION_NEW))
	{
		fatal("EIO file `%s' has incompatible version", fname.c_str());
	}

	if(!!big_endian != !!target_big_endian)
	{
		warn("endian of `%s' does not match host", fname.c_str());
		warn("running with experimental cross-endian execution support");
		warn("****************************************");
		warn("**>> please check results carefully <<**");
		warn("****************************************");
	}
	return file_version;
}

//returns non-zero if file FNAME has a valid EIO header
int eio_valid(const char *fname)
{
	char buf[512];

	//open possible EIO file
	FILE *fd = gzopen(fname, "r");
	if(!fd)
	{
		return FALSE;
	}

	//read and check EIO file header
	if(!fgets(buf, 512, fd))
		return FALSE;

	//all done, close up file
	gzclose(fd);

	//check the header
	if(strcmp(buf, EIO_FILE_HEADER))
	{
		return FALSE;
	}

	//else, has a valid header, go with it...
	return TRUE;
}

void eio_close(FILE *fd)
{
	gzclose(fd);
}

//check point current architected state to stream FD, returns EIO transaction count (an EIO file pointer)
counter_t eio_write_chkpt(regs_t *regs,		//regs to dump
	mem_t *mem,				//memory to dump
	std::string filename,			//filename to write to
	counter_t & sim_num_insn)		//Reference to number of instructions executed
{
	std::ofstream outfile(filename.c_str());
	if(!outfile.is_open())
	{
		warn("Could not open %s for eio checkpoint creation",filename.c_str());
		return -1;
	}

	//These could be const, maybe not file_version which we will allow to be 4.
	int file_format(2), file_version(4), big_endian(0);
	eio_trans_icnt = -1;

	outfile << "/* This is a SimpleScalar EIO file - DO NOT MOVE OR EDIT THIS LINE! */\n\n";
	outfile << "/* file_format: " << file_format << ", file_version: " << file_version << ", big_endian: " << big_endian << " */\n";
	outfile << "(" << file_format << ", " << file_version << ", " << big_endian << ")\n\n";

	outfile << "/* ** start checkpoint @ " << eio_trans_icnt << "... */\n\n";

	outfile << "/* EIO file pointer: -1... */\n";

	//Needed to be -1 in old case, doesn't anymore, but retaining for now.
	outfile << (unsigned long long)(-1);

	outfile << "\n\n/* Translation Table */" << std::endl;
	outfile << contexts[regs->context_id].file_table << std::endl;

	outfile << (*regs);

	outfile << mem;

	outfile << "/* ** end checkpoint @ " << eio_trans_icnt << "... */\n\n";

	outfile.close();
	return eio_trans_icnt;
}

//read check point of architected state from stream FD, returns EIO transaction count (an EIO file pointer)
counter_t eio_read_chkpt(regs_t *regs,		//regs to dump
	mem_t *mem,				//memory to dump
	std::ifstream & in,
	counter_t & sim_num_insn,		//Reference to number of instructions executed
	int version)
{
	std::string eio_pointer_header("/* EIO file pointer: -1... */");
	std::string buf;

	std::getline(in,buf);
	if(buf!=eio_pointer_header)
	{
		std::cerr << "Failed reading EIO file pointer, read: " << buf << std::endl;
		std::cerr << "Wanted to read: " << eio_pointer_header << std::endl;
		exit(-1);
	}
	unsigned long long trans_icnt;
	in >> trans_icnt;
	if(trans_icnt!=(unsigned long long)-1)
	{
		std::cerr << "Failed reading trans_icnt, read: " << trans_icnt << "\nWanted to read: " << (unsigned long long)(-1) << std::endl;
		exit(-1);
	}

	std::getline(in,buf);
	std::getline(in,buf);

	if(version == EIO_FILE_VERSION_NEW)
	{
		std::getline(in,buf);
		assert(buf=="/* Translation Table */");
		in >> contexts[num_contexts].file_table;
		std::getline(in,buf);
		std::getline(in,buf);
	}

	in >> contexts[num_contexts].regs;
	in >> contexts[num_contexts].mem;

	in.close();
	return trans_icnt;
}
