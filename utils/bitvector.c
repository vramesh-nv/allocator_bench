#include "bitvector.h"

// The bit vector is divided into "chunks" where each chunk is 64-bits wide
// and is the smallest bit vector that can be allocated. Normally, memory
// is allocated for the chunks. But if the size can be contained within a
// single chunk, then it is "inlined" (since a pointer in a 64-bit process
// is 64-bits wide, the bit vector can be inlined into the pointer itself
// instead of allocating memory and storing the bit vector there).

#define CHUNK_SIZE           sizeof(NvU64)
#define BITS_PER_CHUNK       (CHUNK_SIZE * 8)

#define CHUNK_INDEX(bit)     (bit / BITS_PER_CHUNK)
#define CHUNK_BITMASK(bit)   (1ULL << (bit & (BITS_PER_CHUNK - 1)))

#define ROUND_UP(x, n) (((x) + ((n) - 1)) & ~((n) - 1))

#define NUM_CHUNKS(numBits)  (ROUND_UP(numBits, BITS_PER_CHUNK) / BITS_PER_CHUNK)
#define INLINE_LIMIT         BITS_PER_CHUNK

#define BITS_IN_LAST_CHUNK(numBits) (((numBits - 1) % BITS_PER_CHUNK) + 1)

struct CUbitvector_st {
    NvU64 numBits;
    union {
        NvU64 inlineVec;
        NvU64 *vecPtr;
    } bits;
};

NvBool
cubitvectorCreate(CUbitvector **bitvector, NvU64 numBits)
{
    CUbitvector *lbitvector;
    NvU64 byteSize = NUM_CHUNKS(numBits) * CHUNK_SIZE;

    CU_ASSERT(bitvector);

    *bitvector = NULL;

    // Make sure the requested size is not too small or too big
    if (!byteSize || ((size_t)byteSize != byteSize)) {
        return NV_FALSE;
    }

    lbitvector = (CUbitvector *)calloc(1, sizeof(CUbitvector));
    if (lbitvector == NULL) {
        return NV_FALSE;
    }

    lbitvector->numBits = numBits;

    // Allocate memory for the bit vector if it's bigger than the inline threshold
    if (lbitvector->numBits > INLINE_LIMIT) {
        lbitvector->bits.vecPtr = (NvU64 *)calloc(1, (size_t)byteSize);
        if (!lbitvector->bits.vecPtr) {
            free(lbitvector);
            return NV_FALSE;
        }
    }

    *bitvector = lbitvector;

    return NV_TRUE;
}

void
cubitvectorDestroy(CUbitvector *bitvector)
{
    if (bitvector) {
        if (bitvector->numBits > INLINE_LIMIT) {
            free(bitvector->bits.vecPtr);
        }
        free(bitvector);
    }
}

NvU64
cubitvectorGetSize(CUbitvector *bitvector)
{
    return (bitvector) ? bitvector->numBits : 0;
}

NvBool
cubitvectorCreateCopy(CUbitvector **copy, CUbitvector *original)
{
    NvBool status = NV_TRUE;

    if (!original) {
        return NV_FALSE;
    }

    status = cubitvectorCreate(copy, original->numBits);
    if (status != NV_TRUE) {
        return status;
    }

    if (original->numBits > INLINE_LIMIT) {
        memcpy((*copy)->bits.vecPtr, original->bits.vecPtr, CHUNK_SIZE * (size_t)NUM_CHUNKS(original->numBits));
    }
    else {
        (*copy)->bits.inlineVec = original->bits.inlineVec;
    }

    return status;
}

NvBool cubitvectorGrow(CUbitvector *bitvector, NvU64 newNumBits)
{
    if (!bitvector) {
        return NV_FALSE;
    }
 
    if (bitvector->numBits >= newNumBits) {
        // bitvector is already large enough to hold requested number of bits.
        return NV_TRUE;
    }
 
    NvU64 newByteSize = NUM_CHUNKS(newNumBits) * CHUNK_SIZE;
    NvU64 originalByteSize = NUM_CHUNKS(bitvector->numBits) * CHUNK_SIZE;
 
    if (bitvector->numBits > INLINE_LIMIT) {
        if (newByteSize != originalByteSize) {
            NvU64 *newVector = (NvU64 *)realloc(bitvector->bits.vecPtr, (size_t)newByteSize);
            if (newVector == NULL) {
                return NV_FALSE;
            }

            memset((char *)newVector + originalByteSize, 0, (size_t)(newByteSize - originalByteSize));
            bitvector->bits.vecPtr = newVector;
        }
    }
    else if (newNumBits > INLINE_LIMIT) {
        NvU64 *newVector = (NvU64 *)calloc(1, (size_t)newByteSize);
        if (newVector == NULL) {
            return NV_FALSE;
        }
        newVector[0] = bitvector->bits.inlineVec;
        bitvector->bits.vecPtr = newVector;
    }
 
    bitvector->numBits = newNumBits;
 
    return NV_TRUE;
}

