/* memory.c - flat memory space routines */

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
#include<iomanip>

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "options.h"
#include "stats.h"
#include "memory.h"
#include<sys/mman.h>

//translate address ADDR in memory space MEM, returns pointer to host page
byte_t * mem_t::mem_translate(md_addr_t addr)		//virtual address to translate
{
	//got here via a first level miss in the page tables
	ptab_misses++;
	ptab_accesses++;

	//locate accessed PTE
	for(mem_pte_t *prev=NULL, *pte=ptab[MEM_PTAB_SET(addr)]; pte != NULL; prev=pte, pte=pte->next)
	{
		if(pte->tag == MEM_PTAB_TAG(addr))
		{
			//move this PTE to head of the bucket list
			if(prev)
			{
				prev->next = pte->next;
				pte->next = ptab[MEM_PTAB_SET(addr)];
				ptab[MEM_PTAB_SET(addr)] = pte;
			}
			return pte->page;
		}
	}
	//no translation found, return NULL
	return NULL;
}

//allocate a memory page
void mem_t::mem_newpage(md_addr_t addr)			//virtual address to allocate
{
	byte_t *page = new byte_t[MD_PAGE_SIZE];
	memset(page,0,MD_PAGE_SIZE);

	//generate a new PTE
	mem_pte_t *pte = new mem_pte_t(ptab[MEM_PTAB_SET(addr)],MEM_PTAB_TAG(addr),page);

	//insert PTE into inverted hash table
	ptab[MEM_PTAB_SET(addr)] = pte;

	//one more page allocated
	page_count++;
}

mem_t::mem_t(char *name)
: name(name), ptab(MEM_PTAB_SIZE,static_cast<mem_pte_t *>(NULL)), page_count(0), ptab_misses(0), ptab_accesses(0)
{}

mem_t::mem_t(const mem_t & source)
: name(source.name), ptab(MEM_PTAB_SIZE,static_cast<mem_pte_t *>(NULL)), memory_map(source.memory_map), internal_map(source.internal_map),
page_count(source.page_count), ptab_misses(source.ptab_misses), ptab_accesses(source.ptab_accesses),
context_id(source.context_id), ld_text_base(source.ld_text_base), ld_text_size(source.ld_text_size), ld_data_base(source.ld_data_base), ld_brk_point(source.ld_brk_point),
ld_data_size(source.ld_data_size), ld_stack_base(source.ld_stack_base), ld_stack_size(source.ld_stack_size), ld_stack_min(source.ld_stack_min), ld_prog_fname(source.ld_prog_fname),
ld_prog_entry(source.ld_prog_entry), ld_environ_base(source.ld_environ_base), ld_target_big_endian(source.ld_target_big_endian)
{
	//FIXME: This is in reverse compared to the original, do we care is the question.
	for(unsigned int i = 0;i<source.ptab.size();i++)
	{
		for(mem_pte_t *pte=source.ptab[i];pte;pte=pte->next)
		{
			byte_t *page = new byte_t[MD_PAGE_SIZE];
			memcpy(page,pte->page,MD_PAGE_SIZE);
			mem_pte_t *newpte = new mem_pte_t(ptab[i], pte->tag, page);
			ptab[i] = newpte;
		}
	}
	page_count = source.page_count;
}

mem_t::~mem_t()
{
	for(unsigned int i=0;i<ptab.size();i++)
	{
		while(ptab[i])
		{
			mem_pte_t *temp = ptab[i]->next;
			delete ptab[i];
			ptab[i] = temp;
		}
	}
}

std::ostream & operator << (std::ostream & out, const mem_t * source)
{
	return operator<<(out,*source);
}

