/* I included this copyright since we're using Cacti for some stuff */

/*------------------------------------------------------------
 *  Copyright 1994 Digital Equipment Corporation and Steve Wilton
 *                         All Rights Reserved
 *
 * Permission to use, copy, and modify this software and its documentation is
 * hereby granted only under the following terms and conditions.  Both the
 * above copyright notice and this permission notice must appear in all copies
 * of the software, derivative works or modified versions, and any portions
 * thereof, and both notices must appear in supporting documentation.
 *
 * Users of this software agree to the terms and conditions set forth herein,
 * and hereby grant back to Digital a non-exclusive, unrestricted, royalty-
 * free right and license under any changes, enhancements or extensions
 * made to the core functions of the software, including but not limited to
 * those affording compatibility with other hardware or software
 * environments, but excluding applications which incorporate this software.
 * Users further agree to use their best efforts to return to Digital any
 * such changes, enhancements or extensions that they make and inform Digital
 * of noteworthy uses of this software.  Correspondence should be provided
 * to Digital at:
 *
 *                       Director of Licensing
 *                       Western Research Laboratory
 *                       Digital Equipment Corporation
 *                       100 Hamilton Avenue
 *                       Palo Alto, California  94301
 *
 * This software may be distributed (but not offered for sale or transferred
 * for compensation) to third parties, provided such third parties agree to
 * abide by the terms and conditions of this notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 *------------------------------------------------------------*/

#include<cmath>
#include"power.h"
#include"machine.h"
#include<cassert>

#define SensePowerfactor (Mhz)*(Vdd/2)*(Vdd/2)
#define Sense2Powerfactor (Mhz)*(2*.3+.1*Vdd)
#define Powerfactor (Mhz)*Vdd*Vdd
#define LowSwingPowerfactor (Mhz)*.2*.2

//set scale for crossover (vdd->gnd) currents
double crossover_scaling = 1.2;

//set non-ideal turnoff percentage
double turnoff_factor = 0.1;

#define MSCALE (LSCALE * .624 / .2250)

/*----------------------------------------------------------------------*/

inline int pow2(int x)
{
	return 1<<x;
}

inline double logfour(double x)
{
	if (x<=0)
	{
		fprintf(stderr,"%e\n",x);
	}
	return((double)(log(x)/log(4.0)) );
}

//safer pop count to validate the fast algorithm
int pop_count_slow(quad_t bits)
{
	int count = 0;
	quad_t tmpbits = bits;
	while(tmpbits)
	{
		count += (tmpbits & 1);
		tmpbits >>= 1;
	}
	return count;
}

//fast pop count
int pop_count(quad_t bits)
{
#define T unsigned long long
#define ONES ((T)(-1))
#define TWO(k) ((T)1 << (k))
#define CYCL(k) (ONES/(1 + (TWO(TWO(k)))))
#define BSUM(x,k) ((x)+=(x) >> TWO(k), (x) &= CYCL(k))
	quad_t x = bits;
	x = (x & CYCL(0)) + ((x>>TWO(0)) & CYCL(0));
	x = (x & CYCL(1)) + ((x>>TWO(1)) & CYCL(1));
	BSUM(x,2);
	BSUM(x,3);
	BSUM(x,4);
	BSUM(x,5);
	return x;
}

processor_power::processor_power()
: opcode_length(8), inst_length(32),
rename_power(4,0), bpred_power(4,0), window_power(4,0), lsq_power(4,0), regfile_power(4,0),
icache_power(4,0), dcache_power(4,0), dcache2_power(4,0), dcache3_power(4,0), alu_power(4,0),
falu_power(4,0), resultbus_power(4,0), clock_power(4,0),
total_cycle_power(4,0), last_single_total_cycle_power(4,0), current_total_cycle_power(4,0), max_cycle_power(4,0),
total_rename_access(0), total_bpred_access(0), total_window_access(0), total_lsq_access(0), total_regfile_access(0),
total_icache_access(0), total_dcache_access(0), total_dcache2_access(0), total_dcache3_access(0),
total_alu_access(0), total_resultbus_access(0),
max_rename_access(0),max_bpred_access(0),max_window_access(0),max_lsq_access(0),
max_regfile_access(0),max_icache_access(0),max_dcache_access(0),max_dcache2_access(0),
max_dcache3_access(0),max_alu_access(0),max_resultbus_access(0)
{}

void processor_power::clear_access_stats()
{
	rename_access=bpred_access=window_access=lsq_access=regfile_access=icache_access=dcache_access=0;
	dcache2_access=dcache3_access=alu_access=ialu_access=falu_access=resultbus_access=0;

	window_preg_access=window_selection_access=window_wakeup_access=lsq_store_data_access=0;
	lsq_load_data_access=lsq_wakeup_access=lsq_preg_access=0;

	//FIXME: Is total pop count suppose to be reset each cycle?
	window_total_pop_count_cycle=window_num_pop_count_cycle=lsq_total_pop_count_cycle=0;
	lsq_num_pop_count_cycle=regfile_total_pop_count_cycle=regfile_num_pop_count_cycle=0;
	resultbus_total_pop_count_cycle=resultbus_num_pop_count_cycle=0;
}

//compute bitline activity factors which we use to scale bitline power. Here it is very
//important whether we assume 0's or 1's are responsible for dissipating power in pre-charged
//stuctures. (since most of the bits are 0's, we assume the design is power-efficient enough
//to allow 0's to _not_ discharge
double processor_power::compute_af(counter_t num_pop_count_cycle,counter_t total_pop_count_cycle,int pop_width)
{
	if(num_pop_count_cycle)
	{
		double avg_pop_count = (double)total_pop_count_cycle / (double)num_pop_count_cycle;
		double af = avg_pop_count / (double)pop_width;
		return (1.0-af);
	}
	return 1.0;
}

