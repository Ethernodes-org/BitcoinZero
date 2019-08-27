// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"
#include "consensus/consensus.h"
#include "main.h"
#include "zerocoin.h"
#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "crypto/common.h"
#include "chainparams.h"
#include "crypto/scrypt.h"
#include "crypto/Lyra2Z/Lyra2Z.h"
#include "crypto/Lyra2Z/Lyra2.h"
#include "util.h"
#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <string>
#include "precomputed_hash.h"


uint256 CBlockHeader::GetHash() const {
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash(int nHeight, bool forceCalc) const {
//    int64_t start = std::chrono::duration_cast<std::chrono::milliseconds>(
//            std::chrono::system_clock::now().time_since_epoch()).count();
    bool fTestNet = (Params().NetworkIDString() == CBaseChainParams::TESTNET);
    if (!fTestNet) {
        if (nHeight < 0) {
            if (!mapPoWHash.count(1)) {
//            std::cout << "Start Build Map" << std::endl;
                buildMapPoWHash();
            }
        }
        if (!forceCalc && mapPoWHash.count(nHeight)) {
//        std::cout << "GetPowHash nHeight=" << nHeight << ", hash= " << mapPoWHash[nHeight].ToString() << std::endl;
            return mapPoWHash[nHeight];
        }
    }
    uint256 powHash;

    try
    {
        LYRA2(BEGIN(powHash), 32, BEGIN(nVersion), 80, BEGIN(nVersion), 80, 2, 330, 256);
    }
    catch (std::exception &e) {
        LogPrintf("excepetion: %s", e.what());
    }
//    int64_t end = std::chrono::duration_cast<std::chrono::milliseconds>(
//            std::chrono::system_clock::now().time_since_epoch()).count();
//    std::cout << "GetPowHash nHeight=" << nHeight << ", hash= " << powHash.ToString() << " done in= " << (end - start) << " miliseconds" << std::endl;
    mapPoWHash.insert(make_pair(nHeight, powHash));
//    SetPoWHash(thash);
    return powHash;
}

void CBlockHeader::InvalidateCachedPoWHash(int nHeight) const {
    if (nHeight >= 0 && mapPoWHash.count(nHeight) > 0)
        mapPoWHash.erase(nHeight);
}

std::string CBlock::ToString() const {
    std::stringstream s;
    s << strprintf(
            "CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
            GetHash().ToString(),
            nVersion,
            hashPrevBlock.ToString(),
            hashMerkleRoot.ToString(),
            nTime, nBits, nNonce,
            vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++) {
        s << "  " << vtx[i].ToString() << "\n";
    }
    return s.str();
}
int64_t GetBlockWeight(const CBlock& block)
{
//     This implements the weight = (stripped_size * 4) + witness_size formula,
//     using only serialization with and without witness data. As witness_size
//     is equal to total_size - stripped_size, this formula is identical to:
//     weight = (stripped_size * 3) + total_size.
//    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}

void CBlock::ZerocoinClean() const {
    zerocoinTxInfo = nullptr;
}
