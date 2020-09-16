/*
 *   bitmap_test.cpp
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

using namespace std;

int main(int argc, char *argv[]) {

    int ret;
    bitmap *bmap = new bitmap();
    uint64_t fail = 0, pass = 0;

    void *buf = malloc(10 * sizeof(int64_t));
    bmap->size = 10 * sizeof(int64_t);
    bmap->map = buf;

    ret = bitmap_init(bmap);
    if (ret) {
        cout << "bitmap initialization failed" << endl;
        return -1;
    }

    uint64_t i;
    bool bitValue;

    for (i = 1; i < 64 * 10; i++) {
        bitValue = bitmap_get(bmap, i);
        if (bitValue) {
            cout << "Expected 0, got " << bitValue << " at " << i << endl;
            fail++;
        } else {
            pass++;
        }
    }
    bitValue = bitmap_get(bmap, 10);
    if (bitValue) {
        cout << "get(10)Expected 0, got " << bitValue << endl;
        fail++;
    }

    bitValue = bitmap_get(bmap, 100);
    if (bitValue) {
        cout << "get(100)Expected 0, got " << bitValue << endl;
        fail++;
    }
    bitmap_set(bmap, 100);

    bitValue = bitmap_get(bmap, 100);
    if (!bitValue) {
        cout << "get(100)Expected 1, got " << bitValue << endl;
        fail++;
    }
    bitmap_reset(bmap, 100);

    bitValue = bitmap_get(bmap, 100);
    if (bitValue) {
        cout << "get(100)Expected 0, got " << bitValue << endl;
        fail++;
    }

    bitmap_set(bmap, 200);

    bitValue = bitmap_get(bmap, 200);
    if (!bitValue) {
        cout << "get(200)Expected 1, got " << bitValue << endl;
        fail++;
    }
    bitmap_reset(bmap, 200);

    bitValue = bitmap_get(bmap, 100);
    if (bitValue) {
        cout << "get(100)Expected 0, got " << bitValue << endl;
        fail++;
    }

    for (i = 1; i < 100; i++) {
        bitmap_set(bmap, i);
    }

    for (i = 1; i < 100; i++) {
        bitValue = bitmap_get(bmap, i);
        if (!bitValue) {
            cout << "Expected 1, got " << bitValue << " at " << i << endl;
            fail++;
        }
    }
    for (i = 101; i < 200; i++) {
        bitValue = bitmap_get(bmap, i);
        if (bitValue) {
            cout << "Expected 0, got " << bitValue << " at " << i << endl;
            fail++;
        }
    }

    int64_t pos;

    pos = bitmap_find(bmap, 0, 0);
    if (pos != 0) {
        cout << "Expected 0 at pos 0, but got at " << pos << endl;
        fail++;
    }

    pos = bitmap_find(bmap, 1, 1);
    if (pos != 1) {
        cout << "Expected 1 at pos 1, but got at " << pos << endl;
        fail++;
    }

    for (i = 0; i < 10; i++) {
        bitmap_reset(bmap, i);
    }
    for (i = 0; i < 10; i++) {
        bitValue = bitmap_get(bmap, i);
        if (bitValue) {
            cout << "Expected 0, got " << bitValue << " at " << i << endl;
            fail++;
        }
    }

    pos = bitmap_find(bmap, 0, 5);
    if (pos != 5) {
        cout << "Expected 0 at pos 5, but got at " << pos << endl;
        fail++;
    }

    pos = bitmap_find(bmap, 1, 1);
    if (pos != 10) {
        cout << "Expected 1 at pos 10, but got at " << pos << endl;
        fail++;
    }

    pos = bitmap_find(bmap, 1, 200);
    if (pos != -1) {
        cout << "Expected 1 at pos -1, but got at " << pos << endl;
        fail++;
    }
    bitmap_set(bmap, 200);
    pos = bitmap_find(bmap, 1, 192);
    if (pos != 200) {
        cout << "Expected 1 at pos 200, but got at " << pos << endl;
        fail++;
    }

    cout << "Test completed" << endl;

    if (fail != 0) {
        cout << "Test failed" << endl;
        exit(1);
    } else {
        cout << "Test passed" << endl;
        exit(0);
    }
}
