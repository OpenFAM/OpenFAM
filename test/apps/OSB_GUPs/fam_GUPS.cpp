/* -*- mode: C; tab-width: 2; indent-tabs-mode: nil; -*- */

/* Modifications copyright (C) 2021 Advanced Micro Devices, Inc. All rights
 * reserved.*/

/*
 * This code has been contributed by the DARPA HPCS program.  Contact
 * David Koester <dkoester@mitre.org> or Bob Lucas <rflucas@isi.edu>
 * if you have questions.
 *
 *
 * GUPS (Giga UPdates per Second) is a measurement that profiles the memory
 * architecture of a system and is a measure of performance similar to MFLOPS.
 * The HPCS HPCchallenge RandomAccess benchmark is intended to exercise the
 * GUPS capability of a system, much like the LINPACK benchmark is intended to
 * exercise the MFLOPS capability of a computer.  In each case, we would
 * expect these benchmarks to achieve close to the "peak" capability of the
 * memory system. The extent of the similarities between RandomAccess and
 * LINPACK are limited to both benchmarks attempting to calculate a peak system
 * capability.
 *
 * GUPS is calculated by identifying the number of memory locations that can be
 * randomly updated in one second, divided by 1 billion (1e9). The term
 * "randomly" means that there is little relationship between one address to be
 * updated and the next, except that they occur in the space of one half the
 * total system memory.  An update is a read-modify-write operation on a table
 * of 64-bit words. An address is generated, the value at that address read from
 * memory, modified by an integer operation (add, and, or, xor) with a literal
 * value, and that new value is written back to memory.
 *
 * We are interested in knowing the GUPS performance of both entire systems and
 * system subcomponents --- e.g., the GUPS rating of a distributed memory
 * multiprocessor the GUPS rating of an SMP node, and the GUPS rating of a
 * single processor.  While there is typically a scaling of FLOPS with processor
 * count, a similar phenomenon may not always occur for GUPS.
 *
 *
 */

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "GUPS_common.h"
#include "RandomAccess.h"
#include <chrono>
#include <common/fam_options.h>
#include <common/fam_test_config.h>
#include <fam/fam.h>
#include <fam/fam_exception.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>
using namespace std::chrono;
using namespace std;
using namespace openfam;

const char *GUPSTable = "GUPSTable";
const char *sAbortVariable = "SABORTVARIABLE";
const char *rAbortVariable = "RABORTVARIABLE";
const char *GUPsVariable = "GUPSVARIABLE";
const char *temp_GUPsVariable = "TEMP_GUPSVARIABLE";
const char *GlbNumErrorsVariable = "GLBNUMERRORSVARIABLE";
const char *NumErrorsVariable = "NUMERRORSVARIABLE";
const char *HPCC_TableVariable = "HPCCTABLEVARIABLE";
const char *countVariable = "COUNTVARIABLE";
const char *ranVariable = "RANVARIABLE";
const char *updatesVariable = "UPDATESVARIABLE";
const char *all_updatesVariable = "ALLUPDATEVARIABLE";

u64Int srcBuf[] = {0xb1ffd1da};
u64Int targetBuf[sizeof(srcBuf) / sizeof(u64Int)];

/* Allocate main table (in global memory) */
u64Int *HPCC_Table;