NvBool
cubitvectorSetBit(CUbitvector *bitvector, NvU64 bit)
{
    if (!bitvector || (bit >= bitvector->numBits)) {
        return NV_FALSE;
    }

    if (bitvector->numBits <= INLINE_LIMIT) {
        bitvector->bits.inlineVec |= CHUNK_BITMASK(bit);
    }
    else {
        bitvector->bits.vecPtr[CHUNK_INDEX(bit)] |= CHUNK_BITMASK(bit);
    }

    return NV_TRUE;
}

static NvU64
lowestBitsSet(NvU64 numBits)
{
    if (numBits == INLINE_LIMIT) {
        return ~0ll; // shift by 64 bits may be a no-op.
    }
    else {
        return ~(-1ll << numBits);
    }
}

NvBool
cubitvectorSetAllBits(CUbitvector *bitvector)
{
    if (!bitvector) {
        return NV_FALSE;
    }

    if (bitvector->numBits <= INLINE_LIMIT) {
        bitvector->bits.inlineVec = lowestBitsSet(bitvector->numBits);
    }
    else {
        memset(bitvector->bits.vecPtr, -1, CHUNK_SIZE * (size_t)(NUM_CHUNKS(bitvector->numBits) - 1));
        bitvector->bits.vecPtr[NUM_CHUNKS(bitvector->numBits) - 1] = lowestBitsSet(BITS_IN_LAST_CHUNK(bitvector->numBits));
    }

    return NV_TRUE;
}

void
cubitvectorSetBitsInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit)
{
    if (!bitvector || lowBit > highBit || highBit > bitvector->numBits - 1) {
        return;
    }

    NvU64 *vector = bitvector->bits.vecPtr;
    if (bitvector->numBits <= INLINE_LIMIT) {
        vector = &bitvector->bits.inlineVec;
    }

    size_t lowChunkIdx = CHUNK_INDEX(lowBit);
    size_t highChunkIdx = CHUNK_INDEX(highBit);

    size_t i;
    for (i = lowChunkIdx; i <= highChunkIdx; i++) {
        NvU64 mask = ~0ULL;
        if (i == lowChunkIdx) {
            mask <<= lowBit % BITS_PER_CHUNK;
        }
        if (i == highChunkIdx) {
            mask &= ~0ULL >> (BITS_PER_CHUNK - 1 - highBit % BITS_PER_CHUNK);
        }

        vector[i] |= mask;
    }
}

void
cubitvectorClearBitsInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit)
{
    if (!bitvector || lowBit > highBit || highBit > bitvector->numBits - 1) {
        return;
    }

    NvU64 *vector = bitvector->bits.vecPtr;
    if (bitvector->numBits <= INLINE_LIMIT) {
        vector = &bitvector->bits.inlineVec;
    }

    size_t lowChunkIdx = CHUNK_INDEX(lowBit);
    size_t highChunkIdx = CHUNK_INDEX(highBit);

    size_t i;
    for (i = lowChunkIdx; i <= highChunkIdx; i++) {
        NvU64 mask = ~0ULL;
        if (i == lowChunkIdx) {
            mask <<= lowBit % BITS_PER_CHUNK;
        }
        if (i == highChunkIdx) {
            mask &= ~0ULL >> (BITS_PER_CHUNK - 1 - highBit % BITS_PER_CHUNK);
        }

        vector[i] &= ~mask;
    }
}

NvBool
cubitvectorClearBit(CUbitvector *bitvector, size_t bit)
{
    if (!bitvector || (bit >= bitvector->numBits)) {
        return NV_FALSE;
    }

    if (bitvector->numBits <= INLINE_LIMIT) {
        bitvector->bits.inlineVec &= ~CHUNK_BITMASK(bit);
    }
    else {
        bitvector->bits.vecPtr[CHUNK_INDEX(bit)] &= ~CHUNK_BITMASK(bit);
    }

    return NV_TRUE;
}

NvBool
cubitvectorIsBitSet(CUbitvector *bitvector, size_t bit)
{
    if (!bitvector || (bit >= bitvector->numBits)) {
        return NV_FALSE;
    }

    if (bitvector->numBits <= INLINE_LIMIT) {
        return ((bitvector->bits.inlineVec & CHUNK_BITMASK(bit)) != 0);
    }
    else {
        return ((bitvector->bits.vecPtr[CHUNK_INDEX(bit)] & CHUNK_BITMASK(bit)) != 0);
    }
}

