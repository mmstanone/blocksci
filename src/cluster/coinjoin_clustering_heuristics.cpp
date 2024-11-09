#include <blocksci/cluster/coinjoin_clustering_heuristics.hpp>

namespace blocksci {
    namespace coinjoin_heuristics {
        static void one_output_consolidation(const Transaction& tx,
                                             const std::unordered_set<Transaction>& coinjoinTransactions,
                                             AddressDisjointSets& ds,
                                             std::unordered_map<Address, uint32_t>& collectedAddresses) {
            // <-- go this way
            for (const auto& input : tx.inputs()) {
                auto input_tx = input.getSpentTx();
                if (coinjoinTransactions.count(input_tx)) {
                    continue;
                }
                if (input_tx.outputCount() != 1) {
                    continue;
                }

                auto inputAddress = input_tx.outputs()[0].getAddress();
                if (collectedAddresses.find(inputAddress) == collectedAddresses.end()) {
                    continue;
                }

                for (const auto& next_level_input : input_tx.inputs()) {
                    auto next_level_input_tx = next_level_input.getSpentTx();

                    auto next_level_inputAddress = next_level_input.getAddress();

                    if (coinjoinTransactions.count(next_level_input_tx)) {
                        continue;
                    }

                    ds.link_addresses(inputAddress, next_level_inputAddress);
                }
            }

            // --> go this way
            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                auto nextTx = output.getSpendingTx().value();

                if (nextTx.outputCount() != 1) {
                    continue;
                }

                auto nextTxOutputAddress = nextTx.outputs()[0].getAddress();

                if (coinjoinTransactions.count(nextTx)) {
                    continue;
                }

                if (collectedAddresses.find(nextTxOutputAddress) == collectedAddresses.end()) {
                    continue;
                }

                for (const auto& nextLevelInput : nextTx.inputs()) {
                    auto nextLevelInputAddress = nextLevelInput.getAddress();
                    if (collectedAddresses.find(nextTxOutputAddress) == collectedAddresses.end()) {
                        continue;
                    }

                    ds.link_addresses(nextTxOutputAddress, nextLevelInputAddress);
                }
            }
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            std::unordered_map<Address, uint32_t>& collectedAddresses) const {
            one_output_consolidation(tx, coinjoinTransactions, ds, collectedAddresses);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::None>::operator()(
            const Transaction&, const std::unordered_set<Transaction>&, AddressDisjointSets&,
            std::unordered_map<Address, uint32_t>&) const {
            // Do nothing
        }

        ClusteringHeuristic getClusteringHeuristic(const std::string& heuristicName) {
            if (heuristicName == "OneOutputConsolidation") {
                return ClusteringHeuristic(OneOutputConsolidation{});
            } else if (heuristicName == "None") {
                return ClusteringHeuristic(NoClustering{});
            } else {
                throw std::invalid_argument("Invalid heuristic name");
            }
        }
    }  // namespace coinjoin_heuristics
}  // namespace blocksci