std::ostream & operator << (std::ostream & out, const mem_t & source)
{
	out << "/* writing `" << (int)source.page_count << "' memory pages... */" << std::endl;
	out << "(" << source.page_count << ", 0x" << std::hex << source.ld_brk_point << ", 0x" << std::hex << source.ld_stack_min << ")" << std::endl;

	out << std::endl;
	out << "/* text segment specifiers (base & size) */" << std::endl;
	out << "(0x" << std::hex << source.ld_text_base << ", " << std::dec << source.ld_text_size << ")" << std::endl;

	out << std::endl;
	out << "/* data segment specifiers (base & size) */" << std::endl;
	out << "(0x" << std::hex << source.ld_data_base << ", " << std::dec << source.ld_data_size << ")" << std::endl;

	out << std::endl;
	out << "/* stack segment specifiers (base & size) */" << std::endl;
	out << "(0x" << std::hex << source.ld_stack_base << ", " << std::dec << source.ld_stack_size << ")" << std::endl;

	out << std::endl;
	for(unsigned int i = 0;i<source.ptab.size();i++)
	{
		for(mem_pte_t *pte=source.ptab[i];pte;pte=pte->next)
		{
			out << "(0x";
			unsigned long long mem_pte_addr((pte->tag << (MD_LOG_PAGE_SIZE + MEM_LOG_PTAB_SIZE)) | (i << MD_LOG_PAGE_SIZE));
			out << std::hex << mem_pte_addr << ", ";
			out << "{" << std::dec << MD_PAGE_SIZE << "}<\n";

			bool newline(false);
			for(unsigned int j=0;j<MD_PAGE_SIZE;j++)
			{
				if((j!=0) && ((j%32)==0))
				{
					out << "\n";
					newline = true;
				}
				unsigned int toprint = (unsigned int)pte->page[j];
				out << std::hex << (toprint/16) << std::hex << (toprint%16);
			}
			if(!newline)
			{
				out << "\n";
			}
			out << ">)\n" << std::dec << std::endl;
		}
	}
	return out;
}

std::istream & operator >> (std::istream & in, mem_t * target)
{
	return operator>>(in,*target);
}

std::istream & operator >> (std::istream & in, mem_t & target)
{
	std::string mem_header("' memory pages... */");
	std::string text_header("/* text segment specifiers (base & size) */");
	std::string data_header("/* data segment specifiers (base & size) */");
	std::string stack_header("/* stack segment specifiers (base & size) */");

	std::string buf;
	char c_buf;
	in >> buf >> buf >> c_buf >> target.page_count;
	getline(in,buf);
	if(buf!=mem_header)
	{
		std::cerr << "Failed reading memory header, read: " << buf << std::endl;
		std::cerr << "Wanted to read: " << mem_header << std::endl;
		exit(-1);
	}
	in >> c_buf >> target.page_count >> c_buf >> std::hex >> target.ld_brk_point >> c_buf >> std::hex >> target.ld_stack_min;
	getline(in,buf);
	getline(in,buf);
	getline(in,buf);
	if(buf!=text_header)
	{
		std::cerr << "Failed reading text header, read: " << buf << std::endl;
		std::cerr << "Wanted to read: " << text_header << std::endl;
		exit(-1);
	}
	in >> c_buf >> std::hex >> target.ld_text_base >> c_buf >> std::dec >> target.ld_text_size;
	getline(in,buf);
	getline(in,buf);
	getline(in,buf);
	if(buf!=data_header)
	{
		std::cerr << "Failed reading data header, read: " << buf << std::endl;
		std::cerr << "Wanted to read: " << data_header << std::endl;
		exit(-1);
	}
	in >> c_buf >> std::hex >> target.ld_data_base >> c_buf >> std::dec >> target.ld_data_size;
	getline(in,buf);
	getline(in,buf);
	getline(in,buf);
	if(buf!=stack_header)
	{
		std::cerr << "Failed reading stack header, read: " << buf << std::endl;
		std::cerr << "Wanted to read: " << stack_header << std::endl;
		exit(-1);
	}
	in >> c_buf >> std::hex >> target.ld_stack_base >> c_buf >> std::dec >> target.ld_stack_size;
	getline(in,buf);
	getline(in,buf);

	int page_count(target.page_count);
	for(int i=0; i < page_count; i++)
	{
		in >> c_buf;
		unsigned long long page_addr;
		in >> std::hex >> page_addr >> c_buf >> c_buf;
		unsigned long long page_size;
		in >> std::dec >> page_size;
		if(page_size!=MD_PAGE_SIZE)
		{
			std::cerr << page_size << " does not match " << MD_PAGE_SIZE << std::endl;
			exit(-1);
		}
		getline(in,buf);

		for(int j=0; j < MD_PAGE_SIZE; j++)
                {
			char t[3];
			t[2] = NULL;
			in >> t[0] >> t[1];
			unsigned int val = strtol(t,NULL,16);
                        //unchecked access...
                        MEM_WRITE_BYTE(&target, page_addr, val);
                        page_addr++;
                }
		in >> c_buf;
		if(c_buf!='>')
		{
			std::cerr << "Failed finding end of memory page" << std::endl;
			exit(-1);
		}
		getline(in,buf);
		getline(in,buf);
        }
	target.page_count = page_count;
	return in;
}

