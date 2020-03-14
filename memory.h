/* memory.h - flat memory space interfaces */

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

#ifndef MEMORY_H
#define MEMORY_H

#include<cstdio>
#include<string>
#include<vector>
#include<iostream>
#include<sys/types.h>
#include<unistd.h>

#include"host.h"
#include"misc.h"
#include"machine.h"
#include"options.h"
#include"stats.h"

//number of entries in page translation hash table (must be power-of-two)
#define MEM_PTAB_SIZE		(32*1024)
#define MEM_LOG_PTAB_SIZE	15

//memory access command
enum mem_cmd
{
	Read,			//read memory from target (simulated prog) to host
	Write			//write memory from host (simulator) to target
};

//page table entry
class mem_pte_t
{
	public:
		mem_pte_t()
		: next(NULL), tag(static_cast<md_addr_t>(0)), page(NULL)
		{}
		~mem_pte_t()
		{
			delete [] page;
			page = NULL;
			next = NULL;
		}
		mem_pte_t(mem_pte_t *next, md_addr_t tag, byte_t *page)
		: next(next), tag(tag), page(page)
		{}

		mem_pte_t *next;		//next translation in this bucket
		md_addr_t tag;			//virtual page number tag
		byte_t *page;			//page pointer
};

class mmap_t
{
	public:
		md_addr_t base_address;
		md_addr_t size;
		void * mapping;
		int protections;
		int flags;

		bool is_within(md_addr_t addr);
		bool bounds_check(md_addr_t addr, md_addr_t len);
		void * translate(md_addr_t addr);
};

//memory object
class mem_t
{
	public:
		mem_t(char *name);
		mem_t(const mem_t & source);
		~mem_t();

		//memory state
		std::string name;			//name of this memory space
		std::vector<mem_pte_t *> ptab;		//inverted page table
		std::vector<mmap_t> memory_map;		//list of active memory mappings
		std::vector<mmap_t> internal_map;	//Non-translated (internal) memory mappings

		//memory statistics
		counter_t page_count;			//total number of pages allocated
		counter_t ptab_misses;			//total first level page table misses
		counter_t ptab_accesses;		//total page table accesses

		int context_id; 			//the context id for this memory space

		//memory control values (set when executable is loaded)
		md_addr_t ld_text_base;			//program text (code) segment base
		unsigned long long ld_text_size;	//program text (code) size in bytes
		md_addr_t ld_data_base;			//program initialized data segment base
		md_addr_t ld_brk_point;			//top of the data segment
		unsigned long long ld_data_size;	//program initialized ".data" and uninitialized ".bss" size in bytes
		md_addr_t ld_stack_base;		//program stack segment base (highest address in stack)
		unsigned long long ld_stack_size;	//program initial stack size
		md_addr_t ld_stack_min;			//lowest address accessed on the stack
		std::string ld_prog_fname;		//program file name
		md_addr_t ld_prog_entry;		//program entry point (initial PC)
		md_addr_t ld_environ_base;		//program environment base address address
		int ld_target_big_endian;		//target executable endian-ness, non-zero if big endian

		//generic memory access function, it's safe because alignments and permissions
		//are checked, handles any natural transfer sizes; note, faults out if nbytes
		//is not a power-of-two or larger then MD_PAGE_SIZE
		md_fault_type mem_access(mem_cmd cmd,	//Read (from sim mem) or Write
			md_addr_t addr,			//target address to access
			void *vp,			//host memory address to access
			int nbytes);			//number of bytes to access

		//See "mem_access", does not fail alignment checks
		md_fault_type mem_access_direct(mem_cmd cmd,	//Read (from sim mem) or Write
			md_addr_t addr,				//target address to access
			void *vp,				//host memory address to access
			int nbytes);				//number of bytes to access

		//copy a '\0' terminated string to/from simulated memory space, returns
		//the number of bytes copied, returns any fault encountered
		md_fault_type mem_strcpy(mem_cmd cmd,	//Read (from sim mem) or Write
			md_addr_t addr,			//target address to access
			std::string & s);		//host memory string buffer

		//copy NBYTES to/from simulated memory space, returns any faults
		md_fault_type mem_bcopy(mem_cmd cmd,	//Read (from sim mem) or Write
			md_addr_t addr,			//target address to access
			void *vp,			//host memory address to access
			int nbytes);			//number of bytes to access

