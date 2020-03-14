#ifndef BTB_H
#define BTB_H

#include"machine.h"
#include"host.h"
#include"misc.h"
#include<vector>
#include<list>

//#define NEW_BTB

//an entry in a BTB
class bpred_btb_ent_t
{
	public:
		bpred_btb_ent_t();
		md_addr_t addr;				//address of branch being tracked
		md_addr_t target;			//last destination of branch when taken
#ifdef NEW_BTB
#else
		bpred_btb_ent_t *prev, *next;		//lru chaining pointers
#endif
};

class btb_t
{
	public:
		btb_t(unsigned int btb_sets, unsigned int btb_assoc);

		bpred_btb_ent_t * update_pbtb(bool taken, md_addr_t baddr);

		bpred_btb_ent_t * find_pbtb(md_addr_t baddr);

		void update(bpred_btb_ent_t * pbtb, bool taken, md_addr_t baddr, bool correct, md_opcode op, md_addr_t btarget);

#ifdef NEW_BTB
		std::vector<std::list<bpred_btb_ent_t> > btb_data; 	//BTB addr-prediction table
#else
		size_t sets;				//num BTB sets
		size_t assoc;				//BTB associativity
		std::vector<bpred_btb_ent_t> btb_data; 	//BTB addr-prediction table
#endif
};


#endif