//compute power statistics on each cycle, for each conditional clocking style.  Obviously most of the speed penalty 
//comes here, so if you don't want per-cycle power estimates you could post-process
//
//See README.wattch for details on the various clock gating styles.
void processor_power::update_power_stats()
{
#ifdef DYNAMIC_AF
	double window_af_b = compute_af(window_num_pop_count_cycle,window_total_pop_count_cycle,data_width);
	double lsq_af_b = compute_af(lsq_num_pop_count_cycle,lsq_total_pop_count_cycle,data_width);
	double regfile_af_b = compute_af(regfile_num_pop_count_cycle,regfile_total_pop_count_cycle,data_width);
	double resultbus_af_b = compute_af(resultbus_num_pop_count_cycle,resultbus_total_pop_count_cycle,data_width);
#endif
	total_rename_access+=rename_access;
	total_bpred_access+=bpred_access;
	total_window_access+=window_access;
	total_lsq_access+=lsq_access;
	total_regfile_access+=regfile_access;
	total_icache_access+=icache_access;
	total_dcache_access+=dcache_access;
	total_dcache2_access+=dcache2_access;
	total_dcache3_access+=dcache3_access;
	total_alu_access+=alu_access;
	total_resultbus_access+=resultbus_access;

	max_rename_access=MAX(rename_access,max_rename_access);
	max_bpred_access=MAX(bpred_access,max_bpred_access);
	max_window_access=MAX(window_access,max_window_access);
	max_lsq_access=MAX(lsq_access,max_lsq_access);
	max_regfile_access=MAX(regfile_access,max_regfile_access);
	max_icache_access=MAX(icache_access,max_icache_access);
	max_dcache_access=MAX(dcache_access,max_dcache_access);
	max_dcache2_access=MAX(dcache2_access,max_dcache2_access);
	max_dcache3_access=MAX(dcache3_access,max_dcache3_access);
	max_alu_access=MAX(alu_access,max_alu_access);
	max_resultbus_access=MAX(resultbus_access,max_resultbus_access);

	rename_power[0]+=power.rename_power;
	if(rename_access)
	{
		rename_power[1]+=power.rename_power;
		rename_power[2]+=((double)rename_access/(double)decode_width)*power.rename_power;
		rename_power[3]+=((double)rename_access/(double)decode_width)*power.rename_power;
	}
	else
	{
		rename_power[3]+=turnoff_factor*power.rename_power;
	}

	bpred_power[0]+=power.bpred_power;
	if(bpred_access)
	{
		if(bpred_access <= 2)
		{
			bpred_power[1]+=power.bpred_power;
		}
		else
		{
			bpred_power[1]+=((double)bpred_access/2.0) * power.bpred_power;
		}
		bpred_power[2]+=((double)bpred_access/2.0) * power.bpred_power;
		bpred_power[3]+=((double)bpred_access/2.0) * power.bpred_power;
	}
	else
	{
		bpred_power[3]+=turnoff_factor*power.bpred_power;
	}

	window_power[0]+=power.window_power;
	lsq_power[0]+=power.lsq_power;
	regfile_power[0]+=power.regfile_power;
	resultbus_power[0]+=power.resultbus;
#ifdef STATIC_AF
	if(window_preg_access)
	{
		if(window_preg_access <= 3*issue_width)
		{
			window_power[1]+=power.rs_power;
		}
		else
		{
			window_power[1]+=((double)window_preg_access/(3.0*(double)issue_width))*power.rs_power;
		}
		window_power[2]+=((double)window_preg_access/(3.0*(double)issue_width))*power.rs_power;
		window_power[3]+=((double)window_preg_access/(3.0*(double)issue_width))*power.rs_power;
	}
	else
	{
		window_power[3]+=turnoff_factor*power.rs_power;
	}

	if(lsq_preg_access)
	{
		if(lsq_preg_access <= res_memport)
		{
			lsq_power[1]+=power.lsq_rs_power;
		}
		else
		{
			lsq_power[1]+=((double)lsq_preg_access/((double)res_memport))*power.lsq_rs_power;
		}
		lsq_power[2]+=((double)lsq_preg_access/((double)res_memport))*power.lsq_rs_power;
		lsq_power[3]+=((double)lsq_preg_access/((double)res_memport))*power.lsq_rs_power;
	}
	else
	{
		lsq_power[3]+=turnoff_factor*power.lsq_rs_power;
	}

	if(regfile_access)
	{
		if(regfile_access <= (3.0*commit_width))
		{
			regfile_power[1]+=power.regfile_power;
		}
		else
		{
			regfile_power[1]+=((double)regfile_access/(3.0*(double)commit_width))*power.regfile_power;
		}
		regfile_power[2]+=((double)regfile_access/(3.0*(double)commit_width))*power.regfile_power;
		regfile_power[3]+=((double)regfile_access/(3.0*(double)commit_width))*power.regfile_power;
	}
	else
	{
		regfile_power[3]+=turnoff_factor*power.regfile_power;
	}

	if(resultbus_access)
	{
		assert(issue_width != 0);
		if(resultbus_access <= issue_width)
		{
			resultbus_power[1]+=power.resultbus;
		}
		else
		{
			resultbus_power[1]+=((double)resultbus_access/(double)issue_width)*power.resultbus;
		}
		resultbus_power[2]+=((double)resultbus_access/(double)issue_width)*power.resultbus;
		resultbus_power[3]+=((double)resultbus_access/(double)issue_width)*power.resultbus;
	}
	else
	{
		resultbus_power[3]+=turnoff_factor*power.resultbus;
	}
#elif defined(DYNAMIC_AF)
	if(window_preg_access)
	{
		if(window_preg_access <= 3*issue_width)
		{
			window_power[1]+=power.rs_power_nobit + window_af_b*power.rs_bitline;
		}
		else
		{
			window_power[1]+=((double)window_preg_access/(3.0*(double)issue_width))*(power.rs_power_nobit + window_af_b*power.rs_bitline);
		}
		window_power[2]+=((double)window_preg_access/(3.0*(double)issue_width))*(power.rs_power_nobit + window_af_b*power.rs_bitline);
		window_power[3]+=((double)window_preg_access/(3.0*(double)issue_width))*(power.rs_power_nobit + window_af_b*power.rs_bitline);
	}
	else
	{
		window_power[3]+=turnoff_factor*power.rs_power;
	}

	if(lsq_preg_access)
	{
		if(lsq_preg_access <= res_memport)
		{
			lsq_power[1]+=power.lsq_rs_power_nobit + lsq_af_b*power.lsq_rs_bitline;
		}
		else
		{
			lsq_power[1]+=((double)lsq_preg_access/((double)res_memport))*(power.lsq_rs_power_nobit + lsq_af_b*power.lsq_rs_bitline);
		}
		lsq_power[2]+=((double)lsq_preg_access/((double)res_memport))*(power.lsq_rs_power_nobit + lsq_af_b*power.lsq_rs_bitline);
		lsq_power[3]+=((double)lsq_preg_access/((double)res_memport))*(power.lsq_rs_power_nobit + lsq_af_b*power.lsq_rs_bitline);
	}
	else
	{
		lsq_power[3]+=turnoff_factor*power.lsq_rs_power;
	}

	if(regfile_access)
	{
		if(regfile_access <= (3.0*commit_width))
		{
			regfile_power[1]+=power.regfile_power_nobit + regfile_af_b*power.regfile_bitline;
		}
		else
		{
			regfile_power[1]+=((double)regfile_access/(3.0*(double)commit_width))*(power.regfile_power_nobit + regfile_af_b*power.regfile_bitline);
		}
		regfile_power[2]+=((double)regfile_access/(3.0*(double)commit_width))*(power.regfile_power_nobit + regfile_af_b*power.regfile_bitline);
		regfile_power[3]+=((double)regfile_access/(3.0*(double)commit_width))*(power.regfile_power_nobit + regfile_af_b*power.regfile_bitline);
	}
	else
	{
		regfile_power[3]+=turnoff_factor*power.regfile_power;
	}

	if(resultbus_access)
	{
		assert(issue_width != 0);
		if(resultbus_access <= issue_width)
		{
			resultbus_power[1]+=resultbus_af_b*power.resultbus;
		}
		else
		{
			resultbus_power[1]+=((double)resultbus_access/(double)issue_width)*resultbus_af_b*power.resultbus;
		}
		resultbus_power[2]+=((double)resultbus_access/(double)issue_width)*resultbus_af_b*power.resultbus;
		resultbus_power[3]+=((double)resultbus_access/(double)issue_width)*resultbus_af_b*power.resultbus;
	}
	else
	{
		resultbus_power[3]+=turnoff_factor*power.resultbus;
	}
#else
	panic("no AF-style defined\n");
#endif

	if(window_selection_access)
	{
		if(window_selection_access <= issue_width)
		{
			window_power[1]+=power.selection;
		}
		else
		{
			window_power[1]+=((double)window_selection_access/((double)issue_width))*power.selection;
		}
		window_power[2]+=((double)window_selection_access/((double)issue_width))*power.selection;
		window_power[3]+=((double)window_selection_access/((double)issue_width))*power.selection;
	}
	else
	{
		window_power[3]+=turnoff_factor*power.selection;
	}

	if(window_wakeup_access)
	{
		if(window_wakeup_access <= issue_width)
		{
			window_power[1]+=power.wakeup_power;
		}
		else
		{
			window_power[1]+=((double)window_wakeup_access/((double)issue_width))*power.wakeup_power;
		}
		window_power[2]+=((double)window_wakeup_access/((double)issue_width))*power.wakeup_power;
		window_power[3]+=((double)window_wakeup_access/((double)issue_width))*power.wakeup_power;
	}
	else
	{
		window_power[3]+=turnoff_factor*power.wakeup_power;
	}

	if(lsq_wakeup_access)
	{
		if(lsq_wakeup_access <= res_memport)
		{
			lsq_power[1]+=power.lsq_wakeup_power;
		}
		else
		{
			lsq_power[1]+=((double)lsq_wakeup_access/((double)res_memport))*power.lsq_wakeup_power;
		}
		lsq_power[2]+=((double)lsq_wakeup_access/((double)res_memport))*power.lsq_wakeup_power;
		lsq_power[3]+=((double)lsq_wakeup_access/((double)res_memport))*power.lsq_wakeup_power;
	}
	else
	{
		lsq_power[3]+=turnoff_factor*power.lsq_wakeup_power;
	}

	icache_power[0]+=power.icache_power+power.itlb;
	if(icache_access)
	{
		//don't scale icache because we assume 1 line is fetched, unless fetch stalls
		icache_power[1]+=power.icache_power+power.itlb;
		icache_power[2]+=power.icache_power+power.itlb;
		icache_power[3]+=power.icache_power+power.itlb;
	}
	else
	{
		icache_power[3]+=turnoff_factor*(power.icache_power+power.itlb);
	}
	dcache_power[0]+=power.dcache_power+power.dtlb;
	if(dcache_access)
	{
		if(dcache_access <= res_memport)
		{
			dcache_power[1]+=power.dcache_power+power.dtlb;
		}
		else
		{
			dcache_power[1]+=((double)dcache_access/(double)res_memport)*(power.dcache_power + power.dtlb);
		}
		dcache_power[2]+=((double)dcache_access/(double)res_memport)*(power.dcache_power + power.dtlb);
		dcache_power[3]+=((double)dcache_access/(double)res_memport)*(power.dcache_power + power.dtlb);
	}
	else
	{
		dcache_power[3]+=turnoff_factor*(power.dcache_power+power.dtlb);
	}

	dcache2_power[0]+=power.dcache2_power;
	if(dcache2_access)
	{
		if(dcache2_access <= res_memport)
		{
			dcache2_power[1]+=power.dcache2_power;
		}
		else
		{
			dcache2_power[1]+=((double)dcache2_access/(double)res_memport)*power.dcache2_power;
		}
		dcache2_power[2]+=((double)dcache2_access/(double)res_memport)*power.dcache2_power;
		dcache2_power[3]+=((double)dcache2_access/(double)res_memport)*power.dcache2_power;
	}
	else
	{
		dcache2_power[3]+=turnoff_factor*power.dcache2_power;
	}

	dcache3_power[0]+=power.dcache3_power;
	if(dcache3_access)
	{
		if(dcache3_access <= res_memport)
		{
			dcache3_power[1]+=power.dcache3_power;
		}
		else
		{
			dcache3_power[1]+=((double)dcache3_access/(double)res_memport)*power.dcache3_power;
		}
		dcache3_power[2]+=((double)dcache3_access/(double)res_memport)*power.dcache3_power;
		dcache3_power[3]+=((double)dcache3_access/(double)res_memport)*power.dcache3_power;
	}
	else
	{
		dcache3_power[3]+=turnoff_factor*power.dcache3_power;
	}

	alu_power[0]+=power.ialu_power + power.falu_power;
	falu_power[0]+=power.falu_power;
	if(alu_access)
	{
		if(ialu_access)
		{
			alu_power[1]+=power.ialu_power;
		}
		else
		{
			alu_power[3]+=turnoff_factor*power.ialu_power;
		}
		if(falu_access)
		{
			alu_power[1]+=power.falu_power;
		}
		else
		{
			alu_power[3]+=turnoff_factor*power.falu_power;
		}
		alu_power[2]+=((double)ialu_access/(double)res_ialu)*power.ialu_power + ((double)falu_access/(double)res_fpalu)*power.falu_power;
		alu_power[3]+=((double)ialu_access/(double)res_ialu)*power.ialu_power + ((double)falu_access/(double)res_fpalu)*power.falu_power;
	}
	else
	{
		alu_power[3]+=turnoff_factor*(power.ialu_power + power.falu_power);
	}

	//Why wasn't falu_power included?
	for(int i=0;i<4;i++)
	{
		total_cycle_power[i] = rename_power[i] + bpred_power[i] + window_power[i] + lsq_power[i]
			+ regfile_power[i] + icache_power[i] + dcache_power[i] + alu_power[i] + resultbus_power[i];

		//for i==0, this is just clock_power[i] += power.clock_power, is it worth a special case?
		clock_power[i] += power.clock_power * (total_cycle_power[i]/total_cycle_power[0]);

		if(i!=0)
		{
			total_cycle_power[i] += clock_power[i];
			current_total_cycle_power[i] = total_cycle_power[i] - last_single_total_cycle_power[i];
			max_cycle_power[i] = MAX(max_cycle_power[i],current_total_cycle_power[i]);
			last_single_total_cycle_power[i] = total_cycle_power[i];
		}
	}
}

