/* cache.c - cache module routines */

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
#include<cassert>

#include "cache.h"

//cache access macros
#define CACHE_TAG(cp, addr)			((addr) >> (cp)->tag_shift)
#define CACHE_SET(cp, addr)			(((addr) >> (cp)->set_shift) & (cp)->set_mask)
#define CACHE_BLK(cp, addr)			((addr) & (cp)->blk_mask)
#define CACHE_TAGSET(cp, addr)			((addr) & (cp)->tagset_mask)

//extract/reconstruct a block address
#define CACHE_BADDR(cp, addr)			((addr) & ~(cp)->blk_mask)
#define CACHE_MK_BADDR(cp, tag, set)		(((tag) << (cp)->tag_shift)|((set) << (cp)->set_shift))

//index an array of cache blocks, non-trivial due to variable length blocks
#define CACHE_BINDEX(cp, blks, i)		((cache_blk_t *)(((char *)(blks))			\
						+ (i)*(sizeof(cache_blk_t)				\
						+ ((cp)->balloc	? (cp)->bsize*sizeof(byte_t) : 0))))

//cache data block accessor, type parameterized
#define __CACHE_ACCESS(type, data, bofs)	(*((type *)(((char *)data) + (bofs))))

//cache data block accessors, by type
#define CACHE_WORD(data, bofs)			__CACHE_ACCESS(unsigned int, data, bofs)
#define CACHE_HALF(data, bofs)			__CACHE_ACCESS(unsigned short, data, bofs)
#define CACHE_BYTE(data, bofs)			__CACHE_ACCESS(unsigned char, data, bofs)

//cache block hashing macros, this macro is used to index into a cache
//	set hash table (to find the correct block on N in an N-way cache), the
//	cache set index function is CACHE_SET, defined above */
#ifdef USE_HASH
	#define CACHE_HASH(cp, key)			(((key >> 24) ^ (key >> 16) ^ (key >> 8) ^ key) & ((cp)->hsize-1))
#endif

//copy data out of a cache block to buffer indicated by argument pointer p
#define CACHE_BCOPY(cmd, blk, bofs, p, nbytes)	if (cmd == Read)									\
						{											\
							switch (nbytes)									\
							{										\
							case 1:										\
								*((byte_t *)p) = CACHE_BYTE(&blk->data[0], bofs); break;		\
							case 2:										\
								*((half_t *)p) = CACHE_HALF(&blk->data[0], bofs); break;		\
							case 4:										\
								*((word_t *)p) = CACHE_WORD(&blk->data[0], bofs); break;		\
							default:									\
								{ /* >= 8, power of two, fits in block */				\
									int words = nbytes >> 2;					\
									while (words-- > 0)						\
									{								\
										*((word_t *)p) = CACHE_WORD(&blk->data[0], bofs);	\
										p += 4; bofs += 4;					\
									}								\
								}									\
							}										\
						}											\
						else /* cmd == Write */									\
						{											\
							switch (nbytes)									\
							{										\
							case 1:										\
								CACHE_BYTE(&blk->data[0], bofs) = *((byte_t *)p); break;		\
							case 2:										\
								CACHE_HALF(&blk->data[0], bofs) = *((half_t *)p); break;		\
							case 4:										\
								CACHE_WORD(&blk->data[0], bofs) = *((word_t *)p); break;		\
							default:									\
								{ /* >= 8, power of two, fits in block */				\
									int words = nbytes >> 2;					\
									while (words-- > 0)						\
									{								\
										CACHE_WORD(&blk->data[0], bofs) = *((word_t *)p);	\
										p += 4; bofs += 4;					\
									}								\
								}									\
							}										\
						}

