#include "support/mytime.h"
#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"

#include <stdio.h>


int iterNum;
struct timespec initAppl;
struct timespec initComp;
//struct timespec initCpu;
struct timespec initIter;

struct timespec cpuTime;
struct timespec compTime;

struct timespec iter_cpuTime;
struct timespec iter_compTime;

int myCPUS;
int myId;
void JustProf_Init(int me, int num_procs, int node){
	//Read Environment vars

	myCPUS = _default_nthreads;
	myId=me;

	if (clock_gettime(CLOCK_REALTIME, &initAppl)<0){
		fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
	}

	if (clock_gettime(CLOCK_REALTIME, &initComp)<0){
		fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
	}
	reset(&cpuTime);
	reset(&compTime);
	reset(&iter_cpuTime);
	reset(&iter_compTime);
	iterNum=0;
}

void JustProf_Finish(void){
	add_event(ITERATION_EVENT, 0);
	struct timespec fin;
	struct timespec aux;
	double totalTime;

	if (clock_gettime(CLOCK_REALTIME, &fin)<0){
		fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
	}
	
	diff_time(initComp, fin, &aux);
	
	
        add_time(compTime, aux, &compTime);

        mult_time(aux, myCPUS, &aux);

        add_time(cpuTime, aux, &cpuTime);

	diff_time(initAppl, fin, &aux);

	totalTime=to_secs(aux) * myCPUS;

	debug_basic_info("Application -> %.4f secs\n", to_secs(aux));
	debug_basic_info("Computation time: %.4f secs\n", to_secs(compTime));
	debug_basic_info("CPU time: %.4f secs\n", to_secs(cpuTime));

	debug_basic_info("Usage:  %.2f \n", (to_secs(cpuTime)*100)/totalTime);



}


void JustProf_IntoCommunication(void){}

void JustProf_OutOfCommunication(void){}

void JustProf_IntoBlockingCall(int is_iter){
	struct timespec initMPI;
	struct timespec diff;

	if (clock_gettime(CLOCK_REALTIME, &initMPI)<0){
                fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
        }
	
	diff_time(initComp, initMPI, &diff);
	
	add_time(compTime, diff, &compTime);
	add_time(iter_compTime, diff, &iter_compTime);

	mult_time(diff, myCPUS, &diff);

	add_time(cpuTime, diff, &cpuTime);
	add_time(iter_cpuTime, diff, &iter_cpuTime);

}

void JustProf_OutOfBlockingCall(int is_iter){
	if (clock_gettime(CLOCK_REALTIME, &initComp)<0){
                fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
        }
	if (is_iter!=0){
		if(iterNum!=0){
			//Finishing iteration
			struct timespec aux;
			double totalTime;

			diff_time(initIter, initComp, &aux);
	
        		totalTime=to_secs(aux) * myCPUS;

			debug_basic_info("Iteration %d -> %.4f secs Usage: %.2f (%.4f * 100 / %.4f ) \n", iterNum, to_secs(aux), (to_secs(iter_cpuTime)*100)/totalTime, to_secs(iter_cpuTime), totalTime );
		}
		//Starting iteration
		iterNum++;
		add_event(ITERATION_EVENT, iterNum);

		initIter=initComp;

		reset(&iter_cpuTime);
		reset(&iter_compTime);

		debug_basic_info("Iteration %d detected\n", iterNum);
	}
}

void JustProf_UpdateResources(){}