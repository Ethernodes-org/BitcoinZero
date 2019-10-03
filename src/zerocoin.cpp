#include "main.h"
#include "zerocoin.h"
#include "sigma.h"
#include "timedata.h"
#include "chainparams.h"
#include "util.h"
#include "base58.h"
#include "definition.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "bznode-payments.h"
#include "bznode-sync.h"

#include <atomic>
#include <sstream>
#include <chrono>

#include <boost/foreach.hpp>

using namespace std;

// Settings
int64_t nTransactionFee = 0;
int64_t nMinimumInputValue = DUST_HARD_LIMIT;

// btzc: add zerocoin init
// zerocoin init
static CBigNum bnTrustedModulus(ZEROCOIN_MODULUS), bnTrustedModulusV2(ZEROCOIN_MODULUS_V2);

// Set up the Zerocoin Params object
uint32_t securityLevel = 80;
libzerocoin::Params *ZCParams = new libzerocoin::Params(bnTrustedModulus, bnTrustedModulus);
libzerocoin::Params *ZCParamsV2 = new libzerocoin::Params(bnTrustedModulusV2, bnTrustedModulus);

static CZerocoinState zerocoinState;

static bool CheckZerocoinSpendSerial(CValidationState &state, const Consensus::Params &params, CZerocoinTxInfo *zerocoinTxInfo, libzerocoin::CoinDenomination denomination, const CBigNum &serial, int nHeight, bool fConnectTip) {
    if (nHeight > params.nCheckBugFixedAtBlock) {
        // check for zerocoin transaction in this block as well
        if (zerocoinTxInfo && !zerocoinTxInfo->fInfoIsComplete && zerocoinTxInfo->spentSerials.count(serial) > 0)
            return state.DoS(0, error("CTransaction::CheckTransaction() : two or more spends with same serial in the same block"));

        // check for used serials in zerocoinState
        if (zerocoinState.IsUsedCoinSerial(serial)) {
            // Proceed with checks ONLY if we're accepting tx into the memory pool or connecting block to the existing blockchain
            if (nHeight == INT_MAX || fConnectTip) {
                if (false)
                    LogPrintf("ZCSpend: height=%d, denomination=%d, serial=%s\n", nHeight, (int)denomination, serial.ToString());
                else
                    return state.DoS(0, error("CTransaction::CheckTransaction() : The CoinSpend serial has been used"));
            }
        }
    }

    return true;
}

CBigNum ParseZerocoinMintScript(const CScript& script)
{
    if (script.size() < 6) {
        throw std::invalid_argument("Script is not a valid Zerocoin mint");
    }

    return CBigNum(std::vector<unsigned char>(script.begin() + 6, script.end()));
}

std::pair<std::unique_ptr<libzerocoin::CoinSpend>, uint32_t> ParseZerocoinSpend(const CTxIn& in)
{
    // Check arguments.
    uint32_t groupId = in.nSequence;

    if (groupId < 1 || groupId >= INT_MAX) {
        throw CBadSequence();
    }

    if (in.scriptSig.size() < 4) {
        throw CBadTxIn();
    }

    // Determine if version 2 spend.
    bool v2 = groupId >= ZC_MODULUS_V2_BASE_ID;

    // Deserialize spend.
    CDataStream serialized(
        std::vector<unsigned char>(in.scriptSig.begin() + 4, in.scriptSig.end()),
        SER_NETWORK,
        PROTOCOL_VERSION
    );

    std::unique_ptr<libzerocoin::CoinSpend> spend(new libzerocoin::CoinSpend(v2 ? ZCParamsV2 : ZCParams, serialized));

    return std::make_pair(std::move(spend), groupId);
}