		//copy NBYTES to/from simulated memory space, NBYTES must be a multiple of 4
		//bytes, this function is faster than mem_bcopy(), returns any faults encountered
		md_fault_type mem_bcopy4(mem_cmd cmd,	//Read (from sim mem) or Write
			md_addr_t addr,			//target address to access
			void *vp,			//host memory address to access
			int nbytes);			//number of bytes to access

		//zero out NBYTES of simulated memory, returns any faults encountered
		md_fault_type mem_bzero(md_addr_t addr,	//target address to access
			int nbytes);			//number of bytes to clear

		//register memory system-specific statistics, takes stat database
		void print_stats(FILE * stream);

		//dump a block of memory, returns any faults encountered
		md_fault_type mem_dump(md_addr_t addr,	//target address to dump
			int len,			//number bytes to dump
			FILE *stream);			//output stream

		//translate address ADDR in memory space MEM, returns pointer to host page
		byte_t * mem_translate(md_addr_t addr);	//virtual address to translate

		//allocate a memory page
		void mem_newpage(md_addr_t addr);	//virtual address to allocate

#define OSF_MAP_ANON		0x0010		//Don't use a file
#define OSF_MAP_FIXED		0x0100		//Interpret addr exactly
#define OSF_MAP_SHARED		0x0001		//Changes made to the mapping alter the original mapping.
#define OSF_MAP_INHERIT		0x0400		//Retain this mapping after exec (internal mappings are just deallocated (not zeroed))

		//Attempt a memory mapping
		md_addr_t mem_map(md_addr_t simulated_addr, md_gpr_t fd, md_addr_t offset, md_addr_t len, int prot, int flags);

		//Remove a memory mapping
		int mem_unmap(md_addr_t simulated_addr);

		//Bounds checking algorithm for mem_map
		bool bounds_verify(std::vector<mmap_t> & source, md_addr_t addr, md_addr_t len);

		//Address generation for mem_map
		md_addr_t acquire_address(md_addr_t addr, md_addr_t len);

		//Flush memory mappings when exec is called (OSF_MAP_INHERIT)
		void exec_flush();
};

std::ostream & operator << (std::ostream & out, const mem_t & source);
std::ostream & operator << (std::ostream & out, const mem_t * source);
std::istream & operator >> (std::istream & in, mem_t & target);
std::istream & operator >> (std::istream & in, mem_t * target);

//virtual to host page translation macros

//compute page table set
#define MEM_PTAB_SET(ADDR)	(((ADDR) >> MD_LOG_PAGE_SIZE) & (MEM_PTAB_SIZE - 1))

//compute page table tag
#define MEM_PTAB_TAG(ADDR)	((ADDR) >> (MD_LOG_PAGE_SIZE + MEM_LOG_PTAB_SIZE))

//convert a pte entry at idx to a block address
#define MEM_PTE_ADDR(PTE, IDX)						\
	(((PTE)->tag << (MD_LOG_PAGE_SIZE + MEM_LOG_PTAB_SIZE))		\
		| ((IDX) << MD_LOG_PAGE_SIZE))

//locate host page for virtual address ADDR, returns NULL if unallocated
#define MEM_PAGE(MEM, ADDR)								\
	(/* first attempt to hit in first entry, otherwise call xlation fn */		\
		((MEM)->ptab[MEM_PTAB_SET(ADDR)]					\
			&& (MEM)->ptab[MEM_PTAB_SET(ADDR)]->tag == MEM_PTAB_TAG(ADDR))	\
			? (/* hit - return the page address on host */			\
			(MEM)->ptab_accesses++,						\
			(MEM)->ptab[MEM_PTAB_SET(ADDR)]->page)				\
			: (/* first level miss - call the translation helper function */\
			(MEM)->mem_translate((ADDR))))

//compute address of access within a host page
#define MEM_OFFSET(ADDR)	((ADDR) & (MD_PAGE_SIZE - 1))

//memory tickle function, allocates pages when they are first written
#define MEM_TICKLE(MEM, ADDR)						\
	(!MEM_PAGE(MEM, ADDR)						\
	? (/* allocate page at address ADDR */				\
	(MEM)->mem_newpage(ADDR))					\
	: (/* nada... */ (void)0))

//memory page iterator
#define MEM_FORALL(MEM, ITER, PTE)					\
	for ((ITER)=0; (ITER) < MEM_PTAB_SIZE; (ITER)++)		\
		for ((PTE)=(MEM)->ptab[i]; (PTE) != NULL; (PTE)=(PTE)->next)