md_fault_type mem_t::mem_access(mem_cmd cmd,		//Read (from sim mem) or Write
	md_addr_t addr,					//target address to access
	void *vp,					//host memory address to access
	int nbytes)					//number of bytes to access
{
	//check alignments: size || max size
	if((nbytes & (nbytes-1)) != 0 || (nbytes > MD_PAGE_SIZE))
		return md_fault_access;

	//check natural alignment
	if((addr & (nbytes-1)) != 0)
	{
		return md_fault_alignment;
	}

	if(!memory_map.empty())
	{
		for(size_t i=0;i<memory_map.size();i++)
		{
			if(memory_map[i].is_within(addr))
			{
				if(!memory_map[i].bounds_check(addr,nbytes))
				{
					return md_fault_segfault;
				}
				byte_t *p = (byte_t *)vp;
				byte_t *dest = (byte_t *)memory_map[i].translate(addr);
				if(cmd == Read)
				{
					if(!(memory_map[i].protections & PROT_READ))
					{
						return md_fault_access;
					}
					while(nbytes-- > 0)
					{
						*((byte_t *)p) = *dest;
						p += sizeof(byte_t);
						dest += sizeof(byte_t);
					}
				}
				else
				{
					if(!(memory_map[i].protections & PROT_WRITE))
					{
						return md_fault_access;
					}
					while(nbytes-- > 0)
					{
						*dest = *((byte_t *)p);
						p += sizeof(byte_t);
						dest += sizeof(byte_t);
					}
				}
				return md_fault_none;
			}
		}
	}

	//perform the copy
	byte_t *p = (byte_t *)vp;
	if(cmd == Read)
	{
		while(nbytes-- > 0)
		{
			*((byte_t *)p) = MEM_READ_BYTE(this, addr);
			p += sizeof(byte_t);
			addr += sizeof(byte_t);
		}
	}
	else
	{
		while(nbytes-- > 0)
		{
			MEM_WRITE_BYTE(this, addr, *((byte_t *)p));
			p += sizeof(byte_t);
			addr += sizeof(byte_t);
		}
	}
	//no fault...
	return md_fault_none;
}

