/* This is the Main Memory modeling abstraction
 * 
 * Option String:
 *	<type>:<bus_width>:<configuration>
 *
 * Bus Width:
 *	The size of a single transfer from Main Memory
 *	The request size divided by bus width determines the number of chunks requested
 *
 * Types: 
 *	chunk - this is the original model from simplescalar
 *		configuration -> <bus_width>:<first_chunk_latency>:<next_chunk_latency>
 *		Default -> "chunk:4:300:2"
 *
 *	basic - this is a simplistic modeling of DRAM which accounts for row-hits, precharging and row-conflicts
 *		configuration -> <to_dram time>:<from_dram time>:<row-hit time>:<row-closed time>:<row-conflict time>:<number of banks>:<row buffer size>
 *			to_dram time -> Time to send memory request to main memory
 *			from_dram time -> Time to send bus_width bytes from main memory to the cache
 *			row-hit time -> Time required if requested data is in the row buffer
 *			row-closed time -> Time required if row buffer is empty
 *			row-conflict time -> Time required if row buffer has incorrect data
 *			number of banks -> Number of memory banks
 *			row buffer size -> Size of the row buffer, per bank, in bytes
 *		Default -> "basic:4:6:12:90:90:90:8:2048"
 *
 * Adding New Models:
 *	New models should inherit from dram_t
 *	Must provide mem_access_latency as defined in dram_t
 *		- Takes a memory address, request size in bytes, time of request, and requesting context - these do not all have to be used
 *	Reset() is provided to clear data after fast forwarding, if not provided, the base class reset() does nothing.
 *	Add a configuration parser to dram_parser(string config) (in dram.c)
 *
 * Notes:
 *	Each core should share a pointer to this object. There is no provision for heterogeneous main memory accesses.
 */

#ifndef DRAM_H
#define DRAM_H

#include"machine.h"
#include<vector>
#include<string>
#include<sstream>

class dram_t
{
	public:
		virtual unsigned int mem_access_latency(md_addr_t addr, int size, tick_t when, int context_id)=0;
		virtual void reset();	//May not need to actually do anything
};

class dram_default : public dram_t
{
	public:
		dram_default(unsigned int mem_lat, unsigned int mem_chunk_lat, unsigned int bus_width);
		unsigned int mem_access_latency(md_addr_t addr, int size, tick_t when, int context_id);
	private:
		unsigned int mem_lat, mem_chunk_lat;
		unsigned int bus_width;
};

class dram_basic : public dram_t
{
	public:
		dram_basic(unsigned int todram, unsigned int from_dram, unsigned int pre, unsigned int closed, unsigned int conflict, unsigned int bus_width, unsigned int num_banks, unsigned int row_size);
		unsigned int mem_access_latency(md_addr_t addr, int size, tick_t when, int context_id);
		unsigned int mem_access_latency_helper(md_addr_t addr, bool cont, tick_t when, int context_id);

		void reset();
	private:
		unsigned int to_dram, from_dram, pre, closed, conflict;
		unsigned int bus_width, num_banks;
		unsigned int row_size, row_bits;
		std::vector<unsigned int> banks, status;
		std::vector<tick_t> until;
};

dram_t * dram_parser(std::string config);

#endif
