/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "apis/dlb_sp.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>

static dlb_handler_t handler;
static cpu_set_t process_mask;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &process_mask);
}

static void cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &process_mask);
    DLB_LendCpu_sp(handler, cpuid);
}

int main( int argc, char **argv ) {
    CPU_ZERO(&process_mask);
    CPU_SET(0, &process_mask);
    CPU_SET(1, &process_mask);

    handler = DLB_Init_sp(0, &process_mask, "--policy=no");
    assert( DLB_CallbackSet_sp(handler, dlb_callback_disable_cpu,
                (dlb_callback_t)cb_disable_cpu, NULL) == DLB_SUCCESS);
    assert( DLB_CallbackSet_sp(handler, dlb_callback_enable_cpu,
                (dlb_callback_t)cb_enable_cpu, NULL) == DLB_SUCCESS);
    dlb_callback_t cb;
    void *arg;
    assert( DLB_CallbackGet_sp(handler, dlb_callback_enable_cpu, &cb, &arg) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_enable_cpu );
    assert( arg == NULL );

    // Enable/Disable API is no compatible wtih sp for now
    assert( DLB_Disable_sp(handler) == DLB_ERR_NOCOMP );
    assert( DLB_Enable_sp(handler) == DLB_ERR_NOCOMP );
    assert( DLB_SetMaxParallelism_sp(handler, 1) == DLB_SUCCESS );

    // Lend
    assert( DLB_Lend_sp(handler) == DLB_ERR_NOPOL );
    assert( DLB_LendCpu_sp(handler, 0) == DLB_ERR_NOPOL );
    assert( DLB_LendCpus_sp(handler, 1) == DLB_ERR_NOPOL );
    assert( DLB_LendCpuMask_sp(handler, &process_mask) == DLB_ERR_NOPOL );

    // Reclaim
    assert( DLB_Reclaim_sp(handler) == DLB_ERR_NOPOL );
    assert( DLB_ReclaimCpu_sp(handler, 0) == DLB_ERR_NOPOL );
    assert( DLB_ReclaimCpus_sp(handler, 1) == DLB_ERR_NOPOL );
    assert( DLB_ReclaimCpuMask_sp(handler, &process_mask) == DLB_ERR_NOPOL );

    // Acquire
    assert( DLB_AcquireCpu_sp(handler, 0) == DLB_ERR_NOPOL );
    assert( DLB_AcquireCpuMask_sp(handler, &process_mask) == DLB_ERR_NOPOL );

    // Borrow
    assert( DLB_Borrow_sp(handler) == DLB_ERR_NOPOL );
    assert( DLB_BorrowCpu_sp(handler, 0) == DLB_ERR_NOPOL );
    assert( DLB_BorrowCpus_sp(handler, 1) == DLB_ERR_NOPOL );
    assert( DLB_BorrowCpuMask_sp(handler, &process_mask) == DLB_ERR_NOPOL );

    // Return
    assert( DLB_Return_sp(handler) == DLB_ERR_NOPOL );
    assert( DLB_ReturnCpu_sp(handler, 0) == DLB_ERR_NOPOL );

    // Misc */
    assert( DLB_PollDROM_sp(handler, NULL, NULL) == DLB_ERR_DISBLD );
    assert( DLB_SetVariable_sp(handler, "LB_DROM", "1") == DLB_ERR_PERM );
    assert( DLB_SetVariable_sp(handler, "LB_DEBUG_OPTS", "foo") == DLB_SUCCESS );
    char value[32];
    assert( DLB_GetVariable_sp(handler, "LB_DROM", value) == DLB_SUCCESS );
    assert( DLB_PrintVariables_sp(handler, 0) == DLB_SUCCESS );

    assert( DLB_Finalize_sp(handler) == DLB_SUCCESS );

    return 0;
}