/* main.c - main line routines */

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
#include<ctime>
#include<csignal>
#include<sys/types.h>
#include<fcntl.h>
#ifndef _MSC_VER
#include<unistd.h>
#include<sys/time.h>
#endif
#ifdef BFD_LOADER
#include <bfd.h>
#endif /* BFD_LOADER */

#include "host.h"
#include "misc.h"
#include "machine.h"
#include "endian.h"
#include "version.h"
#include "dlite.h"
#include "options.h"
#include "stats.h"
#include "loader.h"
#include "sim.h"
#include "regs.h"
/************** SMT ***************/
#include "smt.h"
/**********************************/
#include "bpreds.h"

#include<vector>
#include<string>
#include<iostream>
#include<fstream>

//stats signal handler
void signal_sim_stats(int sigtype)
{
	sim_dump_stats = TRUE;
}

//exit signal handler
void signal_exit_now(int sigtype)
{
	sim_exit_now = TRUE;
}

#if 0
//6613: JL hates Verizon for trying to rip him off
//not portable... :-(
//total simulator (data) memory usage
unsigned int sim_mem_usage = 0;
#endif

//execution start/end times
time_t sim_start_time;
time_t sim_end_time;
int sim_elapsed_time;

//byte/word swapping required to execute target executable on this host
int sim_swap_bytes;
int sim_swap_words;

//exit when this becomes non-zero
int sim_exit_now = FALSE;

//set to non-zero when simulator should dump statistics
int sim_dump_stats = FALSE;

//options database
opt_odb_t *sim_odb;

//stats database
stat_sdb_t *sim_sdb;

//redirected program/simulator output file names
char *sim_simout = NULL;
char *sim_progout = NULL;
char *sim_progerr = NULL;

//Redirected file handles
md_gpr_t sim_progfd = NULL;
md_gpr_t sim_progerrfd = NULL;

//Used to convert command line arguments into vector format
std::vector<std::string> * v_argv = NULL;
std::vector<std::string> * v_envp = NULL;

//track first argument orphan, this is the program to execute
int exec_index = -1;

//dump help information
int help_me;

//random number generator seed
int rand_seed;

//initialize and quit immediately
int init_quit;

#ifndef _MSC_VER
//simulator scheduling priority
int nice_priority;
#endif

//default simulator scheduling priority
#define NICE_DEFAULT_VALUE		0

//int orphan_fn(int i, int argc, char **argv)
int orphan_fn(int i, int argc, std::vector<std::string> argv)
{
	exec_index = i;
	return /* done */FALSE;
}

void banner(FILE *fd, int argc, char **argv)
{
	char *s;
	fprintf(fd,
		"%s: SimpleScalar/%s Tool Set version %d.%d of %s.\n"
		"Copyright (c) 1994-2003 by Todd M. Austin, Ph.D. and SimpleScalar, LLC.\n"
		"All Rights Reserved. This version of SimpleScalar is licensed for academic non-commercial use.  No portion of this work may be used by any commercial\n"
		"entity, or for any commercial purpose, without the prior written permission of SimpleScalar, LLC (info@simplescalar.com).\n"
		"\n",
		((s = strrchr(argv[0], '/')) ? s+1 : argv[0]),
		VER_TARGET, VER_MAJOR, VER_MINOR, VER_UPDATE);
}

void usage(FILE *fd, int argc, char **argv)
{
	fprintf(fd, "Usage: %s {-options} executable {arguments}\n", argv[0]);
	opt_print_help(sim_odb, fd);
}

int running = FALSE;

//print all simulator stats to output stream fd
void sim_print_stats(FILE *fd)
{
#if 0
	//not portable... :-(
	extern char etext, *sbrk(int);
#endif

	if(!running)
	{
		return;
	}

	//get stats time
	sim_end_time = time((time_t *)NULL);
	sim_elapsed_time = MAX(sim_end_time - sim_start_time, 1);

#if 0
	//not portable... :-(
	//compute simulator memory usag
	sim_mem_usage = (sbrk(0) - &etext) / 1024;
#endif

	//print simulation stats
	fprintf(fd, "\nsim: ** simulation statistics **\n");
	stat_print_stats(sim_sdb, fd);
	sim_aux_stats(fd);
	fprintf(fd, "\n");
}