#ifdef USE_HASH
//unlink BLK from the hash table bucket chain in SET
void cache_t::unlink_htab_ent(cache_set_t *set,		//set containing bkt chain
	cache_blk_t *blk)				//block to unlink
{
	cache_blk_t *prev, *ent;
	int index = CACHE_HASH(this, blk->tag);

	//locate the block in the hash table bucket chain
	for(prev=NULL,ent=set->hash[index]; ent; prev=ent,ent=ent->hash_next)
	{
		if(ent == blk)
		{
			break;
		}
	}
	assert(ent);

	//unlink the block from the hash table bucket chain
	if(!prev)
	{
		//head of hash bucket list
		set->hash[index] = ent->hash_next;
	}
	else
	{
		//middle or end of hash bucket list
		prev->hash_next = ent->hash_next;
	}
	ent->hash_next = NULL;
}

//insert BLK onto the head of the hash table bucket chain in SET
void cache_t::link_htab_ent(cache_set_t *set,		//set containing bkt chain
	cache_blk_t *blk)				//block to insert
{
	int index = CACHE_HASH(this, blk->tag);

	//insert block onto the head of the bucket chain
	blk->hash_next = set->hash[index];
	set->hash[index] = blk;
}
#endif

//where to insert a block onto the ordered way chain
enum list_loc_t
{
	Head,
	Tail
};

//insert BLK into the order way chain in SET at location WHERE
void update_way_list(cache_set_t *set,			//set contained way chain
	cache_blk_t *blk,				//block to insert
	list_loc_t where)				//insert location
{
	//unlink entry from the way list
	if(!blk->way_prev && !blk->way_next)
	{
		//only one entry in list (direct-mapped), no action. Head and Tail implicitly in order
		assert(set->way_head == blk && set->way_tail == blk);
		return;
	}
	else if(!blk->way_prev)		//Else, there is more than one element in the list
	{
		assert(set->way_head == blk && set->way_tail != blk);
		if(where == Head)
		{	//Already at Head, nothing to do
			return;
		}
		//else, move to tail
		set->way_head = blk->way_next;
		blk->way_next->way_prev = NULL;
	}
	else if(!blk->way_next)
	{
		//end of list (and not front of list)
		assert(set->way_head != blk && set->way_tail == blk);
		if(where == Tail)
		{	//Already at Tail, nothing to do
			return;
		}
		set->way_tail = blk->way_prev;
		blk->way_prev->way_next = NULL;
	}
	else
	{
		//middle of list (and not front or end of list)
		assert(set->way_head != blk && set->way_tail != blk);
		blk->way_prev->way_next = blk->way_next;
		blk->way_next->way_prev = blk->way_prev;
	}

	//link BLK back into the list
	if(where == Head)
	{
		//link to the head of the way list
		blk->way_next = set->way_head;
		blk->way_prev = NULL;
		set->way_head->way_prev = blk;
		set->way_head = blk;
	}
	else if(where == Tail)
	{
		//link to the tail of the way list
		blk->way_prev = set->way_tail;
		blk->way_next = NULL;
		set->way_tail->way_next = blk;
		set->way_tail = blk;
	}
	else
	{
		panic("bogus WHERE designator");
	}
}

cache_t::cache_t()
{}

//create and initialize a general cache structure
cache_t::cache_t(std::string name,		//name of the cache
	unsigned int nsets,			//total number of sets in cache
	unsigned int bsize,			//block (line) size of cache
	bool balloc,				//allocate data space for blocks?
	int usize,				//size of user data to alloc w/blks
	unsigned int assoc,			//associativity of cache
	cache_policy policy,			//replacement policy w/in sets

	//block access function, see description w/in struct cache def
	unsigned long long (*blk_access_fn)(mem_cmd cmd,
		md_addr_t baddr, unsigned int bsize,
		cache_blk_t *blk,
		tick_t now,
		int context_id),
		unsigned int hit_latency)
: 
//initialize user parameters
	name(name), nsets(nsets), bsize(bsize), balloc(balloc), usize(usize), assoc(assoc), policy(policy), 
	hit_latency(hit_latency),
//miss/replacement functions
	blk_access_fn(blk_access_fn), 
//compute derived parameters
#ifdef USE_HASH
	hsize(CACHE_HIGHLY_ASSOC(this) ? (assoc >> 2) : 0),
