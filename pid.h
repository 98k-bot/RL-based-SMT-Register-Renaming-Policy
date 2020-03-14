#ifndef PID_H
#define PID_H

#include<iosfwd>
#include<vector>

class pid_handler_t
{
	public:
		pid_handler_t();

		unsigned long long get_new_pid();

		bool add_child(unsigned long long pid, unsigned long long child_pid);

		bool kill_pid(unsigned long long pid, long long return_val);

		bool get_retval(unsigned long long pid, unsigned long long & waiting_for, unsigned long long & target);
		bool is_retval_avail(unsigned long long pid, unsigned long long waiting_for);

	private:
		class process_t
		{
			public:
				unsigned long long pid;
				std::vector<unsigned long long> children;

				bool alive;
				long long retval;

				unsigned long long parent;

				bool operator < (const process_t & rhs) const;
		};

		size_t get_entry(unsigned long long pid);

		std::vector<process_t> p_list;

		unsigned long long next_pid;

};

#endif