bool CheckZerocoinFoundersInputs(const CTransaction &tx, CValidationState &state, const Consensus::Params &params, int nHeight)

    {
            if (sporkManager.IsSporkActive(SPORK_13_F_PAYMENT_ENFORCEMENT) && bznodeSync.IsSynced())
            {
                bool found_1 = false;
                bool found_2 = false;
                bool found_3 = false;
                bool found_4 = false;
                int total_payment_tx = 0; // no more than 1 output for payment
                CScript FOUNDER_1_SCRIPT;
                CScript FOUNDER_2_SCRIPT;
                CScript FOUNDER_3_SCRIPT;
                CScript FOUNDER_4_SCRIPT;
                FOUNDER_1_SCRIPT = GetScriptForDestination(CBitcoinAddress("XCr6GWHTYtx5xckqGJXhmW7XgVQ7JrPXJv").Get());
                FOUNDER_2_SCRIPT = GetScriptForDestination(CBitcoinAddress("XXeBq9aGEjin8er6QCKyMevp6PNFWgiSby").Get());
                FOUNDER_3_SCRIPT = GetScriptForDestination(CBitcoinAddress("XTh9P3ji4N1ReMLD2bUUUY9QG8qA99afjc").Get());
                FOUNDER_4_SCRIPT = GetScriptForDestination(CBitcoinAddress("XZVbVRp43HBgVEVTSPfyPD9vkFSQpfYSaV").Get());
                CAmount bznodePayment = GetBznodePayment(nHeight);
                BOOST_FOREACH(const CTxOut &output, tx.vout)
                {
                    if (output.scriptPubKey == FOUNDER_1_SCRIPT && output.nValue == (int64_t)(8 * COIN))
                    {
                        found_1 = true;
                        continue;
                    }

                    if (output.scriptPubKey == FOUNDER_2_SCRIPT && output.nValue == (int64_t)(8 * COIN))
                    {
                        found_2 = true;
                        continue;
                    }

                    if (output.scriptPubKey == FOUNDER_3_SCRIPT && output.nValue == (int64_t)(5 * COIN))
                    {
                        found_3 = true;
                        continue;
                    }

                    if (output.scriptPubKey == FOUNDER_4_SCRIPT && output.nValue == (int64_t)(2 * COIN))
                    {
                        found_4 = true;
                        continue;
                    }

                        if (bznodePayment == output.nValue)
                        {
                            total_payment_tx = total_payment_tx + 1;
                        }
                    }


                if (!(found_1 && found_2 && found_3 && found_4))
                {
                    return state.DoS(100, false, REJECT_FOUNDER_REWARD_MISSING,
                                     "CTransaction::CheckTransaction() : founders reward missing");
                }

                if (total_payment_tx > 2)
                {
                    return state.DoS(100, false, REJECT_INVALID_BZNODE_PAYMENT,
                                     "CTransaction::CheckTransaction() : invalid bznode payment");
                }
            }

        return true;
    }

void DisconnectTipZC(CBlock & /*block*/, CBlockIndex *pindexDelete) {
    zerocoinState.RemoveBlock(pindexDelete);
}

CBigNum ZerocoinGetSpendSerialNumber(const CTransaction &tx, const CTxIn &txin) {
    if (!txin.IsZerocoinSpend())
        return CBigNum(0);
    try {
        CDataStream serializedCoinSpend((const char *)&*(txin.scriptSig.begin() + 4),
                                    (const char *)&*txin.scriptSig.end(),
                                    SER_NETWORK, PROTOCOL_VERSION);
        libzerocoin::CoinSpend spend(txin.nSequence >= ZC_MODULUS_V2_BASE_ID ? ZCParamsV2 : ZCParams, serializedCoinSpend);
        return spend.getCoinSerialNumber();
    }
    catch (const std::runtime_error &) {
        return CBigNum(0);
    }
}

int ZerocoinGetNHeight(const CBlockHeader &block) {
    CBlockIndex *pindexPrev = NULL;
    int nHeight = 0;
    BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
    if (mi != mapBlockIndex.end()) {
        pindexPrev = (*mi).second;
        nHeight = pindexPrev->nHeight + 1;
    }
    return nHeight;
}

void CZerocoinTxInfo::Complete() {
    // We need to sort mints lexicographically by serialized value of pubCoin. That's the way old code
    // works, we need to stick to it. Denomination doesn't matter but we will sort by it as well
    sort(mints.begin(), mints.end(),
         [](decltype(mints)::const_reference m1, decltype(mints)::const_reference m2)->bool {
            CDataStream ds1(SER_DISK, CLIENT_VERSION), ds2(SER_DISK, CLIENT_VERSION);
            ds1 << m1.second;
            ds2 << m2.second;
            return (m1.first < m2.first) || ((m1.first == m2.first) && (ds1.str() < ds2.str()));
         });

    // Mark this info as complete
    fInfoIsComplete = true;
}

// CZerocoinState::CBigNumHash

std::size_t CZerocoinState::CBigNumHash::operator ()(const CBigNum &bn) const noexcept {
    // we are operating on almost random big numbers and least significant bytes (save for few last bytes) give us a good hash
    vector<unsigned char> bnData = bn.ToBytes();
    if (bnData.size() < sizeof(size_t)*3)
        // rare case, put ones like that into one hash bin
        return 0;
    else
        return ((size_t*)bnData.data())[1];
}

// CZerocoinState

CZerocoinState::CZerocoinState() {
}

void CZerocoinState::AddSpend(const CBigNum &serial) {
    usedCoinSerials.insert(serial);
}

void CZerocoinState::AddBlock(CBlockIndex *index, const Consensus::Params &params) {
    BOOST_FOREACH(const PAIRTYPE(PAIRTYPE(int,int), PAIRTYPE(CBigNum,int)) &accUpdate, index->accumulatorChanges)
    {
        CoinGroupInfo   &coinGroup = coinGroups[accUpdate.first];

        if (coinGroup.firstBlock == NULL)
            coinGroup.firstBlock = index;
        coinGroup.lastBlock = index;
        coinGroup.nCoins += accUpdate.second.second;
    }

    BOOST_FOREACH(const PAIRTYPE(PAIRTYPE(int,int),vector<CBigNum>) &pubCoins, index->mintedPubCoins) {
        latestCoinIds[pubCoins.first.first] = pubCoins.first.second;
        BOOST_FOREACH(const CBigNum &coin, pubCoins.second) {
            CMintedCoinInfo coinInfo;
            coinInfo.denomination = pubCoins.first.first;
            coinInfo.id = pubCoins.first.second;
            coinInfo.nHeight = index->nHeight;
            mintedPubCoins.insert(pair<CBigNum,CMintedCoinInfo>(coin, coinInfo));
        }
    }

    if (index->nHeight > params.nCheckBugFixedAtBlock) {
        BOOST_FOREACH(const CBigNum &serial, index->spentSerials) {
            usedCoinSerials.insert(serial);
        }
    }
}