int main(int argc, char **argv) {
    s64Int i;
    int NumProcs = 0, logNumProcs = 0, MyProc = 0;
    u64Int GlobalStartMyProc;
    s64Int LocalTableSize;    /* Local table width */
    u64Int MinLocalTableSize; /* Integer ratio TableSize/NumProcs */
    u64Int logTableSize, TableSize;

    double RealTime; /* Real time to update table */
    double TotalMem;
    int PowerofTwo = 0;
    u64Int NumUpdates_Default; /* Number of updates to table (suggested: 4x
                                  number of table entries) */
    u64Int
        NumUpdates; /* actual number of updates to table - may be smaller than
                     * NumUpdates_Default due to execution time bounds */
    s64Int ProcNumUpdates; /* number of updates per processor */
                           // s64Int *NumErrors, *GlbNumErrors;
#ifdef RA_TIME_BOUND
    s64Int GlbNumUpdates; /* for reduction */
#endif

    int j;
    int logTableLocal, iterate, niterate;
    int remote_proc;
    int operation_mode;
    operation_mode = atoi(argv[3]); // 0->LoadStore  1->PutGet

    Fam_Region_Descriptor *region = NULL;
    TotalMem = 1024; // 262144; /* max single node memory */
    TotalMem = 262144; /* max single node memory */
    NumProcs = atoi(argv[2]); // #PEs in the Run mode
    TotalMem *= NumProcs; /* max memory in NumProcs nodes */

    TotalMem /= sizeof(u64Int);
    /* calculate TableSize --- the size of update array (must be a power of 2)
     */
    for (TotalMem *= 0.5, logTableSize = 0, TableSize = 1; TotalMem >= 1.0;
         TotalMem *= 0.5, logTableSize++, TableSize <<= 1)
        ; /* EMPTY */

    MinLocalTableSize = (TableSize / NumProcs);
    LocalTableSize = MinLocalTableSize;
    cout << "MinLocalTableSize: " << MinLocalTableSize
         << "W  TableSize: " << TableSize << "W  NumProcs: " << NumProcs
         << " LocalTableSize: " << LocalTableSize
         << "W   OneWord(sizeof(u64Int)): " << sizeof(u64Int) << "B\n";

    // Initialize or run (0: Initialize the memory region; 1: Run mode)
    if (atoi(argv[1]) == 0) {
        auto start = high_resolution_clock::now();
        region = GUPS_fam_initialize((u64Int)(
            TotalMem * 1.5 * sizeof(u64Int))); // Initialize the FAM region
        try {
            /*Fam_Descriptor *gupsTable = */my_fam->fam_allocate(
                GUPSTable, (u64Int)(TotalMem * sizeof(u64Int)), 0777,
                region); // Allocate GUPSTable in the FAM region
            /*Fam_Descriptor *sAbort =*/
                my_fam->fam_allocate(sAbortVariable, sizeof(int), 0777, region);
            /*Fam_Descriptor *rAbort =*/
                my_fam->fam_allocate(rAbortVariable, sizeof(int), 0777, region);

            /*Fam_Descriptor *GUPs = */my_fam->fam_allocate(
                GUPsVariable, sizeof(double), 0777, region);
            /*Fam_Descriptor *temp_GUPs = */my_fam->fam_allocate(
                temp_GUPsVariable, sizeof(double), 0777, region);
            /*Fam_Descriptor *GlbNumErrors = */my_fam->fam_allocate(
                GlbNumErrorsVariable, sizeof(s64Int), 0777, region);
            /*Fam_Descriptor *NumErrors = */my_fam->fam_allocate(
                NumErrorsVariable, sizeof(s64Int), 0777, region);

            /*Fam_Descriptor *HPCC_Table = */my_fam->fam_allocate(
                HPCC_TableVariable, LocalTableSize * sizeof(u64Int), 0777,
                region);
            /*Fam_Descriptor *count = */my_fam->fam_allocate(
                countVariable, sizeof(s64Int), 0777, region);
            /*Fam_Descriptor *ran =*/
                my_fam->fam_allocate(ranVariable, sizeof(s64Int), 0777, region);
            /*Fam_Descriptor *updates = */my_fam->fam_allocate(
                updatesVariable, sizeof(s64Int) * NumProcs, 0777, region);
            /*Fam_Descriptor *all_updates =*/
                my_fam->fam_allocate(all_updatesVariable, sizeof(s64Int), 0777,
                                     region); /*: An array to collect sum*/

            printf("Successully allocated integer elements\n");
        } catch (Fam_Exception &e) {
            printf("fam API failed: %d: %s\n", e.fam_error(),
                   e.fam_error_msg());
        }

        my_fam->fam_finalize("default");
        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(stop - start);

        printf("Check_1: %f\n", TotalMem);
    } else {
        region = GUPS_fam_initialize(); // Look up the region in the metadata
                                        // service.

        // get data items from the defined memory region
        Fam_Descriptor *GupsTable = gups_get_dataitem(GUPSTable);
        Fam_Descriptor *SAbort = gups_get_dataitem(sAbortVariable);
        Fam_Descriptor *RAbort = gups_get_dataitem(rAbortVariable);

        Fam_Descriptor *GUPs = gups_get_dataitem(GUPsVariable);
        Fam_Descriptor *Temp_GUPs = gups_get_dataitem(temp_GUPsVariable);
        Fam_Descriptor *GlbNumErrors = gups_get_dataitem(GlbNumErrorsVariable);
        Fam_Descriptor *NumErrors = gups_get_dataitem(NumErrorsVariable);

        Fam_Descriptor *HPCC_Table = gups_get_dataitem(HPCC_TableVariable);
        Fam_Descriptor *Count = gups_get_dataitem(countVariable);
        Fam_Descriptor *Ran = gups_get_dataitem(ranVariable);
        Fam_Descriptor *Updates = gups_get_dataitem(updatesVariable);
        Fam_Descriptor *All_updates = gups_get_dataitem(all_updatesVariable);

        int thisPeId = myPE;
        int numNodes = numPEs;
        NumProcs = numNodes;
        MyProc = thisPeId;
        GlobalStartMyProc = (MinLocalTableSize * thisPeId);

        /* Default number of global updates to table: 4x number of table entries
         */
        NumUpdates_Default = 4 * TableSize;
        ProcNumUpdates = 4 * LocalTableSize;
        NumUpdates = NumUpdates_Default;
        u64Int *gupsTable = NULL;
        int *sAbort = NULL;
        int *rAbort = NULL;
        double *gUPs = NULL;
        double *temp_GUPs = NULL;
        s64Int *glbNumErrors = NULL;
        s64Int *numErrors = NULL;
        u64Int *hPCC_Table = NULL;
        s64Int *count = NULL;
        s64Int *ran = NULL;
        s64Int *updates = NULL;
        s64Int *all_updates = NULL;
        // Map all defined data items in the FAM to the process virtual address
        // space.
        gupsTable = (u64Int *)my_fam->fam_map(GupsTable);
        sAbort = (int *)my_fam->fam_map(SAbort);
        rAbort = (int *)my_fam->fam_map(RAbort);
        gUPs = (double *)my_fam->fam_map(GUPs);
        temp_GUPs = (double *)my_fam->fam_map(Temp_GUPs);
        glbNumErrors = (s64Int *)my_fam->fam_map(GlbNumErrors);
        numErrors = (s64Int *)my_fam->fam_map(NumErrors);
        count = (s64Int *)my_fam->fam_map(Count);
        ran = (s64Int *)my_fam->fam_map(Ran);
        updates = (s64Int *)my_fam->fam_map(Updates);
        all_updates = (s64Int *)my_fam->fam_map(All_updates);
        *gUPs = -1;
        *sAbort = 0;
        my_fam->fam_barrier_all();

        if (*rAbort > 0) {
            if (MyProc == 0) {
                printf("Failed to allocate memory for the main table.\n");
            }
        }

        if (MyProc == 0) {
            printf("Running on %d processors%s\n", NumProcs,
                   PowerofTwo ? " (PowerofTwo)" : "");
            printf("Total Main table size = 2^" FSTR64 " = " FSTR64 " words\n",
                   logTableSize, TableSize);
            if (PowerofTwo) {
                printf("PE Main table size = 2^" FSTR64 " = " FSTR64
                       " words/PE\n",
                       (logTableSize - logNumProcs), TableSize / NumProcs);
            } else {
                printf("PE Main table size = (2^" FSTR64 ")/%d  = " FSTR64
                       " words/PE MAX\n",
                       logTableSize, NumProcs, LocalTableSize);
            }
            printf("Default number of updates (RECOMMENDED) = " FSTR64
                   "\tand actually done = %lld\n",
                   NumUpdates_Default, ProcNumUpdates * NumProcs);
        }

        if (operation_mode == 0)
            hPCC_Table = (u64Int *)my_fam->fam_map(HPCC_Table);

        /*Initialize the main table*/
        u64Int PE_Id;
        for (i = 0; i < LocalTableSize; i++) {
            if (operation_mode == 0) {
                hPCC_Table[i + sizeof(u64Int) * LocalTableSize * MyProc] =
                    MyProc;
            } else {
                PE_Id = MyProc;
                my_fam->fam_put_nonblocking(
                    &PE_Id, HPCC_Table,
                    i + sizeof(u64Int) * LocalTableSize * MyProc,
                    sizeof(u64Int)); //  Copy the data element from local memory
                                     //  to FAM.
                my_fam->fam_barrier_all();
            }
        }

        int verify = 1;
        if (verify) {
            for (i = 0; i < LocalTableSize; i++)
                if (operation_mode == 0) {
                    cout << "PEID_" << MyProc << ": "
                         << hPCC_Table[i +
                                       sizeof(u64Int) * LocalTableSize * MyProc]
                         << "\n";
                } else {
                    my_fam->fam_get_nonblocking(&PE_Id, HPCC_Table,
                                                i + sizeof(u64Int) *
                                                        LocalTableSize * MyProc,
                                                sizeof(u64Int));
                    cout << "PEID_" << MyProc << ": " << PE_Id << "\n";
                    my_fam->fam_barrier_all();
                }
        }

        *ran = starts(4 * GlobalStartMyProc);
        niterate = (int)ProcNumUpdates;
        logTableLocal = (int)(logTableSize - logNumProcs);

        if (operation_mode == 0) {
            for (j = 0; j < numNodes; j++) {
                updates[j] = 0;
            }
            all_updates[0] = 0;
        }

        u64Int remote_val;
        RealTime = -RTSEC();
        my_fam->fam_barrier_all();

        for (iterate = 0; iterate < niterate; iterate++) {
            *ran = (*ran << 1) ^ ((s64Int)*ran < ZERO64B ? POLY : ZERO64B);
            remote_proc = (int)((*ran >> logTableLocal) & (numNodes - 1));

            // Forces updates to remote PE only
            if (remote_proc == MyProc)
                remote_proc = (remote_proc + 1) / numNodes;

            int offset =
                (int)(sizeof(u64Int) * LocalTableSize *
                remote_proc); // Determine the corresponding offset for the PE
            int wordIndex =
                (int)(*ran & (LocalTableSize - 1)); // Determine the word index

            if (operation_mode == 0)
                remote_val =
                    (u64Int)&hPCC_Table[offset +
                                        wordIndex]; // Load the word from the
                                                    // remote memory
            else {
                my_fam->fam_get_nonblocking(
                    &remote_val, HPCC_Table, offset + wordIndex,
                    sizeof(u64Int)); // Copy the memory element from FAM to the
                                     // temporary variable.
                my_fam->fam_barrier_all();
            }
            remote_val ^=
                *ran; // Modify the word via XORing with a random value

            if (operation_mode == 0) {
                hPCC_Table[offset + wordIndex] =
                    remote_val; // Store the modified version of remote_val into
                                // the remote memory
            } else {
                my_fam->fam_put_nonblocking(
                    &remote_val, HPCC_Table, offset + wordIndex,
                    sizeof(u64Int)); //  Copy the data element from local memory
                                     //  to FAM.
                my_fam->fam_barrier_all();
            }

            if (verify) {
                updates[thisPeId] =
                    updates[thisPeId] +
                    1; // Update the variable to reflect the number of updates
                       // by the corresponding PE
            }
            my_fam->fam_barrier_all();
        }

        my_fam->fam_barrier_all();
        RealTime += RTSEC();

        if (MyProc == 0) {
            *gUPs = 1e-9 * (double)NumUpdates / RealTime;
            printf("Real time used = %.6f seconds\n", RealTime);
            printf("%.9f Billion(10^9) Updates    per second [GUP/s]\n", *gUPs);
            printf("%.9f Billion(10^9) Updates/PE per second [GUP/s]\n",
                   *gUPs / NumProcs);
        }

        if (verify) {
            all_updates[0] +=
                updates[MyProc]; // Each PE requires to add its #updates to the
                                 // global variable all_updates
            my_fam->fam_barrier_all();

            cout << "ProcNumUpdates*NumProcs: " << updates[MyProc] << "\n";
            int cpu = sched_getcpu();

            if (MyProc == 0) {
                printf("PE%d CPU%d  updates:%lld\n", MyProc, cpu, all_updates[0]);

                if (ProcNumUpdates * NumProcs ==
                    all_updates[0]) { // Check the number of updates by PEs with
                                      // the expected number of updates
                    printf("Verification passed!\n");
                } else {
                    printf("Verification failed!\n");
                }
            }
        }
        if (verify) {
            if (MyProc == 0) {
                all_updates[0] = 0;
            }
            my_fam->fam_barrier_all();
        }

        // Unmap all allocated data items in the FAM region
        if (operation_mode == 0)
            my_fam->fam_unmap(hPCC_Table, HPCC_Table);

        my_fam->fam_barrier_all();
        my_fam->fam_unmap(gupsTable, GupsTable);
        my_fam->fam_unmap(sAbort, SAbort);
        my_fam->fam_unmap(rAbort, RAbort);

        my_fam->fam_unmap(gUPs, GUPs);
        my_fam->fam_unmap(temp_GUPs, Temp_GUPs);
        my_fam->fam_unmap(glbNumErrors, GlbNumErrors);
        my_fam->fam_unmap(numErrors, NumErrors);

        my_fam->fam_unmap(count, Count);
        my_fam->fam_unmap(ran, Ran);
        my_fam->fam_unmap(updates, Updates);
        my_fam->fam_finalize("default");
    }
    return 0;
}

/* Utility routine to start random number generator at Nth step */
s64Int starts(u64Int n) {
    /* s64Int i, j; */
    int i, j;
    u64Int m2[64];
    u64Int temp, ran;

    //while (n < 0)
    //    n += PERIOD;
    while (n > PERIOD)
        n -= PERIOD;
    if (n == 0)
        return 0x1;

    temp = 0x1;
    for (i = 0; i < 64; i++) {
        m2[i] = temp;
        temp = (temp << 1) ^ ((s64Int)temp < 0 ? POLY : 0);
        temp = (temp << 1) ^ ((s64Int)temp < 0 ? POLY : 0);
    }

    for (i = 62; i >= 0; i--)
        if ((n >> i) & 1)
            break;

    ran = 0x2;

    while (i > 0) {
        temp = 0;
        for (j = 0; j < 64; j++)
            if ((ran >> j) & 1)
                temp ^= m2[j];
        ran = temp;
        i -= 1;
        if ((n >> i) & 1)
            ran = (ran << 1) ^ ((s64Int)ran < 0 ? POLY : 0);
    }

    return ran;
}
