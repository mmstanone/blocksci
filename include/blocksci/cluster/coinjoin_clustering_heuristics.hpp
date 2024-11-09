#ifndef coinjoin_clustering_heuristics_hpp
#define coinjoin_clustering_heuristics_hpp

#include <blocksci/blocksci_export.h>

#include <blocksci/chain/transaction.hpp>
#include <blocksci/cluster/cluster.hpp>
#include <blocksci/cluster/coinjoin_cluster_manager.hpp>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blocksci {
    namespace coinjoin_heuristics {
        struct BLOCKSCI_EXPORT ClusteringHeuristicsType {
            enum Enum {
                OneOutputConsolidation,
                None,
            };
        };

        template <ClusteringHeuristicsType::Enum heuristic>
        struct BLOCKSCI_EXPORT ClusteringHeuristicImpl {
            void operator()(const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions,
                            AddressDisjointSets& ds, std::unordered_map<Address, uint32_t>& collectedAddresses) const;
        };

        struct BLOCKSCI_EXPORT ClusteringHeuristic {
            using HeuristicFunc =
                std::function<void(const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions,
                                   AddressDisjointSets& ds, std::unordered_map<Address, uint32_t>& collectedAddresses)>;

            HeuristicFunc impl;

            ClusteringHeuristic(HeuristicFunc func) : impl(std::move(func)) {}

            void operator()(const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions,
                            AddressDisjointSets& ds, std::unordered_map<Address, uint32_t>& collectedAddresses) const {
                impl(tx, coinjoinTransactions, ds, collectedAddresses);
            }
        };

        using OneOutputConsolidation = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation>;
        using NoClustering = ClusteringHeuristicImpl<ClusteringHeuristicsType::None>;

        ClusteringHeuristic BLOCKSCI_EXPORT getClusteringHeuristic(const std::string& heuristicName);

    }  // namespace coinjoin_heuristics
}  // namespace blocksci

#endif  // coinjoin_clustering_heuristics_hpp