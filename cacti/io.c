/*------------------------------------------------------------
 *  Copyright 1994 Digital Equipment Corporation and Steve Wilton
 *       All Rights Reserved
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
 *     Director of Licensing
 *     Western Research Laboratory
 *     Digital Equipment Corporation
 *     100 Hamilton Avenue
 *     Palo Alto, California  94301
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


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "def.h"

#define NEXTINT(a)	skip();	scanf("%d",&(a));
#define NEXTFLOAT(a)	skip();	scanf("%lf",&(a));

/*---------------------------------------------------------------*/


int input_data(int argc, char *argv[], cacti_parameter_type *parameters)
{
	//input parameters: C B A

	if(argc!=4)
	{
		fprintf(stderr,"Cmd-line parameters: C B A\n");
		exit(-1);
	}

	int C = atoi((const char *)argv[1]);
	if(C < 64)
	{
		fprintf(stderr,"Cache size must >=64\n");
		exit(-1);
	}

	int B = atoi((const char *)argv[2]);
	if(B < 1)
	{
		fprintf(stderr,"Block size must >=1\n");
		exit(-1);
	}

	int A = atoi((const char *)argv[3]);
	if(A < 1)
	{
		fprintf(stderr,"Associativity must >=1\n");
		exit(-1);
	}

	parameters->cache_size = C;
	parameters->block_size = B;
	parameters->associativity = A;
	parameters->number_of_sets = C/(B*A);

	if(parameters->number_of_sets < 1)
	{
		fprintf(stderr,"Less than one set...\n");
		exit(-1);
	}
	return(OK);
}

void output_time_components(int A, cacti_result_type *result, FILE * output)
{
	if(!output)
	{
		return;
	}
	fprintf(output," decode_data (ns): %g\n",result->decoder_delay_data/1e-9);
	fprintf(output," wordline_data (ns): %g\n",result->wordline_delay_data/1e-9);
	fprintf(output," bitline_data (ns): %g\n",result->bitline_delay_data/1e-9);
	fprintf(output," sense_amp_data (ns): %g\n",result->sense_amp_delay_data/1e-9);
	fprintf(output," decode_tag (ns): %g\n",result->decoder_delay_tag/1e-9);
	fprintf(output," wordline_tag (ns): %g\n",result->wordline_delay_tag/1e-9);
	fprintf(output," bitline_tag (ns): %g\n",result->bitline_delay_tag/1e-9);
	fprintf(output," sense_amp_tag (ns): %g\n",result->sense_amp_delay_tag/1e-9);
	fprintf(output," compare (ns): %g\n",result->compare_part_delay/1e-9);
	if(A == 1)
	{
		fprintf(output," valid signal driver (ns): %g\n",result->drive_valid_delay/1e-9);
	}
	else
	{
		fprintf(output," mux driver (ns): %g\n",result->drive_mux_delay/1e-9);
		fprintf(output," sel inverter (ns): %g\n",result->selb_delay/1e-9);
	}
	fprintf(output," data output driver (ns): %g\n",result->data_output_delay/1e-9);
	fprintf(output," total data path (with output driver) (ns): %g\n",result->decoder_delay_data/1e-9+result->wordline_delay_data/1e-9+result->bitline_delay_data/1e-9+result->sense_amp_delay_data/1e-9);
	if(A==1)
	{
		fprintf(output," total tag path is dm (ns): %g\n", result->decoder_delay_tag/1e-9+result->wordline_delay_tag/1e-9+result->bitline_delay_tag/1e-9+result->sense_amp_delay_tag/1e-9+result->compare_part_delay/1e-9+result->drive_valid_delay/1e-9);
	}
	else
	{
		fprintf(output," total tag path is set assoc (ns): %g\n", result->decoder_delay_tag/1e-9+result->wordline_delay_tag/1e-9+result->bitline_delay_tag/1e-9+result->sense_amp_delay_tag/1e-9+result->compare_part_delay/1e-9+result->drive_mux_delay/1e-9+result->selb_delay/1e-9);
	}
	fprintf(output," precharge time (ns): %g\n",result->precharge_delay/1e-9);
}

void cacti_output_data(cacti_result_type *result, cacti_parameter_type *parameters, FILE * output)
{
	if(!output)
	{
		return;
	}
	double tagpath;
	double datapath = result->decoder_delay_data+result->wordline_delay_data+result->bitline_delay_data+result->sense_amp_delay_data+result->data_output_delay;
	if(parameters->associativity == 1)
	{
		tagpath = result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_valid_delay;
	}
	else
	{
		tagpath = result->decoder_delay_tag+result->wordline_delay_tag+result->bitline_delay_tag+result->sense_amp_delay_tag+result->compare_part_delay+result->drive_mux_delay+result->selb_delay;
	}

#if OUTPUTTYPE == LONG
	fprintf(output,"\nCache Parameters:\n");
	fprintf(output,"  Size in bytes: %d\n",parameters->cache_size);
	fprintf(output,"  Number of sets: %d\n",parameters->number_of_sets);
	fprintf(output,"  Associativity: %d\n",parameters->associativity);
	fprintf(output,"  Block Size (bytes): %d\n",parameters->block_size);

	fprintf(output,"\nAccess Time: %g\n",result->access_time);
	fprintf(output,"Cycle Time:  %g\n",result->cycle_time);
	fprintf(output,"\nBest Ndwl (L1): %d\n",result->best_Ndwl);
	fprintf(output,"Best Ndbl (L1): %d\n",result->best_Ndbl);
	fprintf(output,"Best Nspd (L1): %d\n",result->best_Nspd);
	fprintf(output,"Best Ntwl (L1): %d\n",result->best_Ntwl);
	fprintf(output,"Best Ntbl (L1): %d\n",result->best_Ntbl);
	fprintf(output,"Best Ntspd (L1): %d\n",result->best_Ntspd);
	fprintf(output,"\nTime Components:\n");
	fprintf(output," data side (with Output driver) (ns): %g\n",datapath/1e-9);
	fprintf(output," tag side (ns): %g\n",tagpath/1e-9);
	output_time_components(parameters->associativity,result,output);
#else
	fprintf(output,"%d %d %d  %d %d %d %d %d %d  %e %e %e %e  %e %e %e %e  %e %e %e %e  %e %e %e %e  %e %e\n",
		parameters->cache_size, parameters->block_size, parameters->associativity,
		result->best_Ndwl, result->best_Ndbl, result->best_Nspd, result->best_Ntwl, result->best_Ntbl, result->best_Ntspd,
		result->access_time, result->cycle_time, datapath, tagpath,
		result->decoder_delay_data, result->wordline_delay_data, result->bitline_delay_data, result->sense_amp_delay_data,
		result->decoder_delay_tag, result->wordline_delay_tag, result->bitline_delay_tag, result->sense_amp_delay_tag,
		result->compare_part_delay, result->drive_mux_delay, result->selb_delay, result->drive_valid_delay,
		result->data_output_delay,
		result->precharge_delay);
#endif
}
