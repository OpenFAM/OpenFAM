/*
 * pagerank_standalone.cpp
 * Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All rights
 * reserved. Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */

/* This code is used to validate FAM pagerank output 
 * It saves the pagerank output in file pagerank_seq.out (defined in OUTFILE )
 * Take a diff between pagerank_seq.out and pagerank_fam.out (output file of FAM)
 * to validate. For the same matrix it should show zero difference.
 */

#include <iostream>
#include <algorithm>
#include <ctime>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sstream>
#include <linux/limits.h>
#include <sys/stat.h>
#include <vector>

using namespace std;
#define OUTFILE "pagerank_seq.out"

uint64_t filelength(const char *filpath) { // don't use that. Learn POSIX API
    struct stat st;
    if (stat(filpath, &st)) /*failure*/
        return -1;          // when file does not exist or is not accessible
    return (long)st.st_size;
}

// Pagerank class. It takes config Filename and number of iterations as input.
// Config File has following contents (example for matrix size 1048576 x 1048576:
//
//  1048576 /*mat row count*/
//  8 /* segments */
//  /home/abdulla/work/OpenFAM_SpMV/graph500/src/out_20_4_8
//
// This matrix is created using a modified graph500.
// The matrix is having multiple segments of CSR :
// i.e rowptr<n>.txt , column<n>.txt and weight<n>.txt
//
class pagerank {
  public:
    // Constructor takes configFilename as input reads the config file and
    // sets other class members
    pagerank(std::string configFilename, int num_iteration) {
        num_iteration_ = num_iteration;
        configFilename_ = configFilename;
        spmv_read_config_file();
    }

    // Read the config file and fill the class members
    void spmv_read_config_file() {
        FILE *fp;
        char *str = (char *)malloc(72);
        // Read Config file
        // matRowCount - Number of rows in the matrix
        // numSegments - number of file segments
        // inpPath - path of the input file segments

        fp = fopen(configFilename_.c_str(), "r");
        if (fp == NULL) {
            printf("\nCouldn't locate the config file..Exiting");
            exit(1);
        }
        memset(str, 0, 72);
        str = fgets(str, 72, fp);
        if (str == NULL) {
            printf("\nInvalid config data..Exiting");
            exit(1);
        }
        sscanf(str, "%d", &nrows_);
        memset(str, 0, 72);
        str = fgets(str, 72, fp);
        if (str == NULL) {
            printf("\nInvalid config data..Exiting");
            exit(1);
        }
        sscanf(str, "%d", &numSegments_);
        str = fgets(str, 72, fp);
        if (str == NULL) {
            printf("\nInvalid config data..Exiting");
            exit(1);
        }
        inpPath_ = str;
        size_t last = inpPath_.find_last_not_of(' ');
        inpPath_ = inpPath_.substr(0, last);
        std::cout << "inppath : " << inpPath_ << " " << str << std::endl;
        free(str);
        fclose(fp);
    }