NvBool
cubitvectorIsAnyBitSet(CUbitvector *bitvector)
{
    if (!bitvector) {
        return NV_FALSE;
    }

    if (bitvector->numBits <= INLINE_LIMIT) {
        return (bitvector->bits.inlineVec != 0);
    }
    else {
        NvU64 i, numChunks = NUM_CHUNKS(bitvector->numBits);
        for (i = 0; i < numChunks; i++) {
            if (bitvector->bits.vecPtr[i]) {
                return NV_TRUE;
            }
        }
    }

    return NV_FALSE;
}

static NvBool cubitvectorSetLowestClearBitInChunk(NvU64 *chunk, size_t bitCount, NvU64 *bit)
{
    size_t i;
    for (i = 0; i < bitCount; i++) {
        if ((*chunk & CHUNK_BITMASK(i)) == 0) {
            *chunk |= CHUNK_BITMASK(i);
            *bit = i;
            return NV_TRUE;
        }
    }
 
    return NV_FALSE;
}
 
NvBool cubitvectorSetLowestClearBit(CUbitvector *bitvector, NvU64 *bit)
{
    if (!bitvector) {
        return NV_FALSE;
    }
 
    NvU64 *vector = bitvector->bits.vecPtr;
    if (bitvector->numBits <= INLINE_LIMIT) {
        vector = &bitvector->bits.inlineVec;
    }
 
    size_t highBit = (size_t)bitvector->numBits - 1;
    size_t highChunkIdx = CHUNK_INDEX(highBit);
 
    size_t i;
    for (i = 0; i <= highChunkIdx; i++) {
        if (vector[i] != ~0ULL) {
            size_t totalBitsInChunk = (i == highChunkIdx) ? BITS_IN_LAST_CHUNK(bitvector->numBits) : BITS_PER_CHUNK;
 
            if (cubitvectorSetLowestClearBitInChunk(&vector[i], totalBitsInChunk, bit)) {
                *bit = i * BITS_PER_CHUNK + *bit;
                return NV_TRUE;
            }
        }
    }
 
    return NV_FALSE;
}

static NvBool cuibitvectorFindLowestBitInRange_common(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out, NvBool findClearBit)
{
    if (!bitvector || lowBit > highBit || highBit > bitvector->numBits - 1) {
        return NV_FALSE;
    }
 
    NvU64 *vector = bitvector->bits.vecPtr;
    if (bitvector->numBits <= INLINE_LIMIT) {
        vector = &bitvector->bits.inlineVec;
    }
 
    size_t lowChunkIdx = (size_t)CHUNK_INDEX(lowBit);
    size_t highChunkIdx = (size_t)CHUNK_INDEX(highBit);

    NvU64 i;
    for (i = lowChunkIdx; i <= highChunkIdx; i++) {
        NvU64 chunk = findClearBit ? ~vector[i] : vector[i];
        NvU64 mask = ~0ULL;
        if (i == lowChunkIdx) {
            mask <<= lowBit % BITS_PER_CHUNK;
        }
        if (i == highChunkIdx) {
            mask &= ~0ULL >> (BITS_PER_CHUNK - 1 - highBit % BITS_PER_CHUNK);
        }
        if ((mask & chunk) != 0) {
            NvU64 j;
            for (j = 0; j < BITS_PER_CHUNK; j++) {
                if ((mask & chunk & CHUNK_BITMASK(j)) != 0) {
                    *bit_out = i * BITS_PER_CHUNK + j;
                    return NV_TRUE;
                }
            }
        }
    }
 
    return NV_FALSE;
}

NvBool cubitvectorFindLowestClearBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out)
{
    return cuibitvectorFindLowestBitInRange_common(bitvector, lowBit, highBit, bit_out, NV_TRUE);
}

NvBool cubitvectorFindLowestSetBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out)
{
    return cuibitvectorFindLowestBitInRange_common(bitvector, lowBit, highBit, bit_out, NV_FALSE);
}


NvBool
cubitvectorAreAllBitsSetInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit)
{
    if (!bitvector || lowBit > highBit || highBit > bitvector->numBits - 1) {
        return NV_FALSE;
    }

    NvU64 *vector = bitvector->bits.vecPtr;
    if (bitvector->numBits <= INLINE_LIMIT) {
        vector = &bitvector->bits.inlineVec;
    }

    size_t lowChunkIdx = CHUNK_INDEX(lowBit);
    size_t highChunkIdx = CHUNK_INDEX(highBit);

    size_t i;
    for (i = lowChunkIdx; i <= highChunkIdx; i++) {
        NvU64 mask = ~0ULL;
        if (i == lowChunkIdx) {
            mask <<= lowBit % BITS_PER_CHUNK;
        }
        if (i == highChunkIdx) {
            mask &= ~0ULL >> (BITS_PER_CHUNK - 1 - highBit % BITS_PER_CHUNK);
        }

        if ((mask & vector[i]) != mask) {
            return NV_FALSE;
        }
    }

    return NV_TRUE;
}