#endif
	blk_mask(bsize-1), set_shift(log_base2(bsize)), 
	set_mask(nsets-1), tag_shift(set_shift + log_base2(nsets)), tag_mask((1 << (32 - tag_shift))-1), 
	tagset_mask(~blk_mask), 
#ifndef BUS_CONTENTION
	bus_free(0),
#endif
//initialize cache stats
	hits(0), misses(0), replacements(0), writebacks(0), invalidations(0),
//allocate data blocks
	data((nsets*assoc) * (sizeof(cache_blk_t) + (balloc ? (bsize*sizeof(byte_t)) : 0))),
//allocate the cache structure
	sets(nsets)
#ifdef BUS_CONTENTION
	, contention_time(bsize/8), next_cache(NULL)
#endif
{
	//validate all cache parameters
	if(nsets <= 0)
		fatal("cache size (in sets) `%d' must be non-zero", nsets);
	if((nsets & (nsets-1)) != 0)
		fatal("cache size (in sets) `%d' is not a power of two", nsets);
	//blocks must be at least one datum large, i.e., 8 bytes for SS
	if(bsize < 8)
		fatal("cache block size (in bytes) `%d' must be 8 or greater", bsize);
	if((bsize & (bsize-1)) != 0)
		fatal("cache block size (in bytes) `%d' must be a power of two", bsize);
	if(usize < 0)
		fatal("user data size (in bytes) `%d' must be a positive value", usize);
	if(assoc <= 0)
		fatal("cache associativity `%d' must be non-zero and positive", assoc);
	if((assoc & (assoc-1)) != 0)
		fatal("cache associativity `%d' must be a power of two", assoc);
	if(!blk_access_fn)
		fatal("must specify miss/replacement functions");

	//print derived parameters during debug
#ifdef USE_HASH
	debug("%s: cp->hsize     = %d", name.c_str(), hsize);
#endif
	debug("%s: cp->blk_mask  = 0x%08x", name.c_str(), blk_mask);
	debug("%s: cp->set_shift = %d", name.c_str(), set_shift);
	debug("%s: cp->set_mask  = 0x%08x", name.c_str(), set_mask);
	debug("%s: cp->tag_shift = %d", name.c_str(), tag_shift);
	debug("%s: cp->tag_mask  = 0x%08x", name.c_str(), tag_mask);

	//slice up the data blocks
	for(unsigned int bindex=0,i=0; i<nsets; i++)
	{
		sets[i].way_head = NULL;
		sets[i].way_tail = NULL;

#ifdef USE_HASH
		//get a hash table, if needed
		if(hsize)
		{
			sets[i].hash.resize(hsize,NULL);
		}
#endif
		//NOTE: all the blocks in a set *must* be allocated contiguously otherwise block
		//	accesses through SET->BLKS will fail (used during random replacement selection)
		sets[i].blks = CACHE_BINDEX(this, &data[0], bindex);
      
		//link the data blocks into ordered way chain and hash table bucket chains, if hash table exists
		for(unsigned int j=0; j<assoc; j++)
		{
			//locate next cache block
			cache_blk_t *blk = CACHE_BINDEX(this, &data[0], bindex);
			bindex++;

			//invalidate new cache block
			blk->status = 0;
			blk->tag = 0;
			blk->ready = 0;
			blk->user_data = (usize != static_cast<unsigned int>(0) ? new byte_t[usize] : NULL);
			blk->context_id = -1;

#ifdef USE_HASH
			//insert cache block into set hash table
			if(hsize)
				link_htab_ent(&sets[i], blk);
#endif

			//insert into head of way list, order is arbitrary at this point
			blk->way_next = sets[i].way_head;
			blk->way_prev = NULL;
			if(sets[i].way_head)
				sets[i].way_head->way_prev = blk;
			sets[i].way_head = blk;
			if(!sets[i].way_tail)
				sets[i].way_tail = blk;
		}
	}
}

