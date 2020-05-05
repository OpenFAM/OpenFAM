#!/usr/bin/python3
 #
 # extract_allocator_profiling.py
 # Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
 # reserved. Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions are met:
 # 1. Redistributions of source code must retain the above copyright notice,
 # this list of conditions and the following disclaimer.
 # 2. Redistributions in binary form must reproduce the above copyright notice,
 # this list of conditions and the following disclaimer in the documentation
 # and/or other materials provided with the distribution.
 # 3. Neither the name of the copyright holder nor the names of its contributors
 # may be used to endorse or promote products derived from this software without
 # specific prior written permission.
 #
 #    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 # IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 # ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 # LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 # CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 # SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 #    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 # CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 # ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 # POSSIBILITY OF SUCH DAMAGE.
 #
 # See https://spdx.org/licenses/BSD-3-Clause
 #
 #

import glob
import sys
import csv

arguments = sys.argv[1:]
argc = len(arguments)
if argc != 2:
    print ("usage:")
    print ("./extract_allocator_profiling.py directory csv_file")
    exit()

path = sys.argv[1]
outfile = sys.argv[2]
fam_memserver_api_map = {
        "CR" : ['create_region', 'CreateHeap', 'FindHeap', 'Heap_Open','HeapMapInsertTotal', 'HeapMapInsertOp', 'metadata_insert_region', 'metadata_find_region', 'metadata_maxkeylen'],
        "DR" : ['destroy_region', 'DestroyHeap', 'HeapMapFindTotal', 'HeapMapFindOp', 'metadata_delete_region', 'metadata_find_region'],
        "AI" : ['allocate', 'Heap_AllocOffset', 'Heap_OffsetToLocal', 'HeapMapFindTotal', 'HeapMapFindOp', 'metadata_find_region', 'metadata_insert_dataitem', 'metadata_find_dataitem','metadata_maxkeylen', 'fi_mr_reg', 'fi_mr_key'],
        "DI" : ['deallocate', 'Heap_Free', 'HeapMapFindTotal', 'HeapMapFindOp', 'FindHeap', 'Heap_Open','metadata_delete_dataitem', 'metadata_find_dataitem'],
        "LR" : ['lookup_region', 'metadata_find_region'],
        "LD" : ['lookup', 'metadata_find_dataitem'],
        "RCP" : ['change_region_permission', 'metadata_find_region', 'metadata_modify_region'],
        "DCP" : ['change_dataitem_permission', 'metadata_find_dataitem', 'metadata_modify_dataitem'],
        "CP"  : ['allocate','copy_create', 'copy_process', 'copy_finish'],
        "RSZ" : ['resize_region', 'Heap_Resize', 'HeapMapFindTotal', 'HeapMapFindOp', 'metadata_find_region', 'metadata_modify_region', 'metadata_check_permissions'],
        "CDR" : ['destroy_region', 'DestroyHeap', 'HeapMapFindTotal', 'HeapMapFindOp', 'Heap_Close', 'HeapMapEraseTotal', 'HeapMapEraseOp', 'metadata_delete_region', 'metadata_find_region'],
        "ADI" : ['deallocate', 'Heap_Free', 'HeapMapFindTotal', 'HeapMapFindOp', 'metadata_delete_dataitem', 'metadata_find_dataitem', 'fi_close'],
        "ASR" : ['allocate', 'Heap_AllocOffset', 'Heap_OffsetToLocal', 'HeapMapFindTotal', 'HeapMapFindOp', 'metadata_find_region', 'metadata_insert_dataitem', 'metadata_find_dataitem','metadata_maxkeylen', 'fi_mr_reg', 'fi_mr_key'],
        "DISR" : ['deallocate', 'Heap_Free', 'HeapMapFindTotal', 'HeapMapFindOp', 'FindHeap', 'Heap_Open', 'heap_close', 'metadata_delete_dataitem', 'metadata_find_dataitem'],
        "LDSR" : ['lookup', 'metadata_find_dataitem'],
        "PBUD" : ['check_permission_get_item_info', 'Heap_OffsetToLocal', 'HeapMapFindTotal', 'HeapMapFindOp', 'metadata_find_dataitem', 'metadata_check_permissions']
}

