/* bpred.c - branch predictor routines */

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

#include"bpred.h"

bpred_t::bpred_t()
: retstack(0)
{
	reset();
}

bpred_t::~bpred_t()
{}

bpred_t::bpred_t(std::string name, unsigned int retstack_size)
: name(name), retstack(retstack_size)
{
	reset();
}

//Register the branch predictor statistics with the statistics database provided. Use the name provided as an identifier.
void bpred_t::bpred_reg_stats(stat_sdb_t *sdb, const char *name)
{
	char buf[512], buf1[512];

	sprintf(buf, "%s.lookups", name);
	stat_reg_counter(sdb, buf, "total number of bpred lookups",&lookups, 0, NULL);
	sprintf(buf, "%s.updates", name);
	sprintf(buf1, "%s.dir_hits + %s.misses", name, name);
	stat_reg_formula(sdb, buf, "total number of updates", buf1, "%12.0f");
	sprintf(buf, "%s.addr_hits", name);
	stat_reg_counter(sdb, buf, "total number of address-predicted hits",&addr_hits, 0, NULL);
	sprintf(buf, "%s.dir_hits", name);
	stat_reg_counter(sdb, buf, "total number of direction-predicted hits (includes addr-hits)",&dir_hits, 0, NULL);
	sprintf(buf, "%s.misses", name);
	stat_reg_counter(sdb, buf, "total number of misses", &misses, 0, NULL);
	sprintf(buf, "%s.jr_hits", name);
	stat_reg_counter(sdb, buf, "total number of address-predicted hits for JR's", &jr_hits, 0, NULL);
	sprintf(buf, "%s.jr_seen", name);
	stat_reg_counter(sdb, buf, "total number of JR's seen", &jr_seen, 0, NULL);
	sprintf(buf, "%s.jr_non_ras_hits.PP", name);
	stat_reg_counter(sdb, buf, "total number of address-predicted hits for non-RAS JR's", &jr_non_ras_hits, 0, NULL);
	sprintf(buf, "%s.jr_non_ras_seen.PP", name);
	stat_reg_counter(sdb, buf, "total number of non-RAS JR's seen", &jr_non_ras_seen, 0, NULL);
	sprintf(buf, "%s.bpred_addr_rate", name);
	sprintf(buf1, "%s.addr_hits / %s.updates", name, name);
	stat_reg_formula(sdb, buf, "branch address-prediction rate (i.e., addr-hits/updates)", buf1, "%9.4f");
	sprintf(buf, "%s.bpred_dir_rate", name);
	sprintf(buf1, "%s.dir_hits / %s.updates", name, name);
	stat_reg_formula(sdb, buf, "branch direction-prediction rate (i.e., all-hits/updates)", buf1, "%9.4f");
	sprintf(buf, "%s.bpred_jr_rate", name);
	sprintf(buf1, "%s.jr_hits / %s.jr_seen", name, name);
	stat_reg_formula(sdb, buf, "JR address-prediction rate (i.e., JR addr-hits/JRs seen)", buf1, "%9.4f");
	sprintf(buf, "%s.bpred_jr_non_ras_rate.PP", name);
	sprintf(buf1, "%s.jr_non_ras_hits.PP / %s.jr_non_ras_seen.PP", name, name);
	stat_reg_formula(sdb, buf, "non-RAS JR addr-pred rate (ie, non-RAS JR hits/JRs seen)", buf1, "%9.4f");
	if(retstack.size)
	{
		retstack.reg_stats(sdb,name);
	}
	sprintf(buf, "%s.used_ras.PP", name);
	stat_reg_counter(sdb, buf, "total number of RAS predictions used", &used_ras, 0, NULL);
	sprintf(buf, "%s.ras_hits.PP", name);
	stat_reg_counter(sdb, buf, "total number of RAS hits",&ras_hits, 0, NULL);
	sprintf(buf, "%s.ras_rate.PP", name);
	sprintf(buf1, "%s.ras_hits.PP / %s.used_ras.PP", name, name);
	stat_reg_formula(sdb, buf, "RAS prediction rate (i.e., RAS hits/used RAS)", buf1, "%9.4f");
}

void bpred_t::reset()
{
	addr_hits = dir_hits = 0;
	used_ras = 0;
	jr_hits = jr_seen = 0;
	jr_non_ras_hits = jr_non_ras_seen = 0;
	misses = 0;

	lookups = 0;
	ras_hits = 0;

	retstack.reset();
}