cache_t::~cache_t()
{
	for(unsigned int bindex=0,i=0; i<nsets; i++)
	{
		for(unsigned int j=0; j<assoc; j++)
		{
			delete [] CACHE_BINDEX(this, &data[0], bindex)->user_data;
			bindex++;
		}
	}
}

//parse policy, returns replacement policy enumerated value
cache_policy cache_char2policy(char c)		//replacement policy as a char
{
	switch (c)
	{
	case 'l': return LRU;
	case 'r': return Random;
	case 'f': return FIFO;
	default: fatal("bogus replacement policy, `%c'", c);
	}
}

//resets cache stats after fast forwarding
void cache_t::reset_cache_stats()
{
	//initialize cache stats
	hits = misses = replacements = writebacks = invalidations = 0;

#ifndef BUS_CONTENTION
	bus_free = 0;
#endif
	//reset replacement delays for each block
	for(unsigned int bindex=0, i=0; i<nsets; i++)
	{
		for(unsigned int j=0; j<assoc; j++)
		{
			//locate next cache block and "invalidate"
			CACHE_BINDEX(this, &data[0], bindex)->ready = 0;
			bindex++;
		}
	}
#ifdef BUS_CONTENTION
	bus_usages.clear();
#endif
}

//print cache configuration to the FILE descriptor stream
void cache_t::cache_config(FILE *stream)
{
	fprintf(stream, "cache: %s: %d sets, %d byte blocks, %d bytes user data/block\n",
		name.c_str(), nsets, bsize, usize);
	fprintf(stream, "cache: %s: %d-way, `%s' replacement policy, write-back\n",
		name.c_str(), assoc, policy == LRU ? "LRU"
		: policy == Random ? "Random" : policy == FIFO ? "FIFO" : (abort(), ""));
}

//print cache stats to the file descriptor stream
void cache_t::print_stats(FILE *stream)
{
	unsigned long long accesses = hits + misses;

	fprintf(stream,"%s.accesses             %lld # total number of accesses\n",              name.c_str(), misses+hits);
	fprintf(stream,"%s.hits                 %lld # total number of hits\n",                  name.c_str(), hits);
	fprintf(stream,"%s.misses               %lld # total number of misses\n",                name.c_str(), misses);

//	sprintf(buf, "%s.MPKI", name.c_str());
//	std::string temp(name);	//Assuming name is Core_#_..., since we need MPKI by core
//	temp.erase(temp.find('_',5));
//	sprintf(buf1, "1000 * %s.misses / sim_num_insn_%s", name.c_str(), temp.c_str());
//	stat_reg_formula(sdb, buf, "MPKI (Misses per thousand instructions)", buf1, NULL);

	fprintf(stream,"%s.replacements         %lld # total number of replacements\n",            name.c_str(), replacements);
	fprintf(stream,"%s.writebacks           %lld # total number of writebacks\n",              name.c_str(), writebacks);
	fprintf(stream,"%s.invalidations        %lld # total number of invalidations\n",           name.c_str(), invalidations);

	if(accesses)
	{
		fprintf(stream,"%s.miss_rate            %f # miss rate (misses/ref)\n",                  name.c_str(), (double)misses/(double)accesses);
		fprintf(stream,"%s.repl_rate            %f # replacement rate (repls/ref)\n",            name.c_str(), (double)replacements/(double)accesses);
		fprintf(stream,"%s.wb_rate              %f # writeback rate (wrbks/ref)\n",              name.c_str(), (double)writebacks/(double)accesses);
		fprintf(stream,"%s.inv_rate             %f # invalidation rate (invs/ref)\n",            name.c_str(), (double)invalidations/(double)accesses);
	}
}

