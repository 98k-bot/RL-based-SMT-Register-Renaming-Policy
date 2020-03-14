#ifndef DRAM_CPP
#define DRAM_CPP

#include"dram.h"
#include<cassert>

#define BANK_CLOSED	0
#define BANK_PRECHARGE	1

void dram_t::reset()
{
	//Nothing to do in this case
}

dram_default::dram_default(unsigned int mem_lat, unsigned int mem_chunk_lat, unsigned int bus_width)
: mem_lat(mem_lat), mem_chunk_lat(mem_chunk_lat), bus_width(bus_width)
{
	//Memory latency must be greater than zero.
	assert(mem_lat>0);
	assert(mem_chunk_lat>0);

	//Bus_width must be positive and a power of 2
	assert(bus_width>0);
	assert((bus_width & (bus_width-1))==0);
}

dram_basic::dram_basic(unsigned int todram, unsigned int from_dram, unsigned int pre, unsigned int closed, unsigned int conflict, unsigned int bus_width, unsigned int num_banks, unsigned int row_size)
: to_dram(todram), from_dram(from_dram), pre(pre), closed(closed), conflict(conflict), bus_width(bus_width), num_banks(num_banks),
	row_size(row_size), row_bits(0), banks(num_banks,0), status(num_banks,BANK_CLOSED), until(num_banks,0)
{
	while(row_size>0)
	{
		row_bits++;
		row_size>>=1;
	}

	//Delay times must be greater than 0
	assert((to_dram!=0)&&(from_dram!=0));
	assert((pre!=0)&&(closed!=0)&&(conflict!=0));

	//Bus_width and num_banks must be positive and a power of 2
	assert((bus_width>0)&&(num_banks>0));
	assert((bus_width & (bus_width-1))==0);
	assert((num_banks & (num_banks-1))==0);

	//Row_size must be a power of 2, and 2^(row_bits) must equal row_size
	assert((unsigned int)(1<<(row_bits-1))==this->row_size);
}

unsigned int dram_default::mem_access_latency(md_addr_t addr, int size, tick_t when, int context_id)
{
	unsigned int chunks = (size + (bus_width - 1)) / bus_width;
	assert(chunks > 0);
	return (mem_lat + (mem_chunk_lat * (chunks - 1)));
}

unsigned int dram_basic::mem_access_latency(md_addr_t addr, int size, tick_t when, int context_id)
{
	assert((size%bus_width)==0);
	int offset = 0;
	unsigned int latency = 0;
	unsigned int target_bank = (unsigned int)-1;
	while(size>0)
	{
		size-=bus_width;
		unsigned int cur_bank = ((addr+offset)>>row_bits)%num_banks;
		tick_t doneat = std::max(latency,mem_access_latency_helper(addr+offset,target_bank==cur_bank,when+to_dram,context_id));
		latency = (doneat - when);
		offset+=bus_width;
		target_bank = cur_bank;
	}
	return latency;
}

void dram_basic::reset()
{
	for(unsigned int i=0;i<num_banks;i++)
	{
		until[i] = 0;
	}
}

unsigned int dram_basic::mem_access_latency_helper(md_addr_t addr, bool cont, tick_t when, int context_id)
{
	unsigned int target_row = (addr>>row_bits)/num_banks;
	unsigned int target_bank = (addr>>row_bits)%num_banks;

	if(status[target_bank]==BANK_CLOSED)
	{
		banks[target_bank] = target_row;
		status[target_bank] = BANK_PRECHARGE;
		until[target_bank] = when + pre + closed + from_dram;
		return until[target_bank];
	}

	if(status[target_bank]==BANK_PRECHARGE)
	{
		if(banks[target_bank]==target_row)
		{
			if(cont)
			{
				until[target_bank] += from_dram;
				return until[target_bank];
			}
			until[target_bank] = std::max(until[target_bank], when);
			until[target_bank] += pre + from_dram;
			return until[target_bank];
		}
		//Otherwise, it is a conflict
		//This isn't intelligent, we just First Come First Serve
		banks[target_bank] = target_row;
		//No status change
		until[target_bank] = std::max(until[target_bank],when);
		until[target_bank] += pre + closed + conflict + from_dram;
		return until[target_bank];
	}
	assert(0);	//This should not happen.
	return 0;
}

dram_t * dram_parser(std::string config)
{
	for(unsigned int i=0;i<config.size();i++)
	{
		if(config[i]==':')
		{
			config[i] = ' ';
		}
	}

	std::stringstream parse(config);
	
	std::string type;
	parse >> type;

	unsigned int bus_width;
	parse >> bus_width;

	std::vector<std::string> args;
	//We can overwrite config now
	while(parse >> config)
	{
		args.push_back(config);
	}

	//ADD NEW TYPES HERE!
	//The type name is contained in "type"
	//The remaining arguments are contained in args[0]-args[n]
	if(type=="chunk")
	{
		return new dram_default(atoi(args[0].c_str()),atoi(args[1].c_str()),bus_width);
	}
	if(type=="basic")
	{
		return new dram_basic(atoi(args[0].c_str()),atoi(args[1].c_str()),atoi(args[2].c_str()),atoi(args[3].c_str()),atoi(args[4].c_str()),bus_width,atoi(args[5].c_str()),atoi(args[6].c_str()));
	}

	//No matching type found
	return (dram_t *)NULL;
}

#endif