md_fault_type mem_t::mem_access_direct(mem_cmd cmd,	//Read (from sim mem) or Write
	md_addr_t addr,					//target address to access
	void *vp,					//host memory address to access
	int nbytes)					//number of bytes to access
{
	//check alignments: size || max size
	if((nbytes & (nbytes-1)) != 0 || (nbytes > MD_PAGE_SIZE))
		return md_fault_access;

	if(!memory_map.empty())
	{
		for(size_t i=0;i<memory_map.size();i++)
		{
			if(memory_map[i].is_within(addr))
			{
				if(!memory_map[i].bounds_check(addr,nbytes))
				{
					return md_fault_segfault;
				}
				byte_t *p = (byte_t *)vp;
				byte_t *dest = (byte_t *)memory_map[i].translate(addr);
				if(cmd == Read)
				{
					if(!(memory_map[i].protections & PROT_READ))
					{
						return md_fault_access;
					}
					while(nbytes-- > 0)
					{
						*((byte_t *)p) = *dest;
						p += sizeof(byte_t);
						dest += sizeof(byte_t);
					}
				}
				else
				{
					if(!(memory_map[i].protections & PROT_WRITE))
					{
						return md_fault_access;
					}
					while(nbytes-- > 0)
					{
						*dest = *((byte_t *)p);
						p += sizeof(byte_t);
						dest += sizeof(byte_t);
					}
				}
				return md_fault_none;
			}
		}
	}

	//perform the copy
	byte_t *p = (byte_t *)vp;
	if(cmd == Read)
	{
		while(nbytes-- > 0)
		{
			*((byte_t *)p) = MEM_READ_BYTE(this, addr);
			p += sizeof(byte_t);
			addr += sizeof(byte_t);
		}
	}
	else
	{
		while(nbytes-- > 0)
		{
			MEM_WRITE_BYTE(this, addr, *((byte_t *)p));
			p += sizeof(byte_t);
			addr += sizeof(byte_t);
		}
	}

	//no fault...
	return md_fault_none;
}

//Print statistics
void mem_t::print_stats(FILE * stream)
{
	fprintf(stream,"%s->ld_text_base    0x%llx # program text (code) segment base\n",        name.c_str(), ld_text_base);
	fprintf(stream,"%s->ld_text_size    0x%llx # program text (code) size in bytes\n",       name.c_str(), ld_text_size);
	fprintf(stream,"%s->ld_data_base    0x%llx # program initialized data segment base\n",   name.c_str(), ld_data_base);
	fprintf(stream,"%s->ld_data_size    0x%llx # program init data + bss size in bytes\n",   name.c_str(), ld_data_size);
	fprintf(stream,"%s->ld_stack_base   0x%llx # program stack segment base (highest address in stack)\n",        name.c_str(), ld_stack_base);
#if 0	//FIXME: broken...
	fprintf(stream,"%s->ld_stack_min    0x%llx # program stack segment lowest address\n",    name.c_str(), ld_stack_base);
#endif
	fprintf(stream,"%s->ld_stack_base    0x%llx # program initial stack size\n",             name.c_str(), ld_stack_size);
	fprintf(stream,"%s->ld_prog_entry    0x%llx # program entry point (initial PC)\n",       name.c_str(), ld_prog_entry);
	fprintf(stream,"%s->ld_environ_base  0x%llx # program environment base address\n",       name.c_str(), ld_environ_base);
	fprintf(stream,"%s->ld_target_endian \"%s\" # program endian-ness\n",                    name.c_str(), ld_target_big_endian ? "Big" : "Little");


	fprintf(stream,"%s.page_count     %lld # total number of pages allocated\n",     name.c_str(), page_count);
	fprintf(stream,"%s.page_mem      %lldK # total size of memory pages allocated\n",name.c_str(), ((page_count*MD_PAGE_SIZE)/1024));
	fprintf(stream,"%s.ptab_misses    %lld # total first level page table misses\n", name.c_str(), ptab_misses);
	fprintf(stream,"%s.ptab_accesses  %lld # total page table accesses\n",           name.c_str(), ptab_accesses);
	if(ptab_accesses)
	{
		fprintf(stream,"%s.ptab_miss_rate %f # first level page table miss rate\n",      name.c_str(), (double)ptab_misses/(double)ptab_accesses);
	}
	else
	{
		fprintf(stream,"%s.ptab_miss_rate %s # first level page table miss rate\n",      name.c_str(), "<error: divide by zero>");
	}
}