//access a cache, perform a CMD operation the cache at address ADDR, places NBYTES of 
//	data at *P, returns latency of operation if initiated at NOW (in cycles), places pointer 
//	to block user data in *UDATA, *P is untouched if cache blocks are not allocated
//	(!BALLOC), UDATA should be NULL if no user data is attached to blocks
unsigned long long cache_t::cache_access(mem_cmd cmd,	//access type, Read or Write
	md_addr_t addr,					//address of access
	int context_id,					//context_id of the memory to access
	void *vp,					//ptr to buffer for input/output
	unsigned int nbytes,				//number of bytes to access
	tick_t now,					//time of access
	byte_t **udata,					//for return of user data ptr
	md_addr_t *repl_addr)				//for address of replaced block
{
	byte_t *p = (byte_t *)vp;
	md_addr_t tag = CACHE_TAG(this, addr);
	md_addr_t set = CACHE_SET(this, addr);
	md_addr_t bofs = CACHE_BLK(this, addr);
	long long lat = 0;

	//default replacement address
	if(repl_addr)
		*repl_addr = 0;

	//check alignments
	if((nbytes & (nbytes-1)) != 0 || (addr & (nbytes-1)) != 0)
		fatal("cache: access error: bad size or alignment, addr 0x%08x", addr);

	//access must fit in cache block
	if((addr + nbytes) > ((addr & ~blk_mask) + bsize))
		fatal("cache: access error: access spans block, addr 0x%08x", addr);

	//permissions are checked on cache misses

	cache_blk_t *blk(NULL);
	cache_blk_t * repl(NULL);

#ifdef USE_HASH
	if(hsize)
	{
		//highly-associativity cache, access through the per-set hash tables
		int hindex = CACHE_HASH(this, tag);

		for(blk=sets[set].hash[hindex];blk;blk=blk->hash_next)
		{
			if(blk->tag == tag && (blk->status & CACHE_BLK_VALID) && (blk->context_id == context_id))
				goto cache_hit;
		}
	}
	else
#endif
	{
		//low-associativity cache, linear search the way list
		for(blk=sets[set].way_head;blk;blk=blk->way_next)
		{
			if(blk->tag == tag && (blk->status & CACHE_BLK_VALID) && (blk->context_id == context_id))
				goto cache_hit;
		}
	}

	//Cache block not found, MISS
	misses++;

	//select the appropriate block to replace, and re-link this entry to
	//	the appropriate place in the way list
	switch(policy)
	{
	case LRU:
	case FIFO:
		repl = sets[set].way_tail;
		update_way_list(&sets[set], repl, Head);
		break;
	case Random:
		{
			int bindex = myrand() & (assoc - 1);
			repl = CACHE_BINDEX(this, sets[set].blks, bindex);
		}
		break;
	default:
		panic("bogus replacement policy");
	}

#ifdef USE_HASH
	//remove this block from the hash bucket chain, if hash exists
	if(hsize)
	{
		unlink_htab_ent(&sets[set], repl);
	}
#endif

	//write back replaced block data
	if(repl->status & CACHE_BLK_VALID)
	{
		replacements++;

		if(repl_addr)
			*repl_addr = CACHE_MK_BADDR(this, repl->tag, set);

		//don't replace the block until outstanding misses are satisfied
		lat = MAX(0, repl->ready - now);
//		lat += MAX(0, repl->ready - now);
 
		if(repl->status & CACHE_BLK_DIRTY)
		{
			//The replaced block is dirty, write it back
			writebacks++;

#ifdef BUS_CONTENTION
			if(next_cache)
			{
				lat = next_cache->make_next_request(lat) - now;
				lat = MAX(lat,0);
				assert(lat>=0);
			}
				
#else
//FIXME: Bus_free implementation allows bad overlapping of requests
//The bus communication only takes 1 cycle. However, we can tie up the bus
//based on the service time of this request.
//We need to:
//	Keep track of all bus usage for cycle now and after
//	find the earliest usage slot for this request
//	Tie up the bus usage and apply the latency for this block, but not others
//	This can result in out-of-order communications. Is this a problem?
			//Stall until we can send to the next level of memory
			lat = MAX(lat, bus_free - now);

			//The communication takes 1 cycle, however, if the block isn't serviced right away
			//due to pending misses, it stalls the bus too long.
			bus_free = 1 + MAX(bus_free, (tick_t)(now + lat));
//End FIXME
#endif
			//Add latency needed to write back
			lat += blk_access_fn(Write, CACHE_MK_BADDR(this, repl->tag, set), bsize, repl, now+lat, context_id);
		}
	}

	//update block tags
	repl->tag = tag;
	repl->context_id = context_id;
	repl->status = CACHE_BLK_VALID;

#ifdef BUS_CONTENTION
	if(next_cache)
	{
		lat = next_cache->make_next_request(lat) - now;
		lat = MAX(lat,0);
		assert(lat>=0);
	}
	lat += blk_access_fn(Read, CACHE_BADDR(this, addr), bsize, repl, now+lat, context_id);
#else
	//Trying to incorporate bus_free here
	{
		long long new_bus_free = bus_free;
		long long new_lat = MAX(MAX(0,repl->ready - now), new_bus_free - now);
		new_bus_free = 1 + MAX(new_bus_free, (tick_t)(now + new_lat));
		assert(new_bus_free >= bus_free);
		bus_free = new_bus_free;
		new_lat += blk_access_fn(Read, CACHE_BADDR(this, addr), bsize, repl, now+new_lat, context_id);
		lat = MAX(lat,new_lat);
	}

	//Read the data block (required on all misses, load or store. Writes only occur on write back.
//	lat += blk_access_fn(Read, CACHE_BADDR(this, addr), bsize, repl, now+lat, context_id);
#endif

	//If a write, mark this block as dirty
	if(cmd == Write)
	{
		repl->status |= CACHE_BLK_DIRTY;
	}

	//copy data out of cache block
	if(balloc)
	{
		CACHE_BCOPY(cmd, repl, bofs, p, nbytes);
	}

	//get user block data, if requested and it exists
	if(udata)
		*udata = repl->user_data;

	//update block status
	repl->ready = now+lat;

#ifdef USE_HASH
	//link this entry back into the hash table
	if(hsize)
		link_htab_ent(&sets[set], repl);
#endif

	//return latency of the operation
	return lat;

cache_hit:
	//HIT
	hits++;

	//copy data out of cache block, if block exists
	if(balloc)
	{
		CACHE_BCOPY(cmd, blk, bofs, p, nbytes);
	}

	//update dirty status
	if(cmd == Write)
		blk->status |= CACHE_BLK_DIRTY;

	//if this is not the first element of the list and we are using LRU, move the block to the head of the MRU list
	if(blk->way_prev && (policy == LRU))
	{
		update_way_list(&sets[set], blk, Head);
	}

#ifdef USE_HASH
	//tag is unchanged, so hash links (if they exist) are still valid
#endif

	//get user block data, if requested and it exists
	if(udata)
	{
		*udata = blk->user_data;
	}

	//return first cycle data is available to access
	return MAX(hit_latency, (blk->ready - now));
}