    // Load the CSR into memory
    int loadCSRSpM() {
        FILE *fp;
        int *addr;
        char inpFilename[PATH_MAX];
        char tmpFilename[PATH_MAX];
        std::cout << "Loading CSR matrix: " << numSegments_ << " " << inpPath_
                  << std::endl;

        // Loop through all the segments of matrix.
        for (int i = 0; i < numSegments_; i++) {
            // Load rowptr file to memory
            sprintf(tmpFilename, "/rowptr%d.bin", i);
            strcpy(inpFilename, inpPath_.c_str());
            strcat(inpFilename, tmpFilename);

            fp = fopen(inpFilename, "rb");
            if (fp == NULL) {
                printf("File not found:%s\n", inpFilename);
                return -1;
            }
            uint64_t fileSize = filelength(inpFilename);
            uint64_t rem;
            int64_t nlocalverts;
            int64_t nlocaledges;
            if (fread(&nlocalverts, sizeof(int64_t), 1, fp)) {
		    //Do Noting; Just to avoid unused-result errors
            };
            if (fread(&nlocaledges, sizeof(int64_t), 1, fp)) {
		    //Do Noting; Just to avoid unused-result errors
            };
            rowptr_[i] = (int *)malloc(fileSize);
            if (rowptr_[i] == NULL) {
                cout << "malloc failed" << endl;
                return -1;
            }
            addr = rowptr_[i];
            rem = fileSize;
            while (!feof(fp) && (rem > 0)) {
                int64_t res = 0;
                res = fread(addr, 1, rem, fp);
                addr += res;
                rem -= res;
            }
            std::cout << "loadCSRSpM, loaded rowptr[" << i << "]" << std::endl;

            // Load column file to memory
            sprintf(tmpFilename, "/column%d.bin", i);
            strcpy(inpFilename, inpPath_.c_str());
            strcat(inpFilename, tmpFilename);
            fp = fopen(inpFilename, "rb");
            if (fp == NULL) {
                printf("File not found:%s\n", inpFilename);
                return -1;
            }
            fileSize = filelength(inpFilename);
            colptr_[i] = (int64_t *)malloc(fileSize);
            if (colptr_[i] == NULL) {
                cout << "malloc failed" << endl;
                return -1;
            }
            int64_t *addr_64 = colptr_[i];
            rem = fileSize;
            while (!feof(fp) && (rem > 0)) {
                int64_t res = 0;
                res = fread(addr_64, 1, rem, fp);
                addr_64 += res;
                rem -= res;
            }
            std::cout << "loadCSRSpM, loaded colptr[" << i << "]" << std::endl;

            // Load value file to memory
            sprintf(tmpFilename, "/weight%d.bin", i);
            strcpy(inpFilename, inpPath_.c_str());
            strcat(inpFilename, tmpFilename);
            fp = fopen(inpFilename, "rb");
            if (fp == NULL) {
                printf("File not found:%s\n", inpFilename);
                return -1;
            }
            fileSize = filelength(inpFilename);
            valptr_[i] = (float *)calloc(fileSize, 1);
            if (valptr_[i] == NULL) {
                cout << "malloc failed" << endl;
                return -1;
            }
            float *addr_d = valptr_[i];
            rem = fileSize;
            while (!feof(fp) && (rem > 0)) {
                int64_t res = 0;
                res = fread(addr_d, 1, rem, fp);
                addr_d += res;
                rem -= res;
            }
            std::cout << "loadCSRSpM, loaded value[" << i << "]" << std::endl;
        }
        return 0;
    }

    void Multiply(int iter) {
        std::vector<double> v0;
        std::vector<double> v1;
        std::cout << "Multiplicaion iteration : " << iter << " rows : " << nrows_
                  << " nsegment: " << numSegments_ << std::endl;
        if (iter % 2 == 0) {
            v0 = vector0_;
        } else {
            v0 = vector1_;
        }
        int k;

        ofstream outfile;
        outfile.open (OUTFILE);             
        int segmentSize = nrows_ / numSegments_;
        for (int i = 0; i < numSegments_; i++) {
            for (int j = 0; j < nrows_ / numSegments_; j++) {
                double sum = 0;
                // rowptr_[i][0] stores the total elements in this segment.
                // Ignore it for now.
                int64_t offShift = rowptr_[i][0];

                // Do Multiply
                for (k = rowptr_[i][j]; k < rowptr_[i][j + 1]; k++) {
                    sum +=
                        valptr_[i][k - offShift] * v0[colptr_[i][k - offShift]];
                }
                if (iter % 2 == 0)
                    vector1_[i * segmentSize + j] = sum;
                else
                    vector0_[i * segmentSize + j] = sum;
                if (iter == num_iteration_ - 1)
                    outfile   << "Row : " << i *segmentSize + j
                              << " Vector0 : " << vector0_[i * segmentSize + j]
                              << " Vector1 : " << vector1_[i * segmentSize + j]
                              << std::endl;
            }
        }
        outfile.close();
    }

    // CSR values
    int *rowptr_[255];
    int64_t *colptr_[255];
    float *valptr_[255];

    // std::vector
    std::vector<double> vector0_;
    std::vector<double> vector1_;

    int num_iteration_;
    int nrows_;
    int numSegments_;
    std::string inpPath_;
    std::string configFilename_;
};

int pagerank_sequential(int argc, char **argv) {
    int total_iter = atoi(argv[2]);
    pagerank *pr = new pagerank(argv[1], total_iter);
    int ret = pr->loadCSRSpM();
    if (ret != 0) {
        std::cerr << "Load CSR from file failed... " << std::endl;
        return ret;
    }
   
    pr->vector0_.assign(pr->nrows_, 1.0 / pr->nrows_);
    pr->vector1_.assign(pr->nrows_, 0);
    for (int i = 0; i < total_iter; i++) {
        pr->Multiply(i);
    }
    return 0;
}

int main(int argc, char **argv) { return pagerank_sequential(argc, argv); }
