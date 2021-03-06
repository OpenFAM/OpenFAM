#!/usr/bin/python3
 #
 # extract_libfabric.py
 # Copyright (c) 2020 Hewlett Packard Enterprise Development, LP. All rights
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
    print ("./extract_libfabric.py directory csv_file")
    exit()

def isNumber(item):
    try:
        num_int = int(item)
        return True
    except ValueError:
        return False

path = sys.argv[1]
outfile = sys.argv[2]
# api,api,count, size ratio
fam_api_map = {
        "GINB" : ['fam_gather_nonblocking', 'fam_quiet', 4, 4],
        "GISNB": ['fam_gather_nonblocking', 'fam_quiet', 4, 1],
        "GNB":   ['fam_get_nonblocking', 'fam_quiet', 1, 1],
        "PB":   ['fam_put_blocking', 1,1],
        "GB":   ['fam_get_blocking', 1,1],
        "PNB":   ['fam_put_nonblocking', 'fam_quiet',1,1],
        "SIB":  ['fam_scatter_blocking' ,4,4],
        "GIB":  ['fam_gather_blocking', 4,4],
        "SISB": ['fam_scatter_blocking', 4,1],
        "GISB": ['fam_gather_blocking', 4,1],
        "SINB":  ['fam_scatter_nonblocking', 'fam_quiet',4,4],
        "SISNB": ['fam_scatter_nonblocking', 'fam_quiet',4,1],
        "SETN":  ['fam_set', 'fam_quiet', 1, 1],
        "ADDN":  ['fam_add', 'fam_quiet', 1, 1],
        "SUBN":  ['fam_subtract', 'fam_quiet', 1, 1],
        "ANDN":   ['fam_and', 'fam_quiet', 1, 1],
        "ORN":   ['fam_or', 'fam_quiet', 1, 1],
        "XORN":   ['fam_xor', 'fam_quiet', 1, 1],
        "MINN":   ['fam_min', 'fam_quiet', 1, 1],
        "MAXN":   ['fam_max', 'fam_quiet', 1, 1],
        "FETCH":   ['fam_fetch',  1, 1],
        "FADD":   ['fam_fetch_add',  1, 1],
        "FSUB":   ['fam_fetch_subtract',  1, 1],
        "FAND":   ['fam_fetch_and',  1, 1],
        "FOR":   ['fam_fetch_or',  1, 1],
        "FXOR":   ['fam_fetch_xor',  1, 1],
        "FMIN":   ['fam_fetch_min',  1, 1],
        "FMAX" :  ['fam_fetch_max', 1, 1],
        "FCSWAP":   ['fam_compare_swap', 1, 1],
        "FSWAP":   ['fam_swap',  1, 1]
        }

nonblocking = ["PNB", "GNB", "GINB", "GISNB", "SINB", "SISNB", "SETN", "ADDN", "SUBN", "ANDN", "ORN", "XORN", "MINN", "MAXN"]
loop = ['fi_readmsg', 'fi_writemsg', 'fi_cq_readerr', 'fi_atomicmsg', 'fabric_retry']
fi_api_map = {
        "GINB" : ['fi_readmsg', 'fi_cntr_read', 'fi_cntr_readerr','fi_cq_readerr','fabric_retry', 'read_write_loop'],
        "GISNB": ['fi_readmsg', 'fi_cntr_read', 'fi_cntr_readerr', 'fi_cq_readerr','fabric_retry', 'read_write_loop'],
        "GNB":   ['fi_readmsg', 'fi_cntr_read', 'fi_cntr_readerr','fi_cq_readerr','fabric_retry', 'read_loop'],
        "PB":   ['fi_cq_read', 'fi_writemsg', 'fabric_completion_wait','fi_cq_readerr'],
        "GB":   ['fi_cq_read', 'fi_readmsg', 'fabric_completion_wait', 'fi_cq_readerr'],
        "PNB":   ['fi_writemsg', 'fi_cntr_read', 'fi_cntr_readerr','fi_cq_readerr','fabric_retry', 'write_loop'],
        "SIB":  ['fi_cq_read', 'fi_writemsg','fabric_completion_wait_multictx250','fi_cq_readerr'],
        "GIB":  ['fi_cq_read', 'fi_readmsg', 'fabric_completion_wait_multictx250','fi_cq_readerr'],
        "SISB": ['fi_cq_read', 'fi_writemsg','fabric_completion_wait_multictx250','fi_cq_readerr'],
        "GISB": ['fi_cq_read', 'fi_readmsg', 'fabric_completion_wait_multictx250','fi_cq_readerr'],
        "SINB":  ['fi_writemsg', 'fi_cntr_read','fi_cntr_readerr','fi_cq_readerr','fabric_retry', 'read_write_loop'],
        "SISNB": ['fi_writemsg','fi_cntr_read', 'fi_cntr_readerr', 'fi_cq_readerr','fabric_retry', 'read_write_loop'],
        "SETN":  ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "ADDN":  ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "SUBN":  ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "ANDN":  ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "ORN":   ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "XORN":  ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "MINN":   ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "MAXN":   ['fi_cntr_read', 'fi_cntr_readerr','fi_atomicmsg','fi_cq_readerr','fabric_retry', 'atomic_loop'],
        "FETCH":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FADD":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FSUB":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FAND":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FOR":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FXOR":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FMIN":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FMAX":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FCSWAP":   ['fi_cq_read', 'fi_compare_atomicmsg', 'fabric_completion_wait','fi_cq_readerr'],
        "FSWAP":   ['fi_cq_read', 'fi_fetch_atomicmsg', 'fabric_completion_wait','fi_cq_readerr']
        }

