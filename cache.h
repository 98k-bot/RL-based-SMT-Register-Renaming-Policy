//#define BUS_CONTENTION

/* cache.h - cache module interfaces */

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

#ifndef CACHE_H
#define CACHE_H

#include<cstdio>
#include<vector>
#include<string>

#include "host.h"
#include "memory.h"
#include "stats.h"

/*
 * This module contains code to implement various cache-like structures.  The
 * user instantiates caches using cache_new().  When instantiated, the user
 * may specify the geometry of the cache (i.e., number of set, line size,
 * associativity), and supply a block access function.  The block access
 * function indicates the latency to access lines when the cache misses,
 * accounting for any component of miss latency, e.g., bus acquire latency,
 * bus transfer latency, memory access latency, etc...  In addition, the user
 * may allocate the cache with or without lines allocated in the cache.
 * Caches without tags are useful when implementing structures that map data
 * other than the address space, e.g., TLBs which map the virtual address
 * space to physical page address, or BTBs which map text addresses to
 * branch prediction state.  Tags are always allocated.  User data may also be
 * optionally attached to cache lines, this space is useful to storing
 * auxilliary or additional cache line information, such as predecode data,
 * physical page address information, etc...
 *
 * The caches implemented by this module provide efficient storage management
 * and fast access for all cache geometries.  When sets become highly
 * associative, a hash table (indexed by address) is allocated for each set
 * in the cache.
 *
 * This module also tracks latency of accessing the data cache, each cache has
 * a hit latency defined when instantiated, miss latency is returned by the
 * cache's block access function, the caches may service any number of hits
 * under any number of misses, the calling simulator should limit the number
 * of outstanding misses or the number of hits under misses as per the
 * limitations of the particular microarchitecture being simulated.
 *
 * Due to the organization of this cache implementation, the latency of a
 * request cannot be affected by a later request to this module.  As a result,
 * reordering of requests in the memory hierarchy is not possible.
 */

//highly associative caches are implemented using a hash table lookup to
//speed block access, this macro decides if a cache is "highly associative"
#ifdef USE_HASH
	#define CACHE_HIGHLY_ASSOC(cp)	((cp)->assoc > 4)
#endif

//cache replacement policy
enum cache_policy
{
	LRU,		//replace least recently used block (perfect LRU)
	Random,		//replace a random block
	FIFO		//replace the oldest block in the set
};

//block status values
#define CACHE_BLK_VALID		0x00000001	//block is valid, in use
#define CACHE_BLK_DIRTY		0x00000002	//dirty block, must be written back before eviction

//cache block (or line) definition
class cache_blk_t
{
	public:
	 	~cache_blk_t()
		{
			delete user_data;
		};

		cache_blk_t *way_next;		//next block in the ordered way chain, used to order blocks for replacement
		cache_blk_t *way_prev;		//previous block in the order way chain
#ifdef USE_HASH
		cache_blk_t *hash_next;		//next block in the hash bucket chain, only used in highly-associative caches
		//hash table lists are typically small, so no previous pointer, deletion requires a trip through the hash table bucket list
#endif

		md_addr_t tag;			//tag value for the cache block
		unsigned int status;		//block status, see CACHE_BLK_* defs above
		tick_t ready;			//time when block will be accessible. Set when a miss fetch is initiated
		byte_t *user_data;		//pointer to user defined data, e.g., pre-decode data or physical page address

		//The following comment may not be relevant anymore. 
		//DATA should be pointer-aligned due to preceeding field
		std::vector<byte_t> data;	//actual data block starts here, block size should probably be a multiple of 8
		int context_id;			//context_id of the owner of the data in the block
};

//cache set definition (one or more blocks sharing the same set index)
class cache_set_t
{
	public:
#ifdef USE_HASH
		std::vector<cache_blk_t *> hash;	//hash table: for fast access w/assoc, NULL for low-assoc caches
#endif
		cache_blk_t *way_head;			//head of way list
		cache_blk_t *way_tail;			//tail of way list
		cache_blk_t *blks;			//cache blocks, allocated sequentially, so this pointer can also be used for random access to cache blocks
};

//cache definition
class cache_t
{
	public:
		cache_t();
		~cache_t();
		cache_t(const cache_t & target);
		cache_t(std::string name,		//name of the cache
			unsigned int nsets,		//total number of sets in cache
			unsigned int bsize,		//block (line) size of cache
			bool balloc,			//allocate data space for blocks?
			int usize,			//size of user data to alloc w/blks
			unsigned int assoc,		//associativity of cache
			cache_policy policy,		//replacement policy w/in sets
			//block access function, see description w/in struct cache def
			unsigned long long (*blk_access_fn)(mem_cmd cmd,
				md_addr_t baddr, unsigned int bsize,
				cache_blk_t *blk,
				tick_t now,
				int context_id),
			unsigned int hit_latency); 	//latency in cycles for a hit

		//resets cache stats after fast forwarding
		void reset_cache_stats();

		//print cache configuration to file descriptor stream
		void cache_config(FILE *stream);

		//print cache stats to the file descriptor stream
		void print_stats(FILE *stream);
	
