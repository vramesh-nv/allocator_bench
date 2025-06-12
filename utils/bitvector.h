#ifndef _BITVECTOR_H_
#define _BITVECTOR_H_

#include "utils_types.h"

struct CUbitvector_st;

typedef struct CUbitvector_st CUbitvector;


#ifdef __cplusplus
extern "C" {
#endif

// Allocates a bitvector large enough to hold the specified number of bits
NvBool cubitvectorCreate(CUbitvector **bitvector, NvU64 numBits);

// Frees the given bitvector
void cubitvectorDestroy(CUbitvector *bitvector);

// Allocates a copy of the given bitvector
NvBool cubitvectorCreateCopy(CUbitvector **copy, CUbitvector *original);

// Return numbits tracked by this bitvector
NvU64 cubitvectorGetSize(CUbitvector *bitvector);

// Resizes an existing bitvector to a larger size.
NvBool cubitvectorGrow(CUbitvector *bitvector, NvU64 newNumBits);

// Sets the specified bit in the given bitvector
NvBool cubitvectorSetBit(CUbitvector *bitvector, NvU64 bit);

// Sets all bits in the given bitvector
NvBool cubitvectorSetAllBits(CUbitvector *bitvector);

// Set the bits in the specified range.
// Range is inclusive, so [0, 0] has a size of 1.
void cubitvectorSetBitsInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit);

// Clear the bits in the specified range.
// Range is inclusive, so [0, 0] has a size of 1.
void cubitvectorClearBitsInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit);

// Clears the specified bit in the given bitvector
NvBool cubitvectorClearBit(CUbitvector *bitvector, size_t bit);
    
// Returns true if the specified bit is set in the given bitvector
NvBool cubitvectorIsBitSet(CUbitvector *bitvector, size_t bit);

// Returns true if any bit is set in the given bitvector
NvBool cubitvectorIsAnyBitSet(CUbitvector *bitvector);

// Returns true if all the bits in the specified range are set.
// Range is inclusive, so [0, 0] has a size of 1.
NvBool cubitvectorAreAllBitsSetInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit);

// Returns true if all the bits in the specified range are clear.
// Range is inclusive, so [0, 0] has a size of 1.
NvBool cubitvectorAreAllBitsClearInRange(CUbitvector *bitvector, size_t lowBit, size_t highBit);

// Returns true after setting the first clear bit. The bit which was set is returned in 'bit'.
// If there is no clear bit, returns false.
NvBool cubitvectorSetLowestClearBit(CUbitvector *bitvector, NvU64 *bit);

// Returns true after finding the first clear bit in [lowIdx, highIdx]. The bit which was set is returned in 'bit'.
// If there is no clear bit, returns false.
NvBool cubitvectorFindLowestClearBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out);

// Returns true after finding the first set bit in [lowIdx, highIdx]. The bit which was set is returned in 'bit'.
// If there is no set bit, returns false.
NvBool cubitvectorFindLowestSetBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out);

// Returns true if the two bitvectors are equal
NvBool cubitvectorCompare(CUbitvector *bitvector1, CUbitvector *bitvector2);

// Ands dstBitvector and bitvectorToAnd and stores the result in dstBitvector
// The two bitvectors must be the same size
NvBool cubitvectorAnd(CUbitvector *dstBitvector, CUbitvector *bitvectorToAnd);

// Returns true if there exists a set bit in the specified range and sets 'bit_out' to its location
// Range is inclusive, so [0, 0] has a size of 1.
NvBool cuibitvectorFindHighestSetBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out);

// Returns true if there exists a clear bit in the specified range and sets 'bit_out' to its location
// Range is inclusive, so [0, 0] has a size of 1.
NvBool cuibitvectorFindHighestClearBitInRange(CUbitvector *bitvector, NvU64 lowBit, NvU64 highBit, NvU64 *bit_out);

#ifdef __cplusplus
}
#endif

#endif
