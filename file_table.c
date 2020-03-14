#include"file_table.h"
#include<cassert>
#include<iostream>
#include<algorithm>
#include<fcntl.h>
#include<unistd.h>
#include<sys/errno.h>

file_entry_t::file_entry_t()
: simulated_fd(-1), real_fd(-1), filename("<NULL>"), flags(0), mode(0), open_string("<NULL>"), reserved(0)
{}

file_entry_t::~file_entry_t()
{
}

void file_entry_t::prettyprint(std::ostream & out)
{
	out << "sim/real: " << simulated_fd << "/" << real_fd << " (" << filename << ") Flags: " << flags << " Mode: " << mode << " Open_String: " << open_string << " Reserved: " << reserved << " Offset: " << lseek(real_fd, 0, SEEK_CUR);
}

std::istream & operator>>(std::istream & in, file_entry_t & rhs)
{
	char temp;
	std::string line_temp;

	in >> rhs.simulated_fd >> temp >> rhs.flags >> temp >> rhs.mode >> temp >> rhs.reserved;
	getline(in,line_temp);
	getline(in,rhs.filename);
	getline(in,rhs.open_string);
	//File_Table handles the opening
	return in;
}

std::ostream & operator<<(std::ostream & out, const file_entry_t & rhs)
{
	out << rhs.simulated_fd << ", " << rhs.flags << ", " << rhs.mode << ", " << rhs.reserved << std::endl;
	out << rhs.filename << std::endl;
	out << rhs.open_string << std::endl;
	//FIXME: Do we FE_FILE_ATTACHED here?
	return out;
}


file_table_t::file_table_t()
: entries(3)
{
	for(size_t i=0;i<3;i++)
	{
		entries[i].simulated_fd = i;
		entries[i].real_fd = i;
		entries[i].reserved = FE_STANDARD_IO;
	}
	//Rely on the constructor for flags, mode and open_string
	entries[0].filename = "stdin";
	entries[1].filename = "stdout";
	entries[2].filename = "stderr";
}

void file_table_t::closeall()
{
	std::cerr << "Context closing, clearing file table: " << std::endl;
	prettyprint(std::cerr);
	for(size_t i=0;i<entries.size();i++)
	{
		//Don't try to close anything that is actually standard IO (this may not matter)...
		if(entries[i].real_fd>2)
		{
			close(entries[i].real_fd);
		}
	}
}

void file_table_t::copy_from(const file_table_t & rhs)
{
	//Assumes that existing file information doesn't need destruction.
	entries.clear();

	for(size_t i=0;i<rhs.entries.size();i++)
	{
		const file_entry_t & cur_entry = rhs.entries[i];

		if(cur_entry.reserved & FE_STANDARD_IO)
		{
			entries.push_back(cur_entry);
			entries.back().real_fd = dup(entries.back().real_fd);
		}
		else if(cur_entry.reserved & FE_PIPE)
		{
			entries.push_back(cur_entry);
			entries.back().real_fd = dup(entries.back().real_fd);
		}
		else if(cur_entry.open_string != "<NULL>" && !(cur_entry.reserved & FE_STANDARD_IO))
		{
			md_gpr_t fd = fileno(fopener(cur_entry.filename, cur_entry.open_string, cur_entry.simulated_fd));
			off_t offset = lseek(cur_entry.real_fd, 0, SEEK_CUR);
			if(offset>=0 && !(cur_entry.reserved & FE_STANDARD_IO))
			{
				lseek(fd, offset, SEEK_SET);
			}
		}
		else if(cur_entry.open_string == "<NULL>")
		{
			md_gpr_t fd = opener(cur_entry.filename, cur_entry.flags, cur_entry.mode, cur_entry.simulated_fd);
			off_t offset = lseek(cur_entry.real_fd, 0, SEEK_CUR);
			if(offset>=0 && !(cur_entry.reserved & FE_STANDARD_IO))
			{
				lseek(fd, offset, SEEK_SET);
			}
		}
	}
}

file_table_t::~file_table_t()
{
	//Nothing is here, operations such as vector::push_back, can call this and cause problems if we actually close files here.
}