//print stats, uninitialize simulator components, and exit w/ exitcode
void exit_now(int exit_code)
{
	//print simulation stats and SMT statistics
	sim_print_stats(stderr);
	smt_print_stats();

	//un-initialize the simulator
	sim_uninit();

	//Various cleanup
	if(sim_simout != NULL)
	{
		fclose(stderr);
	}
	if(sim_progout != NULL)
	{
		close(sim_progfd);
	}
	if(sim_progerr != NULL)
	{
		close(sim_progerrfd);
	}
	delete v_argv;
	delete v_envp;
	delete sim_odb;
	delete sim_sdb;

	//all done!
	exit(exit_code);
}

/*
 * Initalizes a thread context. This includes loading the binary into memory,
 * parsing command line options from the argument file, and setting up infile
 * and outfile for redirecting stdin, stdout, and stderr for this process.
 *
 * NOTE: Some of the code in this function, along with the concept of the ".arg"
 * file, was adopted from Dean Tullsen's SMTSIM:
 *
 * Simulaton and Modeling of a Simultaneous Multithreading Processor,
 * D.M. Tullsen, in the 22nd Annual Computer Measurement Group Conference,
 * December, 1996.
 *
 */
void init_thread(std::vector<std::string> env, std::string filename, int curcontext)
{
	char buffer[256];

	// open the file containing the command line arguments for this thread
	FILE * argfile = fopen(filename.c_str(), "r");
	contexts[curcontext].filename = filename;

	if(argfile == NULL)
	{
		std::cerr << "ERROR: cannot open argument file: " << filename << std::endl;
		exit(-1);
	}

	// Anything before the first # is the fastfwd distance.
	int retcode = fscanf(argfile, "%s", buffer);

	if(buffer[0] != '#')
	{
		contexts[curcontext].fastfwd_cnt = atol(buffer);
		contexts[curcontext].fastfwd_left = contexts[curcontext].fastfwd_cnt;
		if(!fscanf(argfile, "%s", buffer)) /* this should be the # */
		{
			fatal("Deformed .arg file, could not find \'#\'");
		}
	}

	std::vector<std::string> locargv;

	//Until we hit a redirecter, we retain the arguments from the .arg file
	bool gather = TRUE;
	while(1)
	{
		retcode = fscanf(argfile, "%s", buffer);

		if(retcode == EOF)
		{
			fclose(argfile);
			std::cerr << "args: ";
			for(unsigned int i=0;i<locargv.size();i++)
			{
				std::cerr << i << ": " << locargv[i] << " ";
			}
			std::cerr << std::endl;

			//actually load the program into memory
			sim_load_prog(locargv[0], locargv, env);
			return;
		}
		//handle input redirection (of stdin)
		else if(std::string(buffer) == "<")
		{
			if(!fscanf(argfile, "%s", buffer))
			{
				fatal("Deformed .arg file, could not read redirected input");
			}
			std::cerr << "Opening " << buffer << " as redirected input:";

			md_gpr_t temp = contexts[curcontext].file_table.opener(buffer,O_RDONLY, S_IRUSR | S_IRWXU | S_IRWXG, 0);
			if(temp == (md_gpr_t)-1)
			{
				std::cerr << " - failed: couldn't open for reading" << std::endl;
				exit(1);
			}
			std::cerr << " fd: " << temp << std::endl;
			gather = FALSE;
		}
		//handle output redirection (of stdout)
		else if(std::string(buffer) == ">")
		{
			if(!fscanf(argfile, "%s", buffer))
			{
				fatal("Deformed .arg file, could not read redirected output (stdout)");
			}
			std::cerr << "Opening " << buffer << " as redirected output (stdout)";
			md_gpr_t temp = contexts[curcontext].file_table.opener(buffer,O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG, 1);
			if(temp == (md_gpr_t)-1)
			{
				std::cerr << " - failed: couldn't open for reading" << std::endl;
				exit(1);
			}
			std::cerr << " fd: " << temp << std::endl;
			gather = FALSE;
		}
		//handle output redirection (of stderr)
		else if(std::string(buffer) == "2>" || std::string(buffer) == ">&" || std::string(buffer) == "2>>")
		{
			if(!fscanf(argfile, "%s", buffer))
			{
				fatal("Deformed .arg file, could not read redirected output (stderr)");
			}
			std::cerr << "Opening " << buffer << " as redirected output (stderr)";
			md_gpr_t temp = contexts[curcontext].file_table.opener(buffer,O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG, 2);
			if(temp == (md_gpr_t)-1)
			{
				std::cerr << " - failed: couldn't open for reading" << std::endl;
				exit(1);
			}
			std::cerr << " fd: " << temp << std::endl;
		}
		if(gather)
		{
			locargv.push_back(buffer);
		}
	}
}

