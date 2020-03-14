#include"btb.h"
#include<cassert>

#ifdef NEW_BTB
#include<algorithm>
#endif

btb_t::btb_t(unsigned int btb_sets, unsigned int btb_assoc)
#ifdef NEW_BTB
: btb_data(btb_sets,std::list<bpred_btb_ent_t>(btb_assoc))
#else
: sets(btb_sets), assoc(btb_assoc)
#endif
{
#ifdef NEW_BTB
	//Sanity checks
	if(!btb_sets || (btb_sets & (btb_sets-1)) != 0)
	{
		fatal("number of BTB sets must be non-zero and a power of two");
	}
	if(!btb_assoc || (btb_assoc & (btb_assoc-1)) != 0)
	{
		fatal("BTB associativity must be non-zero and a power of two");
	}
#else
	//Sanity checks
	if((sets & (sets-1)) != 0)
	{
		fatal("number of BTB sets must be zero or a power of two");
	}
	if((assoc & (assoc-1)) != 0)
	{
		fatal("BTB associativity must be zero or a power of two");
	}
	if((!assoc || !sets) && (sets || assoc))
	{
		fatal("If either sets or associativity is zero, the other must be as well.");
	}

	btb_data.resize(sets*assoc);
	if(assoc > 1)
	{
		for(unsigned int i=0;i<btb_data.size();i++)
		{
			//Make a linked list out of each quantity(btb.assoc) entries.
			if(i % assoc != assoc - 1)
			{
				btb_data[i].next = &btb_data[i+1];
				btb_data[i+1].prev = &btb_data[i];
			}
			else
			{
				btb_data[i].next = NULL;
			}
		}
	}
#endif
}

bpred_btb_ent_t::bpred_btb_ent_t()
: addr(0), target(0), prev(NULL), next(NULL)
{}

bpred_btb_ent_t * btb_t::find_pbtb(md_addr_t baddr)
{
#ifdef NEW_BTB
	unsigned int index = (baddr >> MD_BR_SHIFT) & (btb_data.size() - 1);

	for(std::list<bpred_btb_ent_t>::iterator it = btb_data[index].begin(); it!=btb_data[index].end(); ++it)
	{
		if((*it).addr == baddr)
		{
			return &(*it);
		}
	}
	return NULL;
#else
	unsigned int index = (baddr >> MD_BR_SHIFT) & (sets - 1);
	if(assoc > 1)
	{
		index *= assoc;
		//Now we know the set; look for a PC match
		for(unsigned int i = index; i < (index+assoc) ; i++)
		{
			if(btb_data[i].addr == baddr)
			{
				return &btb_data[i];
			}
		}
	}
	else
	{
		bpred_btb_ent_t *pbtb = &btb_data[index];
		if(pbtb->addr == baddr)
		{
			return pbtb;
		}
	}
	return NULL;
#endif
}

bpred_btb_ent_t * btb_t::update_pbtb(bool taken, md_addr_t baddr)
{
#ifdef NEW_BTB
	//find BTB entry if it's a taken branch (don't allocate for non-taken)
	if(!taken)
	{
		return NULL;
	}

	unsigned int index = (baddr >> MD_BR_SHIFT) & (btb_data.size() - 1);

	if(btb_data[index].size()==1)
	{
		return &(*btb_data[index].begin());
	}

	//Now we know the set; look for a PC match; also identify MRU and LRU items
	std::list<bpred_btb_ent_t>::iterator it = btb_data[index].begin();
	while(it != btb_data[index].end())
	{
		if((*it).addr == baddr)
		{
			break;
		}
		++it;
	}

	if(it == btb_data[index].end())
	{
		--it;
	}

	if(it != btb_data[index].begin())
	{
		bpred_btb_ent_t temp = *it;
		btb_data[index].erase(it);
		btb_data[index].push_front(temp);
	}
	return &btb_data[index].front();
#else
	//find BTB entry if it's a taken branch (don't allocate for non-taken)
	if(taken)
	{
		unsigned int index = (baddr >> MD_BR_SHIFT) & (sets - 1);
		if(assoc > 1)
		{
			bpred_btb_ent_t *pbtb = NULL, *lruhead = NULL, *lruitem = NULL;
			index *= assoc;

			//Now we know the set; look for a PC match; also identify MRU and LRU items
			for(unsigned int i=index;i<(index+assoc);i++)
			{
				if(btb_data[i].addr == baddr)
				{
					//match
					assert(!pbtb);
					pbtb = &btb_data[i];
				}
				assert(btb_data[i].prev != btb_data[i].next);
				if(btb_data[i].prev == NULL)
				{
					//this is the head of the lru list, ie current MRU item
					assert(lruhead == NULL);
					lruhead = &btb_data[i];
				}
				if(btb_data[i].next == NULL)
				{
					//this is the tail of the lru list, ie the LRU item
					assert(lruitem == NULL);
					lruitem = &btb_data[i];
				}
			}
			assert(lruhead && lruitem);
			if(!pbtb)
			{
				//missed in BTB; choose the LRU item in this set as the victim
				pbtb = lruitem;
			}
			//else hit, and pbtb points to matching BTB entry

			//Update LRU state: selected item, whether selected because it matched or because it was LRU and selected as a victim, becomes MRU
			if(pbtb != lruhead)
			{
				//this splices out the matched entry...
				if(pbtb->prev)
				{
					pbtb->prev->next = pbtb->next;
				}
				if(pbtb->next)
				{
					pbtb->next->prev = pbtb->prev;
				}
				//...and this puts the matched entry at the head of the list
				pbtb->next = lruhead;
				pbtb->prev = NULL;
				lruhead->prev = pbtb;
				assert(pbtb->prev || pbtb->next);
				assert(pbtb->prev != pbtb->next);
			}
			//else pbtb is already MRU item; do nothing
			return pbtb;
		}
		else
		{
			return &btb_data[index];
		}
	}
	return NULL;
#endif
}

void btb_t::update(bpred_btb_ent_t * pbtb, bool taken, md_addr_t baddr, bool correct, md_opcode op, md_addr_t btarget)
{
#ifdef NEW_BTB
	if(!pbtb)
	{
		return;
	}
	unsigned int index = (baddr >> MD_BR_SHIFT) & (btb_data.size() - 1);

	assert(taken);
	for(std::list<bpred_btb_ent_t>::iterator it = btb_data[index].begin(); it!=btb_data[index].end(); ++it)
	{
		if((*it).addr == baddr)
		{
			if(!correct)
			{
				(*it).target = btarget;
				return;
			}
		}
	}
	(*btb_data[index].begin()).addr = baddr;
	(*btb_data[index].begin()).target = btarget;
#else
	//update BTB (but only for taken branches)
	if(pbtb)
	{
		//update current information
		assert(taken);
		if(pbtb->addr == baddr)
		{
			if(!correct)
			{
				pbtb->target = btarget;
			}
		}
		else
		{
			//enter a new branch in the table
			pbtb->addr = baddr;
			pbtb->target = btarget;
		}
	}
#endif
}