//return non-zero if block containing address ADDR is contained the cache (cache hit)
//	this interface is used primarily for debugging and asserting cache invariants
bool cache_t::cache_probe(md_addr_t addr)		//address of block to probe
{
	md_addr_t tag = CACHE_TAG(this, addr);
	md_addr_t set = CACHE_SET(this, addr);

#ifdef USE_HASH
	//highly-associativity cache, access through the per-set hash tables
	if(hsize)
	{
		int hindex = CACHE_HASH(this, tag);
		for(cache_blk_t *blk=sets[set].hash[hindex];blk;blk=blk->hash_next)
		{	
			if(blk->tag == tag && (blk->status & CACHE_BLK_VALID))
				return TRUE;
		}
	}
	else
#endif
	{
		//low-associativity cache, linear search the way list
		for(cache_blk_t *blk=sets[set].way_head;blk;blk=blk->way_next)
		{
			if(blk->tag == tag && (blk->status & CACHE_BLK_VALID))
				return TRUE;
		}
	}
	//cache block not found
	return FALSE;
}

//flush the entire cache, returns latency of the operation
unsigned long long cache_t::cache_flush(tick_t now)			//time of cache flush
{
	unsigned long long lat = hit_latency; 			//min latency to probe cache

	//no way list updates required because all blocks are being invalidated
	for(unsigned int i=0; i<nsets; i++)
	{
		for(cache_blk_t *blk=sets[i].way_head; blk; blk=blk->way_next)
		{
			if(blk->status & CACHE_BLK_VALID)
			{
				invalidations++;
				blk->status &= ~CACHE_BLK_VALID;
				if(blk->status & CACHE_BLK_DIRTY)
				{
					//write back the invalidated block
					writebacks++;
					lat += blk_access_fn(Write, CACHE_MK_BADDR(this, blk->tag, i), bsize, blk, now+lat, blk->context_id);
				}
			}
		}
	}
	//return latency of the flush operation
	return lat;
}