md_gpr_t file_table_t::get_fd(md_gpr_t simulated_fd)
{
	for(size_t i=0;i<entries.size();i++)
	{
		if(entries[i].simulated_fd == simulated_fd)
		{
			return entries[i].real_fd;
		}
	}
	return -1;
}

bool file_table_t::require_redirect(md_gpr_t & source)
{
	for(size_t i=0;i<entries.size();i++)
	{
		if(entries[i].simulated_fd == source)
		{
			source = entries[i].real_fd;
			return entries[i].simulated_fd != entries[i].real_fd;
		}
	}
	return false;
}

//FIXME: Can we ensure sorted order such that we can use binary search?
//It probably doesn't matter....
int file_table_t::get_entry(md_gpr_t new_fd)
{
	for(size_t i=0;i<entries.size();i++)
	{
		assert(entries[i].simulated_fd != (md_gpr_t)-1);
		if(entries[i].simulated_fd == new_fd)
		{
			return i;
		}
	}
	entries.push_back(file_entry_t());
	return entries.size()-1;
}

FILE * file_table_t::fopener(std::string filename, std::string parameters, md_gpr_t simulated_handle)
{
	FILE* temp = fopen(filename.c_str(),parameters.c_str());

	//cur_entry points to a new entry or an existing one that we are replacing
	size_t cur_entry = get_entry(simulated_handle);

	assert((entries[cur_entry].simulated_fd==simulated_handle) || (entries[cur_entry].simulated_fd==(md_gpr_t)-1));
	if(simulated_handle==(md_gpr_t)-1)
	{
		entries[cur_entry].simulated_fd = fileno(temp);
	}
	else
	{
		entries[cur_entry].simulated_fd = simulated_handle;
	}
	entries[cur_entry].real_fd = fileno(temp);
	entries[cur_entry].filename = filename;
	entries[cur_entry].flags = 0;
	entries[cur_entry].mode = 0;
	entries[cur_entry].open_string = parameters;
	entries[cur_entry].reserved = 0;

	return temp;
}

md_gpr_t file_table_t::opener(std::string filename, unsigned int flags, unsigned int mode, md_gpr_t simulated_handle)
{
	md_gpr_t temp = open(filename.c_str(), flags, mode);
	if(temp==(md_gpr_t)-1)
	{
		return -1;
	}

	//cur_entry points to a new entry or an existing one that we are replacing
	size_t cur_entry = get_entry(simulated_handle);

	assert((entries[cur_entry].simulated_fd==simulated_handle) || (entries[cur_entry].simulated_fd==(md_gpr_t)-1));

	//FIXME: Make use of lowest_avail_sim_fd here.
	if(simulated_handle==(md_gpr_t)-1)
	{
		std::vector<md_gpr_t> handles;
		for(size_t i=0;i<(entries.size()-1);i++)
		{
			handles.push_back(entries[i].simulated_fd);
		}
		std::sort(handles.begin(),handles.end());
		for(size_t i=0;i<handles.size();i++)
		{
			if(handles[i]!=i)
			{
				entries[cur_entry].simulated_fd = i;
				break;
			}
		}
		if(handles.back()==(handles.size()-1))
		{
			entries[cur_entry].simulated_fd=handles.size();
		}
		assert(entries[cur_entry].simulated_fd!=(md_gpr_t)-1);
	}
	else
	{
		entries[cur_entry].simulated_fd = simulated_handle;
	}
	entries[cur_entry].real_fd = temp;
	entries[cur_entry].filename = filename;
	entries[cur_entry].flags = flags;
	entries[cur_entry].mode = mode;
	entries[cur_entry].open_string = "<NULL>";
	entries[cur_entry].reserved = 0;

	return entries[cur_entry].simulated_fd;
}

md_gpr_t file_table_t::closer(md_gpr_t fd)
{
	size_t cur_entry = get_entry(fd);

	if(entries[cur_entry].reserved <= 2)
	{
		if(entries[cur_entry].simulated_fd==(md_gpr_t)-1)
		{
			return -1;
		}
		entries.erase(entries.begin()+cur_entry);
		return 0;
	}
	md_gpr_t retval(0);

	retval = close(entries[cur_entry].real_fd);
	entries.erase(entries.begin()+cur_entry);
	return retval;
}

