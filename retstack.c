#include"retstack.h"

retstack_t::retstack_t(size_t retstack_size)
: size(retstack_size), tos(size-1), pops(0), pushes(0)
{
	//Sanity checks
	if((size & (size-1)) != 0)
	{
		fatal("Return address stack size must be zero or a power of two");
	}
	stack.resize(size);
}

//Speculative execution can corrupt the ret-addr stack.  So for each lookup we return the top-of-stack (TOS) at that point; a mispredicted
//branch, as part of its recovery, restores the TOS using this value -- hopefully this uncorrupts the stack.
void retstack_t::recover(int stack_recover_idx)		//Non-speculative top-of-stack; used on mispredict recovery
{
	tos = stack_recover_idx;
}

//return the TOS (top-of-stack) data, if available, for speculative rollback
size_t retstack_t::TOS()
{
	if(size)
	{
		return tos;
	}
	return 0;
}

void retstack_t::clear()
{
	stack.clear();
	stack.resize(size);
	tos = 0;
}

md_addr_t retstack_t::pop()
{
	md_addr_t target = stack[tos].target;
	tos = (tos + size - 1) % size;
	pops++;
	return target;
}

void retstack_t::push(md_addr_t baddr)
{
	tos = (tos + 1)% size;
	stack[tos].target = baddr + sizeof(md_inst_t);
	pushes++;
}

//register retstack stats
void retstack_t::reg_stats(stat_sdb_t *sdb, const char *name)
{
	char buf[500];

	sprintf(buf, "%s.retstack_pushes", name);
	stat_reg_counter(sdb, buf, "total number of address pushed onto ret-addr stack", &pushes, 0, NULL);
	sprintf(buf, "%s.retstack_pops", name);
	stat_reg_counter(sdb, buf, "total number of address popped off of ret-addr stack", &pops, 0, NULL);
}

void retstack_t::reset()
{
	pushes = pops = 0;
}