md_fault_type mem_t::mem_dump(md_addr_t addr,	//target address to dump
	int len,				//number bytes to dump
	FILE *stream)				//output stream
{
	if(!stream)
		stream = stderr;

	int data;
	addr &= ~sizeof(word_t);
	len = (len + (sizeof(word_t) - 1)) & ~sizeof(word_t);
	while(len-- > 0)
	{
		md_fault_type fault = mem_access(Read, addr, &data, sizeof(word_t));
		if(fault != md_fault_none)
			return fault;

		myfprintf(stream, "0x%08p: %08x\n", addr, data);
		addr += sizeof(word_t);
	}

	//no faults...
	return md_fault_none;
}

//copy a '\0' terminated string to/from simulated memory space, returns the number of bytes copied, returns any fault encountered
md_fault_type mem_t::mem_strcpy(mem_cmd cmd,	//Read (from sim mem) or Write
	md_addr_t addr,				//target address to access
	std::string & s)
{
	char c = '0';
	md_fault_type fault;

	switch(cmd)
	{
	case Read:
		s.clear();
		//copy until string terminator ('\0') is encountered
		while(c)
		{
			fault = mem_access(Read, addr++, &c, 1);
			if(fault != md_fault_none)
				return fault;
			s += c;
		}
		break;
	case Write:
		//copy until string terminator ('\0') is encountered
		{
			size_t i = 0;
			do
			{
				c = s[i++];
				fault = mem_access(Write, addr++, &c, 1);
				if(fault != md_fault_none)
					return fault;
			} while(c);
		}
		break;
	default:
		return md_fault_internal;
	}

	//no faults...
	return md_fault_none;
}

//copy NBYTES to/from simulated memory space, returns any faults
md_fault_type mem_t::mem_bcopy(mem_cmd cmd,	//Read (from sim mem) or Write
	md_addr_t addr,				//target address to access
	void *vp,				//host memory address to access
	int nbytes)
{
	byte_t *p = (byte_t *)vp;
	md_fault_type fault;

	//copy NBYTES bytes to/from simulator memory
	while(nbytes-- > 0)
	{
		fault = mem_access(cmd, addr++, p++, 1);
		if(fault != md_fault_none)
			return fault;
	}

	//no faults...
	return md_fault_none;
}

//copy NBYTES to/from simulated memory space, NBYTES must be a multiple of 4 bytes,
//this function is faster than mem_bcopy(), returns any faults encountered
md_fault_type mem_t::mem_bcopy4(mem_cmd cmd,	//Read (from sim mem) or Write
	md_addr_t addr,				//target address to access
	void *vp,				//host memory address to access
	int nbytes)
{
	byte_t *p = (byte_t *)vp;
	int words = nbytes >> 2;		//note: nbytes % 2 == 0 is assumed
	md_fault_type fault;

	while(words-- > 0)
	{
		fault = mem_access(cmd, addr, p, sizeof(word_t));
		if(fault != md_fault_none)
			return fault;
		addr += sizeof(word_t);
		p += sizeof(word_t);
	}

	//no faults...
	return md_fault_none;
}

//zero out NBYTES of simulated memory, returns any faults encountered
md_fault_type mem_t::mem_bzero(md_addr_t addr,	//target address to access
	int nbytes)
{
	byte_t c = 0;
	md_fault_type fault;

	//zero out NBYTES of simulator memory
	while(nbytes-- > 0)
	{
		fault = mem_access(Write, addr++, &c, 1);
		if(fault != md_fault_none)
			return fault;
	}

	//no faults...
	return md_fault_none;
}

void mem_t::exec_flush()
{
	for(size_t i=0;i<memory_map.size();i++)
	{
		if(!(memory_map[i].flags & OSF_MAP_INHERIT))
		{
			munmap(memory_map[i].mapping,memory_map[i].size);
			memory_map.erase(memory_map.begin()+i);
			i--;
		}
	}
	for(size_t i=0;i<internal_map.size();i++)
	{
		if(!(internal_map[i].flags & OSF_MAP_INHERIT))
		{
			internal_map.erase(internal_map.begin()+i);
			i--;
		}
	}

	//Kill actual pages
	for(unsigned int i = 0;i<ptab.size();i++)
	{
		mem_pte_t *pte = ptab[i];
		mem_pte_t *prev = NULL;
		while(pte)
		{
			md_addr_t addr = MEM_PTE_ADDR(pte,i);
			if(!bounds_verify(memory_map,addr,1) || !bounds_verify(internal_map,addr,1))
			{
				prev = pte;
				pte = pte->next;
			}
			else
			{
				if(prev==NULL)
				{
					pte = pte->next;
					delete ptab[i];
					ptab[i] = pte;
				}
				else
				{
					prev->next = pte->next;
					delete pte;
					pte = prev->next;
				}
				page_count--;
			}
		}
	}
}