//flush the block containing ADDR from the cache, returns the latency of the block flush operation
unsigned long long cache_t::cache_flush_addr(md_addr_t addr,		//address of block to flush
	tick_t now)						//time of cache flush
{
	unsigned long long lat = hit_latency;			//min latency to probe cache

	md_addr_t tag = CACHE_TAG(this, addr);
	md_addr_t set = CACHE_SET(this, addr);

	cache_blk_t *blk(NULL);
#ifdef USE_HASH
	if(hsize)
	{
		//highly-associativity cache, access through the per-set hash tables
		int hindex = CACHE_HASH(this, tag);
		for(blk=sets[set].hash[hindex];blk;blk=blk->hash_next)
		{
			if(blk->tag == tag && (blk->status & CACHE_BLK_VALID))
				break;
		}
	}
	else
#endif
	{
		//low-associativity cache, linear search the way list
		for(blk=sets[set].way_head;blk;blk=blk->way_next)
		{
			if(blk->tag == tag && (blk->status & CACHE_BLK_VALID))
				break;
		}
	}

	if(blk)
	{
		invalidations++;
		blk->status &= ~CACHE_BLK_VALID;

		if(blk->status & CACHE_BLK_DIRTY)
		{
			//write back the invalidated block
			writebacks++;
			lat += blk_access_fn(Write, CACHE_MK_BADDR(this, blk->tag, set), bsize, blk, now+lat, blk->context_id);
		}
		//move this block to tail of the way (LRU) list
		update_way_list(&sets[set], blk, Tail);
	}
	//return latency of the operation
	return lat;
}

#ifdef BUS_CONTENTION
void cache_t::clear_contention(unsigned long long sim_cycle)
{
	std::vector<unsigned long long>::iterator it = bus_usages.begin();
	while(it!=bus_usages.end())
	{
		
		assert(((*it)+contention_time)>=sim_cycle);
		if(((*it)+contention_time)==sim_cycle)
		{
			it = bus_usages.erase(it);
		}
		else
		{
			it++;
		}
	}
}

//This is for explicitly clearing contention_time for L1 and TLB caches.
//Since these are accessed directly, the controls must happen where they are accessed
//Yes, you can directly set it to zero, this is mostly for explanation sake.
void cache_t::set_contention(int contention_time)
{
	this->contention_time = contention_time;
}

//Returns the first time that a request can be made on the bus.
//FIXME: This is not efficient, at all. It should be refactored.
unsigned long long cache_t::make_next_request(unsigned long long now)
{
	if(bus_usages.empty())
	{
		bus_usages.insert(bus_usages.begin(),now);
		return now;
	}

	if((now < bus_usages[0]) && ((now + contention_time) < bus_usages[0]))
	{
		bus_usages.insert(bus_usages.begin(),now);
		return now;
	}
	
	for(size_t i=1;i<bus_usages.size();i++)
	{
		if((now < bus_usages[i]) && ((now + contention_time) < bus_usages[i]))
		{
			if((now >= (bus_usages[i-1]+ contention_time)))
			{
				bus_usages.insert(bus_usages.begin()+i,now);
				return now;
			}
		}
	}

	unsigned long long new_time = bus_usages.back() + contention_time;
	bus_usages.push_back(MAX(new_time,now));
	return bus_usages.back();
}
#endif
