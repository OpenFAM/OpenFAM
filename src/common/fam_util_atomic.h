/*
 * fam_util_atomic.h
 * Copyright (c) 2019, 2023 Hewlett Packard Enterprise Development, LP. All rights
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

#define FAM_OP_MIN(dst, src) (dst) > (src)
#define FAM_OP_MAX(dst, src) (dst) < (src)
#define FAM_OP_BOR(dst, src) (dst | src)
#define FAM_OP_BAND(dst, src) (dst & src)
#define FAM_OP_BXOR(dst, src) (dst ^ src)
#define FAM_OP_SUM(dst, src) dst + src

#define FAM_DEF_NOOP_NAME NULL,
#define FAM_DEF_NOOP_FUNC

#define FAM_DEF_READWRITEEXT_NAME_32(op, type) fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_FUNC_32(op, type)                                 \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        type *d = (type *)(dst);                                               \
        const type *s = (type *)(src);                                         \
        type val, readValue, oldValue;                                         \
        do {                                                                   \
            readValue = fam_atomic_32_read((int32_t *)d);                      \
            val = op(readValue, *s);                                           \
            oldValue = fam_atomic_32_compare_store(                            \
                (int32_t *)d, (int32_t)readValue, (int32_t)val);               \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = oldValue;                                                      \
    }

#define FAM_DEF_READWRITEEXT_CMP_NAME_32(op, type) fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_CMP_FUNC_32(op, type)                             \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        type *d = (type *)(dst);                                               \
        const type *s = (type *)(src);                                         \
        type readValue, oldValue;                                              \
        do {                                                                   \
            readValue = fam_atomic_32_read((int32_t *)d);                      \
            if (op(readValue, *s)) {                                           \
                oldValue = fam_atomic_32_compare_store(                        \
                    (int32_t *)d, (int32_t)readValue, (int32_t)*s);            \
            } else {                                                           \
                break;                                                         \
            }                                                                  \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = readValue;                                                     \
    }

#define FAM_DEF_READWRITEEXT_NAME_64(op, type) fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_FUNC_64(op, type)                                 \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        type *d = (type *)(dst);                                               \
        const type *s = (type *)(src);                                         \
        type val, readValue, oldValue;                                         \
        do {                                                                   \
            readValue = fam_atomic_64_read((int64_t *)d);                      \
            val = op(readValue, *s);                                           \
            oldValue = fam_atomic_64_compare_store(                            \
                (int64_t *)d, (int64_t)readValue, (int64_t)val);               \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = oldValue;                                                      \
    }

#define FAM_DEF_READWRITEEXT_CMP_NAME_64(op, type) fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_CMP_FUNC_64(op, type)                             \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        type *d = (type *)(dst);                                               \
        const type *s = (type *)(src);                                         \
        type readValue, oldValue;                                              \
        do {                                                                   \
            readValue = fam_atomic_64_read((int64_t *)d);                      \
            if (op(readValue, *s)) {                                           \
                oldValue = fam_atomic_64_compare_store(                        \
                    (int64_t *)d, (int64_t)readValue, (int64_t)*s);            \
            } else {                                                           \
                break;                                                         \
            }                                                                  \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = readValue;                                                     \
    }

#define FAM_DEF_READWRITEEXT_NAME_float(op, type) fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_FUNC_float(op, type)                              \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        int32_t *d = (int32_t *)(dst);                                         \
        const type *s = (type *)(src);                                         \
        int32_t readValue, oldValue, *writeValue;                              \
        type val, *readValueF;                                                 \
        do {                                                                   \
            readValue = fam_atomic_32_read(d);                                 \
            readValueF = reinterpret_cast<float *>(&readValue);                \
            val = op(*readValueF, *s);                                         \
            writeValue = reinterpret_cast<int32_t *>(&val);                    \
            oldValue = fam_atomic_32_compare_store(d, readValue, *writeValue); \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = *readValueF;                                                   \
    }

#define FAM_DEF_READWRITEEXT_CMP_NAME_float(op, type)                          \
    fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_CMP_FUNC_float(op, type)                          \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        int32_t *d = (int32_t *)(dst);                                         \
        const type *s = (type *)(src);                                         \
        int32_t readValue, oldValue, *writeValue;                              \
        type writeValueF, *readValueF;                                         \
        do {                                                                   \
            readValue = fam_atomic_32_read(d);                                 \
            readValueF = reinterpret_cast<float *>(&readValue);                \
            writeValueF = *s;                                                  \
            if (op(*readValueF, *s)) {                                         \
                writeValue = reinterpret_cast<int32_t *>(&writeValueF);        \
                oldValue =                                                     \
                    fam_atomic_32_compare_store(d, readValue, *writeValue);    \
            } else {                                                           \
                break;                                                         \
            }                                                                  \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = *readValueF;                                                   \
    }

#define FAM_DEF_READWRITEEXT_NAME_double(op, type) fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_FUNC_double(op, type)                             \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        int64_t *d = (int64_t *)(dst);                                         \
        const type *s = (type *)(src);                                         \
        int64_t readValue, oldValue, *writeValue;                              \
        type val, *readValueD;                                                 \
        do {                                                                   \
            readValue = fam_atomic_64_read(d);                                 \
            readValueD = reinterpret_cast<type *>(&readValue);                 \
            val = op(*readValueD, *s);                                         \
            writeValue = reinterpret_cast<int64_t *>(&val);                    \
            oldValue = fam_atomic_64_compare_store(d, readValue, *writeValue); \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = *readValueD;                                                   \
    }

#define FAM_DEF_READWRITEEXT_CMP_NAME_double(op, type)                         \
    fam_readwrite_##op##_##type,
#define FAM_DEF_READWRITEEXT_CMP_FUNC_double(op, type)                         \
    static void fam_readwrite_##op##_##type(void *dst, const void *src,        \
                                            void *res) {                       \
        int64_t *d = (int64_t *)(dst);                                         \
        const type *s = (type *)(src);                                         \
        int64_t readValue, oldValue, *writeValue;                              \
        type writeValueD, *readValueD;                                         \
        do {                                                                   \
            readValue = fam_atomic_64_read(d);                                 \
            readValueD = reinterpret_cast<type *>(&readValue);                 \
            writeValueD = *s;                                                  \
            if (op(*readValueD, *s)) {                                         \
                writeValue = reinterpret_cast<int64_t *>(&writeValueD);        \
                oldValue =                                                     \
                    fam_atomic_64_compare_store(d, readValue, *writeValue);    \
            } else {                                                           \
                break;                                                         \
            }                                                                  \
        } while (oldValue != readValue);                                       \
        type *resP = (type *)res;                                              \
        *resP = *readValueD;                                                   \
    }

#define FAM_DEFINE_ALL_HANDLERS(ATOMICTYPE, FUNCNAME, op)                      \
    FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_32(op, int32_t)                        \
        FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_32(op, uint32_t)                   \
            FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_64(op, int64_t)                \
                FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_64(op, uint64_t)           \
                    FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_float(op, float)       \
                        FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_double(op, double)

#define FAM_DEFINE_INT_HANDLERS(ATOMICTYPE, FUNCNAME, op)                      \
    FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_32(op, int32_t)                        \
        FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_32(op, uint32_t)                   \
            FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_64(op, int64_t)                \
                FAM_DEF_##ATOMICTYPE##_##FUNCNAME##_64(op, uint64_t)           \
                    FAM_DEF_NOOP_##FUNCNAME FAM_DEF_NOOP_##FUNCNAME

FAM_DEFINE_ALL_HANDLERS(READWRITEEXT_CMP, FUNC, FAM_OP_MIN)
FAM_DEFINE_ALL_HANDLERS(READWRITEEXT_CMP, FUNC, FAM_OP_MAX)
FAM_DEFINE_INT_HANDLERS(READWRITEEXT, FUNC, FAM_OP_BOR)
FAM_DEFINE_INT_HANDLERS(READWRITEEXT, FUNC, FAM_OP_BAND)
FAM_DEFINE_INT_HANDLERS(READWRITEEXT, FUNC, FAM_OP_BXOR)
FAM_DEFINE_ALL_HANDLERS(READWRITEEXT, FUNC, FAM_OP_SUM)

void (*fam_atomic_readwrite_handlers[6][6])(void *dst, const void *src,
                                            void *res) = {
    {FAM_DEFINE_ALL_HANDLERS(READWRITEEXT_CMP, NAME, FAM_OP_MIN)},
    {FAM_DEFINE_ALL_HANDLERS(READWRITEEXT_CMP, NAME, FAM_OP_MAX)},
    {FAM_DEFINE_INT_HANDLERS(READWRITEEXT, NAME, FAM_OP_BOR)},
    {FAM_DEFINE_INT_HANDLERS(READWRITEEXT, NAME, FAM_OP_BAND)},
    {FAM_DEFINE_INT_HANDLERS(READWRITEEXT, NAME, FAM_OP_BXOR)},
    {FAM_DEFINE_ALL_HANDLERS(READWRITEEXT, NAME, FAM_OP_SUM)},
};