md_addr_t mem_t::acquire_address(md_addr_t addr, md_addr_t len)
{
#ifdef __LP64__
	static md_addr_t base_addr = 0x050000000000;
#else
	static md_addr_t base_addr = 0x50000000;
#endif
	if(addr==0)
	{
		addr = base_addr;
		while(!bounds_verify(memory_map,addr,len) || !bounds_verify(internal_map,addr,len))
		{
			addr += MD_PAGE_SIZE;
		}
		base_addr = addr + len + MD_PAGE_SIZE - 1;
		base_addr &= ~(MD_PAGE_SIZE-1);
		return addr;
	}

	if(!bounds_verify(memory_map,addr,len) || !bounds_verify(internal_map,addr,len))
	{
		return acquire_address(addr+MD_PAGE_SIZE,len);
//		return acquire_address(0,len);
	}
	return addr;
}

bool mem_t::bounds_verify(std::vector<mmap_t> & source, md_addr_t addr, md_addr_t len)
{
	mmap_t temp;
	temp.base_address = addr;
	temp.size = len;

	for(size_t i=0;i<source.size();i++)
	{
		md_addr_t bound = source[i].base_address + source[i].size + MD_PAGE_SIZE - 1;
		bound &= ~(MD_PAGE_SIZE - 1);
		for(md_addr_t check = source[i].base_address; check < bound; check += MD_PAGE_SIZE)
		{
			if(temp.is_within(check))
			{
				return false;
			}
		}
	}
	return true;
}

//attempt a memory mapping
md_addr_t mem_t::mem_map(md_addr_t simulated_addr, md_gpr_t fd, md_addr_t offset, md_addr_t len, int prot, int flags)
{
	//Ensure that len is a quad_word bounary
	if(len & 0x7)
	{
		len += 8 - (len % 8);
	}

	mmap_t temp;

	temp.base_address = acquire_address(simulated_addr,len);
	temp.mapping = (void *)temp.base_address;
	temp.size = len;
	temp.protections = prot;
	temp.flags = flags;

	if(flags & OSF_MAP_FIXED)
	{
		temp.base_address = simulated_addr;
		temp.mapping = (void *)temp.base_address;
		internal_map.insert(internal_map.begin(),temp);

		//We have to handle the fact that the fixed mapping unmaps some pages.
		//Insertion at the front makes sure it sees this one but if it is unmapped, the old data (which should have unmapped) is present.

		if(fd != (md_gpr_t)-1)
		{
			char * buf = new char[len];
			unsigned long long count = 0;
			lseek(fd,offset,SEEK_SET);
			while(count < len)
			{
				int read_result = read(fd,buf+count,len-count);
				if(read_result==-1)
				{
					break;
				}
				count+=read_result;
			}
			mem_bcopy(Write,temp.base_address,buf,len);
			delete [] buf;
		}
		else
		{
			mem_bzero(temp.base_address,len);
		}
	}
	else if(simulated_addr != 0)
	{
		if(fd!=(md_gpr_t)-1)
		{
			char * buf = new char[len];
			unsigned long long count = 0;
			lseek(fd,offset,SEEK_SET);
			while(count < len)
			{
				int read_result = read(fd,buf+count,len-count);
				if(read_result==-1)
				{
					break;
				}
				count+=read_result;
			}
			mem_bcopy(Write,temp.base_address,buf,len);
			delete [] buf;
		}
		else
		{
			mem_bzero(temp.base_address,len);
		}
		internal_map.push_back(temp);
	}
	else if(flags & OSF_MAP_ANON)
	{
		if(fd==(md_gpr_t)-1)
		{
			mem_bzero(temp.base_address,len);
		}
		else
		{
			return -1;
		}
		internal_map.push_back(temp);
	}

	else if(flags & OSF_MAP_SHARED)
	{
		temp.mapping = mmap(NULL,len,prot,MAP_SHARED,fd,offset);
		if(temp.mapping == (void *)-1)
		{
			return -1;
		}
		memory_map.push_back(temp);
	}
	else
	{
		//Mapping is private, with no desired address... we could bring this file into simulated memory and avoid the real mapping....
		//Anyway, it appears that prot might be all in this case.
		temp.protections = prot = PROT_READ | PROT_WRITE | PROT_EXEC;
		temp.mapping = mmap(NULL,len,prot,MAP_PRIVATE,fd,offset);
		if(temp.mapping == (void *)-1)
		{
			return -1;
		}
		memory_map.push_back(temp);
	}
	return temp.base_address;
}

