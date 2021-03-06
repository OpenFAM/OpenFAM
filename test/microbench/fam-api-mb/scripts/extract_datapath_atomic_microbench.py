#!/usr/bin/python3
#
# extract_datapath_atomic_microbench.py
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
    print("usage:")
    print("./extract_datapath_atomic_microbench.py directory csv_file")
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
    "GINB": ['fam_gather_nonblocking', 'fam_quiet', 4, 4],
    "GISNB": ['fam_gather_nonblocking', 'fam_quiet', 4, 1],
    "GNB":   ['fam_get_nonblocking', 'fam_quiet', 1, 1],
    "PB":   ['fam_put_blocking', 1, 1],
    "GB":   ['fam_get_blocking', 1, 1],
    "PNB":   ['fam_put_nonblocking', 'fam_quiet', 1, 1],
    "SIB":  ['fam_scatter_blocking', 4, 4],
    "GIB":  ['fam_gather_blocking', 4, 4],
    "SISB": ['fam_scatter_blocking', 4, 1],
    "GISB": ['fam_gather_blocking', 4, 1],
    "SINB":  ['fam_scatter_nonblocking', 'fam_quiet', 4, 4],
    "SISNB": ['fam_scatter_nonblocking', 'fam_quiet', 4, 1],
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
    "FMAX":  ['fam_fetch_max', 1, 1],
    "FCSWAP":   ['fam_compare_swap', 1, 1],
    "FSWAP":   ['fam_swap',  1, 1]
}


# Get list of files
files = [f for f in glob.glob(path + "/*.log", recursive=True)]

# Create and OpenCSV file
with open(outfile, "w") as outf:
    writer = csv.writer(outf, dialect='excel')
    writer.writerow(['Level 1', 'Level 2', 'Level 3', 'Level 4', 'total PEs', 'num memory servers',
                     'Count', 'Size', 'PE ID', 'Iteration', 'Total Pct', 'Total time(ns)', 'Avg time/call(ns)'])
    for f in files:
        fname = f.split('/')[-1]
        fparam = fname.split('_')

        prefix = list()
        tmp_apis = list()
        test_type = fparam[3]
        num_memory_server = fparam[4].split('.')[0]
        # API
        fam_apis = fam_api_map[fparam[3]]
        # PE
        prefix.append(fparam[0].split('PE')[0])
        prefix.append(num_memory_server)
        # PE server type

        if (fparam[3] == "ALLF"):
            # Count
            prefix.append(str(fam_apis[10]))
            # Size
            prefix.append(str(int(int(fparam[1])/fam_apis[11])))
            tmp_apis = fam_apis[:10]
        else:
            # Count
            prefix.append(str(fam_apis[-2]))
            # Size
            prefix.append(str(int(int(fparam[1])/fam_apis[-1])))
            if(isNumber(fam_apis[1])):
                tmp_apis = fam_apis[:1]
            else:
                tmp_apis = fam_apis[:2]
        lines = list()
        # Loop through all lines and extract required functions
        for line in open(f):
            line = line.strip()
            if any(line.startswith(word) for word in tmp_apis):
                entry = list()
                line = line.split()
                for l in line:
                    entry.append(l)
                lines.append(entry)
        total_lines = len(lines)
        if (total_lines) == 0:
            continue
        time_idx = len(lines[0])-1
        api_pe = dict()
        for api in tmp_apis:
            api_pe[api] = 0
        if fam_apis[1] == 'fam_quiet':
            i = 0
            while i < total_lines:
                if i == total_lines-1:
                    break
                if (lines[i][0] == fam_apis[0]) and (lines[i+1][0] == fam_apis[1]):
                    lines[i][time_idx -
                             1] = str(int(lines[i][time_idx-1]) + int(lines[i+1][time_idx-1]))
                    lines[i][time_idx-2] = str(
                        round(float(lines[i][time_idx-2]) + float(lines[i+1][time_idx-2])))
                    lines[i][time_idx] = str(
                        int(int(lines[i][time_idx-1])/int(lines[i][1])))
                    lines.pop(i+1)
                    total_lines = total_lines-1
                    pe = api_pe[lines[i][0]]
                    api_pe[lines[i][0]] = api_pe[lines[i][0]] + 1
                    lines[i].insert(0, "PE" + str(pe))
                    i = i+1
                else:
                    lines.pop(i)
                    total_lines = total_lines-1
        else:
            i = 0
            while i < total_lines:
                pe = api_pe[lines[i][0]]
                api_pe[lines[i][0]] = api_pe[lines[i][0]] + 1
                lines[i].insert(0, "PE" + str(pe))
                i = i+1

        for line in lines:
            outputline = list()
            outputline.append(line.pop(1))
            outputline.append("NA")
            outputline.append("NA")
            outputline.append("NA")
            for l in prefix:
                outputline.append(l)
            for l in line:
                outputline.append(l)
            writer.writerow(outputline)