//Registers the statistics into the M-sim database
void processor_power::power_reg_stats(stat_sdb_t *sdb)
{
	std::string prepend = "Core_";
	std::stringstream in;
	in << core_id;
	std::string cnum;
	in >> cnum;
	prepend += (cnum + "_");

//This is very ugly, we could fix this up... we probably should. But, it is annoying.
//stat_reg uses the names we feed it for formulas, therefore, they need to match and can't conflict with other names
//This is a problem with stat_reg_formula in particular as each element in the formula requires the appropriate name

	for(int i=0;i<4;i++)
	{
		std::string postpend;
		if(i!=0)
		{
			std::stringstream in;
			in << i;
			in >> postpend;
			postpend = ("_cc" + postpend);
		}
		//Strings that make the registering of stats much easier to read
		//Yes, this is wasteful, but it occurs only once, and is worth making this code sane.
		std::string RENAME_POWER = prepend + "rename_power" + postpend;
		std::string BPRED_POWER = prepend + "bpred_power" + postpend;
		std::string WINDOW_POWER = prepend + "window_power" + postpend;
		std::string LSQ_POWER = prepend + "lsq_power" + postpend;
		std::string REGFILE_POWER = prepend + "regfile_power" + postpend;
		std::string ICACHE_POWER = prepend + "icache_power" + postpend;
		std::string DCACHE_POWER = prepend + "dcache_power" + postpend;
		std::string DCACHE2_POWER = prepend + "dcache2_power" + postpend;
		std::string DCACHE3_POWER = prepend + "dcache3_power" + postpend;
		std::string ALU_POWER = prepend + "alu_power" + postpend;
		std::string FALU_POWER = prepend + "falu_power" + postpend;
		std::string RESULTBUS_POWER = prepend + "resultbus_power" + postpend;
		std::string CLOCK_POWER = prepend + "clock_power" + postpend;
stat_reg_double(sdb, RENAME_POWER, "total power usage of rename unit"+postpend, &rename_power[i], 0, NULL);
stat_reg_double(sdb, BPRED_POWER, "total power usage of bpred unit"+postpend, &bpred_power[i], 0, NULL);
stat_reg_double(sdb, WINDOW_POWER, "total power usage of instruction window"+postpend, &window_power[i], 0, NULL);
stat_reg_double(sdb, LSQ_POWER, "total power usage of load/store queue"+postpend, &lsq_power[i], 0, NULL);
stat_reg_double(sdb, REGFILE_POWER, "total power usage of arch. regfile"+postpend, &regfile_power[i], 0, NULL);
stat_reg_double(sdb, ICACHE_POWER, "total power usage of icache"+postpend, &icache_power[i], 0, NULL);
stat_reg_double(sdb, DCACHE_POWER, "total power usage of dcache"+postpend, &dcache_power[i], 0, NULL);
stat_reg_double(sdb, DCACHE2_POWER, "total power usage of dcache2"+postpend, &dcache2_power[i], 0, NULL);
stat_reg_double(sdb, DCACHE3_POWER, "total power usage of dcache3"+postpend, &dcache3_power[i], 0, NULL);
stat_reg_double(sdb, ALU_POWER, "total power usage of alu"+postpend, &alu_power[i], 0, NULL);
stat_reg_double(sdb, FALU_POWER, "total power usage of falu"+postpend, &falu_power[i], 0, NULL);
stat_reg_double(sdb, RESULTBUS_POWER, "total power usage of resultbus"+postpend, &resultbus_power[i], 0, NULL);
stat_reg_double(sdb, CLOCK_POWER, "total power usage of clock"+postpend, &clock_power[i], 0, NULL);
		std::string AVG_RENAME_POWER = prepend + "avg_rename_power" + postpend;
		std::string AVG_BPRED_POWER = prepend + "avg_bpred_power" + postpend;
		std::string AVG_WINDOW_POWER = prepend + "avg_window_power" + postpend;
		std::string AVG_LSQ_POWER = prepend + "avg_lsq_power" + postpend;
		std::string AVG_REGFILE_POWER = prepend + "avg_regfile_power" + postpend;
		std::string AVG_ICACHE_POWER = prepend + "avg_icache_power" + postpend;
		std::string AVG_DCACHE_POWER = prepend + "avg_dcache_power" + postpend;
		std::string AVG_DCACHE2_POWER = prepend + "avg_dcache2_power" + postpend;
		std::string AVG_DCACHE3_POWER = prepend + "avg_dcache3_power" + postpend;
		std::string AVG_ALU_POWER = prepend + "avg_alu_power" + postpend;
		std::string AVG_FALU_POWER = prepend + "avg_falu_power" + postpend;
		std::string AVG_RESULTBUS_POWER = prepend + "avg_resultbus_power" + postpend;
		std::string AVG_CLOCK_POWER = prepend + "avg_clock_power" + postpend;
		std::string FETCH_STAGE_POWER = prepend + "fetch_stage_power" + postpend;
		std::string DISPATCH_STAGE_POWER = prepend + "dispatch_stage_power" + postpend;
		std::string ISSUE_STAGE_POWER = prepend + "issue_stage_power" + postpend;
		std::string AVG_FETCH_POWER = prepend + "avg_fetch_power" + postpend;
		std::string AVG_DISPATCH_POWER = prepend + "avg_dispatch_power" + postpend;
		std::string AVG_ISSUE_POWER = prepend + "avg_issue_power" + postpend;
		std::string TOTAL_POWER = prepend + "total_power" + postpend;
		std::string AVG_TOTAL_POWER_CYCLE = prepend + "avg_total_power_cycle" + postpend;
		std::string AVG_TOTAL_POWER_INSN = prepend + "avg_total_power_insn" + postpend;
stat_reg_formula(sdb, AVG_RENAME_POWER, "avg power usage of rename unit"+postpend, RENAME_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_BPRED_POWER, "avg power usage of bpred unit"+postpend, BPRED_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_WINDOW_POWER, "avg power usage of instruction window"+postpend, WINDOW_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_LSQ_POWER, "avg power usage of lsq"+postpend, LSQ_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_REGFILE_POWER, "avg power usage of arch. regfile"+postpend, REGFILE_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_ICACHE_POWER, "avg power usage of icache"+postpend, ICACHE_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_DCACHE_POWER, "avg power usage of dcache"+postpend, DCACHE_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_DCACHE2_POWER, "avg power usage of dcache2"+postpend, DCACHE2_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_DCACHE3_POWER, "avg power usage of dcache3"+postpend, DCACHE3_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_ALU_POWER, "avg power usage of alu"+postpend, ALU_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_FALU_POWER, "avg power usage of falu"+postpend, FALU_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_RESULTBUS_POWER, "avg power usage of resultbus"+postpend, RESULTBUS_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_CLOCK_POWER, "avg power usage of clock"+postpend, CLOCK_POWER + "/sim_cycle", NULL);

stat_reg_formula(sdb, FETCH_STAGE_POWER, "total power usage of fetch stage"+postpend, ICACHE_POWER + "+" + BPRED_POWER, NULL);
stat_reg_formula(sdb, DISPATCH_STAGE_POWER, "total power usage of dispatch stage"+postpend, RENAME_POWER, NULL);
stat_reg_formula(sdb, ISSUE_STAGE_POWER, "total power usage of issue stage"+postpend, RESULTBUS_POWER + "+" + ALU_POWER + "+" + DCACHE_POWER + "+" + DCACHE2_POWER + "+" + DCACHE3_POWER + "+" + WINDOW_POWER + "+" + LSQ_POWER, NULL);

stat_reg_formula(sdb, AVG_FETCH_POWER, "average power of fetch unit per cycle"+postpend, FETCH_STAGE_POWER + "/sim_cycle", /* format */NULL);
stat_reg_formula(sdb, AVG_DISPATCH_POWER, "average power of dispatch unit per cycle"+postpend, DISPATCH_STAGE_POWER + "/sim_cycle", /* format */NULL);
stat_reg_formula(sdb, AVG_ISSUE_POWER, "average power of issue unit per cycle"+postpend, ISSUE_STAGE_POWER + "/sim_cycle", /* format */NULL);

stat_reg_formula(sdb, TOTAL_POWER, "total power"+postpend, "(" + RENAME_POWER + "+" + BPRED_POWER + "+" + WINDOW_POWER + "+" + LSQ_POWER + "+" + REGFILE_POWER + "+" + ICACHE_POWER + "+" + RESULTBUS_POWER + "+" + CLOCK_POWER + "+" + ALU_POWER + "+" + DCACHE_POWER + "+" + DCACHE2_POWER + "+" + DCACHE3_POWER + ")", NULL);

stat_reg_formula(sdb, AVG_TOTAL_POWER_CYCLE, "average total power per cycle"+postpend, TOTAL_POWER + "/sim_cycle", NULL);
stat_reg_formula(sdb, AVG_TOTAL_POWER_INSN, "average total power per insn"+postpend, TOTAL_POWER + "/sim_total_insn_" + cnum, NULL);
		if(i!=0)
		{
			std::string MAX_CYCLE_POWER = prepend + "max_cycle_power" + postpend;
			stat_reg_double(sdb, MAX_CYCLE_POWER, "maximum cycle power usage of "+postpend, &max_cycle_power[i], 0, NULL);
		}
	}
//What are these for?
/*
stat_reg_formula(sdb, prepend+"avg_total_power_cycle_nofp_nod2"+postpend, "average total power per cycle"+postpend,
string("(")+prepend+"rename_power+"+prepend+"bpred_power+"+prepend+"window_power+"+prepend+"lsq_power+"+prepend+"regfile_power+"+prepend+"icache_power+"+prepend+"resultbus_power+"+prepend+"clock_power+"+prepend+"alu_power+"+prepend+"dcache_power - "+prepend+"falu_power)/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_total_power_insn_nofp_nod2"+postpend, "average total power per insn"+postpend,
string("(")+prepend+"rename_power+"+prepend+"bpred_power+"+prepend+"window_power+"+prepend+"lsq_power+"+prepend+"regfile_power+"+prepend+"icache_power+"+prepend+"resultbus_power+"+prepend+"clock_power+"+prepend+"alu_power+"+prepend+"dcache_power - "+prepend+"falu_power)/sim_total_insn_"+cnum, NULL);
*/

stat_reg_counter(sdb, prepend+"total_rename_access", "total number accesses of rename unit", &total_rename_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_bpred_access", "total number accesses of bpred unit", &total_bpred_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_window_access", "total number accesses of instruction window", &total_window_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_lsq_access", "total number accesses of load/store queue", &total_lsq_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_regfile_access", "total number accesses of arch. regfile", &total_regfile_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_icache_access", "total number accesses of icache", &total_icache_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_dcache_access", "total number accesses of dcache", &total_dcache_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_dcache2_access", "total number accesses of dcache2", &total_dcache2_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_dcache3_access", "total number accesses of dcache3", &total_dcache3_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_alu_access", "total number accesses of alu", &total_alu_access, 0, NULL);
stat_reg_counter(sdb, prepend+"total_resultbus_access", "total number accesses of resultbus", &total_resultbus_access, 0, NULL);

stat_reg_formula(sdb, prepend+"avg_rename_access", "avg number accesses of rename unit", prepend+"total_rename_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_bpred_access", "avg number accesses of bpred unit", prepend+"total_bpred_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_window_access", "avg number accesses of instruction window", prepend+"total_window_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_lsq_access", "avg number accesses of lsq", prepend+"total_lsq_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_regfile_access", "avg number accesses of arch. regfile", prepend+"total_regfile_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_icache_access", "avg number accesses of icache", prepend+"total_icache_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_dcache_access", "avg number accesses of dcache", prepend+"total_dcache_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_dcache2_access", "avg number accesses of dcache2", prepend+"total_dcache2_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_dcache3_access", "avg number accesses of dcache3", prepend+"total_dcache3_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_alu_access", "avg number accesses of alu", prepend+"total_alu_access/sim_cycle", NULL);
stat_reg_formula(sdb, prepend+"avg_resultbus_access", "avg number accesses of resultbus", prepend+"total_resultbus_access/sim_cycle", NULL);

stat_reg_counter(sdb, prepend+"max_rename_access", "max number accesses of rename unit", &max_rename_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_bpred_access", "max number accesses of bpred unit", &max_bpred_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_window_access", "max number accesses of instruction window", &max_window_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_lsq_access", "max number accesses of load/store queue", &max_lsq_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_regfile_access", "max number accesses of arch. regfile", &max_regfile_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_icache_access", "max number accesses of icache", &max_icache_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_dcache_access", "max number accesses of dcache", &max_dcache_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_dcache2_access", "max number accesses of dcache2", &max_dcache2_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_dcache3_access", "max number accesses of dcache3", &max_dcache3_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_alu_access", "max number accesses of alu", &max_alu_access, 0, NULL);
stat_reg_counter(sdb, prepend+"max_resultbus_access", "max number accesses of resultbus", &max_resultbus_access, 0, NULL);
}

//this routine takes the number of rows and cols of an array structure and attemps to make
//it make it more of a reasonable circuit structure by trying to make the number of rows and
//cols as close as possible. (scaling both by factors of 2 in opposite directions). It
//returns a scale factor which is the amount that the rows should be divided by and the
//columns should be multiplied by.
int processor_power::squarify(int rows, int cols)
{
	if(rows == cols)
	{
		return 1;
	}

	int scale_factor = 1;
	//printf("init rows == %d\n",rows);
	//printf("init cols == %d\n",cols);

	while(rows > cols)
	{
		rows = rows/2;
		cols = cols*2;

		//printf("rows == %d\n",rows);
		//printf("cols == %d\n",cols);
		//printf("scale_factor == %d (2^ == %d)\n\n",scale_factor,(int)pow(2.0,(double)scale_factor));

		if(rows/2 <= cols)
		{
			return((int)pow(2.0,(double)scale_factor));
		}
		scale_factor++;
	}
	return 1;
}

//could improve squarify to work when rows < cols
double processor_power::squarify_new(int rows, int cols)
{
	if(rows==cols)
	{
		return 1;
	}
	double scale_factor = 0.0;

	while(rows > cols)
	{
		rows = rows/2;
		cols = cols*2;
		if(rows <= cols)
		{
			return(pow(2.0,scale_factor));
		}
		scale_factor++;
	}

	while(cols > rows)
	{
		rows = rows*2;
		cols = cols/2;
		if(cols <= rows)
		{
			return(pow(2.0,scale_factor));
		}
		scale_factor--;
	}
	return 1;
}

void processor_power::dump_power_stats(FILE * output)
{
	double ambient_power = 2.0;

	double icache_power = power.icache_power;
	double dcache_power = power.dcache_power;
	double dcache2_power = power.dcache2_power;
	double dcache3_power = power.dcache3_power;

	double itlb_power = power.itlb;
	double dtlb_power = power.dtlb;

	double bpred_power = power.btb + power.local_predict + power.global_predict + power.chooser + power.ras;

//	double rat_power = power.rat_decoder + power.rat_wordline + power.rat_bitline + power.rat_senseamp;
//	double dcl_power = power.dcl_compare + power.dcl_pencode;

	double rename_power = power.rat_power + power.dcl_power + power.inst_decoder_power;

	double wakeup_power = power.wakeup_tagdrive + power.wakeup_tagmatch + power.wakeup_ormatch;

	double rs_power = power.rs_decoder + power.rs_wordline + power.rs_bitline + power.rs_senseamp;

	double window_power = wakeup_power + rs_power + power.selection;

	double lsq_rs_power = power.lsq_rs_decoder + power.lsq_rs_wordline + power.lsq_rs_bitline + power.lsq_rs_senseamp;
	double lsq_wakeup_power = power.lsq_wakeup_tagdrive + power.lsq_wakeup_tagmatch + power.lsq_wakeup_ormatch;
	double lsq_power = lsq_wakeup_power + lsq_rs_power;

//	double reorder_power = power.reorder_decoder + power.reorder_wordline + power.reorder_bitline + power.reorder_senseamp;

	double regfile_power = power.regfile_decoder + power.regfile_wordline + power.regfile_bitline + power.regfile_senseamp;

	double total_power = bpred_power + rename_power + window_power + regfile_power + power.resultbus
		+ lsq_power + icache_power + dcache_power + dcache2_power + dcache3_power + dtlb_power
		+ itlb_power + power.clock_power + power.ialu_power + power.falu_power;

	if(!output)
	{
		return;
	}
	fprintf(output,"\nProcessor %d Parameters:\n",core_id);
	fprintf(output,"Issue Width: %d\n",issue_width);
	fprintf(output,"Window Size (ROB_size * contexts_on_core): %d\n",ROB_size * contexts_on_core);
	fprintf(output,"Number of Virtual Registers: %d\n",MD_NUM_IREGS);
	fprintf(output,"Number of Physical Registers: %d\n",rf_size);
	fprintf(output,"Datapath Width: %d\n",data_width);

	fprintf(output,"Total Power Consumption: %g\n",total_power+ambient_power);
	fprintf(output,"Branch Predictor Power Consumption: %g (%.3g%%)\n",bpred_power,100*bpred_power/total_power);
	fprintf(output," branch target buffer power (W): %g\n",power.btb);
	fprintf(output," local predict power (W): %g\n",power.local_predict);
	fprintf(output," global predict power (W): %g\n",power.global_predict);
	fprintf(output," chooser power (W): %g\n",power.chooser);
	fprintf(output," RAS power (W): %g\n",power.ras);
	fprintf(output,"Rename Logic Power Consumption: %g (%.3g%%)\n",rename_power,100*rename_power/total_power);
	fprintf(output," Instruction Decode Power (W): %g\n",power.inst_decoder_power);
	fprintf(output," RAT decode_power (W): %g\n",power.rat_decoder);
	fprintf(output," RAT wordline_power (W): %g\n",power.rat_wordline);
	fprintf(output," RAT bitline_power (W): %g\n",power.rat_bitline);
	fprintf(output," DCL Comparators (W): %g\n",power.dcl_compare);
	fprintf(output,"Instruction Window Power Consumption: %g (%.3g%%)\n",window_power,100*window_power/total_power);
	fprintf(output," tagdrive (W): %g\n",power.wakeup_tagdrive);
	fprintf(output," tagmatch (W): %g\n",power.wakeup_tagmatch);
	fprintf(output," Selection Logic (W): %g\n",power.selection);
	fprintf(output," decode_power (W): %g\n",power.rs_decoder);
	fprintf(output," wordline_power (W): %g\n",power.rs_wordline);
	fprintf(output," bitline_power (W): %g\n",power.rs_bitline);
	fprintf(output,"Load/Store Queue Power Consumption: %g (%.3g%%)\n",lsq_power,100*lsq_power/total_power);
	fprintf(output," tagdrive (W): %g\n",power.lsq_wakeup_tagdrive);
	fprintf(output," tagmatch (W): %g\n",power.lsq_wakeup_tagmatch);
	fprintf(output," decode_power (W): %g\n",power.lsq_rs_decoder);
	fprintf(output," wordline_power (W): %g\n",power.lsq_rs_wordline);
	fprintf(output," bitline_power (W): %g\n",power.lsq_rs_bitline);
	fprintf(output,"Arch. Register File Power Consumption: %g (%.3g%%)\n",regfile_power,100*regfile_power/total_power);
	fprintf(output," decode_power (W): %g\n",power.regfile_decoder);
	fprintf(output," wordline_power (W): %g\n",power.regfile_wordline);
	fprintf(output," bitline_power (W): %g\n",power.regfile_bitline);
	fprintf(output,"Result Bus Power Consumption: %g (%.3g%%)\n",power.resultbus,100*power.resultbus/total_power);
	fprintf(output,"Total Clock Power: %g (%.3g%%)\n",power.clock_power,100*power.clock_power/total_power);
	fprintf(output,"Int ALU Power: %g (%.3g%%)\n",power.ialu_power,100*power.ialu_power/total_power);
	fprintf(output,"FP ALU Power: %g (%.3g%%)\n",power.falu_power,100*power.falu_power/total_power);
	fprintf(output,"Instruction Cache Power Consumption: %g (%.3g%%)\n",icache_power,100*icache_power/total_power);
	fprintf(output," decode_power (W): %g\n",power.icache_decoder);
	fprintf(output," wordline_power (W): %g\n",power.icache_wordline);
	fprintf(output," bitline_power (W): %g\n",power.icache_bitline);
	fprintf(output," senseamp_power (W): %g\n",power.icache_senseamp);
	fprintf(output," tagarray_power (W): %g\n",power.icache_tagarray);
	fprintf(output,"Itlb_power (W): %g (%.3g%%)\n",power.itlb,100*power.itlb/total_power);
	fprintf(output,"Data Cache Power Consumption: %g (%.3g%%)\n",dcache_power,100*dcache_power/total_power);
	fprintf(output," decode_power (W): %g\n",power.dcache_decoder);
	fprintf(output," wordline_power (W): %g\n",power.dcache_wordline);
	fprintf(output," bitline_power (W): %g\n",power.dcache_bitline);
	fprintf(output," senseamp_power (W): %g\n",power.dcache_senseamp);
	fprintf(output," tagarray_power (W): %g\n",power.dcache_tagarray);
	fprintf(output,"Dtlb_power (W): %g (%.3g%%)\n",power.dtlb,100*power.dtlb/total_power);
	if(cache_dl2)
	{
		fprintf(output,"Level 2 Cache Power Consumption: %g (%.3g%%)\n",dcache2_power,100*dcache2_power/total_power);
		fprintf(output," decode_power (W): %g\n",power.dcache2_decoder);
		fprintf(output," wordline_power (W): %g\n",power.dcache2_wordline);
		fprintf(output," bitline_power (W): %g\n",power.dcache2_bitline);
		fprintf(output," senseamp_power (W): %g\n",power.dcache2_senseamp);
		fprintf(output," tagarray_power (W): %g\n",power.dcache2_tagarray);
	}
	if(cache_dl3)
	{
		fprintf(output,"Level 3 Cache Power Consumption: %g (%.3g%%)\n",dcache3_power,100*dcache3_power/total_power);
		fprintf(output," decode_power (W): %g\n",power.dcache3_decoder);
		fprintf(output," wordline_power (W): %g\n",power.dcache3_wordline);
		fprintf(output," bitline_power (W): %g\n",power.dcache3_bitline);
		fprintf(output," senseamp_power (W): %g\n",power.dcache3_senseamp);
		fprintf(output," tagarray_power (W): %g\n",power.dcache3_tagarray);
	}
}

/*======================================================================*/
//This part of the code contains routines for each section as described in
//the tech report.  See the tech report for more details and explanations
/*----------------------------------------------------------------------*/

double processor_power::driver_size(double driving_cap, double desiredrisetime)
{
	double Rpdrive = desiredrisetime/(driving_cap*log(VSINV)*-1.0);
	double psize = restowidth(Rpdrive,PCH);
//	double nsize = restowidth(Rpdrive,NCH);
	if(psize > Wworddrivemax)
	{
		psize = Wworddrivemax;
	}
	if(psize < 4.0 * LSCALE)
	{
		psize = 4.0 * LSCALE;
	}
	return psize;
}

//Decoder delay: (see section 6.1 of tech report)
double processor_power::array_decoder_power(int rows,int cols,double predeclength,int rports,int wports,int cache)
{
	//read and write ports are the same here
	int ports = rports + wports;

	double rowsb = (double)rows;

	//number of input bits to be decoded
	int decode_bits=(int)ceil((logtwo(rowsb)));

	//First stage: driving the decoders

	//This is the capacitance for driving one bit (and its complement).
	//     -There are #rowsb 3->8 decoders contributing gatecap.
	//     - 2.0 factor from 2 identical sets of drivers in parallel
	double Ceq = 2.0*(draincap(Wdecdrivep,PCH,1)+draincap(Wdecdriven,NCH,1)) + gatecap(Wdec3to8n+Wdec3to8p,10.0)*rowsb;

	//There are ports * #decode_bits total
	double Ctotal=ports*decode_bits*Ceq;

	if(verbose)
	{
		fprintf(stderr,"Decoder -- Driving decoders == %g\n",.3*Ctotal*Powerfactor);
	}

	//second stage: driving a bunch of nor gates with a nand numstack is the
	//size of the nor gates -- ie. a 7-128 decoder has 3-input NAND followed by 3-input NOR

	int numstack = (int)ceil((1.0/3.0)*logtwo(rows));

	numstack = MAX(numstack,1);
	numstack = MIN(5,numstack);

	//There are #rowsb NOR gates being driven
	Ceq = (3.0*draincap(Wdec3to8p,PCH,1) +draincap(Wdec3to8n,NCH,3) + gatecap(WdecNORn+WdecNORp,((numstack*40)+20.0)))*rowsb;

	Ctotal+=ports*Ceq;

	if(verbose)
	{
		fprintf(stderr,"Decoder -- Driving nor w/ nand == %g\n",.3*ports*Ceq*Powerfactor);
	}

	//Final stage: driving an inverter with the nor (inverter preceding wordline driver)
	// -- wordline driver is in the next section*/

	Ceq = (gatecap(Wdecinvn+Wdecinvp,20.0)+numstack*draincap(WdecNORn,NCH,1)+draincap(WdecNORp,PCH,numstack));

	if(verbose)
	{
		fprintf(stderr,"Decoder -- Driving inverter w/ nor == %g\n",.3*ports*Ceq*Powerfactor);
	}

	Ctotal+=ports*Ceq;

	//assume Activity Factor == .3
	return(.3*Ctotal*Powerfactor);
}

double processor_power::simple_array_decoder_power(int rows,int cols,int rports,int wports,int cache)
{
	return(array_decoder_power(rows,cols,0.0,rports,wports,cache));
}


double processor_power::array_wordline_power(int rows,int cols,double wordlinelength,int rports,int wports,int cache)
{
	int ports = rports+wports;

	double colsb = (double)cols;

	//Calculate size of wordline drivers assuming rise time == Period / 8
	//	- estimate cap on line
	//	- compute min resistance to achieve this with RC
	//	- compute width needed to achieve this resistance */

	double desiredrisetime = Period/16;
	double Cline = (gatecappass(Wmemcellr,1.0))*colsb + wordlinelength*CM3metal;
	double psize = driver_size(Cline,desiredrisetime);

	//how do we want to do p-n ratioing? -- here we just assume the same ratio from an inverter pair
	double nsize = psize * Wdecinvn/Wdecinvp;

	if(verbose)
	{
		fprintf(stderr,"Wordline Driver Sizes -- nsize == %f, psize == %f\n",nsize,psize);
	}

	double Ceq = draincap(Wdecinvn,NCH,1) + draincap(Wdecinvp,PCH,1) + gatecap(nsize+psize,20.0);

	double Ctotal=ports*Ceq;

	if(verbose)
	{
		fprintf(stderr,"Wordline -- Inverter -> Driver == %g\n",ports*Ceq*Powerfactor);
	}

	//Compute caps of read wordline and write wordlines
	//	- wordline driver caps, given computed width from above
	//	- read wordlines have 1 nmos access tx, size ~4
	//	- write wordlines have 2 nmos access tx, size ~2
	//	- metal line cap

	double Cliner = (gatecappass(Wmemcellr,(BitWidth-2*Wmemcellr)/2.0))*colsb+
		wordlinelength*CM3metal+ 2.0*(draincap(nsize,NCH,1) + draincap(psize,PCH,1));
	double Clinew = (2.0*gatecappass(Wmemcellw,(BitWidth-2*Wmemcellw)/2.0))*colsb+
		wordlinelength*CM3metal+ 2.0*(draincap(nsize,NCH,1) + draincap(psize,PCH,1));

	if(verbose)
	{
		fprintf(stderr,"Wordline -- Line == %g\n",1e12*Cline);
		fprintf(stderr,"Wordline -- Line -- access -- gatecap == %g\n",1e12*colsb*2*gatecappass(Wmemcella,(BitWidth-2*Wmemcella)/2.0));
		fprintf(stderr,"Wordline -- Line -- driver -- draincap == %g\n",1e12*draincap(nsize,NCH,1) + draincap(psize,PCH,1));
		fprintf(stderr,"Wordline -- Line -- metal == %g\n",1e12*wordlinelength*CM3metal);
	}
	Ctotal+=rports*Cliner+wports*Clinew;

	//AF == 1 assuming a different wordline is charged each cycle, but only 1 wordline (per port) is actually used
	return(Ctotal*Powerfactor);
}

double processor_power::simple_array_wordline_power(int rows,int cols,int rports,int wports,int cache)
{
	int ports = rports + wports;
	double wordlinelength = cols * (RegCellWidth + 2 * ports * BitlineSpacing);
	return(array_wordline_power(rows,cols,wordlinelength,rports,wports,cache));
}

double processor_power::array_bitline_power(int rows,int cols,double bitlinelength,int rports,int wports,int cache) {
	double Ctotal=0;
	double Ccolmux=0;
	double Cwritebitdrive=0;
	double Cliner=0;
	double Clinew=0;
	//int ports = rports + wports;

	//Draincaps of access tx's

	double Cbitrowr = draincap(Wmemcellr,NCH,1);
	double Cbitroww = draincap(Wmemcellw,NCH,1);

	//Cprerow -- precharge cap on the bitline
	//	-simple scheme to estimate size of pre-charge tx's in a similar fashion to wordline driver size estimation.
	//	-FIXME: it would be better to use precharge/keeper pairs, i've omitted this from this version because it couldn't autosize as easily.

	double desiredrisetime = Period/8;

	double Cline = static_cast<double>(rows)*Cbitrowr+CM2metal*bitlinelength;
	double psize = driver_size(Cline,desiredrisetime);

	//compensate for not having an nmos pre-charging
	psize = psize + psize * Wdecinvn/Wdecinvp;

	if(verbose)
	{
		printf("Cprerow auto == %g (psize == %g)\n",draincap(psize,PCH,1),psize);
	}

	double Cprerow = draincap(psize,PCH,1);

	//Cpregate -- cap due to gatecap of precharge transistors -- tack this onto bitline cap, again this could have a keeper
	double Cpregate = 4.0*gatecap(psize,10.0);
	global_clockcap+=rports*2.0*Cpregate*static_cast<double>(cols);

	//Cwritebitdrive -- write bitline drivers are used instead of the precharge stuff for write bitlines
	//	- 2 inverter drivers within each driver pair */

	Cline = static_cast<double>(rows)*Cbitroww+CM2metal*bitlinelength;

	psize = driver_size(Cline,desiredrisetime);
	double nsize = psize * Wdecinvn/Wdecinvp;

	Cwritebitdrive = 2.0*(draincap(psize,PCH,1)+draincap(nsize,NCH,1));

	//	reg files (cache==0)
	//	=> single ended bitlines (1 bitline/col)
	//	=> AFs from pop_count
	//	caches (cache ==1)
	//	=> double-ended bitlines (2 bitlines/col)
	//	=> AFs = .5 (since one of the two bitlines is always charging/discharging)

#ifdef STATIC_AF
//There is only one difference between these two cases. (cache==0, code line 5)
	if(cache == 0
	{
		//compute the total line cap for read/write bitlines
		Cliner = static_cast<double>(rows)*Cbitrowr+CM2metal*bitlinelength+Cprerow;
		Clinew = static_cast<double>(rows)*Cbitroww+CM2metal*bitlinelength+Cwritebitdrive;

		//Bitline inverters at the end of the bitlines (replaced w/ sense amps in cache styles)
		Ccolmux = gatecap(MSCALE*(29.9+7.8),0.0)+gatecap(MSCALE*(47.0+12.0),0.0);
		Ctotal += (1.0-POPCOUNT_AF)*rports*cols*(Cliner+Ccolmux+2.0*Cpregate);
		Ctotal += .3*wports*cols*(Clinew+Cwritebitdrive);
	}
	else
	{
		Cliner = static_cast<double>(rows)*Cbitrowr+CM2metal*bitlinelength+Cprerow + draincap(Wbitmuxn,NCH,1);
		Clinew = static_cast<double>(rows)*Cbitroww+CM2metal*bitlinelength+Cwritebitdrive;
		Ccolmux = (draincap(Wbitmuxn,NCH,1))+2.0*gatecap(WsenseQ1to4,10.0);
		Ctotal+=.5*rports*2.0*cols*(Cliner+Ccolmux+2.0*Cpregate);
		Ctotal+=.5*wports*2.0*cols*(Clinew+Cwritebitdrive);
	}
#elif defined(DYNAMIC_AF)
	if(cache == 0)
	{
		//compute the total line cap for read/write bitlines
		Cliner = static_cast<double>(rows)*Cbitrowr+CM2metal*bitlinelength+Cprerow;
		Clinew = static_cast<double>(rows)*Cbitroww+CM2metal*bitlinelength+Cwritebitdrive;

		//Bitline inverters at the end of the bitlines (replaced w/ sense amps in cache styles)
		Ccolmux = gatecap(MSCALE*(29.9+7.8),0.0)+gatecap(MSCALE*(47.0+12.0),0.0);
		Ctotal += rports*cols*(Cliner+Ccolmux+2.0*Cpregate);
		Ctotal += .3*wports*cols*(Clinew+Cwritebitdrive);
	}
	else
	{
		Cliner = static_cast<double>(rows)*Cbitrowr+CM2metal*bitlinelength+Cprerow + draincap(Wbitmuxn,NCH,1);
		Clinew = static_cast<double>(rows)*Cbitroww+CM2metal*bitlinelength+Cwritebitdrive;
		Ccolmux = (draincap(Wbitmuxn,NCH,1))+2.0*gatecap(WsenseQ1to4,10.0);
		Ctotal+=.5*rports*2.0*cols*(Cliner+Ccolmux+2.0*Cpregate);
		Ctotal+=.5*wports*2.0*cols*(Clinew+Cwritebitdrive);
	}
#endif

	if(verbose)
	{
		fprintf(stderr,"Bitline -- Precharge == %g\n",1e12*Cpregate);
		fprintf(stderr,"Bitline -- Line == %g\n",1e12*(Cliner+Clinew));
		fprintf(stderr,"Bitline -- Line -- access draincap == %g\n",1e12*static_cast<double>(rows)*Cbitrowr);
		fprintf(stderr,"Bitline -- Line -- precharge draincap == %g\n",1e12*Cprerow);
		fprintf(stderr,"Bitline -- Line -- metal == %g\n",1e12*bitlinelength*CM2metal);
		fprintf(stderr,"Bitline -- Colmux == %g\n",1e12*Ccolmux);
		fprintf(stderr,"\n");
	}

	if(cache==0)
	{
		return(Ctotal*Powerfactor);
	}
	else
	{
		return(Ctotal*SensePowerfactor*.4);
	}
}

double processor_power::simple_array_bitline_power(int rows,int cols,int rports,int wports,int cache)
{
	int ports = rports + wports;
	double bitlinelength = rows * (RegCellHeight + ports * WordlineSpacing);
	return (array_bitline_power(rows,cols,bitlinelength,rports,wports,cache));
}

//estimate senseamp power dissipation in cache structures (Zyuban's method)
double processor_power::senseamp_power(int cols)
{
	return((double)cols * Vdd/8 * .5e-3);
}

//estimate comparator power consumption (this comparator is similar to the tag-match structure in a CAM
double processor_power::compare_cap(int compare_bits)
{
	//bottom part of comparator
	double c2 = (compare_bits)*(draincap(Wcompn,NCH,1)+draincap(Wcompn,NCH,2))+ draincap(Wevalinvp,PCH,1) + draincap(Wevalinvn,NCH,1);

	//top part of comparator
	double c1 = (compare_bits)*(draincap(Wcompn,NCH,1)+draincap(Wcompn,NCH,2)+ draincap(Wcomppreequ,NCH,1))
		+ gatecap(WdecNORn,1.0)+ gatecap(WdecNORp,3.0);
	return(c1 + c2);
}

//power of depency check logic
double processor_power::dcl_compare_power(int compare_bits)
{
	int num_comparators = (decode_width - 1) * (decode_width);
	double Ctotal = num_comparators * compare_cap(compare_bits);
	return(Ctotal*Powerfactor*AF);
}

double processor_power::simple_array_power(int rows,int cols,int rports,int wports,int cache)
{
	if(cache==0)
	{
		return(simple_array_decoder_power(rows,cols,rports,wports,cache)
			+simple_array_wordline_power(rows,cols,rports,wports,cache)
			+simple_array_bitline_power(rows,cols,rports,wports,cache));
	}
	else
	{
		return(simple_array_decoder_power(rows,cols,rports,wports,cache)
			+simple_array_wordline_power(rows,cols,rports,wports,cache)
			+simple_array_bitline_power(rows,cols,rports,wports,cache)
			+senseamp_power(cols));
	}
}

double processor_power::cam_tagdrive(int rows,int cols,int rports,int wports)
{
	int ports = rports + wports;

	double taglinelength = rows * (CamCellHeight + ports * MatchlineSpacing);
	double wordlinelength = cols * (CamCellWidth + ports * TaglineSpacing);

	//Compute tagline cap
	double Ctlcap = Cmetal * taglinelength + rows * gatecappass(Wcomparen2,2.0)
		+ draincap(Wcompdrivern,NCH,1)+draincap(Wcompdriverp,PCH,1);

	//Compute bitline cap (for writing new tags)
	double Cblcap = Cmetal * taglinelength + rows * draincap(Wmemcellr,NCH,2);

	//autosize wordline driver
	double psize = driver_size(Cmetal * wordlinelength + 2 * cols * gatecap(Wmemcellr,2.0),Period/8);
	double nsize = psize * Wdecinvn/Wdecinvp;

	//Compute wordline cap (for writing new tags)
	double Cwlcap = Cmetal * wordlinelength + draincap(nsize,NCH,1)+draincap(psize,PCH,1)
		+ 2 * cols * gatecap(Wmemcellr,2.0);

	double Ctotal = (rports * cols * 2 * Ctlcap) + (wports * ((cols * 2 * Cblcap) + (rows * Cwlcap)));

	return(Ctotal*Powerfactor*AF);
}

double processor_power::cam_tagmatch(int rows,int cols,int rports,int wports)
{
	int ports = rports + wports;

	double matchlinelength = cols * (CamCellWidth + ports * TaglineSpacing);

	double Cmlcap = 2 * cols * draincap(Wcomparen1,NCH,2) + Cmetal * matchlinelength + draincap(Wmatchpchg,NCH,1)
		+ gatecap(Wmatchinvn+Wmatchinvp,10.0) + gatecap(Wmatchnandn+Wmatchnandp,10.0);

	double Ctotal = rports * rows * Cmlcap;

	global_clockcap += rports * rows * gatecap(Wmatchpchg,5.0);

	//noring the nanded match lines
	if(issue_width >= 8)
	{
		Ctotal += 2 * gatecap(Wmatchnorn+Wmatchnorp,10.0);
	}
	return(Ctotal*Powerfactor*AF);
}

double processor_power::cam_array(int rows,int cols,int rports,int wports)
{
	return(cam_tagdrive(rows,cols,rports,wports) + cam_tagmatch(rows,cols,rports,wports));
}

double processor_power::selection_power(int win_entries)
{
	int num_arbiter=1;

	while(win_entries > 4)
	{
		win_entries = (int)ceil((double)win_entries / 4.0);
		num_arbiter += win_entries;
	}

	double Cor = 4 * draincap(WSelORn,NCH,1) + draincap(WSelORprequ,PCH,1);

	double Cpencode = draincap(WSelPn,NCH,1) + draincap(WSelPp,PCH,1)
		+ 2*draincap(WSelPn,NCH,1) + draincap(WSelPp,PCH,2)
		+ 3*draincap(WSelPn,NCH,1) + draincap(WSelPp,PCH,3)
		+ 4*draincap(WSelPn,NCH,1) + draincap(WSelPp,PCH,4)
		+ 4*gatecap(WSelEnn+WSelEnp,20.0)
		+ 4*draincap(WSelEnn,NCH,1) + 4*draincap(WSelEnp,PCH,1);

	double Ctotal = issue_width * num_arbiter*(Cor+Cpencode);

	return(Ctotal*Powerfactor*AF);
}

//very rough clock power estimates
double processor_power::total_clockpower(double die_length)
{
	int npreg_width = (int)ceil(logtwo((double)ROB_size * contexts_on_core));

	//Assume say 8 stages (kinda low now).
     	//FIXME: this could be a lot better; user could input number of pipestages, etc

	//assume 8 pipe stages and try to estimate bits per pipe stage

	//pipe stage 0/1
	double num_piperegs = issue_width*inst_length + data_width;

	//pipe stage 1/2
	num_piperegs += issue_width*(inst_length + 3 * ROB_size * contexts_on_core);

	//pipe stage 2/3
	num_piperegs += issue_width*(inst_length + 3 * ROB_size * contexts_on_core);

	//pipe stage 3/4
	num_piperegs += issue_width*(3 * npreg_width + pow2(opcode_length));

	//pipe stage 4/5
	num_piperegs += issue_width*(2*data_width + pow2(opcode_length));

	//pipe stage 5/6
	num_piperegs += issue_width*(data_width + pow2(opcode_length));

	//pipe stage 6/7
	num_piperegs += issue_width*(data_width + pow2(opcode_length));

	//pipe stage 7/8
	num_piperegs += issue_width*(data_width + pow2(opcode_length));

	//assume 50% extra in control signals (rule of thumb)
	num_piperegs = num_piperegs * 1.5;

	double pipereg_clockcap = num_piperegs * 4*gatecap(10.0,0);

	//estimate based on 3% of die being in clock metal
	double Cline2 = Cmetal * (.03 * die_length * die_length/BitlineSpacing) * 1e6 * 1e6;

	//another estimate
	double clocklinelength = die_length*(.5 + 4 * (.25 + 2*(.25) + 4 * (.125)));
	double Cline = 20 * Cmetal * (clocklinelength) * 1e6;
	double global_buffercap = 12*gatecap(1000.0,10.0)+16*gatecap(200,10.0)+16*8*2*gatecap(100.0,10.00) + 2*gatecap(.29*1e6,10.0);

	//global_clockcap is computed within each array structure for pre-charge tx's
	double Ctotal = Cline+global_clockcap+pipereg_clockcap+global_buffercap;

	if(verbose)
	{
		fprintf(stderr,"num_piperegs == %f\n",num_piperegs);
	}

	//add I_ADD Clockcap and F_ADD Clockcap
	double Clockpower = Ctotal*Powerfactor + res_ialu*I_ADD_CLOCK + res_fpalu*F_ADD_CLOCK;

	if(verbose)
	{
		fprintf(stderr,"Global Clock Power: %g\n",Clockpower);
		fprintf(stderr," Global Metal Lines (W): %g\n",Cline*Powerfactor);
		fprintf(stderr," Global Metal Lines (3%%) (W): %g\n",Cline2*Powerfactor);
		fprintf(stderr," Global Clock Buffers (W): %g\n",global_buffercap*Powerfactor);
		fprintf(stderr," Global Clock Cap (Explicit) (W): %g\n",global_clockcap*Powerfactor+I_ADD_CLOCK+F_ADD_CLOCK);
		fprintf(stderr," Global Clock Cap (Implicit) (W): %g\n",pipereg_clockcap*Powerfactor);
	}
	return(Clockpower);
}

//Very rough global clock power estimates
double processor_power::global_clockpower(double die_length)
{
	double Cline2 = Cmetal * (.03 * die_length * die_length/BitlineSpacing) * 1e6 * 1e6;

	double clocklinelength = die_length*(.5 + 4 * (.25 + 2*(.25) + 4 * (.125)));
	double Cline = 20 * Cmetal * (clocklinelength) * 1e6;
	double global_buffercap = 12*gatecap(1000.0,10.0)+16*gatecap(200,10.0)+16*8*2*gatecap(100.0,10.00) + 2*gatecap(.29*1e6,10.0);
	double Ctotal = Cline+global_buffercap;

	if(verbose)
	{
		fprintf(stderr,"Global Clock Power: %g\n",Ctotal*Powerfactor);
		fprintf(stderr," Global Metal Lines (W): %g\n",Cline*Powerfactor);
		fprintf(stderr," Global Metal Lines (3%%) (W): %g\n",Cline2*Powerfactor);
		fprintf(stderr," Global Clock Buffers (W): %g\n",global_buffercap*Powerfactor);
	}
	return(Ctotal*Powerfactor);
}

double processor_power::compute_resultbus_power()
{
	//compute size of result bus tags
	int npreg_width = (int)ceil(logtwo((double)ROB_size * contexts_on_core));

	double regfile_height = rf_size * (RegCellHeight + WordlineSpacing * 3 * issue_width);

	//assume num alu's == ialu (FIXME: generate a more detailed result bus network model
	double Cline = Cmetal * (regfile_height + .5 * res_ialu * 3200.0 * LSCALE);

	//or use result bus length measured from 21264 die photo
	//Cline = Cmetal * 3.3*1000;

	//Assume ruu_issue_width result busses -- power can be scaled linearly
	// for number of result busses (scale by writeback_access)
	double Ctotal = 2.0 * (data_width + npreg_width) * (issue_width) * Cline;

#ifdef STATIC_AF
	Ctotal*=AF;
#endif
	return Ctotal*Powerfactor;
}

void processor_power::calculate_power(FILE * output)
{
	double clockpower;
	int ndwl, ndbl, nspd, ntwl, ntbl, ntspd, c,b,a,rowsb, colsb;
	int trowsb, tcolsb, tagsize;
	int va_size = 48;

	int npreg_width = (int)ceil(logtwo((double)ROB_size * contexts_on_core));

	//these variables are needed to use Cacti to auto-size cache arrays (for optimal delay)
	time_result_type time_result;
	time_parameter_type time_parameters;

	//used to autosize other structures, like bpred tables
	int scale_factor;

	global_clockcap = 0;

	int cache=0;

	//FIXME: ALU power is a simple constant, it would be better to include bit AFs and have different
	//numbers for different types of operations
	power.ialu_power = res_ialu * I_ADD;
	power.falu_power = res_fpalu * F_ADD;

	nvreg_width = (int)ceil(logtwo((double)MD_NUM_IREGS));
	npreg_width = (int)ceil(logtwo((double)ROB_size * contexts_on_core));

	//RAT has shadow bits stored in each cell, this makes the cell size
	//larger than normal array structures, so we must compute it here
	double predeclength = MD_NUM_IREGS * (RatCellHeight + 3 * decode_width * WordlineSpacing);
	double wordlinelength = npreg_width * (RatCellWidth + 6 * decode_width * BitlineSpacing + RatShiftRegWidth*RatNumShift);
	double bitlinelength = MD_NUM_IREGS * (RatCellHeight + 3 * decode_width * WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"rat power stats\n");
	}
	power.rat_decoder = array_decoder_power(MD_NUM_IREGS,npreg_width,predeclength,2*decode_width,decode_width,cache);
	power.rat_wordline = array_wordline_power(MD_NUM_IREGS,npreg_width,wordlinelength,2*decode_width,decode_width,cache);
	power.rat_bitline = array_bitline_power(MD_NUM_IREGS,npreg_width,bitlinelength,2*decode_width,decode_width,cache);
	power.rat_senseamp = 0;


	power.dcl_compare = dcl_compare_power(nvreg_width);
	power.dcl_pencode = 0;
	power.inst_decoder_power = decode_width * simple_array_decoder_power(opcode_length,1,1,1,cache);
	power.wakeup_tagdrive =cam_tagdrive(iq_size,npreg_width,issue_width,issue_width);
	power.wakeup_tagmatch =cam_tagmatch(iq_size,npreg_width,issue_width,issue_width);
	power.wakeup_ormatch =0;

	power.selection = selection_power(iq_size);

	predeclength = MD_NUM_IREGS * (RegCellHeight + 3 * issue_width * WordlineSpacing);
	wordlinelength = data_width * (RegCellWidth + 6 * issue_width * BitlineSpacing);
	bitlinelength = MD_NUM_IREGS * (RegCellHeight + 3 * issue_width * WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"regfile power stats\n");
	}

	power.regfile_decoder = array_decoder_power(MD_NUM_IREGS,data_width,predeclength,2*issue_width,issue_width,cache);
	power.regfile_wordline = array_wordline_power(MD_NUM_IREGS,data_width,wordlinelength,2*issue_width,issue_width,cache);
	power.regfile_bitline = array_bitline_power(MD_NUM_IREGS,data_width,bitlinelength,2*issue_width,issue_width,cache);
	power.regfile_senseamp =0;

	predeclength = rf_size * (RegCellHeight + 3 * issue_width * WordlineSpacing);
	wordlinelength = data_width * (RegCellWidth + 6 * issue_width * BitlineSpacing);
	bitlinelength = rf_size * (RegCellHeight + 3 * issue_width * WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"res station power stats\n");
	}
	power.rs_decoder = array_decoder_power(rf_size,data_width,predeclength,2*issue_width,issue_width,cache);
	power.rs_wordline = array_wordline_power(rf_size,data_width,wordlinelength,2*issue_width,issue_width,cache);
	power.rs_bitline = array_bitline_power(rf_size,data_width,bitlinelength,2*issue_width,issue_width,cache);

	//no senseamps in reg file structures (only caches)
	power.rs_senseamp =0;

	//addresses go into lsq tag's
	power.lsq_wakeup_tagdrive =cam_tagdrive(LSQ_size,data_width,res_memport,res_memport);
	power.lsq_wakeup_tagmatch =cam_tagmatch(LSQ_size,data_width,res_memport,res_memport);
	power.lsq_wakeup_ormatch =0;

	wordlinelength = data_width * (RegCellWidth + 4 * res_memport * BitlineSpacing);
	bitlinelength = rf_size * (RegCellHeight + 4 * res_memport * WordlineSpacing);

	//rs's hold data
	if(verbose)
	{
		fprintf(stderr,"lsq station power stats\n");
	}
	power.lsq_rs_decoder = array_decoder_power(LSQ_size,data_width,predeclength,res_memport,res_memport,cache);
	power.lsq_rs_wordline = array_wordline_power(LSQ_size,data_width,wordlinelength,res_memport,res_memport,cache);
	power.lsq_rs_bitline = array_bitline_power(LSQ_size,data_width,bitlinelength,res_memport,res_memport,cache);
	power.lsq_rs_senseamp =0;

	power.resultbus = compute_resultbus_power();

	//Load cache values into what cacti is expecting
	time_parameters.cache_size = btb_config[0] * (data_width/8) * btb_config[1];	//C
	time_parameters.block_size = (data_width/8);					//B
	time_parameters.associativity = btb_config[1];					//A
	time_parameters.number_of_sets = btb_config[0];					//C/(B*A)

	//have Cacti compute optimal cache config
	time_result = cacti_calculate_time(&time_parameters);
	cacti_output_data(&time_result,&time_parameters,output);

	// extract Cacti results
	ndwl=time_result.best_Ndwl;
	ndbl=time_result.best_Ndbl;
	nspd=time_result.best_Nspd;
	ntwl=time_result.best_Ntwl;
	ntbl=time_result.best_Ntbl;
	ntspd=time_result.best_Ntspd;
	c = time_parameters.cache_size;
	b = time_parameters.block_size;
	a = time_parameters.associativity;

	cache=1;

	//Figure out how many rows/cols there are now
	rowsb = c/(b*a*ndbl*nspd);
	colsb = 8*b*a*nspd/ndwl;

	if(verbose)
	{
		fprintf(stderr,"%d KB %d-way btb (%d-byte block size):\n",c,a,b);
		fprintf(stderr,"ndwl == %d, ndbl == %d, nspd == %d\n",ndwl,ndbl,nspd);
		fprintf(stderr,"%d sets of %d rows x %d cols\n",ndwl*ndbl,rowsb,colsb);
	}

	predeclength = rowsb * (RegCellHeight + WordlineSpacing);
	wordlinelength = colsb * (RegCellWidth + BitlineSpacing);
	bitlinelength = rowsb * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"btb power stats\n");
	}
	power.btb = ndwl*ndbl*(array_decoder_power(rowsb,colsb,predeclength,1,1,cache)
		+ array_wordline_power(rowsb,colsb,wordlinelength,1,1,cache)
		+ array_bitline_power(rowsb,colsb,bitlinelength,1,1,cache) + senseamp_power(colsb));

	cache=1;

	scale_factor = squarify(twolev_config[0],twolev_config[2]);
	predeclength = (twolev_config[0] / scale_factor)* (RegCellHeight + WordlineSpacing);
	wordlinelength = twolev_config[2] * scale_factor * (RegCellWidth + BitlineSpacing);
	bitlinelength = (twolev_config[0] / scale_factor) * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"local predict power stats\n");
	}

	power.local_predict = array_decoder_power(twolev_config[0]/scale_factor,twolev_config[2]*scale_factor,predeclength,1,1,cache)
		+ array_wordline_power(twolev_config[0]/scale_factor,twolev_config[2]*scale_factor,wordlinelength,1,1,cache)
		+ array_bitline_power(twolev_config[0]/scale_factor,twolev_config[2]*scale_factor,bitlinelength,1,1,cache)
		+ senseamp_power(twolev_config[2]*scale_factor);

	scale_factor = squarify(twolev_config[1],3);

	predeclength = (twolev_config[1] / scale_factor)* (RegCellHeight + WordlineSpacing);
	wordlinelength = 3 * scale_factor * (RegCellWidth + BitlineSpacing);
	bitlinelength = (twolev_config[1] / scale_factor) * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"local predict power stats\n");
	}
	power.local_predict += array_decoder_power(twolev_config[1]/scale_factor,3*scale_factor,predeclength,1,1,cache)
		+ array_wordline_power(twolev_config[1]/scale_factor,3*scale_factor,wordlinelength,1,1,cache)
		+ array_bitline_power(twolev_config[1]/scale_factor,3*scale_factor,bitlinelength,1,1,cache)
		+ senseamp_power(3*scale_factor);

	if(verbose)
	{
		fprintf(stderr,"bimod_config[0] == %d\n",bimod_config[0]);
	}

	scale_factor = squarify(bimod_config[0],2);

	predeclength = bimod_config[0]/scale_factor * (RegCellHeight + WordlineSpacing);
	wordlinelength = 2*scale_factor * (RegCellWidth + BitlineSpacing);
	bitlinelength = bimod_config[0]/scale_factor * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"global predict power stats\n");
	}
	power.global_predict = array_decoder_power(bimod_config[0]/scale_factor,2*scale_factor,predeclength,1,1,cache)
		+ array_wordline_power(bimod_config[0]/scale_factor,2*scale_factor,wordlinelength,1,1,cache)
		+ array_bitline_power(bimod_config[0]/scale_factor,2*scale_factor,bitlinelength,1,1,cache)
		+ senseamp_power(2*scale_factor);

	scale_factor = squarify(comb_config[0],2);

	predeclength = comb_config[0]/scale_factor * (RegCellHeight + WordlineSpacing);
	wordlinelength = 2*scale_factor * (RegCellWidth + BitlineSpacing);
	bitlinelength = comb_config[0]/scale_factor * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"chooser predict power stats\n");
	}
	power.chooser = array_decoder_power(comb_config[0]/scale_factor,2*scale_factor,predeclength,1,1,cache)
		+ array_wordline_power(comb_config[0]/scale_factor,2*scale_factor,wordlinelength,1,1,cache)
		+ array_bitline_power(comb_config[0]/scale_factor,2*scale_factor,bitlinelength,1,1,cache)
		+ senseamp_power(2*scale_factor);

	if(verbose)
	{
		fprintf(stderr,"RAS predict power stats\n");
	}
	power.ras = simple_array_power(ras_size,data_width,1,1,0);

	tagsize = va_size - ((int)logtwo(cache_dl1->nsets) + (int)logtwo(cache_dl1->bsize));

	if(verbose)
	{
		fprintf(stderr,"dtlb predict power stats\n");
	}
	power.dtlb = res_memport*(cam_array(dtlb->nsets, va_size - (int)logtwo((double)dtlb->bsize),1,1)
		+ simple_array_power(dtlb->nsets,tagsize,1,1,cache));

	tagsize = va_size - ((int)logtwo(cache_il1->nsets) + (int)logtwo(cache_il1->bsize));

	predeclength = itlb->nsets * (RegCellHeight + WordlineSpacing);
	wordlinelength = logtwo((double)itlb->bsize) * (RegCellWidth + BitlineSpacing);
	bitlinelength = itlb->nsets * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"itlb predict power stats\n");
	}
	power.itlb = cam_array(itlb->nsets, va_size - (int)logtwo((double)itlb->bsize),1,1)
		+ simple_array_power(itlb->nsets,tagsize,1,1,cache);

	cache=1;

	time_parameters.cache_size = cache_il1->nsets * cache_il1->bsize * cache_il1->assoc; 	//C
	time_parameters.block_size = cache_il1->bsize;						//B
	time_parameters.associativity = cache_il1->assoc;					//A
	time_parameters.number_of_sets = cache_il1->nsets;					//C/(B*A)

	time_result = cacti_calculate_time(&time_parameters);
	cacti_output_data(&time_result,&time_parameters,output);

	ndwl=time_result.best_Ndwl;
	ndbl=time_result.best_Ndbl;
	nspd=time_result.best_Nspd;
	ntwl=time_result.best_Ntwl;
	ntbl=time_result.best_Ntbl;
	ntspd=time_result.best_Ntspd;

	c = time_parameters.cache_size;
	b = time_parameters.block_size;
	a = time_parameters.associativity;

	rowsb = c/(b*a*ndbl*nspd);
	colsb = 8*b*a*nspd/ndwl;

	tagsize = va_size - ((int)logtwo(cache_il1->nsets) + (int)logtwo(cache_il1->bsize));
	trowsb = c/(b*a*ntbl*ntspd);
	tcolsb = a * (tagsize + 1 + 6) * ntspd/ntwl;
 
	if(verbose)
	{
		fprintf(stderr,"%d KB %d-way cache (%d-byte block size):\n",c,a,b);
		fprintf(stderr,"ndwl == %d, ndbl == %d, nspd == %d\n",ndwl,ndbl,nspd);
		fprintf(stderr,"%d sets of %d rows x %d cols\n",ndwl*ndbl,rowsb,colsb);
		fprintf(stderr,"tagsize == %d\n",tagsize);
	}

	predeclength = rowsb * (RegCellHeight + WordlineSpacing);
	wordlinelength = colsb * (RegCellWidth + BitlineSpacing);
	bitlinelength = rowsb * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"icache power stats\n");
	}
	power.icache_decoder = ndwl*ndbl*array_decoder_power(rowsb,colsb,predeclength,1,1,cache);
	power.icache_wordline = ndwl*ndbl*array_wordline_power(rowsb,colsb,wordlinelength,1,1,cache);
	power.icache_bitline = ndwl*ndbl*array_bitline_power(rowsb,colsb,bitlinelength,1,1,cache);
	power.icache_senseamp = ndwl*ndbl*senseamp_power(colsb);
	power.icache_tagarray = ntwl*ntbl*(simple_array_power(trowsb,tcolsb,1,1,cache));

	power.icache_power = power.icache_decoder + power.icache_wordline + power.icache_bitline 
		+ power.icache_senseamp + power.icache_tagarray;

	time_parameters.cache_size = cache_dl1->nsets * cache_dl1->bsize * cache_dl1->assoc; 	//C
	time_parameters.block_size = cache_dl1->bsize;						//B
	time_parameters.associativity = cache_dl1->assoc;					//A
	time_parameters.number_of_sets = cache_dl1->nsets;					//C/(B*A)

	time_result = cacti_calculate_time(&time_parameters);
	cacti_output_data(&time_result,&time_parameters,output);

	ndwl=time_result.best_Ndwl;
	ndbl=time_result.best_Ndbl;
	nspd=time_result.best_Nspd;
	ntwl=time_result.best_Ntwl;
	ntbl=time_result.best_Ntbl;
	ntspd=time_result.best_Ntspd;
	c = time_parameters.cache_size;
	b = time_parameters.block_size;
	a = time_parameters.associativity;

	cache=1;

	rowsb = c/(b*a*ndbl*nspd);
	colsb = 8*b*a*nspd/ndwl;

	tagsize = va_size - ((int)logtwo(cache_dl1->nsets) + (int)logtwo(cache_dl1->bsize));
	trowsb = c/(b*a*ntbl*ntspd);
	tcolsb = a * (tagsize + 1 + 6) * ntspd/ntwl;

	if(verbose)
	{
		fprintf(stderr,"%d KB %d-way cache (%d-byte block size):\n",c,a,b);
		fprintf(stderr,"ndwl == %d, ndbl == %d, nspd == %d\n",ndwl,ndbl,nspd);
		fprintf(stderr,"%d sets of %d rows x %d cols\n",ndwl*ndbl,rowsb,colsb);
		fprintf(stderr,"tagsize == %d\n",tagsize);
		fprintf(stderr,"\nntwl == %d, ntbl == %d, ntspd == %d\n",ntwl,ntbl,ntspd);
		fprintf(stderr,"%d sets of %d rows x %d cols\n",ntwl*ntbl,trowsb,tcolsb);
	}

	predeclength = rowsb * (RegCellHeight + WordlineSpacing);
	wordlinelength = colsb * (RegCellWidth + BitlineSpacing);
	bitlinelength = rowsb * (RegCellHeight + WordlineSpacing);

	if(verbose)
	{
		fprintf(stderr,"dcache power stats\n");
	}
	power.dcache_decoder = res_memport*ndwl*ndbl*array_decoder_power(rowsb,colsb,predeclength,1,1,cache);
	power.dcache_wordline = res_memport*ndwl*ndbl*array_wordline_power(rowsb,colsb,wordlinelength,1,1,cache);
	power.dcache_bitline = res_memport*ndwl*ndbl*array_bitline_power(rowsb,colsb,bitlinelength,1,1,cache);
	power.dcache_senseamp = res_memport*ndwl*ndbl*senseamp_power(colsb);
	power.dcache_tagarray = res_memport*ntwl*ntbl*(simple_array_power(trowsb,tcolsb,1,1,cache));

	power.dcache_power = power.dcache_decoder + power.dcache_wordline + power.dcache_bitline
		+ power.dcache_senseamp + power.dcache_tagarray;

	clockpower = total_clockpower(.018);
	power.clock_power = clockpower;
	if(verbose)
	{
		fprintf(stderr,"result bus power == %f\n",power.resultbus);
		fprintf(stderr,"global clock power == %f\n",clockpower);
	}

	if(cache_dl2)
	{
		time_parameters.cache_size = cache_dl2->nsets * cache_dl2->bsize * cache_dl2->assoc; 	//C
		time_parameters.block_size = cache_dl2->bsize;						//B
		time_parameters.associativity = cache_dl2->assoc;					//A
		time_parameters.number_of_sets = cache_dl2->nsets;					//C/(B*A)

		time_result = cacti_calculate_time(&time_parameters);
		cacti_output_data(&time_result,&time_parameters,output);

		ndwl=time_result.best_Ndwl;
		ndbl=time_result.best_Ndbl;
		nspd=time_result.best_Nspd;
		ntwl=time_result.best_Ntwl;
		ntbl=time_result.best_Ntbl;
		ntspd=time_result.best_Ntspd;
		c = time_parameters.cache_size;
		b = time_parameters.block_size;
		a = time_parameters.associativity;

		rowsb = c/(b*a*ndbl*nspd);
		colsb = 8*b*a*nspd/ndwl;

		tagsize = va_size - ((int)logtwo(cache_dl2->nsets) + (int)logtwo(cache_dl2->bsize));
		trowsb = c/(b*a*ntbl*ntspd);
		tcolsb = a * (tagsize + 1 + 6) * ntspd/ntwl;

		if(verbose)
		{
			fprintf(stderr,"%d KB %d-way cache (%d-byte block size):\n",c,a,b);
			fprintf(stderr,"ndwl == %d, ndbl == %d, nspd == %d\n",ndwl,ndbl,nspd);
			fprintf(stderr,"%d sets of %d rows x %d cols\n",ndwl*ndbl,rowsb,colsb);
			fprintf(stderr,"tagsize == %d\n",tagsize);
		}

		predeclength = rowsb * (RegCellHeight + WordlineSpacing);
		wordlinelength = colsb * (RegCellWidth + BitlineSpacing);
		bitlinelength = rowsb * (RegCellHeight + WordlineSpacing);

		if(verbose)
		{
			fprintf(stderr,"dcache2 power stats\n");
		}
		power.dcache2_decoder = array_decoder_power(rowsb,colsb,predeclength,1,1,cache);
		power.dcache2_wordline = array_wordline_power(rowsb,colsb,wordlinelength,1,1,cache);
		power.dcache2_bitline = array_bitline_power(rowsb,colsb,bitlinelength,1,1,cache);
		power.dcache2_senseamp = senseamp_power(colsb);
		power.dcache2_tagarray = simple_array_power(trowsb,tcolsb,1,1,cache);

		power.dcache2_power = power.dcache2_decoder + power.dcache2_wordline + power.dcache2_bitline
			+ power.dcache2_senseamp + power.dcache2_tagarray;
	}
	else
	{
		power.dcache2_decoder = 0;
		power.dcache2_wordline = 0;
		power.dcache2_bitline = 0;
		power.dcache2_senseamp = 0;
		power.dcache2_tagarray = 0;
		power.dcache2_power = 0;
	}
	if(cache_dl3)
	{
		time_parameters.cache_size = cache_dl3->nsets * cache_dl3->bsize * cache_dl3->assoc; 	//C
		time_parameters.block_size = cache_dl3->bsize;						//B
		time_parameters.associativity = cache_dl3->assoc;					//A
		time_parameters.number_of_sets = cache_dl3->nsets;					//C/(B*A)

		time_result = cacti_calculate_time(&time_parameters);
		cacti_output_data(&time_result,&time_parameters,output);

		ndwl=time_result.best_Ndwl;
		ndbl=time_result.best_Ndbl;
		nspd=time_result.best_Nspd;
		ntwl=time_result.best_Ntwl;
		ntbl=time_result.best_Ntbl;
		ntspd=time_result.best_Ntspd;
		c = time_parameters.cache_size;
		b = time_parameters.block_size;
		a = time_parameters.associativity;

		rowsb = c/(b*a*ndbl*nspd);
		colsb = 8*b*a*nspd/ndwl;

		tagsize = va_size - ((int)logtwo(cache_dl3->nsets) + (int)logtwo(cache_dl3->bsize));
		trowsb = c/(b*a*ntbl*ntspd);
		tcolsb = a * (tagsize + 1 + 6) * ntspd/ntwl;

		if(verbose)
		{
			fprintf(stderr,"%d KB %d-way cache (%d-byte block size):\n",c,a,b);
			fprintf(stderr,"ndwl == %d, ndbl == %d, nspd == %d\n",ndwl,ndbl,nspd);
			fprintf(stderr,"%d sets of %d rows x %d cols\n",ndwl*ndbl,rowsb,colsb);
			fprintf(stderr,"tagsize == %d\n",tagsize);
		}

		predeclength = rowsb * (RegCellHeight + WordlineSpacing);
		wordlinelength = colsb * (RegCellWidth + BitlineSpacing);
		bitlinelength = rowsb * (RegCellHeight + WordlineSpacing);

		if(verbose)
		{
			fprintf(stderr,"dcache3 power stats\n");
		}
		power.dcache3_decoder = array_decoder_power(rowsb,colsb,predeclength,1,1,cache);
		power.dcache3_wordline = array_wordline_power(rowsb,colsb,wordlinelength,1,1,cache);
		power.dcache3_bitline = array_bitline_power(rowsb,colsb,bitlinelength,1,1,cache);
		power.dcache3_senseamp = senseamp_power(colsb);
		power.dcache3_tagarray = simple_array_power(trowsb,tcolsb,1,1,cache);

		power.dcache3_power = power.dcache3_decoder + power.dcache3_wordline + power.dcache3_bitline
			+ power.dcache3_senseamp + power.dcache3_tagarray;
	}
	else
	{
		power.dcache3_decoder = 0;
		power.dcache3_wordline = 0;
		power.dcache3_bitline = 0;
		power.dcache3_senseamp = 0;
		power.dcache3_tagarray = 0;
		power.dcache3_power = 0;
	}
	power.rat_decoder *= crossover_scaling;
	power.rat_wordline *= crossover_scaling;
	power.rat_bitline *= crossover_scaling;

	power.dcl_compare *= crossover_scaling;
	power.dcl_pencode *= crossover_scaling;
	power.inst_decoder_power *= crossover_scaling;
	power.wakeup_tagdrive *= crossover_scaling;
	power.wakeup_tagmatch *= crossover_scaling;
	power.wakeup_ormatch *= crossover_scaling;

	power.selection *= crossover_scaling;

	power.regfile_decoder *= crossover_scaling;
	power.regfile_wordline *= crossover_scaling;
	power.regfile_bitline *= crossover_scaling;
	power.regfile_senseamp *= crossover_scaling;

	power.rs_decoder *= crossover_scaling;
	power.rs_wordline *= crossover_scaling;
	power.rs_bitline *= crossover_scaling;
	power.rs_senseamp *= crossover_scaling;

	power.lsq_wakeup_tagdrive *= crossover_scaling;
	power.lsq_wakeup_tagmatch *= crossover_scaling;

	power.lsq_rs_decoder *= crossover_scaling;
	power.lsq_rs_wordline *= crossover_scaling;
	power.lsq_rs_bitline *= crossover_scaling;
	power.lsq_rs_senseamp *= crossover_scaling;

	power.resultbus *= crossover_scaling;

	power.btb *= crossover_scaling;
	power.local_predict *= crossover_scaling;
	power.global_predict *= crossover_scaling;
	power.chooser *= crossover_scaling;

	power.dtlb *= crossover_scaling;

	power.itlb *= crossover_scaling;

	power.icache_decoder *= crossover_scaling;
	power.icache_wordline*= crossover_scaling;
	power.icache_bitline *= crossover_scaling;
	power.icache_senseamp*= crossover_scaling;
	power.icache_tagarray*= crossover_scaling;

	power.icache_power *= crossover_scaling;

	power.dcache_decoder *= crossover_scaling;
	power.dcache_wordline *= crossover_scaling;
	power.dcache_bitline *= crossover_scaling;
	power.dcache_senseamp *= crossover_scaling;
	power.dcache_tagarray *= crossover_scaling;

	power.dcache_power *= crossover_scaling;

	power.clock_power *= crossover_scaling;

	power.dcache2_decoder *= crossover_scaling;
	power.dcache2_wordline *= crossover_scaling;
	power.dcache2_bitline *= crossover_scaling;
	power.dcache2_senseamp *= crossover_scaling;
	power.dcache2_tagarray *= crossover_scaling;

	power.dcache2_power *= crossover_scaling;

	power.dcache3_decoder *= crossover_scaling;
	power.dcache3_wordline *= crossover_scaling;
	power.dcache3_bitline *= crossover_scaling;
	power.dcache3_senseamp *= crossover_scaling;
	power.dcache3_tagarray *= crossover_scaling;

	power.dcache3_power *= crossover_scaling;

	power.total_power = power.local_predict + power.global_predict
		+ power.chooser + power.btb
		+ power.rat_decoder + power.rat_wordline
		+ power.rat_bitline + power.rat_senseamp
		+ power.dcl_compare + power.dcl_pencode
		+ power.inst_decoder_power
		+ power.wakeup_tagdrive + power.wakeup_tagmatch
		+ power.selection
		+ power.regfile_decoder + power.regfile_wordline
		+ power.regfile_bitline + power.regfile_senseamp
		+ power.rs_decoder + power.rs_wordline
		+ power.rs_bitline + power.rs_senseamp
		+ power.lsq_wakeup_tagdrive + power.lsq_wakeup_tagmatch
		+ power.lsq_rs_decoder + power.lsq_rs_wordline
		+ power.lsq_rs_bitline + power.lsq_rs_senseamp
		+ power.resultbus
		+ power.clock_power
		+ power.icache_power + power.itlb + power.dcache_power + power.dtlb + power.dcache2_power + power.dcache3_power;

	//This value isn't used anywhere....
	power.total_power_nodcache2 =power.local_predict + power.global_predict
		+ power.chooser + power.btb
		+ power.rat_decoder + power.rat_wordline
		+ power.rat_bitline + power.rat_senseamp
		+ power.dcl_compare + power.dcl_pencode
		+ power.inst_decoder_power
		+ power.wakeup_tagdrive + power.wakeup_tagmatch
		+ power.selection
		+ power.regfile_decoder + power.regfile_wordline
		+ power.regfile_bitline + power.regfile_senseamp
		+ power.rs_decoder + power.rs_wordline
		+ power.rs_bitline + power.rs_senseamp
		+ power.lsq_wakeup_tagdrive + power.lsq_wakeup_tagmatch
		+ power.lsq_rs_decoder + power.lsq_rs_wordline
		+ power.lsq_rs_bitline + power.lsq_rs_senseamp
		+ power.resultbus
		+ power.clock_power
		+ power.icache_power + power.itlb + power.dcache_power + power.dtlb + power.dcache2_power + power.dcache3_power;

	power.bpred_power = power.btb + power.local_predict + power.global_predict + power.chooser + power.ras;

	power.rat_power = power.rat_decoder + power.rat_wordline + power.rat_bitline + power.rat_senseamp;

	power.dcl_power = power.dcl_compare + power.dcl_pencode;

	power.rename_power = power.rat_power + power.dcl_power + power.inst_decoder_power;

	power.wakeup_power = power.wakeup_tagdrive + power.wakeup_tagmatch + power.wakeup_ormatch;

	power.rs_power = power.rs_decoder + power.rs_wordline + power.rs_bitline + power.rs_senseamp;

	power.rs_power_nobit = power.rs_decoder + power.rs_wordline + power.rs_senseamp;

	power.window_power = power.wakeup_power + power.rs_power + power.selection;

	power.lsq_rs_power = power.lsq_rs_decoder + power.lsq_rs_wordline + power.lsq_rs_bitline + power.lsq_rs_senseamp;

	power.lsq_rs_power_nobit = power.lsq_rs_decoder + power.lsq_rs_wordline + power.lsq_rs_senseamp;

	power.lsq_wakeup_power = power.lsq_wakeup_tagdrive + power.lsq_wakeup_tagmatch;

	power.lsq_power = power.lsq_wakeup_power + power.lsq_rs_power;

	power.regfile_power = power.regfile_decoder + power.regfile_wordline + power.regfile_bitline + power.regfile_senseamp;

	power.regfile_power_nobit = power.regfile_decoder + power.regfile_wordline + power.regfile_senseamp;

	dump_power_stats(output);
}
