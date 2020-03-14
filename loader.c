#define ENVP_REDUCE
#define FORCE_CONSISTENT
//#define LOADER_DEBUG
//#define STACK_DEBUG

#include<cerrno>

/* loader.c - program loader routines */

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

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "endian.h"
#include "regs.h"
#include "memory.h"
#include "sim.h"
#include "eio.h"
#include "loader.h"

#ifdef BFD_LOADER
#include <bfd.h>
#else /* !BFD_LOADER */
#include "ecoff.h"
#endif /* BFD_LOADER */

//amount of tail padding added to all loaded text segments
#define TEXT_TAIL_PADDING 0 /* was: 128 */

loader_t::loader_t(counter_t & sim_num_insn)
: sim_num_insn(sim_num_insn)
{}

//load program text and initialized data into simulated virtual memory space and initialize program segment range variables
int loader_t::ld_load_prog(std::string fname,			//program to load
	std::vector<std::string> argv,				//simulated program cmd line args
	std::vector<std::string> envp,				//simulated program environment
	regs_t *regs,						//registers to initialize for load
	mem_t *mem,						//memory space to load prog into
	int zero_bss_segs)					//zero uninit data segment?
{
	int argc = argv.size();
	//Quote handling:
	//If "two words" is part of the command line, it is read as two parameters instead of one (with quotes). This handles that problem
	bool quote = false;
	for(int i=0;i<argc;i++)
	{
		if(quote)
		{
			argv[i-1] += std::string(" ") + argv[i];
			argv.erase(argv.begin()+i);
			i--;
			argc--;
			if(argv[i][argv[i].size()-1]=='\"')
			{
				argv[i].erase(argv[i].begin() + argv[i].size() - 1);
				quote = false;
			}
		}
		if(argv[i][0]=='\"')
		{
			assert(!quote);
			quote = true;
			argv[i].erase(argv[i].begin());
		}
	}
	assert(!quote);

	md_addr_t sp, data_break = 0, null_ptr = 0, argv_addr, envp_addr;
//	fprintf(stderr,"sim: loading binary...\n");
	if(eio_valid(fname.c_str()))
	{
		if(argc != 1)
		{
			fprintf(stderr, "error: EIO file has arguments\n");
			exit(1);
		}

		fprintf(stderr, "sim: loading EIO file: %s\n", fname.c_str());
		std::ifstream eio_fd;

		//open the EIO file stream
		int version = eio_open(fname,eio_fd);

		//load initial state checkpoint
		if(eio_read_chkpt(regs, mem, eio_fd, sim_num_insn, version) != -1)
		{
			fprintf(stderr,"bad initial checkpoint in EIO file");
			return EACCES;
		}

		//computed state...
		mem->ld_environ_base = regs->regs_R[MD_REG_SP];
		mem->ld_prog_entry = regs->regs_PC;

		return 0;
	}
#ifdef MD_CROSS_ENDIAN
	else
	{
		warn("endian of `%s' does not match host", fname.c_str());
		warn("running with experimental cross-endian execution support");
		warn("****************************************");
		warn("**>> please check results carefully <<**");
		warn("****************************************");
#if 0
		fatal("SimpleScalar/Alpha only supports binary execution on little-endian hosts, use EIO files on big-endian hosts");
#endif
	}
#endif /* MD_CROSS_ENDIAN */

#ifdef BFD_LOADER
	{
		bfd *abfd;
		asection *sect;

		//set up a local stack pointer, this is where the argv and envp data is written into program memory
		mem->ld_stack_base = MD_STACK_BASE;
		sp = ROUND_DOWN(MD_STACK_BASE - MD_MAX_ENVIRON, sizeof(MD_DOUBLE_TYPE));
		mem->ld_stack_size = mem->ld_stack_base - sp;

		//initial stack pointer value
		mem->ld_environ_base = sp;

		//load the program into memory, try both endians
		if(!(abfd = bfd_openr(argv[0].c_str(), "ss-coff-big")))
		{
			if(!(abfd = bfd_openr(argv[0].c_str(), "ss-coff-little")))
			{
				fprintf(stderr,"cannot open executable `%s'", argv[0].c_str());
				return EFAULT;
			}
		}

		//this call is mainly for its side effect of reading in the sections. we follow the traditional
		//behavior of `strings' in that we don't complain if we don't recognize a file to be an object file.
		if(!bfd_check_format(abfd, bfd_object))
		{
			bfd_close(abfd);
			fprintf(stderr,"cannot open executable `%s'", argv[0].c_str());
			return EFAULT;
		}

		//record profile file name
		mem->ld_prog_fname = argv[0];

		//record endian of target
		mem->ld_target_big_endian = abfd->xvec->byteorder_big_p;

		debug("processing %d sections in `%s'...", bfd_count_sections(abfd), argv[0].c_str());

		//read all sections in file
		for(sect=abfd->sections; sect; sect=sect->next)
		{
			debug("processing section `%s', %d bytes @ 0x%08x...", bfd_section_name(abfd, sect), bfd_section_size(abfd, sect), bfd_section_vma(abfd, sect));

			//read the section data, if allocated and loadable and non-NULL
			if((bfd_get_section_flags(abfd, sect) & SEC_ALLOC) && (bfd_get_section_flags(abfd, sect) & SEC_LOAD)
				&& bfd_section_vma(abfd, sect) && bfd_section_size(abfd, sect))
			{
				//allocate a section buffer
				char * p = calloc(bfd_section_size(abfd, sect), sizeof(char));
				if(!p)
				{
					fprintf(stderr,"cannot allocate %d bytes for section `%s'", bfd_section_size(abfd, sect), bfd_section_name(abfd, sect));
					return EIO;
				}

				if(!bfd_get_section_contents(abfd, sect, p, (file_ptr)0, bfd_section_size(abfd, sect)))
				{
					fprintf(stderr,"could not read entire `%s' section from executable", bfd_section_name(abfd, sect));
					return EIO;
				}

				//copy program section it into simulator target memory
				mem_bcopy(mem_fn, Write, bfd_section_vma(abfd, sect), p, bfd_section_size(abfd, sect));

				//release the section buffer
				free(p);
			}
			//zero out the section if it is loadable but not allocated in exec
			else if(zero_bss_segs && (bfd_get_section_flags(abfd, sect) & SEC_LOAD) && bfd_section_vma(abfd, sect) && bfd_section_size(abfd, sect))
			{
				//zero out the section region
				mem_bzero(mem_fn, bfd_section_vma(abfd, sect), bfd_section_size(abfd, sect));
			}
			else
			{
				//else do nothing with this section, it's probably debug data
				debug("ignoring section `%s' during load...", bfd_section_name(abfd, sect));
			}

			//expected text section
			if(!strcmp(bfd_section_name(abfd, sect), ".text"))
			{
				//.text section processing
				mem->ld_text_size = ((bfd_section_vma(abfd, sect) + bfd_section_size(abfd, sect))
					- MD_TEXT_BASE)	+ /* for speculative fetches/decodes */TEXT_TAIL_PADDING;

				//create tail padding and copy into simulator target memory
#if 0
				mem_bzero(mem_fn, bfd_section_vma(abfd, sect) + bfd_section_size(abfd, sect), TEXT_TAIL_PADDING);
#endif
			}
			//expected data sections
			else if(!strcmp(bfd_section_name(abfd, sect), ".rdata")
				|| !strcmp(bfd_section_name(abfd, sect), ".data")
				|| !strcmp(bfd_section_name(abfd, sect), ".sdata")
				|| !strcmp(bfd_section_name(abfd, sect), ".bss")
				|| !strcmp(bfd_section_name(abfd, sect), ".sbss"))
			{
				//data section processing
				if(bfd_section_vma(abfd, sect) + bfd_section_size(abfd, sect) > data_break)
				{
					data_break = (bfd_section_vma(abfd, sect) + bfd_section_size(abfd, sect));
				}
			}
			else
			{
				//what is this section???
				fprintf(stderr,"encountered unknown section `%s', %d bytes @ 0x%08x", bfd_section_name(abfd, sect), bfd_section_size(abfd, sect), bfd_section_vma(abfd, sect));
				return EIO;
			}
		}

		//compute data segment size from data break point
		mem->ld_text_base = MD_TEXT_BASE;
		mem->ld_data_base = MD_DATA_BASE;
		mem->ld_prog_entry = bfd_get_start_address(abfd);
		mem->ld_data_size = data_break - mem->ld_data_base;

		//done with the executable, close it
		if(!bfd_close(abfd))
		{
			fprintf(stderr,"could not close executable `%s'", argv[0].c_str());
			return EIO;
		}
	}
#else /* !BFD_LOADER, i.e., standalone loader */
	FILE *fobj;
	long floc;
	ecoff_filehdr fhdr;
	ecoff_aouthdr ahdr;
	ecoff_scnhdr shdr;

	//record profile file name
//	fprintf(stderr, "sim: loading %s\n", argv[0].c_str());
	//load the program into memory, try both endians
#if defined(_MSC_VER)
	fobj = fopen(argv[0].c_str(), "rb");
#else
	fobj = fopen(argv[0].c_str(), "r");
#endif
	if(!fobj)
	{
		fprintf(stderr,"cannot open executable `%s'", argv[0].c_str());
		return EFAULT;
	}

	if(fread(&fhdr, sizeof(ecoff_filehdr), 1, fobj) < 1)
	{
		fprintf(stderr,"cannot read header from executable `%s'", argv[0].c_str());
		return EFAULT;
	}

	//record endian of target
	if(fhdr.f_magic == MD_SWAPH(ECOFF_ALPHAMAGIC))
	{
		mem->ld_target_big_endian = false;
	}
	else if(fhdr.f_magic == MD_SWAPH(ECOFF_EB_MAGIC) || fhdr.f_magic == MD_SWAPH(ECOFF_EL_MAGIC)
		|| fhdr.f_magic == MD_SWAPH(ECOFF_EB_OTHER) || fhdr.f_magic == MD_SWAPH(ECOFF_EL_OTHER))
	{
		fprintf(stderr,"Alpha simulator cannot run PISA binary `%s'", argv[0].c_str());
		return EACCES;
	}
	else
	{
		fprintf(stderr,"bad magic number in executable `%s' (not an executable)", argv[0].c_str());
		return EACCES;
	}

	if(fread(&ahdr, sizeof(ecoff_aouthdr), 1, fobj) < 1)
	{
		fprintf(stderr,"cannot read AOUT header from executable `%s'", argv[0].c_str());
		return EACCES;
	}

	if(fhdr.f_flags & 0x3000)
	{
#ifdef LOADER_DEBUG
		fprintf(stderr,"File requires shared libraries - giving control to /sbin/loader\n");
#endif
		if(fclose(fobj))
		{
			fprintf(stderr,"could not close executable `%s'", argv[0].c_str());
			return EIO;
		}

		//Uncomment for additional debug info from /sbin/loader
		//argv.insert(argv.begin(),"-v");
		//argv.insert(argv.begin(),"-stat");
		argv.insert(argv.begin(),"../sysfiles/alpha-sys-root/sbin/loader");

		//Indicate that this executable is in the loader and can be fast-forwarded to the entry point
		contexts[regs->context_id].entry_point = MD_SWAPQ(ahdr.entry);
		contexts[regs->context_id].interrupts |= 0x1;

		return ld_load_prog("../sysfiles/alpha-sys-root/sbin/loader",argv,envp,regs,mem,zero_bss_segs);
	}

	mem->ld_text_base = MD_SWAPQ(ahdr.text_start);
	mem->ld_text_size = MD_SWAPQ(ahdr.tsize);
	mem->ld_prog_entry = MD_SWAPQ(ahdr.entry);
	mem->ld_data_base = MD_SWAPQ(ahdr.data_start);
	mem->ld_data_size = MD_SWAPQ(ahdr.dsize) + MD_SWAPQ(ahdr.bsize);

#ifdef LOADER_DEBUG
	fprintf(stderr,"Text_base: %llx, text_size: %llx, prog_entry: %llx, data_base: %llx, data_size: %llx\n", mem->ld_text_base,mem->ld_text_size,mem->ld_prog_entry, mem->ld_data_base ,mem->ld_data_size);
#endif

	regs->regs_R[MD_REG_GP] = MD_SWAPQ(ahdr.gp_value);

	//compute data segment size from data break point
	data_break = mem->ld_data_base + mem->ld_data_size;

	//seek to the beginning of the first section header, the file header comes first, followed by the optional header (this is the aouthdr), the size of the aouthdr is given in Fdhr.f_opthdr
	fseek(fobj, sizeof(ecoff_filehdr) + MD_SWAPH(fhdr.f_opthdr), 0);

	debug("processing %d sections in `%s'...", MD_SWAPH(fhdr.f_nscns), argv[0].c_str());

	//loop through the section headers
	floc = ftell(fobj);
	for(int i = 0; i < MD_SWAPH(fhdr.f_nscns); i++)
	{
		char *p;

		if(fseek(fobj, floc, 0) == -1)
		{
			fprintf(stderr,"could not reset location in executable");
			return EIO;
		}
		if(fread(&shdr, sizeof(ecoff_scnhdr), 1, fobj) < 1)
		{
			fprintf(stderr,"could not read section %d from executable", i);
			return EIO;
		}
		floc = ftell(fobj);

#ifdef LOADER_DEBUG
		fprintf(stderr,"At section header: %s (flags: 0x%x)\n", std::string(shdr.s_name, shdr.s_name+8).c_str(),MD_SWAPW(shdr.s_flags));
#endif
		switch(MD_SWAPW(shdr.s_flags))
		{
		case ECOFF_STYP_TEXT:
			p = (char *)calloc(MD_SWAPQ(shdr.s_size), sizeof(char));
			if(!p)
			{
				fprintf(stderr,"out of virtual memory");
				return ENOMEM;
			}

			if(fseek(fobj, MD_SWAPQ(shdr.s_scnptr), 0) == -1)
			{
				fprintf(stderr,"could not read `.text' from executable (%d)", i);
				return EIO;
			}
			if(fread(p, MD_SWAPQ(shdr.s_size), 1, fobj) < 1)
			{
				fprintf(stderr,"could not read text section from executable");
				return EIO;
			}
#ifdef LOADER_DEBUG
			fprintf(stderr,"Writing into simulator memory at %llx, %llx bytes\n", MD_SWAPQ(shdr.s_vaddr),MD_SWAPQ(shdr.s_size));
#endif
			//copy program section into simulator target memory
			mem->mem_bcopy(Write, MD_SWAPQ(shdr.s_vaddr), p, MD_SWAPQ(shdr.s_size));

			//release the section buffer
			free(p);
#if 0
			Text_seek = MD_SWAPQ(shdr.s_scnptr);
			Text_start = MD_SWAPQ(shdr.s_vaddr);
			Text_size = MD_SWAPQ(shdr.s_size) / 4;
			//there is a null routine after the supposed end of text
			Text_size += 10;
			Text_end = Text_start + Text_size * 4;
			//create_text_reloc(shdr.s_relptr, shdr.s_nreloc);
#endif
			break;

		case ECOFF_STYP_INIT:
		case ECOFF_STYP_FINI:
			if(MD_SWAPQ(shdr.s_size) > 0)
			{
				p = (char *)calloc(MD_SWAPQ(shdr.s_size), sizeof(char));
				if(!p)
				{
					fprintf(stderr,"out of virtual memory");
					return ENOMEM;
				}

				if(fseek(fobj, MD_SWAPQ(shdr.s_scnptr), 0) == -1)
				{
					fprintf(stderr,"could not read .init or .fini from executable (%d)", i);
					return EIO;
				}
				if(fread(p, MD_SWAPQ(shdr.s_size), 1, fobj) < 1)
				{
					fprintf(stderr,"could not read .init or .fini section from executable");
					return EIO;
				}
#ifdef LOADER_DEBUG
				fprintf(stderr,"Writing into simulator memory at %llx, %llx bytes\n", MD_SWAPQ(shdr.s_vaddr),MD_SWAPQ(shdr.s_size));
#endif
				//copy program section into simulator target memory
				mem->mem_bcopy(Write, MD_SWAPQ(shdr.s_vaddr), p, MD_SWAPQ(shdr.s_size));

				//release the section buffer
				free(p);
			}
			else
			{
				warn("section `%s' is empty...", shdr.s_name);
			}
			break;

		case ECOFF_STYP_LITA:
		case ECOFF_STYP_LIT8:
		case ECOFF_STYP_LIT4:
		case ECOFF_STYP_XDATA:
		case ECOFF_STYP_PDATA:
		case ECOFF_STYP_RCONST:
		//fall through

		case ECOFF_STYP_RDATA:
		//The .rdata section is sometimes placed before the text section instead of being contiguous with the .data section.
#if 0
			Rdata_start = MD_SWAPQ(shdr.s_vaddr);
			Rdata_size = MD_SWAPQ(shdr.s_size);
			Rdata_seek = MD_SWAPQ(shdr.s_scnptr);
#endif
		case ECOFF_STYP_DATA:
#if 0
			Data_seek = MD_SWAPQ(shdr.s_scnptr);
#endif
		case ECOFF_STYP_SDATA:
#if 0
			Sdata_seek = MD_SWAPQ(shdr.s_scnptr);
#endif

		//Cross-compiled code is accessing data from .got which is ignored, this handles that issue.
		//fall through is appropriate here.
		case ECOFF_STYP_GOT:
			if(MD_SWAPQ(shdr.s_size) > 0)
			{
				p = (char *)calloc(MD_SWAPQ(shdr.s_size), sizeof(char));
				if(!p)
				{
					fprintf(stderr,"out of virtual memory");
					return ENOMEM;
				}

				if(fseek(fobj, MD_SWAPQ(shdr.s_scnptr), 0) == -1)
				{
					fprintf(stderr,"could not read `.text' from executable (%d)", i);
					return EIO;
				}
				if(fread(p, MD_SWAPQ(shdr.s_size), 1, fobj) < 1)
				{
					fprintf(stderr,"could not read text section from executable");
					return EIO;
				}
#ifdef LOADER_DEBUG
				fprintf(stderr,"Writing into simulator memory at %llx, %llx bytes\n", MD_SWAPQ(shdr.s_vaddr),MD_SWAPQ(shdr.s_size));
#endif
				//copy program section it into simulator target memory
				mem->mem_bcopy(Write, MD_SWAPQ(shdr.s_vaddr), p, MD_SWAPQ(shdr.s_size));

				//release the section buffer
				free(p);
			}
			else
			{
				warn("section `%s' is empty...", shdr.s_name);
			}
			break;

		case ECOFF_STYP_BSS:
		case ECOFF_STYP_SBSS:
			//no data to read...
			break;

#ifdef LOADER_DEBUG
		default:
			fprintf(stderr,"section header: '%s' ignored... (flags: 0x%x)\n", std::string(shdr.s_name, shdr.s_name+8).c_str(),MD_SWAPW(shdr.s_flags));
#endif
		}
	}

	//done with the executable, close it
	if(fclose(fobj))
	{
		fprintf(stderr,"could not close executable `%s'", argv[0].c_str());
		return EIO;
	}

#endif /* BFD_LOADER */

	//perform sanity checks on segment ranges
	if(!mem->ld_text_base || !mem->ld_text_size)
	{
		fprintf(stderr,"executable is missing a `.text' section (text_base: %lld, text_size: %lld)",mem->ld_text_base,mem->ld_text_size);
		return EACCES;
	}
	if(!mem->ld_data_base || !mem->ld_data_size)
	{
		fprintf(stderr,"executable is missing a `.data' section (data_base: %lld, data_size: %lld)",mem->ld_data_base,mem->ld_data_size);
		return EACCES;
	}
	if(!mem->ld_prog_entry)
	{
		fprintf(stderr,"program entry point not specified (prog_entry: %lld)",mem->ld_prog_entry);
		return EACCES;
	}

	//determine byte/words swapping required to execute on this host
	sim_swap_bytes = (endian_host_byte_order() != endian_target_byte_order(mem->ld_target_big_endian));
	if(sim_swap_bytes)
	{
#if 0		//FIXME: disabled until further notice...
		//cross-endian is never reliable, why this is so is beyond the scope of this comment, e-mail me for details...
		fprintf(stderr, "sim: *WARNING*: swapping bytes to match host...\n");
		fprintf(stderr, "sim: *WARNING*: swapping may break your program!\n");
		//#else
		fprintf(stderr,"binary endian does not match host endian");
		return EACCES;
#endif
	}
	sim_swap_words = (endian_host_word_order() != endian_target_word_order(mem->ld_target_big_endian));
	if(sim_swap_words)
	{
#if 0		//FIXME: disabled until further notice...
		//cross-endian is never reliable, why this is so is beyond the scope of this comment, e-mail me for details...
		fprintf(stderr, "sim: *WARNING*: swapping words to match host...\n");
		fprintf(stderr, "sim: *WARNING*: swapping may break your program!\n");
		//#else
		fprintf(stderr,"binary endian does not match host endian");
		return EACCES;
#endif
	}

	//set up a local stack pointer, this is where the argv and envp data is written into program memory
	mem->ld_stack_base = mem->ld_text_base - (409600+4096);

//	It seems fine to increase the local stack pointer here, but we don't really see a need for it either.
//	If someone can find a reason (aside from a large number of arguments and environment variables) let us know.
//	We don't know if a large number of arguments/environment is problematic (it won't work because we don't adjust the stack as is done in Tru64).
//	mem->ld_stack_base = mem->ld_text_base - (-16) - 409600;
	mem->ld_stack_base = mem->ld_text_base - (-16) - 8192;

#ifdef STACK_DEBUG
	fprintf(stderr,"ld_stack_base at: %llx\n",mem->ld_stack_base);
#endif
#if 0
	sp = ROUND_DOWN(mem->ld_stack_base - MD_MAX_ENVIRON, sizeof(MD_DOUBLE_TYPE));
#endif

//	sp = mem->ld_stack_base - MD_MAX_ENVIRON;
//	sp = mem->ld_stack_base - MD_MAX_ENVIRON * 8;
	sp = mem->ld_stack_base - 8192;

	mem->ld_stack_size = mem->ld_stack_base - sp;

	//initial stack pointer value
	mem->ld_environ_base = sp;

#ifdef STACK_DEBUG
	fprintf(stderr,"Stack pointer at: %llx\n",sp);
#endif
	//write [argc] to stack
	qword_t temp = MD_SWAPQ(argc);
	mem->mem_access(Write, sp, &temp, sizeof(qword_t));
	regs->regs_R[MD_REG_A0] = temp;
	sp += sizeof(qword_t);

#ifdef STACK_DEUBG
	fprintf(stderr,"Number of arguments %lld, Argv_addr: %lld\n",temp,argv_addr);
#endif

	//skip past argv array and NULL
	argv_addr = sp;
	regs->regs_R[MD_REG_A1] = argv_addr;
	sp = sp + (argc + 1) * sizeof(md_addr_t);

#ifdef STACK_DEBUG
	fprintf(stderr,"Envp_addr: %llx\n",sp);
#endif

	//save space for envp array and NULL
	envp_addr = sp;
	for(size_t i=0; i < envp.size(); i++)
	{
		sp += sizeof(md_addr_t);
	}
	sp += sizeof(md_addr_t);

	sp = mem->ld_stack_base - 8;

#ifdef STACK_DEBUG
	fprintf(stderr,"Start of argv data: %llx\n",sp);
#endif

	bool wroteagain = false;
	//fill in the argv pointer array and data
	for(int i=0; i<argc; i++)
	{
		//write the argv pointer array entry
		temp = MD_SWAPQ(sp);
		mem->mem_access(Write, argv_addr + i*sizeof(md_addr_t), &temp, sizeof(md_addr_t));
#ifdef STACK_DEBUG
		fprintf(stderr,"Wrote %s at %llx\n",argv[i].c_str(),temp);
#endif

		//and the data
		{
			md_addr_t addr = sp;
			char c;
			md_fault_type fault;
			for(unsigned int j = 0;j<argv[i].size();j++)
			{
				c = argv[i][j];
				fault = mem->mem_access(Write,addr++,&c,1);

				if(fault!=md_fault_none)
				{
					break;
				}
			}
			c = '\0';
			mem->mem_access(Write,addr++,&c,1);
		}
		sp += argv[i].size()+1;

		//This is how it is done on alpha (qword aligned argv)
		while((sp%8)!=0)
		{
			char c = '\0';
			mem->mem_access(Write,sp,&c,1);
			sp++;
		}
		if(!wroteagain)
		{
			wroteagain = true;
			i--;
		}
	}
	//terminate argv array with a NULL
	mem->mem_access(Write, argv_addr + argc*sizeof(md_addr_t), &null_ptr, sizeof(md_addr_t));

#ifdef ENVP_REDUCE
	//Many envp variables are unneeded and can cause discreptancies when running on different machines or with different sessions.
	//This will set some default values instead.
	std::string PWD, BLANK, OLDPWD;
	for(size_t i=0;i<envp.size();i++)
	{
		if(envp[i].find("PWD=") == 0)
		{
			PWD = envp[i];
		}
		if(envp[i].find("OLDPWD=") == 0)
		{
			OLDPWD = envp[i];
		}
		if(envp[i].find("_=") == 0)
		{
			BLANK = envp[i];
		}
	}

	envp.clear();
	envp.push_back("TERM=xterm");
	envp.push_back("SHELL=/bin/bash");
	envp.push_back("USER=msimuser");
	envp.push_back("MAIL=/var/mail/msimuser");
	envp.push_back("PATH=/usr/local/bin:/usr/local/sbin:/usr/sbin:/usr/bin:/bin:/usr/games");
	envp.push_back("LANG=en_US.ISO8859-1");
//	envp.push_back("LANG=en_US.UTF-8");
	envp.push_back("HOME=/usr/users/msimuser");
	envp.push_back("LOGNAME=msimuser");
	envp.push_back("SHLVL=1");
	envp.push_back("HISTCONTRL=ignoreboth");
//	envp.push_back("LESSOPEN=| /usr/bin/lesspipe %s");
//	envp.push_back("LESSCLOSE=/usr/bin/lesspipe %s %s");
#ifdef FORCE_CONSISTENT
	envp.push_back("_=../sim-outorder");
	envp.push_back("PWD=/usr/users/msimuser");
//	envp.push_back("OLDPWD=/usr/users/msimuser");
#else
	if(!PWD.empty())
	{
		envp.push_back(PWD);
	}
	if(!OLDPWD.empty())
	{
		envp.push_back(OLDPWD);
	}
	if(!BLANK.empty())
	{
		envp.push_back(BLANK);
	}
#endif
#ifdef STACK_DEBUG
	fprintf(stderr,"envp has been shrunk to a reduced set --> ");
#endif
#endif

#ifdef STACK_DEBUG
	fprintf(stderr,"Start of envp data: %llx, envp vars: %ld\n",sp,envp.size());
#endif

	size_t i;
	//write envp pointer array and data to stack
	for(i = 0; i < envp.size(); i++)
	{
		//write the envp pointer array entry
		temp = MD_SWAPQ(sp);
		mem->mem_access(Write, envp_addr + i*sizeof(md_addr_t), &temp, sizeof(md_addr_t));
		//and the data
#ifdef STACK_DEBUG
		fprintf(stderr,"Wrote %s at %llx\n",envp[i].c_str(),sp);
#endif
		mem->mem_strcpy(Write, sp, envp[i]);
		sp += envp[i].size() + 1;

		//argv is qword aligned, we never checked envp but it probably is too
		while((sp%8)!=0)
		{
			char c = '\0';
			mem->mem_access(Write,sp,&c,1);
			sp++;
		}
	}
	//terminate the envp array with a NULL
	mem->mem_access(Write, envp_addr + i*sizeof(md_addr_t), &null_ptr, sizeof(md_addr_t));

	//These are values that appear in an alpha debugger at runtime.
	md_addr_t other_name = mem->ld_stack_base - 8;
	md_addr_t bizarre_val = 0x3e9;
	mem->mem_access(Write, envp_addr + (i+1)*sizeof(md_addr_t), &bizarre_val, sizeof(md_addr_t));
	mem->mem_access(Write, envp_addr + (i+2)*sizeof(md_addr_t), &other_name, sizeof(md_addr_t));

	//did we tromp off the top of the stack?
	if(sp > mem->ld_text_base)
	{
		//we did, indicate to the user that MD_MAX_ENVIRON must be increased, alternatively, you can use a smaller environment, or fewer command line arguments
		fprintf(stderr,"environment overflow, increase MD_MAX_ENVIRON in alpha.h");
		return E2BIG;
	}

	//initialize the bottom of heap to top of data segment
	mem->ld_brk_point = ROUND_UP(mem->ld_data_base + mem->ld_data_size, MD_PAGE_SIZE);

	//set initial minimum stack pointer value to initial stack value
	mem->ld_stack_min = regs->regs_R[MD_REG_SP];

	regs->regs_R[MD_REG_SP] = mem->ld_environ_base;
	regs->regs_PC = mem->ld_prog_entry;

	debug("mem->ld_text_base: 0x%08x  mem->ld_text_size: 0x%08x", mem->ld_text_base, mem->ld_text_size);
	debug("mem->ld_data_base: 0x%08x  mem->ld_data_size: 0x%08x", mem->ld_data_base, mem->ld_data_size);
	debug("mem->ld_stack_base: 0x%08x  mem->ld_stack_size: 0x%08x",	mem->ld_stack_base, mem->ld_stack_size);
	debug("mem->ld_prog_entry: 0x%08x", mem->ld_prog_entry);
	return 0;
}
