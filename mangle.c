/*
 *
 * honggfuzz - buffer mangling routines
 * -----------------------------------------
 *
 * Author:
 * Robert Swiecki <swiecki@google.com>
 *
 * Copyright 2010-2015 by Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 */

#include "common.h"
#include "mangle.h"

#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "log.h"
#include "util.h"

static inline void mangle_Overwrite(uint8_t * dst, const uint8_t * src, size_t dstSz, size_t off,
                                    size_t sz)
{
    size_t maxToCopy = dstSz - off;
    if (sz > maxToCopy) {
        sz = maxToCopy;
    }

    memcpy(&dst[off], src, sz);
}

static void mangle_Byte(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    buf[off] = (uint8_t) util_rndGet(0, UINT8_MAX);
}

static void mangle_Bytes(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    uint32_t val = (uint32_t) util_rndGet(0, UINT32_MAX);

    /* Overwrite with random 2,3,4-byte values */
    size_t toCopy = util_rndGet(2, 4);
    mangle_Overwrite(buf, (uint8_t *) & val, fuzzer->dynamicFileSz, off, toCopy);
}

static void mangle_Bit(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    buf[off] ^= (uint8_t) (1U << util_rndGet(0, 7));
}

static void mangle_Dictionary(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    if (hfuzz->dictionaryCnt == 0) {
        mangle_Bit(hfuzz, fuzzer, fuzzer->dynamicFile);
        return;
    }

    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    uint64_t choice = util_rndGet(0, hfuzz->dictionaryCnt - 1);

    struct strings_t *str = TAILQ_FIRST(&hfuzz->dictq);
    for (uint64_t i = 0; i < choice; i++) {
        str = TAILQ_NEXT(str, pointers);
    }

    mangle_Overwrite(buf, (uint8_t *) str->s, fuzzer->dynamicFileSz, off, str->len);
}