void file_table_t::handle_cloexec()
{
	for(size_t i=0;i<entries.size();i++)
	{
		if(entries[i].reserved & FE_FD_CLOEXEC)
		{
			closer(entries[i].simulated_fd);
			i--;
		}
	}
}

md_gpr_t file_table_t::lowest_avail_sim_fd()
{
	std::vector<md_gpr_t> handles;
	for(size_t i=0;i<entries.size();i++)
	{
		handles.push_back(entries[i].simulated_fd);
	}
	std::sort(handles.begin(),handles.end());
	for(size_t i=0;i<handles.size();i++)
	{
		if(handles[i]!=i)
		{
			return i;
		}
	}
	if(handles.back()==(entries.size()-1))
	{
		return handles.size();
	}
	assert(0);
	return -1;
}

md_gpr_t file_table_t::getfd_cloexec(md_gpr_t handle)
{
	size_t cur_entry = get_entry(handle);
	if(entries[cur_entry].simulated_fd!=handle)
	{
		errno = EBADF;
		entries.erase(entries.begin() + cur_entry);
		return 0;
	}
	return !!(entries[cur_entry].reserved & FE_FD_CLOEXEC);
}

md_gpr_t file_table_t::setfd_cloexec(md_gpr_t handle, md_gpr_t newval)
{
	size_t cur_entry = get_entry(handle);
	if(entries[cur_entry].simulated_fd!=handle)
	{
		errno = EBADF;
		entries.erase(entries.begin() + cur_entry);
		return -1;
	}
	assert(newval<=1);

	//if newval is 0, do nothing, otherwise it is 1 and we set FE_FD_CLOEXEC
	entries[cur_entry].reserved |= (FE_FD_CLOEXEC * newval);
	return 0;
}

bool file_table_t::istty(md_gpr_t handle)
{
	size_t cur_entry = get_entry(handle);
	assert(entries[cur_entry].simulated_fd==handle);
	return !!(entries[cur_entry].reserved & FE_STANDARD_IO);
}


md_gpr_t file_table_t::duper(unsigned int _handle_old)
{
	md_gpr_t handle_old = _handle_old;
	if(require_redirect(handle_old))
	{
		std::cerr << "Redirected (old): " << handle_old << "\t";
	}
	size_t old_entry = get_entry(_handle_old);
	unsigned int new_real_fd = dup(handle_old);
	if(new_real_fd==(md_gpr_t)-1)
	{
		new_real_fd = handle_old;
	}
	assert(new_real_fd!=(md_gpr_t)-1);

	md_gpr_t handle_new = lowest_avail_sim_fd();
	size_t cur_entry = get_entry(handle_new);

	assert((entries[cur_entry].simulated_fd==handle_new) || (entries[cur_entry].simulated_fd==(md_gpr_t)-1));
	entries[cur_entry].simulated_fd = handle_new;
	entries[cur_entry].real_fd = new_real_fd;
	entries[cur_entry].filename = entries[old_entry].filename;
	entries[cur_entry].flags = entries[old_entry].flags;
	entries[cur_entry].mode = entries[old_entry].mode;
	entries[cur_entry].open_string = entries[old_entry].open_string;

	//Note, this should not copy the flag: FD_CLOEXEC
	entries[cur_entry].reserved = entries[old_entry].reserved;

	return handle_new;
}

