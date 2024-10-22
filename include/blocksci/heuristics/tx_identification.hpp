//
//  tx_identification.hpp
//  blocksci
//
//  Created by Harry Kalodner on 12/1/17.
//

#ifndef tx_identification_hpp
#define tx_identification_hpp

#include <blocksci/blocksci_export.h>
#include <blocksci/chain/chain_fwd.hpp>
#include <blocksci/scripts/scripts_fwd.hpp>
#include <optional>
#include <string>

namespace blocksci {
    class DataAccess;
    namespace heuristics {
    
    enum class BLOCKSCI_EXPORT CoinJoinResult {
        True, False, Timeout
    };

    enum class BLOCKSCI_EXPORT HWWalletRemixResult {
        Trezor, SW, False
    };

    enum class BLOCKSCI_EXPORT ConsolidationType {
        Certain, Possible, BigRoundOutput, None
    };

    bool BLOCKSCI_EXPORT isPeelingChain(const Transaction &tx);
    bool BLOCKSCI_EXPORT isCoinjoin(const Transaction &tx);
    CoinJoinResult BLOCKSCI_EXPORT isPossibleCoinjoin(const Transaction &tx, int64_t minBaseFee, double percentageFee, size_t maxDepth);
    CoinJoinResult BLOCKSCI_EXPORT isCoinjoinExtra(const Transaction &tx, int64_t minBaseFee, double percentageFee, size_t maxDepth);
    bool BLOCKSCI_EXPORT isDeanonTx(const Transaction &tx);
    bool BLOCKSCI_EXPORT containsKeysetChange(const Transaction &tx);
    bool BLOCKSCI_EXPORT isChangeOverTx(const Transaction &tx);
    bool BLOCKSCI_EXPORT isWasabi2CoinJoin(const Transaction &tx, std::optional<uint64_t> minInputCount = std::nullopt);
    bool BLOCKSCI_EXPORT isWasabi1CoinJoin(const Transaction &tx);
    bool BLOCKSCI_EXPORT isWhirlpoolCoinJoin(const Transaction &tx);

    bool BLOCKSCI_EXPORT isCoinjoinOfGivenType(const Transaction &tx, const std::string &type);

    HWWalletRemixResult BLOCKSCI_EXPORT isLongDormantInRemixes(const Transaction &tx);

    // consolidation

    ConsolidationType BLOCKSCI_EXPORT getConsolidationType(const Transaction &tx, double inputOutputRatio = 2);
}} // namespace blocksci

#endif /* tx_identification_hpp */