static void mangle_Magic(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    /*  *INDENT-OFF* */
    static const struct {
        const uint8_t val[8];
        const size_t size;
    } mangleMagicVals[] = {
        /* 1B - No endianness */
        { "\x00\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x01\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x02\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x03\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x04\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x08\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x0C\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x10\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x20\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x40\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x7E\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x7F\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x80\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\x81\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\xC0\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\xFE\x00\x00\x00\x00\x00\x00\x00", 1},
        { "\xFF\x00\x00\x00\x00\x00\x00\x00", 1},
        /* 2B - NE */
        { "\x00\x00\x00\x00\x00\x00\x00\x00", 2},
        { "\x01\x01\x00\x00\x00\x00\x00\x00", 2},
        { "\x80\x80\x00\x00\x00\x00\x00\x00", 2},
        { "\xFF\xFF\x00\x00\x00\x00\x00\x00", 2},
        /* 2B - BE */
        { "\x00\x01\x00\x00\x00\x00\x00\x00", 2},
        { "\x00\x02\x00\x00\x00\x00\x00\x00", 2},
        { "\x00\x03\x00\x00\x00\x00\x00\x00", 2},
        { "\x00\x04\x00\x00\x00\x00\x00\x00", 2},
        { "\x7E\xFF\x00\x00\x00\x00\x00\x00", 2},
        { "\x7F\xFF\x00\x00\x00\x00\x00\x00", 2},
        { "\x80\x00\x00\x00\x00\x00\x00\x00", 2},
        { "\x80\x01\x00\x00\x00\x00\x00\x00", 2},
        { "\xFF\xFE\x00\x00\x00\x00\x00\x00", 2},
        /* 2B - LE */
        { "\x01\x00\x00\x00\x00\x00\x00\x00", 2},
        { "\x02\x00\x00\x00\x00\x00\x00\x00", 2},
        { "\x03\x00\x00\x00\x00\x00\x00\x00", 2},
        { "\x04\x00\x00\x00\x00\x00\x00\x00", 2},
        { "\xFF\x7E\x00\x00\x00\x00\x00\x00", 2},
        { "\xFF\x7F\x00\x00\x00\x00\x00\x00", 2},
        { "\x00\x80\x00\x00\x00\x00\x00\x00", 2},
        { "\x01\x80\x00\x00\x00\x00\x00\x00", 2},
        { "\xFE\xFF\x00\x00\x00\x00\x00\x00", 2},
        /* 4B - NE */
        { "\x00\x00\x00\x00\x00\x00\x00\x00", 4},
        { "\x01\x01\x01\x01\x00\x00\x00\x00", 4},
        { "\x80\x80\x80\x80\x00\x00\x00\x00", 4},
        { "\xFF\xFF\xFF\xFF\x00\x00\x00\x00", 4},
        /* 4B - BE */
        { "\x00\x00\x00\x01\x00\x00\x00\x00", 4},
        { "\x00\x00\x00\x02\x00\x00\x00\x00", 4},
        { "\x00\x00\x00\x03\x00\x00\x00\x00", 4},
        { "\x00\x00\x00\x04\x00\x00\x00\x00", 4},
        { "\x7E\xFF\xFF\xFF\x00\x00\x00\x00", 4},
        { "\x7F\xFF\xFF\xFF\x00\x00\x00\x00", 4},
        { "\x80\x00\x00\x00\x00\x00\x00\x00", 4},
        { "\x80\x00\x00\x01\x00\x00\x00\x00", 4},
        { "\xFF\xFF\xFF\xFE\x00\x00\x00\x00", 4},
        /* 4B - LE */
        { "\x01\x00\x00\x00\x00\x00\x00\x00", 4},
        { "\x02\x00\x00\x00\x00\x00\x00\x00", 4},
        { "\x03\x00\x00\x00\x00\x00\x00\x00", 4},
        { "\x04\x00\x00\x00\x00\x00\x00\x00", 4},
        { "\xFF\xFF\xFF\x7E\x00\x00\x00\x00", 4},
        { "\xFF\xFF\xFF\x7F\x00\x00\x00\x00", 4},
        { "\x00\x00\x00\x80\x00\x00\x00\x00", 4},
        { "\x01\x00\x00\x80\x00\x00\x00\x00", 4},
        { "\xFE\xFF\xFF\xFF\x00\x00\x00\x00", 4},
        /* 8B - NE */
        { "\x00\x00\x00\x00\x00\x00\x00\x00", 8},
        { "\x01\x01\x01\x01\x01\x01\x01\x01", 8},
        { "\x80\x80\x80\x80\x80\x80\x80\x80", 8},
        { "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8},
        /* 8B - BE */
        { "\x00\x00\x00\x00\x00\x00\x00\x01", 8},
        { "\x00\x00\x00\x00\x00\x00\x00\x02", 8},
        { "\x00\x00\x00\x00\x00\x00\x00\x03", 8},
        { "\x00\x00\x00\x00\x00\x00\x00\x04", 8},
        { "\x7E\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8},
        { "\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8},
        { "\x80\x00\x00\x00\x00\x00\x00\x00", 8},
        { "\x80\x00\x00\x00\x00\x00\x00\x01", 8},
        { "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFE", 8},
        /* 8B - LE */
        { "\x01\x00\x00\x00\x00\x00\x00\x00", 8},
        { "\x02\x00\x00\x00\x00\x00\x00\x00", 8},
        { "\x03\x00\x00\x00\x00\x00\x00\x00", 8},
        { "\x04\x00\x00\x00\x00\x00\x00\x00", 8},
        { "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7E", 8},
        { "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x7F", 8},
        { "\x00\x00\x00\x00\x00\x00\x00\x80", 8},
        { "\x01\x00\x00\x00\x00\x00\x00\x80", 8},
        { "\xFE\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 8},
    };
    /*  *INDENT-ON* */

    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    uint64_t choice = util_rndGet(0, ARRAYSIZE(mangleMagicVals) - 1);
    mangle_Overwrite(buf, mangleMagicVals[choice].val, fuzzer->dynamicFileSz, off,
                     mangleMagicVals[choice].size);
}

static void mangle_MemSet(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    size_t sz = util_rndGet(1, fuzzer->dynamicFileSz - off);
    int val = (int)util_rndGet(0, UINT8_MAX);

    memset(&buf[off], val, sz);
}

static void mangle_MemMove(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);

    uint64_t mangleTo = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    uint64_t mangleSzTo = fuzzer->dynamicFileSz - mangleTo;

    uint64_t mangleSzFrom = util_rndGet(1, fuzzer->dynamicFileSz - off);
    uint64_t mangleSz = mangleSzFrom < mangleSzTo ? mangleSzFrom : mangleSzTo;

    memmove(&buf[mangleTo], &buf[off], mangleSz);
}

static void mangle_Random(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    size_t len = util_rndGet(1, fuzzer->dynamicFileSz - off);
    util_rndBuf(&buf[off], len);
}

static void mangle_AddSub(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);

    /* 1,2,4 */
    uint64_t varLen = 1ULL << util_rndGet(0, 2);
    if ((fuzzer->dynamicFileSz - off) < varLen) {
        varLen = 1;
    }

    int delta = (int)util_rndGet(0, 64);
    delta -= 32;

    switch (varLen) {
    case 1:
        {
            buf[off] += delta;
            return;
            break;
        }
    case 2:
        {
            uint16_t val = *((uint16_t *) & buf[off]);
            if (util_rndGet(0, 1) == 0) {
                val += delta;
            } else {
                /* Foreign endianess */
                val = __builtin_bswap16(val);
                val += delta;
                val = __builtin_bswap16(val);
            }
            mangle_Overwrite(buf, (uint8_t *) & val, fuzzer->dynamicFileSz, off, varLen);
            return;
            break;
        }
    case 4:
        {
            uint32_t val = *((uint32_t *) & buf[off]);
            if (util_rndGet(0, 1) == 0) {
                val += delta;
            } else {
                /* Foreign endianess */
                val = __builtin_bswap32(val);
                val += delta;
                val = __builtin_bswap32(val);
            }
            mangle_Overwrite(buf, (uint8_t *) & val, fuzzer->dynamicFileSz, off, varLen);
            return;
            break;
        }
    default:
        {
            LOG_F("Unknown variable length size: %" PRIu64, varLen);
            break;
        }
    }
}

static void mangle_IncByte(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    buf[off] += (uint8_t) 1UL;
}

static void mangle_DecByte(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    buf[off] -= (uint8_t) 1UL;
}

static void mangle_CloneByte(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf)
{
    size_t off1 = util_rndGet(0, fuzzer->dynamicFileSz - 1);
    size_t off2 = util_rndGet(0, fuzzer->dynamicFileSz - 1);

    uint8_t tmp = buf[off1];
    buf[off1] = buf[off2];
    buf[off2] = tmp;
}

static void mangle_Trunc(honggfuzz_t * hfuzz UNUSED, fuzzer_t * fuzzer, uint8_t * buf UNUSED)
{
    fuzzer->dynamicFileSz = util_rndGet(1, fuzzer->dynamicFileSz);
}

static void mangle_Expand(honggfuzz_t * hfuzz, fuzzer_t * fuzzer, uint8_t * buf UNUSED)
{
    fuzzer->dynamicFileSz = util_rndGet(fuzzer->dynamicFileSz, hfuzz->maxFileSz);
}

void mangle_mangleContent(honggfuzz_t * hfuzz, fuzzer_t * fuzzer)
{
    /*  *INDENT-OFF* */
    static void (*const mangleFuncs[]) (honggfuzz_t* hfuzz, fuzzer_t* fuzzer, uint8_t* buf) = {
        mangle_Byte,
        mangle_Byte,
        mangle_Byte,
        mangle_Byte,
        mangle_Bit,
        mangle_Bit,
        mangle_Bit,
        mangle_Bit,
        mangle_Bytes,
        mangle_Magic,
        mangle_IncByte,
        mangle_DecByte,
        mangle_AddSub,
        mangle_Dictionary,
        mangle_MemMove,
        mangle_MemSet,
        mangle_Random,
        mangle_CloneByte,
        mangle_Trunc,
        mangle_Expand,
    };
    /*  *INDENT-ON* */

    /*
     * Minimal number of changes is 1
     */
    uint64_t changesCnt = fuzzer->dynamicFileSz * fuzzer->flipRate;
    if (changesCnt == 0ULL) {
        changesCnt = 1;
    }
    changesCnt = util_rndGet(1, changesCnt);

    for (uint64_t x = 0; x < changesCnt; x++) {
        uint64_t choice = util_rndGet(0, ARRAYSIZE(mangleFuncs) - 1);
        mangleFuncs[choice] (hfuzz, fuzzer, fuzzer->dynamicFile);
    }
}