//memory accessors macros, fast but difficult to debug...

//safe version, works only with scalar types
//FIXME: write a more efficient GNU C expression for this...
#define MEM_READ(MEM, ADDR, TYPE)						\
	(MEM_PAGE(MEM, (md_addr_t)(ADDR))					\
	? *((TYPE *)(MEM_PAGE(MEM, (md_addr_t)(ADDR)) + MEM_OFFSET(ADDR)))	\
	: /* page not yet allocated, return zero value */ 0)

//unsafe version, works with any type
#define __UNCHK_MEM_READ(MEM, ADDR, TYPE)				\
	(*((TYPE *)(MEM_PAGE(MEM, (md_addr_t)(ADDR)) + MEM_OFFSET(ADDR))))

//safe version, works only with scalar types
//FIXME: write a more efficient GNU C expression for this...
#define MEM_WRITE(MEM, ADDR, TYPE, VAL)						\
	(MEM_TICKLE(MEM, (md_addr_t)(ADDR)),					\
	*((TYPE *)(MEM_PAGE(MEM, (md_addr_t)(ADDR)) + MEM_OFFSET(ADDR))) = (VAL))
      
//unsafe version, works with any type
#define __UNCHK_MEM_WRITE(MEM, ADDR, TYPE, VAL)				\
	(*((TYPE *)(MEM_PAGE(MEM, (md_addr_t)(ADDR)) + MEM_OFFSET(ADDR))) = (VAL))


//fast memory accessor macros, typed versions
#define MEM_READ_BYTE(MEM, ADDR)	MEM_READ(MEM, ADDR, byte_t)
#define MEM_READ_SBYTE(MEM, ADDR)	MEM_READ(MEM, ADDR, sbyte_t)
#define MEM_READ_HALF(MEM, ADDR)	MD_SWAPH(MEM_READ(MEM, ADDR, half_t))
#define MEM_READ_SHALF(MEM, ADDR)	MD_SWAPH(MEM_READ(MEM, ADDR, shalf_t))
#define MEM_READ_WORD(MEM, ADDR)	MD_SWAPW(MEM_READ(MEM, ADDR, word_t))
#define MEM_READ_SWORD(MEM, ADDR)	MD_SWAPW(MEM_READ(MEM, ADDR, sword_t))

#define MEM_READ_QWORD(MEM, ADDR)	MD_SWAPQ(MEM_READ(MEM, ADDR, qword_t))
#define MEM_READ_SQWORD(MEM, ADDR)	MD_SWAPQ(MEM_READ(MEM, ADDR, sqword_t))

#define MEM_WRITE_BYTE(MEM, ADDR, VAL)	MEM_WRITE(MEM, ADDR, byte_t, VAL)
#define MEM_WRITE_SBYTE(MEM, ADDR, VAL)	MEM_WRITE(MEM, ADDR, sbyte_t, VAL)
#define MEM_WRITE_HALF(MEM, ADDR, VAL)	MEM_WRITE(MEM, ADDR, half_t, MD_SWAPH(VAL))
#define MEM_WRITE_SHALF(MEM, ADDR, VAL)	MEM_WRITE(MEM, ADDR, shalf_t, MD_SWAPH(VAL))
#define MEM_WRITE_WORD(MEM, ADDR, VAL)	MEM_WRITE(MEM, ADDR, word_t, MD_SWAPW(VAL))
#define MEM_WRITE_SWORD(MEM, ADDR, VAL)	MEM_WRITE(MEM, ADDR, sword_t, MD_SWAPW(VAL))
#define MEM_WRITE_SFLOAT(MEM, ADDR, VAL) MEM_WRITE(MEM, ADDR, sfloat_t, MD_SWAPW(VAL))
#define MEM_WRITE_DFLOAT(MEM, ADDR, VAL) MEM_WRITE(MEM, ADDR, dfloat_t, MD_SWAPQ(VAL))

#define MEM_WRITE_QWORD(MEM, ADDR, VAL)	MEM_WRITE(MEM, ADDR, qword_t, MD_SWAPQ(VAL))
#define MEM_WRITE_SQWORD(MEM, ADDR, VAL) MEM_WRITE(MEM, ADDR, sqword_t, MD_SWAPQ(VAL))

#endif // MEMORY_H