		//access a cache, perform a CMD operation the cache at address ADDR,
		//places NBYTES of data at *P, returns latency of operation if initiated
		//at NOW, places pointer to block user data in *UDATA, *P is untouched if
		//cache blocks are not allocated (!BALLOC), UDATA should be NULL if no
		//user data is attached to blocks
		unsigned long long			//latency of access in cycles
		cache_access(mem_cmd cmd,		//access type, Read or Write
			md_addr_t addr,			//address of access
			int context_id,        		//context_id of the memory to access
			void *vp,			//ptr to buffer for input/output
			unsigned int nbytes,		//number of bytes to access
			tick_t now,			//time of access
			byte_t **udata,			//for return of user data ptr
			md_addr_t *repl_addr);		//for address of replaced block

		//return true if block containing address ADDR is contained in cache	
		//this interface is used primarily for debugging and asserting cache invariants
		bool cache_probe(md_addr_t addr);	//address of block to probe

		//flush the entire cache, returns latency of the operation
		unsigned long long cache_flush(tick_t now);		//time of cache flush

		//flush the block containing ADDR from the cache, returns the latency of the block flush operation
		unsigned long long cache_flush_addr(md_addr_t addr,	//address of block to flush
			tick_t now);					//time of cache flush

	private:
#ifdef USE_HASH
		//insert BLK onto the head of the hash table bucket chain in SET
		void link_htab_ent(cache_set_t *set,	//set containing bkt chain
			cache_blk_t *blk);		//block to insert

		//unlink BLK from the hash table bucket chain in SET
		void unlink_htab_ent(cache_set_t *set,	//set containing bkt chain
			cache_blk_t *blk);		//block to unlink
#endif
	public:
		//parameters
		std::string name;		//cache name
		unsigned int nsets;		//number of sets
		unsigned int bsize;		//block size in bytes
		bool balloc;			//maintain cache contents?
		int usize;			//user allocated data size
		unsigned int assoc;		//cache associativity
		cache_policy policy;		//cache replacement policy
		unsigned int hit_latency;	//cache hit latency

		//miss/replacement handler, read/write BSIZE bytes starting at BADDR from/into cache block
		//BLK, returns the latency of the operation if initiated at NOW, returned latencies
		//indicate how long it takes for the cache access to continue (e.g., fill a write buffer),
		//the miss/repl functions are required to track how this operation will effect the latency
		//of later operations (e.g., write buffer fills), if !BALLOC, then just return the latency;
		//BLK_ACCESS_FN is also responsible for generating any user data and incorporating the
		//latency of that operation
		unsigned long long			//latency of block access
		(*blk_access_fn)(mem_cmd cmd,		//block access command
			md_addr_t baddr,		//program address to access
			unsigned int bsize,		//size of the cache block
			cache_blk_t *blk,		//ptr to cache block struct
			tick_t now,			//when fetch was initiated
			int context_id);

		//derived data, for fast decoding
#ifdef USE_HASH
		int hsize;			//cache set hash table size
#endif
		md_addr_t blk_mask;
		int set_shift;
		md_addr_t set_mask;		//use *after* shift
		int tag_shift;
		md_addr_t tag_mask;		//use *after* shift
		md_addr_t tagset_mask;		//used for fast hit detection

#ifndef BUS_CONTENTION
		//bus resource
		tick_t bus_free;		//time when bus to next level of cache is free, NOTE: the
						//bus model assumes only a single, fully-pipelined port to
						//the next level of memory that requires the bus only one
						//cycle for cache line transfer (the latency of the access
						//to the lower level may be more than one cycle,
						//as specified by the miss handler
#endif

		//per-cache stats
		counter_t hits;			//total number of hits
		counter_t misses;		//total number of misses
		counter_t replacements;		//total number of replacements at misses
		counter_t writebacks;		//total number of writebacks at misses
		counter_t invalidations;	//total number of external invalidations

		//data blocks
		std::vector<byte_t> data;	//pointer to data blocks allocation

		std::vector<cache_set_t> sets;	//each entry is a set

#ifdef BUS_CONTENTION
		int contention_time;
		std::vector<unsigned long long> bus_usages;
		void clear_contention(unsigned long long sim_cycle);
		void set_contention(int contention_time);
		cache_t * next_cache;
		unsigned long long make_next_request(unsigned long long now);
#endif

};

//parse policy, returns the replacement policy enum, takes a char that represents the replacement policy
cache_policy cache_char2policy(char c);

//These don't seem to be used anywhere. They could be replaced with a templated cache_access
//cache access functions, these are safe, they check alignment and permissions
#define cache_double(cp, cmd, addr, p, now, udata)	cache_access(cp, cmd, addr, p, sizeof(double), now, udata)
#define cache_float( cp, cmd, addr, p, now, udata)	cache_access(cp, cmd, addr, p, sizeof(float), now, udata)
#define cache_dword( cp, cmd, addr, p, now, udata)	cache_access(cp, cmd, addr, p, sizeof(long long), now, udata)
#define cache_word(  cp, cmd, addr, p, now, udata)	cache_access(cp, cmd, addr, p, sizeof(int), now, udata)
#define cache_half(  cp, cmd, addr, p, now, udata)	cache_access(cp, cmd, addr, p, sizeof(short), now, udata)
#define cache_byte(  cp, cmd, addr, p, now, udata)	cache_access(cp, cmd, addr, p, sizeof(char), now, udata)

#endif