//remove a memory mapping
int mem_t::mem_unmap(md_addr_t simulated_addr)
{
	for(size_t i=0;i<memory_map.size();i++)
	{
		if(memory_map[i].base_address == simulated_addr)
		{
			int retval = munmap(memory_map[i].mapping,memory_map[i].size);
			if(retval == -1)
			{
				return -1;
			}
			memory_map.erase(memory_map.begin()+i);
			return 0;
		}
	}
	for(size_t i=0;i<internal_map.size();i++)
	{
		if(internal_map[i].base_address == simulated_addr)
		{
			internal_map.erase(internal_map.begin()+i);
			return 0;
		}
	}
	return -1;
}

bool mmap_t::is_within(md_addr_t addr)
{
	if((addr>=base_address) && ((addr-base_address)<size))
	{
		return true;
	}
	return false;
}
bool mmap_t::bounds_check(md_addr_t addr, md_addr_t len)
{
	//assert(len);
	return is_within(addr) && is_within(addr+len-1);
}
void * mmap_t::translate(md_addr_t addr)
{
	md_addr_t offset = addr - base_address;
	return ((byte_t *)mapping) + offset;
}

#if 0

/*
 * The SimpleScalar virtual memory address space is 2^31 bytes mapped from
 * 0x00000000 to 0x7fffffff.  The upper 2^31 bytes are currently reserved for
 * future developments.  The address space from 0x00000000 to 0x00400000 is
 * currently unused.  The address space from 0x00400000 to 0x10000000 is used
 * to map the program text (code), although accessing any memory outside of
 * the defined program space causes an error to be declared.  The address
 * space from 0x10000000 to "mem_brk_point" is used for the program data
 * segment.  This section of the address space is initially set to contain the
 * initialized data segment and then the uninitialized data segment.
 * "mem_brk_point" then grows to higher memory when sbrk() is called to
 * service heap growth.  The data segment can continue to expand until it
 * collides with the stack segment.  The stack segment starts at 0x7fffc000
 * and grows to lower memory as more stack space is allocated.  Initially,
 * the stack contains program arguments and environment variables (see
 * loader.c for details on initial stack layout).  The stack may continue to
 * expand to lower memory until it collides with the data segment.
 *
 * The SimpleScalar virtual memory address space is implemented with a
 * one level page table, where the first level table contains MEM_TABLE_SIZE
 * pointers to MEM_BLOCK_SIZE byte pages in the second level table.  Pages
 * are allocated in MEM_BLOCK_SIZE size chunks when first accessed, the initial
 * value of page memory is all zero.
 *
 * Graphically, it all looks like this:
 *
 *                 Virtual        Level 1    Host Memory Pages
 *                 Address        Page       (allocated as needed)
 *                 Space          Table
 * 0x00000000    +----------+      +-+      +-------------------+
 *               | unused   |      | |----->| memory page (64k) |
 * 0x00400000    +----------+      +-+      +-------------------+
 *               |          |      | |
 *               | text     |      +-+
 *               |          |      | |
 * 0x10000000    +----------+      +-+
 *               |          |      | |
 *               | data seg |      +-+      +-------------------+
 *               |          |      | |----->| memory page (64k) |
 * mem_brk_point +----------+      +-+      +-------------------+
 *               |          |      | |
 *               |          |      +-+
 *               |          |      | |
 * regs_R[29]    +----------+      +-+
 * (stack ptr)   |          |      | |
 *               | stack    |      +-+
 *               |          |      | |
 * 0x7fffc000    +----------+      +-+      +-------------------+
 *               | unsed    |      | |----->| memory page (64k) |
 * 0x7fffffff    +----------+      +-+      +-------------------+

 */

