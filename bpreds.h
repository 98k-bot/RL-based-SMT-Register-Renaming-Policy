#ifndef BPREDS_H
#define BPREDS_H

#include"bpred.h"

//Remove branch predictors by commenting them out here.
//This is essentially a master control for branch predictors.
//FIXME: This does not change the option string in sim-outorder.c
#include"bpred_taken.h"
#include"bpred_not_taken.h"
#include"bpred_two_level.h"
#include"bpred_bimodal.h"
#include"bpred_combining.h"

/*
 * This module implements a number of branch predictor mechanisms.  The
 * following predictors are supported:
 *
 *	bpred_two_level.h:
 *	BPred2Level:  two level adaptive branch predictor
 *
 *		It can simulate many prediction mechanisms that have up to
 *		two levels of tables. Parameters are:
 *		     N   # entries in first level (# of shift register(s))
 *		     W   width of shift register(s)
 *		     M   # entries in 2nd level (# of counters, or other FSM)
 *		One BTB entry per level-2 counter.
 *
 *		Configurations:   N, W, M
 *
 *		    counter based: 1, 0, M
 *
 *		    GAg          : 1, W, 2^W
 *		    GAp          : 1, W, M (M > 2^W)
 *		    PAg          : N, W, 2^W
 *		    PAp          : N, W, M (M == 2^(N+W))
 *
 *	bpred_bimodal.h:
 *	BPred2bit:  a simple direct mapped bimodal predictor
 *
 *		This predictor has a table of two bit saturating counters.
 *		Where counter states 0 & 1 are predict not taken and
 *		counter states 2 & 3 are predict taken, the per-branch counters
 *		are incremented on taken branches and decremented on
 *		no taken branches.  One BTB entry per counter.
 *
 *	bpred_taken.h:
 *	BPredTaken:  always predict taken
 *
 *	bpred_not_taken.h:
 *	BPredNotTaken:  always predict not taken
 *
 */


#endif
