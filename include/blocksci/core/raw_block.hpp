//
//  raw_block.hpp
//  blocksci
//
//  Created by Harry Kalodner on 11/18/17.
//

#ifndef raw_block_hpp
#define raw_block_hpp

#include "bitcoin_uint256.hpp"

#include <blocksci/blocksci_export.h>

namespace blocksci {
    struct BLOCKSCI_EXPORT RawBlock {
        uint256 hash;
        uint64_t coinbaseOffset;
        uint32_t firstTxIndex;
        uint32_t txCount;
        uint32_t inputCount;
        uint32_t outputCount;
        uint32_t height;
        int32_t version;
        uint32_t timestamp;
        uint32_t bits;
        uint32_t nonce;
        uint32_t realSize;
        uint32_t baseSize;
        
        RawBlock(uint32_t firstTxIndex_, uint32_t txCount_, uint32_t inputCount_, uint32_t outputCount_, uint32_t height_, uint256 hash_, int32_t version_, uint32_t timestamp_, uint32_t bits_, uint32_t nonce_, uint32_t realSize_, uint32_t baseSize_, uint64_t coinbaseOffset_) : hash(hash_), coinbaseOffset(coinbaseOffset_), firstTxIndex(firstTxIndex_), txCount(txCount_), inputCount(inputCount_), outputCount(outputCount_), height(height_), version(version_), timestamp(timestamp_), bits(bits_), nonce(nonce_), realSize(realSize_), baseSize(baseSize_) {}
        
        bool operator==(const RawBlock& other) const {
            return firstTxIndex == other.firstTxIndex
            && txCount == other.txCount
            && inputCount == other.inputCount
            && outputCount == other.outputCount
            && height == other.height
            && hash == other.hash
            && version == other.version
            && timestamp == other.timestamp
            && bits == other.bits
            && nonce == other.nonce
            && coinbaseOffset == other.coinbaseOffset;
        }
    };
} // namespace blocksci

#endif /* raw_block_hpp */