md_gpr_t file_table_t::dup2(unsigned int _handle_old, unsigned int _handle_new)
{
	md_gpr_t handle_old = _handle_old;
	md_gpr_t handle_new = _handle_new;
	if(require_redirect(handle_old))
	{
		std::cerr << "Redirected (old): " << handle_old << "\t";
	}
	closer(handle_new);
	size_t old_entry = get_entry(_handle_old);
	unsigned int new_real_fd = dup(handle_old);
	if(new_real_fd==(md_gpr_t)-1)
	{
		new_real_fd = handle_old;
	}
	assert(new_real_fd!=(md_gpr_t)-1);

	size_t cur_entry = get_entry(handle_new);

	assert((entries[cur_entry].simulated_fd==handle_new) || (entries[cur_entry].simulated_fd==(md_gpr_t)-1));
	entries[cur_entry].simulated_fd = handle_new;
	entries[cur_entry].real_fd = new_real_fd;
	entries[cur_entry].filename = entries[old_entry].filename;
	entries[cur_entry].flags = entries[old_entry].flags;
	entries[cur_entry].mode = entries[old_entry].mode;
	entries[cur_entry].open_string = entries[old_entry].open_string;

	//Note, this should not copy the flag: FD_CLOEXEC
	entries[cur_entry].reserved = entries[old_entry].reserved;

	return handle_new;
}


md_gpr_t file_table_t::insert(md_gpr_t fd, std::string name)
{
	md_gpr_t orig_fd = fd;
	size_t cur_entry = -1;
	fd--;
	do
	{
		fd++;
		cur_entry = get_entry(fd);
	} while(entries[cur_entry].simulated_fd != (md_gpr_t)-1);

	entries[cur_entry].simulated_fd = fd;
	entries[cur_entry].real_fd = orig_fd;
	entries[cur_entry].filename = name;
	entries[cur_entry].flags = 0;
	entries[cur_entry].mode = 0;
	entries[cur_entry].open_string = "<NULL>";
	entries[cur_entry].reserved = FE_PIPE;
	return entries[cur_entry].simulated_fd;
}

void file_table_t::reassign(unsigned int simulated_handle, unsigned int real_handle, std::string filename)
{
	size_t cur_entry = get_entry(simulated_handle);
	assert(entries[cur_entry].simulated_fd==simulated_handle);

	entries[cur_entry].real_fd = real_handle;
	entries[cur_entry].filename = filename;

	//If filename is not NULL or standard IO, remove FE_STANDARD_IO flag
	if((filename != "<NULL>") && (filename != "stdin") && (filename != "stdout") && (filename !="stderr"))
	{
		entries[cur_entry].reserved &= ~FE_STANDARD_IO;
	}
	else
	{
		entries[cur_entry].reserved |= FE_STANDARD_IO;
	}
}

std::istream & operator>>(std::istream & in, file_table_t & rhs)
{
	//This is not expected to be used after initialization...
	char temp;
	size_t count;
	file_entry_t cur_entry;

	in >> temp >> count;
	for(size_t i=0;i<count;i++)
	{
		long long offset;
		in >> temp >> cur_entry >> temp >> offset;

		if(cur_entry.open_string != "<NULL>" && !(cur_entry.reserved & FE_STANDARD_IO))
		{
			md_gpr_t fd = fileno(rhs.fopener(cur_entry.filename, cur_entry.open_string, cur_entry.simulated_fd));
			if(offset>=0)
			{
				lseek(fd, offset, SEEK_SET);
			}
		}
		if(cur_entry.open_string == "<NULL>")
		{
			md_gpr_t fd = rhs.opener(cur_entry.filename, cur_entry.flags, cur_entry.mode, cur_entry.simulated_fd);
			if(offset>=0)
			{
				lseek(fd, offset, SEEK_SET);
			}
		}
	}

	for(size_t i=0;i<rhs.entries.size();i++)
	{
		rhs.entries[i].prettyprint(std::cerr);
		std::cerr << std::endl;
	}

	in >> temp;
	assert(temp==')');
	
	return in;
}

std::ostream & operator<<(std::ostream & out, const file_table_t & rhs)
{
	out << "(" << rhs.entries.size();
	for(size_t i=0;i<rhs.entries.size();i++)
	{
		out << ", " << rhs.entries[i];
		out << ", " << lseek(rhs.entries[i].real_fd, 0, SEEK_CUR);
	}
	out << ")" << std::endl;
	return out;
}

void file_table_t::prettyprint(std::ostream & out)
{
	for(size_t i=0;i<entries.size();i++)
	{
		out << i << ": ";
		entries[i].prettyprint(out);
		out << std::endl;
	}
}