NvBool
cubitvectorAreAllBitsClearInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit)
{
    if (!bitvector || lowBit > highBit || highBit > bitvector->numBits - 1) {
        return NV_FALSE;
    }

    NvU64 *vector = bitvector->bits.vecPtr;
    if (bitvector->numBits <= INLINE_LIMIT) {
        vector = &bitvector->bits.inlineVec;
    }

    size_t lowChunkIdx = CHUNK_INDEX(lowBit);
    size_t highChunkIdx = CHUNK_INDEX(highBit);

    size_t i;
    for (i = lowChunkIdx; i <= highChunkIdx; i++) {
        NvU64 mask = ~0ULL;
        if (i == lowChunkIdx) {
            mask <<= lowBit % BITS_PER_CHUNK;
        }
        if (i == highChunkIdx) {
            mask &= ~0ULL >> (BITS_PER_CHUNK - 1 - highBit % BITS_PER_CHUNK);
        }

        if ((~mask | vector[i]) != ~mask) {
            return NV_FALSE;
        }
    }

    return NV_TRUE;
}

NvBool
cubitvectorCompare(CUbitvector *bitvector1, CUbitvector *bitvector2)
{
    if (!bitvector1 || !bitvector2) {
        return NV_FALSE;
    }

    if (bitvector1->numBits != bitvector2->numBits) {
        return NV_FALSE;
    }

    if (bitvector1->numBits <= INLINE_LIMIT) {
        return (bitvector1->bits.inlineVec == bitvector2->bits.inlineVec);
    }
    else {
        NvU64 i, numChunks = NUM_CHUNKS(bitvector1->numBits);
        for (i = 0; i < numChunks; i++) {
            if (bitvector1->bits.vecPtr[i] != bitvector2->bits.vecPtr[i]) {
                return NV_FALSE;
            }
        }
    }

    return NV_TRUE;
}

NvBool
cubitvectorAnd(CUbitvector *dstBitvector, CUbitvector *bitvectorToAnd)
{
    if (!dstBitvector || !bitvectorToAnd || (dstBitvector->numBits != bitvectorToAnd->numBits)) {
        return NV_FALSE;
    }

    if (dstBitvector->numBits <= INLINE_LIMIT) {
        dstBitvector->bits.inlineVec &= bitvectorToAnd->bits.inlineVec;
    }
    else {
        NvU64 i, numChunks = NUM_CHUNKS(dstBitvector->numBits);
        for (i = 0; i < numChunks; i++) {
            dstBitvector->bits.vecPtr[i] &= bitvectorToAnd->bits.vecPtr[i];
        }
    }

    return NV_TRUE;
}

static NvBool
cuibitvectorFindHighestBitInRange_common(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out, NvBool findClearBit)
{
    if (!bitvector || lowBit > highBit || highBit > bitvector->numBits - 1 || !bit_out) {
        return NV_FALSE;
    }

    NvU64 *vector = bitvector->bits.vecPtr;
    if (bitvector->numBits <= INLINE_LIMIT) {
        vector = &bitvector->bits.inlineVec;
    }

    NvU64 lowChunkIdx = CHUNK_INDEX(lowBit);
    NvU64 highChunkIdx = CHUNK_INDEX(highBit);

    // To not roll-over when lowChunkIdx == 0, shift i by one
    NvU64 i;
    for (i = highChunkIdx + 1; i >= lowChunkIdx + 1; i--) {
        NvU64 idx = i - 1;
        NvU64 chunk = findClearBit ? ~vector[idx] : vector[idx];
        NvU64 mask = ~0ULL;
        if (idx == lowChunkIdx) {
            mask <<= lowBit % BITS_PER_CHUNK;
        }
        if (idx == highChunkIdx) {
            mask &= ~0ULL >> (BITS_PER_CHUNK - 1 - highBit % BITS_PER_CHUNK);
        }
        if ((mask & chunk) != 0) {
            NvU64 j;
            for (j = BITS_PER_CHUNK; j > 0; j--) {
                NvU64 bit = j - 1;
                if ((mask & chunk & CHUNK_BITMASK(bit)) != 0) {
                    *bit_out = idx * BITS_PER_CHUNK + bit;
                    return NV_TRUE;
                }
            }
        }
    }

    return NV_FALSE;
}

NvBool
cuibitvectorFindHighestSetBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out)
{
    return cuibitvectorFindHighestBitInRange_common(bitvector, lowBit, highBit, bit_out, NV_FALSE);
}

NvBool
cuibitvectorFindHighestClearBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out)
{
    return cuibitvectorFindHighestBitInRange_common(bitvector, lowBit, highBit, bit_out, NV_TRUE);
}
