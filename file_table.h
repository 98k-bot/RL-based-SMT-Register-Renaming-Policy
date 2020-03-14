#ifndef FILE_TABLE_H
#define FILE_TABLE_H

#include"machine.h"
#include<string>
#include<vector>

class file_entry_t
{
	public:
		file_entry_t();
		~file_entry_t();

		md_gpr_t simulated_fd;			//File Descriptor that the simulated code uses
		md_gpr_t real_fd;			//The actual File Descriptor used by the simulator to access files
		std::string filename;			//The filename relative to the execution
		int flags;				//Flags used at opening
		int mode;				//Mode used at opening
		std::string open_string;		//If not empty, used fopen(filename.c_str(),open_string[X]);
							//otherwise, used open(filename.c_str(),flags[X],mode[X]);

		//FIXME: Include F_CLOEXEC flag, then provide exec handling in file_table_t

		unsigned long long reserved;		//This is a bit field used to indicate information needed by the simulator for the file handle

		off_t fake_offset;			//fake offset used for lseek of stdin, stdout, stderr

		void prettyprint(std::ostream & out);

//Values for reserved
//FE_FILE 		-> Regular File
//FE_STANDARD_IO	-> Replicates stdin/stdout/stderr
//FE_FILE_ATTACHED	-> This file is provided by the eio file (should not be seen during simulation, only at initialization of an eio file or creation of one).
		#define FE_FILE				0x00000
		#define	FE_STANDARD_IO			0x00001
		#define FE_PIPE				0x00010
		#define FE_FD_CLOEXEC			0x00100
		#define FE_FILE_ATTACHED		0x10000
};

std::istream & operator>>(std::istream & in, file_entry_t & rhs);
std::ostream & operator<<(std::ostream & in, const file_entry_t & rhs);


class file_table_t
{
	public:
		file_table_t();
		~file_table_t();

		void copy_from(const file_table_t & rhs);

		md_gpr_t get_fd(md_gpr_t simulated_fd);		//Returns the File Descriptor that should be used for real accesses
		bool require_redirect(md_gpr_t & source);	//Returns true if the File Descriptor does not match the "real" File Descriptor
								//Also replaces source with the correct File Descriptor
		int get_entry(md_gpr_t new_fd);			//Returns the entry index for new_fd (creates one if necessary)

		//fopen wrapper that replicates fopen("filename","rw") or similar.
		//Must provide the file handle that is expected by the simulated program
		//This also inserts the data into entries
		FILE * fopener(std::string filename, std::string parameters, md_gpr_t simulated_handle=-1);

		//open wrapper that replaces open(filename, flags, mode) or similar.
		//Must provide the file handle that is expected by the simulated program.
		//This also inserts the data into entries
		md_gpr_t opener(std::string filename, unsigned int flags, unsigned int mode, md_gpr_t simulated_handle=-1);

		md_gpr_t closer(md_gpr_t fd);

		//Assign a simulated file handler to some "real handle". This is used for redirection
		void reassign(unsigned int simulated_handle, unsigned int real_handle, std::string filename);

		//dup, dup2 handler
		md_gpr_t duper(unsigned int handle_old);
		md_gpr_t dup2(unsigned int handle_old, unsigned int handle_new);

		//Returns the lowest available simulated fd.
		md_gpr_t lowest_avail_sim_fd();

		//Insert a file descriptor into the table. These descriptors are not preserved in a checkpoint at this point)
		md_gpr_t insert(md_gpr_t fd, std::string name);

		void prettyprint(std::ostream & out);

		//Closes all files owned by the context. Used when a context exits (not all fds may be explicitly closed)
		void closeall();

		//Handles execve requirements (FD_CLOEXEC)
		void handle_cloexec();

		//lseek handler
		off_t lseeker(md_gpr_t fd, md_gpr_t offset, md_gpr_t whence);
		bool istty(md_gpr_t handle);
		md_gpr_t getfd_cloexec(md_gpr_t handle);
		md_gpr_t setfd_cloexec(md_gpr_t handle, md_gpr_t newval);

	private:
	public:
		std::vector<file_entry_t> entries;		//File Descriptor Entries
};

std::istream & operator>>(std::istream & in, file_table_t & rhs);
std::ostream & operator<<(std::ostream & in, const file_table_t & rhs);

#endif