# Get list of files
files = [f for f in glob.glob(path + "/*.log" )]

# Create and Open CSV file
with open(outfile, "w") as outf:
    writer = csv.writer(outf,dialect='excel')
    writer.writerow(['Level 1', 'Level 2', 'Level 3', 'Level 4', 'total PEs', 'num memeory servers', 'Count','Size', 'PE ID','Iteration','Total Pct','Total time(ns)','Avg time/call(ns)'])

    for f in files:
        fname = f.split('/')[-1]
        fparam = fname.split('_')
        prefix = list()
        tmp_apis = list()
        #API
        if(fparam[3] == "PBUD" or fparam[3] == "GBUD"):
            continue
        test_type = fam_api_map[fparam[3]][0]
        fam_apis = fam_api_map[fparam[3]]
        fabric_apis = fi_api_map[fparam[3]]
        #PE
        total_pes = int(fparam[0].split('PE')[0])
        num_memory_server = fparam[4].split('.')[0]
        prefix.append(str(total_pes))
        prefix.append(str(num_memory_server))

        if (fparam[3] == "ALLF"):
            #Count
            prefix.append(str(fam_apis[10]))
            #Size
            prefix.append(str(int(int(fparam[1])/fam_apis[11])))
            tmp_apis = fam_apis[:10]
            tmp_fi_apis = fabric_apis[:3]
        else:
            #Count
            prefix.append(str(fam_apis[-2]))
            #Size
            prefix.append(str(int(int(fparam[1])/fam_apis[-1])))
            if(isNumber(fam_apis[1])):
                tmp_apis = fam_apis[:1]
            else:
                tmp_apis = fam_apis[:2]
            tmp_fi_apis = fabric_apis

        lines = list()
        # Loop through all lines and extract required functions
        for line in open(f):
            line = line.strip()
            if any(line.startswith(word) for word in tmp_apis):
                line = line.split()
                iteration=line[1]
                break
        for line in open(f):
            line = line.strip()
            if any(line.startswith(word) for word in tmp_fi_apis):
                entry = list()
                line = line.split()
                for l in line:
                    entry.append(l)
                lines.append(entry)
        total_lines = len(lines)
        if (total_lines) == 0:
            continue;
        time_idx = len(lines[0])-1
        api_pe = dict()
        for api in tmp_fi_apis:
            api_pe[api] = 0

        i = 0
        while i < total_lines:
            pe = api_pe[lines[i][0]]
            api_pe[lines[i][0]] = api_pe[lines[i][0]] + 1
            lines[i].insert(0,"PE" +str(pe))
            if any(fparam[3] == word for word in nonblocking):
                if any(lines[i][1] == word for word in loop):
                    if(lines[i][1] == 'fi_cq_readerr'):
                        lines[i].insert(1,fi_api_map[fparam[3]][-1])
                        lines[i].insert(2,fi_api_map[fparam[3]][-2])
                        lines[i][-1] = str(int(int(lines[i][-2])/int(iteration)))
                    else :
                        lines[i].insert(1,fi_api_map[fparam[3]][-1])
                        lines[i].insert(3,"NA")
                        lines[i][-1] = str(int(int(lines[i][-2])/int(iteration)))
                else:
                    if ((lines[i][1] == 'fi_cntr_read') or (lines[i][1] == 'fi_cntr_readerr')):
                        lines[i].insert(1,fam_api_map[fparam[3]][1])
                        lines[i][-1] = str(int(int(lines[i][-2])/int(iteration)))
                    else:
                        lines[i].insert(2,"NA")
                    lines[i].insert(3,"NA")
            else :
                    if((lines[i][1] == 'fi_cq_read') or (lines[i][1] == 'fi_cq_readerr')):
                        lines[i].insert(1,fi_api_map[fparam[3]][2])
                        lines[i][-1] = str(int(int(lines[i][-2])/int(iteration)))
                    elif((lines[i][1] == 'fi_cntr_read') or (lines[i][1] == 'fi_cntr_readerr')):
                        lines[i].insert(1,fam_api_map[fparam[3]][1])
                        lines[i][-1] = str(int(int(lines[i][-2])/int(iteration)))
                    elif ((lines[i][1] == 'fabric_completion_wait') or (lines[i][1] == 'fabric_completion_wait_multictx')):
                        lines[i].insert(1,fi_api_map[fparam[3]][2])
                        lines[i][2] = str("NA")
                    else :
                        if(lines[i][1] == 'fi_readmsg' or lines[i][1] == 'fi_writemsg'):
                            lines[i][-1] = str(int(int(lines[i][-2])/int(iteration)))
                        lines[i].insert(2, str("NA"))
                    lines[i].insert(3,"NA")
            i = i+1
        for line in lines:
            outputline = list()
            outputline.append(test_type)
            outputline.append(line.pop(1))
            outputline.append(line.pop(1))
            outputline.append(line.pop(1))
            for l in prefix:
                outputline.append(l)
            for l in line:
                outputline.append(l)
            writer.writerow(outputline)
