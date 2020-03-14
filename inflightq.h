/*
 * inflightq.h: Specifies the data structure used for the inflight queues
 *              - event_queue, ready_queue, waiting_queue and issue_exec_queue
 *              This is largely a wrapper for std::list
 *
 * Author: Jason Loew <jloew@cs.binghamton.edu>, January 2009
 *
 */

#ifndef INFLIGHTQ_H
#define INFLIGHTQ_H

#include<list>
#include<functional>

/* Defines a queue of type T which is sorted by the default "<" operator for type T (unless explicitly provided)
 *	//provides a queue of integers which is sorted in ascending order
 *	inflight_queue_t<int>				intq;
 *	//provides a queue of integers which is sorted using ">" (descending order)
 *	inflight_queue_t<int,std::greater<int> >	intq2;
*/
template<typename T, typename Compare = std::less<T> >
class inflight_queue_t
{
	public:
		inflight_queue_t()
		{};

		//Wrapper functions
		inline unsigned int size()
		{
			return data.size();
		}

		inline typename std::list<T>::iterator begin()
		{
			return data.begin();
		}

		inline typename std::list<T>::iterator end()
		{
			return data.end();
		}

		inline typename std::list<T>::iterator erase(const typename std::list<T>::iterator & it)
		{
			return data.erase(it);
		}

		inline void clear()
		{
			return data.clear();
		}

		inline bool empty()
		{
			return data.empty();
		}

		inline void remove(const T & target)
		{
			return data.remove(target);
		}

		void inorderinsert(const T & toinsert)
		{
			typename std::list<T>::iterator it = data.begin();
			while(it!=data.end())
			{
				if(comp(toinsert,(*it)))
				{
					break;
				}
				it++;
			}
			data.insert(it,toinsert);
		}

	private:
		std::list<T> data;
		Compare comp;
};

#endif