fam_client_api_map = {
        "CR" : ['fam_create_region'],
        "DR" : ['fam_destroy_region'],
        "AI" : ['fam_allocate'],
        "DI" : ['fam_deallocate'],
        "LR" : ['fam_lookup_region'],
        "LD" : ['fam_lookup'],
        "RCP" : ['fam_change_permissions'],
        "DCP" : ['fam_change_permissions'],
        "CP" : ['fam_copy','fam_copy_wait'],
        "RSZ" : ['fam_resize_region'],
        "CDR" : ['fam_destroy_region'],
        "ADI" : ['fam_deallocate'],
        "ASR" : ['fam_allocate'],
        "DISR" : ['fam_deallocate'],
        "LDSR" : ['fam_lookup'],
        "PBUD" : ['fam_put_blocking', 'fi_cq_read', 'fi_cq_readerr', 'fi_writemsg', 'fabric_completion_wait']
}


# Get list of files
files = [f for f in glob.glob(path + "/*.log" )]

# Create and OpenCSV file
with open(outfile, "w") as outf:
    writer = csv.writer(outf,dialect='excel')
    writer.writerow(['Region type','OpenFAM API', 'Memserver Level 1', 'Memserver Level 2', 'total PEs', 'PE ID','Iteration','Total Pct','Total time(ns)','Avg time/call(ns)'])
    for f in files:
        print(f)
        fname = f.split('/')[-1]
        print(fname)
        fparam = fname.split('_')
        test_type = fparam[2].split('.')[0]
        if(test_type == "ASR" or test_type == "DISR" or test_type == "LDSR") :
            region = "SINGLE"
        else:
            region = "PER PE"
        test_type = fam_client_api_map[fparam[2].split('.')[0]][0][0]
        if (fparam[1] == "MEMSERVER"):
            fam_apis = fam_memserver_api_map[fparam[2].split('.')[0]]
        else:
            fam_apis = fam_client_api_map[fparam[2].split('.')[0]]

        prefix = list()
        #PE
        total_pes = int(fparam[0].split('PE')[0])
        prefix.append(str(total_pes))
        lines = list()
        # Loop through all lines and extract required functions
        for line in open(f):
                line = line.strip()
                if any(line.startswith(word) for word in fam_apis):
                     entry = list()
                     tmp_line = line.split()
                     for l in tmp_line:
                         entry.append(l)
                     lines.append(entry)
        total_lines = len(lines)
        if (total_lines) == 0:
            continue;
        time_idx = len(lines[0])-1
        api_pe = dict()

        if (fparam[1] == "CLIENT"):
            for api in fam_apis:
                api_pe[api] = 0

        i = 0
        while i < total_lines:
            if (fparam[1] == "CLIENT"):
                pe = api_pe[lines[i][0]]
                api_pe[lines[i][0]] = api_pe[lines[i][0]] + 1
                lines[i].insert(0,"PE" +str(pe))
                lines[i].insert(2,"NA")
                lines[i].insert(3,"NA")
            else:
                lines[i].insert(0,"MEMSERVER")
                if(fparam[2].split('.')[0] == "CP"):
                            lines[i].insert(1, fam_client_api_map[fparam[2].split('.')[0]][0])
                            lines[i].insert(3, "NA")
                else :
                     if(lines[i][1] == fam_apis[0]):
                        lines[i].insert(1, fam_client_api_map[fparam[2].split('.')[0]][0])
                        lines[i].insert(3, "NA")
                     else:
                        lines[i].insert(1, fam_client_api_map[fparam[2].split('.')[0]][0])
                        lines[i].insert(2, fam_apis[0])

            i = i+1
        for line in lines:
            outputline = list()
            outputline.append(region)
            outputline.append(line.pop(1))
            outputline.append(line.pop(1))
            outputline.append(line.pop(1))
            for l in prefix:
                outputline.append(l)
            for l in line:
                outputline.append(l)
            writer.writerow(outputline)