int main(int argc, char **argv, char **envptemp)
{
	//std::vector<std::string> envp;
	v_envp = new std::vector<std::string>;
	std::vector<std::string> & envp = *v_envp;
	for(int i = 0; envptemp[i]; i++)
	{
		envp.push_back(envptemp[i]);

		//Fix locale settings, otherwise it will try to use your environment's LANG setting
		if(envp.back().substr(0,5)=="LANG=")
		{
			envp.back() = "LANG=en_US.ISO8859-1";
		}
	}

	//vector of strings for v_argv
	v_argv = new std::vector<std::string>;

	//Here we pre-process the number of contexts and cores. This enables us to modify the options handler to
	//take parameters for multiple cores - and ultimately, to place multiple contexts
	contexts_at_init_time = 0;
	cores_at_init_time = 1;
	for(int i=0;i<argc;i++)
	{
		std::string temp(argv[i]);
		v_argv->push_back(argv[i]);
		if(temp.find(".arg")!=std::string::npos)
		{
			contexts_at_init_time++;
			contexts.insert(contexts.end(),context());
		}
		if(temp.find("-num_cores")!=std::string::npos)
		{
			cores_at_init_time = atoi(argv[i+1]);
		}
		//If the num_cores option is in the config file, it won't be read in time
		//Here, we can read it early
		if(temp.find("-config")!=std::string::npos)
		{
			std::ifstream infile;
			infile.open(argv[i+1],std::ios::in);
			std::string temp;
			while(infile >> temp)
			{
				if(temp=="-num_cores")
				{
					infile >> cores_at_init_time;
					break;
				}
			}
			infile.close();
		}
	}
	std::cout << contexts_at_init_time << " contexts detected" << std::endl;
	std::cout << cores_at_init_time << " cores specified" <<std::endl;
//	FIXME: Minor
//	Resize is not used since the constructor is only called once
//	This may be ok, but this is a potential source of the uninitialized register value problem
//	contexts.resize(contexts_at_init_time);

#ifndef _MSC_VER
	//catch SIGUSR1 and dump intermediate stats
	signal(SIGUSR1, signal_sim_stats);

	//catch SIGUSR2 and dump final stats and exit
	signal(SIGUSR2, signal_exit_now);
#endif

	//register an error handler
	fatal_hook(sim_print_stats);

	//register global options
	sim_odb = new opt_odb_t(orphan_fn);
	opt_reg_flag(sim_odb, "-h","", "print help message",
		&help_me, /* default */FALSE, /* !print */FALSE, NULL);
	opt_reg_flag(sim_odb, "-v","", "verbose operation",
		&verbose, /* default */FALSE, /* !print */FALSE, NULL);
#ifdef DEBUG
	opt_reg_flag(sim_odb, "-d","", "enable debug message",
		&debugging, /* default */FALSE, /* !print */FALSE, NULL);
#endif

	//FIXME: Can't setup starting in DLite debugger at this point.
	//There are debuggers for each context now - this must be taken into account.
//	opt_reg_flag(sim_odb, "-i","", "start in Dlite debugger",
//		&dlite_active, /* default */FALSE, /* !print */FALSE, NULL);
	opt_reg_int(sim_odb, "-seed","",
		"random number generator seed (0 for timer seed)",
		&rand_seed, /* default */1, /* print */TRUE, NULL);
	opt_reg_flag(sim_odb, "-q","", "initialize and terminate immediately",
		&init_quit, /* default */FALSE, /* !print */FALSE, NULL);

	//stdio, stdout, stderr redirection options
	opt_reg_string(sim_odb, "-redir:sim","",
		"redirect simulator output to file (non-interactive only)",
		&sim_simout, /*default*/ NULL, /*!print*/ FALSE, NULL);
	opt_reg_string(sim_odb, "-redir:prog","",
		"redirect simulated program output to file (all benchmarks)",
		&sim_progout, /* default */NULL, /* !print */FALSE, NULL);
	opt_reg_string(sim_odb, "-redir:err","",
		"redirect simulated program output (stderr) to file (all benchmarks)",
		&sim_progerr, /* default */NULL, /* !print */FALSE, NULL);

#ifndef _MSC_VER
	//scheduling priority option
	opt_reg_int(sim_odb, "-nice","",
		"simulator scheduling priority", &nice_priority,
		/* default */NICE_DEFAULT_VALUE, /* print */TRUE, NULL);
#endif

	//FIXME: add stats intervals and max insts...

	//register all simulator-specific options
	sim_reg_options(sim_odb);

	//parse simulator options
	exec_index = -1;
	opt_process_options(sim_odb, *v_argv);

	//redirect I/O?
	if(sim_simout != NULL)
	{
		//send simulator non-interactive output (STDERR) to file SIM_SIMOUT
		fflush(stderr);
		if(!freopen(sim_simout, "w", stderr))
		{
			fatal("unable to redirect simulator output to file `%s'", sim_simout);
		}
	}

	if(sim_progout != NULL)
	{
		//redirect simulated program output to file SIM_PROGOUT
		sim_progfd = open(sim_progout ,O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if(sim_progfd == (md_gpr_t)-1)
		{
			fatal("unable to redirect program output to file `%s'", sim_progout);
		}
	}

	if(sim_progerr != NULL)
	{
		//redirect simulated program output (stderr) to file SIM_PROGERR
		sim_progerrfd = open(sim_progerr ,O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if(sim_progerrfd == (md_gpr_t)-1)
		{
			fatal("unable to redirect program output (stderr) to file `%s'", sim_progerr);
		}
	}

	//need at least two argv values to run
	if(argc < 2)
	{
		banner(stderr, argc, argv);
		usage(stderr, argc, argv);
		exit(1);
	}

	//opening banner
	banner(stderr, argc, argv);

	if(help_me)
	{
		//print help message and exit
		usage(stderr, argc, argv);
		exit(1);
	}

	//seed the random number generator
	if(rand_seed == 0)
	{
		//seed with the timer value, true random
		mysrand(time((time_t *)NULL));
	}
	else
	{
		//seed with default or user-specified random number generator seed
		mysrand(rand_seed);
	}

	//exec_index is set in orphan_fn()
	if(exec_index == -1)
	{
		//executable was not found
		std::cerr << "error: no executable specified" << std::endl;
		usage(stderr, argc, argv);
		exit(1);
	}
	//else, exec_index points to simulated program arguments

	//check simulator-specific options
	sim_check_options();

#ifndef _MSC_VER
	//set simulator scheduling priority
	if(nice(0) < nice_priority)
	{
		if(nice(nice_priority - nice(0)) < 0)
		{
			fatal("could not renice simulator process");
		}
	}
#endif

#ifdef BFD_LOADER
	//initialize the bfd library
	bfd_init();
#endif // BFD_LOADER

	//initialize the instruction decoder
	md_init_decoder();

	//initialize all simulation modules
	sim_init();

	/******************* SMT ********************/
	//Support added to load multiple binaries
	//initalize the number of contexts to zero
	num_contexts = 0;

	for(int i=0; (i<contexts_at_init_time && (i + exec_index) < argc);i++)
	{
		//initalize the next context
		std::cerr << "sim-main: initializing context " << i << ":" << argv[exec_index + i] << std::endl;
		init_thread(envp, argv[exec_index + i], i);

		//If program output redirection was requested it supercedes all .arg specifications
		if(sim_progfd)
		{
			contexts[i].file_table.reassign(1,sim_progfd,sim_progout);
		}
		if(sim_progerrfd)
		{
			contexts[i].file_table.reassign(2,sim_progerrfd,sim_progerr);
		}
	}
	/******************************************/

	//register all simulator stats
	sim_sdb = new stat_sdb_t;

	sim_reg_stats(sim_sdb);

#if 0
	//not portable... :-(
	stat_reg_uint(sim_sdb, "sim_mem_usage",
		"total simulator (data) memory usage",
		&sim_mem_usage, sim_mem_usage, "%11dk");
#endif

	//record start of execution time, used in rate stats
	sim_start_time = time((time_t *)NULL);

	//emit the command line for later reuse
	std::cerr << "sim: command line: ";
	for(int i=0; i < argc; i++)
	{
		std::cerr << argv[i] << " ";
	}
	std::cerr << std::endl;

	//output simulation conditions
	std::string *s = new std::string(ctime(&sim_start_time));
	s->erase(s->end()-1);	//Last character is always \n, so we delete it.
	std::cerr << "\nsim: simulation started @ " << *s << ", options follow:" << std::endl;
	delete s;
	opt_print_options(sim_odb, stderr, /* short */TRUE, /* notes */TRUE);
	sim_aux_config(stderr);
	std::cerr << std::endl;

	//omit option dump time from rate stats
	sim_start_time = time((time_t *)NULL);

	if(init_quit)
	{
		exit_now(0);
	}

	running = TRUE;
	sim_main();

	//simulation finished early
	exit_now(0);

	return 0;
}