void CZerocoinState::RemoveBlock(CBlockIndex *index) {
    // roll back accumulator updates
    BOOST_FOREACH(const PAIRTYPE(PAIRTYPE(int,int), PAIRTYPE(CBigNum,int)) &accUpdate, index->accumulatorChanges)
    {
        CoinGroupInfo   &coinGroup = coinGroups[accUpdate.first];
        int  nMintsToForget = accUpdate.second.second;

        assert(coinGroup.nCoins >= nMintsToForget);

        if ((coinGroup.nCoins -= nMintsToForget) == 0) {
            // all the coins of this group have been erased, remove the group altogether
            coinGroups.erase(accUpdate.first);
            // decrease pubcoin id for this denomination
            latestCoinIds[accUpdate.first.first]--;
        }
        else {
            // roll back lastBlock to previous position
            do {
                assert(coinGroup.lastBlock != coinGroup.firstBlock);
                coinGroup.lastBlock = coinGroup.lastBlock->pprev;
            } while (coinGroup.lastBlock->accumulatorChanges.count(accUpdate.first) == 0);
        }
    }

    // roll back mints
    BOOST_FOREACH(const PAIRTYPE(PAIRTYPE(int,int),vector<CBigNum>) &pubCoins, index->mintedPubCoins) {
        BOOST_FOREACH(const CBigNum &coin, pubCoins.second) {
            auto coins = mintedPubCoins.equal_range(coin);
            auto coinIt = find_if(coins.first, coins.second, [=](const decltype(mintedPubCoins)::value_type &v) {
                return v.second.denomination == pubCoins.first.first &&
                        v.second.id == pubCoins.first.second;
            });
            assert(coinIt != coins.second);
            mintedPubCoins.erase(coinIt);
        }
    }

    // roll back spends
    BOOST_FOREACH(const CBigNum &serial, index->spentSerials) {
        usedCoinSerials.erase(serial);
    }
}

bool CZerocoinState::GetCoinGroupInfo(int denomination, int id, CoinGroupInfo &result) {
    pair<int,int>   key = make_pair(denomination, id);
    if (coinGroups.count(key) == 0)
        return false;

    result = coinGroups[key];
    return true;
}

bool CZerocoinState::IsUsedCoinSerial(const CBigNum &coinSerial) {
    return usedCoinSerials.count(coinSerial) != 0;
}

bool CZerocoinState::HasCoin(const CBigNum &pubCoin) {
    return mintedPubCoins.count(pubCoin) != 0;
}

int CZerocoinState::GetMintedCoinHeightAndId(const CBigNum &pubCoin, int denomination, int &id) {
    auto coins = mintedPubCoins.equal_range(pubCoin);
    auto coinIt = find_if(coins.first, coins.second,
                          [=](const decltype(mintedPubCoins)::value_type &v) { return v.second.denomination == denomination; });

    if (coinIt != coins.second) {
        id = coinIt->second.id;
        return coinIt->second.nHeight;
    }
    else
        return -1;
}

bool CZerocoinState::AddSpendToMempool(const vector<CBigNum> &coinSerials, uint256 txHash) {
    BOOST_FOREACH(CBigNum coinSerial, coinSerials){
        if (IsUsedCoinSerial(coinSerial) || mempoolCoinSerials.count(coinSerial))
            return false;

        mempoolCoinSerials[coinSerial] = txHash;
    }

    return true;
}

bool CZerocoinState::AddSpendToMempool(const CBigNum &coinSerial, uint256 txHash) {
    if (IsUsedCoinSerial(coinSerial) || mempoolCoinSerials.count(coinSerial))
        return false;

    mempoolCoinSerials[coinSerial] = txHash;
    return true;
}

void CZerocoinState::RemoveSpendFromMempool(const CBigNum &coinSerial) {
    mempoolCoinSerials.erase(coinSerial);
}

uint256 CZerocoinState::GetMempoolConflictingTxHash(const CBigNum &coinSerial) {
    if (mempoolCoinSerials.count(coinSerial) == 0)
        return uint256();

    return mempoolCoinSerials[coinSerial];
}

bool CZerocoinState::CanAddSpendToMempool(const CBigNum &coinSerial) {
    return !IsUsedCoinSerial(coinSerial) && mempoolCoinSerials.count(coinSerial) == 0;
}

void CZerocoinState::Reset() {
    coinGroups.clear();
    usedCoinSerials.clear();
    mintedPubCoins.clear();
    latestCoinIds.clear();
    mempoolCoinSerials.clear();
}

CZerocoinState *CZerocoinState::GetZerocoinState() {
    return &zerocoinState;
}
