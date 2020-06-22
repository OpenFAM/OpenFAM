/*
 * fam_create_alloc_destroy_mt.cpp
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
#include <fam/fam_exception.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include <fam/fam.h>
using namespace std;
using namespace openfam;

#define NUM_THREADS 10

/* thread function */
void *thr_func1(void *arg) {

    Fam_Region_Descriptor *desc1, *desc2, *desc3;
    Fam_Descriptor *item1, *item2, *item3;

    Fam_Options fam_opts;

    fam *my_fam;
    my_fam = new fam();
    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    fam_opts.allocator = strdup("SHM");
    fam_opts.runtime = strdup("NONE");

    if (my_fam->fam_initialize("default", &fam_opts) < 0) {
        cout << "fam initialization failed" << endl;
        exit(1);
    } else {
        cout << "fam initialization successful" << endl;
    }

    desc1 = my_fam->fam_lookup_region("test111");
    if (desc1 == NULL) {
        cout << "fam lookup region failed" << endl;
        exit(1);
    }

    desc2 = my_fam->fam_lookup_region("test112");
    if (desc2 == NULL) {
        cout << "fam lookup region failed" << endl;
        exit(1);
    }

    desc3 = my_fam->fam_lookup_region("test113");
    if (desc3 == NULL) {
        cout << "fam lookup region failed" << endl;
        exit(1);
    }

    item2 = my_fam->fam_lookup("second", "test112");
    if (item2 == NULL) {
        cout << "fam lookup of data item 'second' failed" << endl;
        exit(1);
    }

    item1 = my_fam->fam_lookup("first", "test111");
    if (item1 == NULL) {
        cout << "fam lookup of data item 'first' failed" << endl;
        exit(1);
    }

    item3 = my_fam->fam_lookup("third", "test113");
    if (item3 == NULL) {
        cout << "fam lookup of data item 'third' failed" << endl;
        exit(1);
    }

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;
    delete my_fam;

    pthread_exit(NULL);
}

int main() {
    Fam_Options fam_opts;
    pthread_t thr[NUM_THREADS];
    int i, rc;
    fam *my_fam;

    Fam_Region_Descriptor *desc1, *desc2, *desc3;
    Fam_Descriptor *item1, *item2, *item3;

    my_fam = new fam();
    memset((void *)&fam_opts, 0, sizeof(Fam_Options));

    fam_opts.allocator = strdup("SHM");
    fam_opts.runtime = strdup("NONE");

    if (my_fam->fam_initialize("default", &fam_opts) < 0) {
        cout << "fam initialization failed" << endl;
        exit(1);
    } else {
        cout << "fam initialization successful" << endl;
    }

    desc1 = my_fam->fam_create_region("test111", 8192, 0777, RAID1);
    if (desc1 == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }

    desc2 = my_fam->fam_create_region("test112", 8192, 0777, RAID1);
    if (desc2 == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }

    desc3 = my_fam->fam_create_region("test113", 8192, 0777, RAID1);
    if (desc3 == NULL) {
        cout << "fam create region failed" << endl;
        exit(1);
    }

    // Allocating data items in the created region
    item1 = my_fam->fam_allocate("first", 1024, 0777, desc1);
    if (item1 == NULL) {
        cout << "fam allocation of data item 'first' failed" << endl;
        exit(1);
    }

    item2 = my_fam->fam_allocate("second", 1024, 0777, desc2);
    if (item2 == NULL) {
        cout << "fam allocation of data item 'second' failed" << endl;
        exit(1);
    }

    item3 = my_fam->fam_allocate("third", 1024, 0777, desc3);
    if (item3 == NULL) {
        cout << "fam allocation of data item 'third' failed" << endl;
        exit(1);
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        if ((rc = pthread_create(&thr[i], NULL, thr_func1, NULL))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            return -1;
        }
    }

    for (i = 0; i < NUM_THREADS; ++i) {
        pthread_join(thr[i], NULL);
    }

    if (desc1 != NULL)
        my_fam->fam_destroy_region(desc1);

    if (desc2 != NULL)
        my_fam->fam_destroy_region(desc2);

    if (desc3 != NULL)
        my_fam->fam_destroy_region(desc3);

    my_fam->fam_finalize("default");
    cout << "fam finalize successful" << endl;

    return 0;
}
