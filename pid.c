#include"pid.h"

#include<vector>
#include<map>
#include<cassert>
#include<cerrno>

pid_handler_t::pid_handler_t()
: next_pid(0x3500)
{}

unsigned long long pid_handler_t::get_new_pid()
{
	process_t temp;
	temp.pid = next_pid++;
	temp.alive = true;
	temp.parent = 0;

	p_list.push_back(temp);

	return temp.pid;
}

bool pid_handler_t::kill_pid(unsigned long long pid, long long return_val)
{
	size_t index = get_entry(pid);
	if((index == (size_t)-1) || (!p_list[index].alive))
	{
		return false;
	}
	p_list[index].alive = false;
	p_list[index].retval = return_val;

	//Erase p_list[index].children here

	return true;
}


bool pid_handler_t::add_child(unsigned long long pid, unsigned long long child_pid)
{
	size_t index = get_entry(pid);
	if(index == (size_t)-1)
	{
		return false;
	}

	size_t c_index = get_entry(child_pid);
	if(c_index == (size_t)-1)
	{
		return false;
	}

	p_list[index].children.push_back(child_pid);
	assert(!p_list[c_index].parent);
	p_list[c_index].parent = pid;

	return true;
}

bool pid_handler_t::get_retval(unsigned long long pid, unsigned long long & waiting_for, unsigned long long & target)
{
	size_t index = get_entry(pid);
	if(index == (size_t)-1)
	{
		return false;
	}
	if(waiting_for != (unsigned long long)-1)
	{
		size_t c_index = get_entry(waiting_for);
		if(p_list[c_index].parent != pid)
		{
			return false;
		}
		if(p_list[c_index].alive)
		{
			return false;
		}
		for(size_t i=0;i<p_list[index].children.size();i++)
		{
			if(p_list[index].children[i] == waiting_for)
			{
				p_list[index].children.erase(p_list[index].children.begin() + i);
				target = p_list[c_index].retval;
				return true;
			}
		}
	}
	else
	{
		for(size_t i=0;i<p_list[index].children.size();i++)
		{
			unsigned long long cur = p_list[index].children[i];
			size_t c_index = get_entry(cur);
			if(!p_list[c_index].alive)
			{
				waiting_for = cur;
				target = p_list[c_index].retval;
				p_list[index].children.erase(p_list[index].children.begin() + i);
				return true;
			}
		}
		if(p_list[index].children.empty())
		{
			target = ECHILD;
			return true;
		}
	}
	return false;
}

bool pid_handler_t::is_retval_avail(unsigned long long pid, unsigned long long waiting_for)
{
	size_t index = get_entry(pid);
	if(index == (size_t)-1)
	{
		return false;
	}
	if(waiting_for != (unsigned long long)-1)
	{
		size_t c_index = get_entry(waiting_for);
		if(p_list[c_index].parent != pid)
		{
			return false;
		}
		if(p_list[c_index].alive)
		{
			return false;
		}
		return true;
	}
	else
	{
		if(p_list[index].children.empty())
		{
			return true;
		}
		for(size_t i=0;i<p_list[index].children.size();i++)
		{
			unsigned long long cur = p_list[index].children[i];
			size_t c_index = get_entry(cur);
			if(!p_list[c_index].alive)
			{
				return true;
			}
		}
	}
	return false;
}

size_t pid_handler_t::get_entry(unsigned long long pid)
{
	for(size_t i=0;i<p_list.size();i++)
	{
		if(p_list[i].pid == pid)
		{
			return i;
		}
	}
	return (size_t)-1;
}

