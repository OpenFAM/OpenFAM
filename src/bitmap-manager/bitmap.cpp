/*
 * bitmap.cpp
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
#include "bitmap.h"

using namespace std;

/* bitwise functions */
static bool get(uint64_t, uint64_t);
static void set(uint64_t *, uint64_t);
static void reset(uint64_t *, uint64_t);

/* Registers the bitmap with fam_atomic and initialize the bitmap to 0*/
int bitmap_init(bitmap *bmap) {
    uint64_t size = bmap->size;

    int fd = -1;

    if (fam_atomic_register_region(bmap->map, size, fd, 0)) {
        cout << "unable to register atomic region" << endl;
        return -1;
    }
    // Intialize the bitmap with zero
    for (uint64_t i = 0; i < size / sizeof(int64_t); i++) {
        fam_atomic_64_write((int64_t *)bmap->map + i, 0);
    }
    return 0;
}

void bitmap_free(bitmap *bmap) {

    fam_atomic_unregister_region(bmap->map, bmap->size);
}

/* Returns the value of the @n'th bit of the bitmap */
bool bitmap_get(bitmap *bmap, uint64_t n) {
    uint64_t offset = n / BITSIZE;

    int64_t *addr = (int64_t *)bmap->map + offset;

    uint64_t value = fam_atomic_64_read(addr);

    uint64_t pos = n % BITSIZE;

    return get(value, pos);
}

/* Sets the n'th bit of the bitmap to true */
void bitmap_set(bitmap *bmap, uint64_t n) {
    uint64_t newValue, result;
    uint64_t offset = n / BITSIZE;
    uint64_t pos = n % BITSIZE;

    int64_t *addr = (int64_t *)bmap->map + offset;

    uint64_t oldValue = fam_atomic_64_read(addr);

    int retry = 0;

    do {
        newValue = oldValue;

        set(&newValue, pos);

        result = fam_atomic_64_compare_store(addr, oldValue, newValue);  
        if (result != oldValue) {
            oldValue = result;
            retry = 1;
        } else {
            retry = 0;
        }
    } while (retry);
}

/* Sets the n'th bit of the bitmap to false */
void bitmap_reset(bitmap *bmap, uint64_t n) {
    uint64_t newValue, result;
    uint64_t offset = n / BITSIZE;
    uint64_t pos = n % BITSIZE;

    int64_t *addr = (int64_t *)bmap->map + offset;

    uint64_t oldValue = fam_atomic_64_read(addr);

    int retry = 0;

    do {
        newValue = oldValue;

        reset(&newValue, pos);

        result = fam_atomic_64_compare_store(addr, oldValue, newValue);  
        if (result != oldValue) {
            oldValue = result;
            retry = 1;
        } else {
            retry = 0;
        }
    } while (retry);

}

/* Check the n'th bit value and then sets the n'th bit 
 * of the bitmap to true/false.
 * If val is true, it's reset to false and if val is
 * false, it's set to true.
 */ 
int bitmap_reserve(bitmap *bmap, bool val, uint64_t n) {
    uint64_t newValue, result;
    uint64_t offset = n / BITSIZE;
    uint64_t pos = n % BITSIZE;

    int64_t *addr = (int64_t *)bmap->map + offset;

    uint64_t oldValue = fam_atomic_64_read(addr);

    int retry = 0;

    do {
        newValue = oldValue;

        // Check if the bit value at "pos" is "val".
        // If not, it means someone else has set/reset the
        // bit value. Return failure.
        if (get(newValue, pos) !=val) {
            return -1;
        }
        
        if (val) 
            reset(&newValue, pos);
        else
            set(&newValue, pos);
        
        result = fam_atomic_64_compare_store(addr, oldValue, newValue);  
        if (result != oldValue) {
            // Check if bit value at pos has changed. If not retry
            // the operation, else return failure.
            if (get(result, pos) !=val) {
                return -1;
            }
            oldValue = result;
            retry = 1;
        } else {
            retry = 0;
        }
    } while (retry);

    return 0;
}

/* Finds the first free "val" in bitmap after start bit 
 * and set/reset the bit and return the bit.
 * if val was 0, it will be set to 1 and if val is 1, it 
 * will be reset to 0 and bit position will be returned.
 */
uint64_t bitmap_find_and_reserve(bitmap *bmap, bool val, uint64_t start) {
    uint64_t i;
    uint64_t size = bmap->size;

    /* size is now the Bitmap size in bits */
    for (i = start, size *= 8; i < size; i++) {
        if (bitmap_get(bmap, i) == val) {
            // If bitmap_reserve() fails to reserve the bit,
            // try next available bit.
            if (bitmap_reserve(bmap, val, i) == 0) {
                return i; 
            }
        }
    }

    return BITMAP_NOTFOUND;
}

/* Finds the first n value in bitmap after start */
/* size is the Bitmap size in bytes */
uint64_t bitmap_find(bitmap *bmap, bool n, uint64_t start) {
    uint64_t i;
    uint64_t size = bmap->size;

    /* size is now the Bitmap size in bits */
    for (i = start, size *= 8; i < size; i++) {
        if (bitmap_get(bmap, i) == n)
            return i;
    }
    return BITMAP_NOTFOUND;
}

/* Returns the value of byte at bit position*/
static bool get(uint64_t byte, uint64_t bit) { return (byte >> bit) & 1UL; }

/* Sets byte's bit position to 1 */
static void set(uint64_t *byte, uint64_t bit) { *byte |= (1UL << bit); }

/* Sets byte's bit position to 0 */
static void reset(uint64_t *byte, uint64_t bit) {
    *byte &= (~(1UL << bit));
}
