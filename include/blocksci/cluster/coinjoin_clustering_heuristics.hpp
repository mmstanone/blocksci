#ifndef coinjoin_clustering_heuristics_hpp
#define coinjoin_clustering_heuristics_hpp

#include <blocksci/blocksci_export.h>

#include <blocksci/chain/transaction.hpp>
#include <blocksci/cluster/cluster.hpp>
#include <blocksci/cluster/coinjoin_cluster_manager.hpp>
#include <functional>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace blocksci {
    namespace coinjoin_heuristics {
        struct BLOCKSCI_EXPORT ClusteringHeuristicsType {
            enum Enum {
                OneOutputConsolidation,
                OneOutputConsolidation1Hop,
                OneOutputConsolidation2Hops,
                OneOutputConsolidation3Hops,
                OneOutputConsolidationWithChange,
                OneOutputConsolidationWithChange1Hop,
                OneOutputConsolidationWithChange2Hops,
                OneOutputConsolidationWithChange3Hops,
                OneInputConsolidation,
                OutputThresholdConsolidation,
                OutputThresholdConsolidation1Hop,
                OutputThresholdConsolidation2Hops,
                OutputThresholdConsolidation3Hops,
                FakeOutputConsolidation,
                FakeOutputConsolidation1Hop,
                FakeOutputConsolidation2Hops,
                FakeOutputConsolidation3Hops,
                None,
            };
        };

        template <ClusteringHeuristicsType::Enum heuristic>
        struct BLOCKSCI_EXPORT ClusteringHeuristicImpl {
            void operator()(const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions,
                            AddressDisjointSets& ds,
                            const std::unordered_map<Address, uint32_t>& collectedAddresses);
        };

        struct BLOCKSCI_EXPORT ClusteringHeuristic {
            using HeuristicFunc =
                std::function<void(const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions,
                                   AddressDisjointSets& ds, std::unordered_map<Address, uint32_t>& collectedAddresses)>;

            HeuristicFunc impl;

            ClusteringHeuristic(HeuristicFunc func) : impl(std::move(func)) {}

            void operator()(const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions,
                            AddressDisjointSets& ds, std::unordered_map<Address, uint32_t>& collectedAddresses) {
                impl(tx, coinjoinTransactions, ds, collectedAddresses);
            }

            ClusteringHeuristic operator&(ClusteringHeuristic other) const {
                auto first_impl = impl;
                auto second_impl = other.impl;

                return ClusteringHeuristic(
                    [first_impl, second_impl](
                        const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions,
                        AddressDisjointSets& ds, std::unordered_map<Address, uint32_t>& collectedAddresses) {
                        first_impl(tx, coinjoinTransactions, ds, collectedAddresses);
                        second_impl(tx, coinjoinTransactions, ds, collectedAddresses);
                    });
            }
        };

        // Many inputs into one output
        using OneOutputConsolidation = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation>;
        using OneOutputConsolidation1Hop = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation1Hop>;
        using OneOutputConsolidation2Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation2Hops>;
        using OneOutputConsolidation3Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation3Hops>;

        // Many inputs into two outputs, merge with smaller (change ?)
        using OneOutputConsolidationWithChange = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange>;
        using OneOutputConsolidationWithChange1Hop = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange1Hop>;
        using OneOutputConsolidationWithChange2Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange2Hops>;
        using OneOutputConsolidationWithChange3Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange3Hops>;

        using OneInputConsolidation = ClusteringHeuristicImpl<ClusteringHeuristicsType::OneInputConsolidation>;

        // Many inputs, less outputs, take only inputs
        using OutputThresholdConsolidation = ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation>;
        using OutputThresholdConsolidation1Hop = ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation1Hop>;
        using OutputThresholdConsolidation2Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation2Hops>;
        using OutputThresholdConsolidation3Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation3Hops>;

        // Fake tx heuristic (many inputs, 2 "same" outputs)
        using FakeOutputConsolidation = ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation>;
        using FakeOutputConsolidation1Hop = ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation1Hop>;
        using FakeOutputConsolidation2Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation2Hops>;
        using FakeOutputConsolidation3Hops = ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation3Hops>;

        using NoClustering = ClusteringHeuristicImpl<ClusteringHeuristicsType::None>;

    }  // namespace coinjoin_heuristics
}  // namespace blocksci

#endif  // coinjoin_clustering_heuristics_hpp