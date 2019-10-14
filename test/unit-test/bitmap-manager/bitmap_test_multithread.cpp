/*
 *   bitmap_test_multithread.cpp
 *   Copyright (c) 2019 Hewlett Packard Enterprise Development, LP. All
 *   rights reserved.
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *   1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 *      THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 *      IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 *      BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *      FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 *      SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *      INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *      DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *      OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *      INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *      CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *      OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *      IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * See https://spdx.org/licenses/BSD-3-Clause
 *
 */

#include "bitmap-manager/bitmap.h"

#include <string.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <sys/types.h>
#include <sys/syscall.h>

using namespace std;

bitmap *bmap = new bitmap();

void *worker1(void *vargp) {

    uint64_t i;
    bool bitValue;

    int64_t pos;

    for (i = 0; i < 100; i++) {

        pos = bitmap_find_and_reserve(bmap, 0, 0);
        if (pos ==-1) {
            int *ret;
            ret = (int *) malloc (sizeof(int));
            cout << syscall(SYS_gettid)  << 
                  ": failed to get free bitmap" << endl;
            *ret = -1;
            pthread_exit((void *)ret);
        } else {
            bitValue = bitmap_get(bmap, pos);
            if (!bitValue) {
                int *ret;
                ret = (int *) malloc (sizeof(int));
                cout << syscall(SYS_gettid)  << ": Expected 1, got " 
                         << bitValue << " at " << pos << endl;
                *ret = -1;
                pthread_exit((void *)ret);
            }
        }
    }

    pthread_exit(NULL);
}

void *worker2(void *vargp) {
    uint64_t j;
    bool bitValue;

    int64_t pos;

    for (j = 0; j < 100; j++) {

        pos = bitmap_find_and_reserve(bmap, 0, 0);
        if (pos ==-1) {
            int *ret;
            ret = (int *) malloc (sizeof(int));
            cout << syscall(SYS_gettid)  << 
                  ": failed to get free bitmap" << endl;
            *ret = -1;
            pthread_exit((void *)ret);
        } else {
            bitValue = bitmap_get(bmap, pos);
            if (!bitValue) {
                int *ret;
                ret = (int *) malloc (sizeof(int));
                cout << syscall(SYS_gettid)  << ": Expected 1, got " 
                         << bitValue << " at " << pos << endl;
                *ret = -1;
                pthread_exit((void *)ret);
            }
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {

    int ret;

    int64_t buf[10];
    bmap->size = 10 * sizeof(int64_t);
    bmap->map = &buf;
    void *result1, *result2;

    ret = bitmap_init(bmap);
    if (ret) {
        cout << "bitmap initialization failed" << endl;
        return -1;
    }

    pthread_t t1, t2;
    int pret;

    pret = pthread_create( &t1, NULL, worker1, NULL );
    if (pret) {
        cout << "pthread_create() return code: " << pret << endl;
        exit (1);
    } 

    pret = pthread_create( &t2, NULL, worker2, NULL );
    if (pret) {
        cout << "pthread_create() return code: " << pret << endl;
        exit (1);
    } 

    pthread_join(t1, &result1);
    pthread_join(t2, &result2);

    if ( (result1!= NULL) || result2 != NULL) {
       return -1;
    }

    return 0;

}