/* top of the data segment, sbrk() moves this to higher memory */
extern SS_ADDR_TYPE mem_brk_point;

/* lowest address accessed on the stack */
extern SS_ADDR_TYPE mem_stack_min;

/*
 * memory page table defs
 */

/* memory indirect table size (upper mem is not used) */
#define MEM_TABLE_SIZE		0x8000 /* was: 0x7fff */

#ifndef HIDE_MEM_TABLE_DEF	/* used by sim-fast.c */
/* the level 1 page table map */
extern char *mem_table[MEM_TABLE_SIZE];
#endif /* HIDE_MEM_TABLE_DEF */

/* memory block size, in bytes */
#define MEM_BLOCK_SIZE		0x10000

	/* check permissions, no probes allowed into undefined segment regions */
	if(!(/* text access and a read */
		(addr >= ld_text_base && addr < (ld_text_base+ld_text_size)
		&& cmd == Read)
		/* data access within bounds */
		|| (addr >= ld_data_base && addr < ld_stack_base)))
		fatal("access error: segmentation violation, addr 0x%08p", addr);

	/* track the minimum SP for memory access stats */
	if (addr > mem_brk_point && addr < mem_stack_min)
		mem_stack_min = addr;

/* determines if the memory access is valid, returns error str or NULL */
char *					/* error string, or NULL */
mem_valid(mem_t *mem,			/* memory space to probe */
	mem_cmd cmd,			/* Read (from sim'ed mem) or Write */
	md_addr_t addr,			/* target address to access */
	int nbytes,			/* number of bytes to access */
	int declare);			/* declare any detected error? */

/* determines if the memory access is valid, returns error str or NULL */
char *					/* error string, or NULL */
mem_valid(mem_cmd cmd,			/* Read (from sim mem) or Write */
	SS_ADDR_TYPE addr,		/* target address to access */
	int nbytes,			/* number of bytes to access */
	int declare)			/* declare the error if detected? */
{
	char *err_str = NULL;

	/* check alignments */
	if((nbytes & (nbytes-1)) != 0 || (addr & (nbytes-1)) != 0)
	{
		err_str = "bad size or alignment";
	}
	/* check permissions, no probes allowed into undefined segment regions */
	else if (!(/* text access and a read */
		(addr >= ld_text_base && addr < (ld_text_base+ld_text_size)
		&& cmd == Read)
		/* data access within bounds */
		|| (addr >= ld_data_base && addr < ld_stack_base)))
	{
		err_str = "segmentation violation";
	}

	/* track the minimum SP for memory access stats */
	if(addr > mem_brk_point && addr < mem_stack_min)
		mem_stack_min = addr;

	if(!declare)
		return err_str;
	else if(err_str != NULL)
		fatal(err_str);
	else /* no error */
		return NULL;
}

/* initialize memory system, call after loader.c */
void mem_init1(void)
{

	/* initialize the bottom of heap to top of data segment */
	mem_brk_point = ROUND_UP(ld_data_base + ld_data_size, SS_PAGE_SIZE);

	/* set initial minimum stack pointer value to initial stack value */
	mem_stack_min = regs_R[SS_STACK_REGNO];
}

#endif
