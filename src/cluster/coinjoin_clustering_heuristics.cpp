#include <blocksci/cluster/coinjoin_clustering_heuristics.hpp>
#include <blocksci/heuristics/tx_identification.hpp>
#include <unordered_set>

namespace blocksci {
    namespace coinjoin_heuristics {
        static void one_input_consolidation(const Transaction& tx,
                                            const std::unordered_set<Transaction>& coinjoinTransactions,
                                            AddressDisjointSets& ds,
                                            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
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

                    if (collectedAddresses.find(next_level_inputAddress) == collectedAddresses.end()) {
                        continue;
                    }

                    if (coinjoinTransactions.count(next_level_input_tx)) {
                        continue;
                    }

                    ds.link_addresses(inputAddress, next_level_inputAddress);
                }
            }
        }

        static void one_output_consolidation(const Transaction& tx,
                                             const std::unordered_set<Transaction>& coinjoinTransactions,
                                             AddressDisjointSets& ds,
                                             const std::unordered_map<Address, uint32_t>& collectedAddresses) {
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

        /**
         * Consolidate addresses of transactions which have exactly 2 outputs - one main and one change.
         * Link all of the input addresses with the change address.
         */
        static void one_output_consolidation_with_change(const Transaction& tx,
                                             const std::unordered_set<Transaction>& coinjoinTransactions,
                                             AddressDisjointSets& ds,
                                             const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            // --> go this way
            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                auto nextTx = output.getSpendingTx().value();

                if (nextTx.outputCount() != 2) {
                    continue;
                }

                auto nextTxOutputAddress = nextTx.outputs()[0].getValue() < nextTx.outputs()[1].getValue() ? nextTx.outputs()[0].getAddress() : nextTx.outputs()[1].getAddress();

                if (coinjoinTransactions.count(nextTx)) {
                    continue;
                }

                if (collectedAddresses.find(nextTxOutputAddress) == collectedAddresses.end()) {
                    continue;
                }

                for (const auto& nextLevelInput : nextTx.inputs()) {
                    auto nextLevelInputAddress = nextLevelInput.getAddress();
                    if (collectedAddresses.find(nextLevelInputAddress) == collectedAddresses.end()) {
                        continue;
                    }

                    ds.link_addresses(nextTxOutputAddress, nextLevelInputAddress);
                }
            }
        }

        /**
         * Consolidate addresses based on the output threshold heuristic.
         * This is different than other heuristics, because the emphasis is on the ratio of inputs to outputs.
         * Since there can be more outputs and we are not sure which one belongs to the same entity, we link only the inputs.
         * 
         * @param tx The transaction to process
         * @param coinjoinTransactions The set of coinjoin transactions
         * @param ds The disjoint set to update
         * @param collectedAddresses The set of addresses to consider
         * @param hops The number of hops to consider
         * @param visitedTransactions The set of visited transactions (for recursion)
         * 
         * @return void
         */
        void one_hop_output_threshold_consolidation(const Transaction& tx,
                                                    const std::unordered_set<Transaction>& coinjoinTransactions,
                                                    AddressDisjointSets& ds,
                                                    const std::unordered_map<Address, uint32_t>& collectedAddresses,
                                                    int hops, std::unordered_set<Transaction>& visitedTransactions) {
            if (visitedTransactions.count(tx)) {
                return;
            }
            visitedTransactions.insert(tx);


            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                auto spending_tx = output.getSpendingTx().value();

                if (coinjoinTransactions.count(spending_tx)) {
                    continue;
                }

                if (hops > 0) {
                    one_hop_output_threshold_consolidation(spending_tx, coinjoinTransactions, ds, collectedAddresses,
                                                           hops - 1, visitedTransactions);
                }

                if (tx.outputCount() > 1 && 5 * spending_tx.inputCount() <= spending_tx.outputCount()) {
                    continue;
                }

                auto base_address = spending_tx.inputs()[0].getAddress();
                if (collectedAddresses.find(base_address) == collectedAddresses.end()) {
                    continue;
                }

                for (const auto& input : spending_tx.inputs()) {
                    auto input_tx = input.getSpentTx();
                    if (coinjoinTransactions.count(input_tx)) {
                        continue;
                    }

                    auto input_address = input.getAddress();
                    if (collectedAddresses.find(input_address) == collectedAddresses.end()) {
                        continue;
                    }

                    ds.link_addresses(base_address, input_address);
                }
            }
        }

        // Wrapper function to initialize the visitedTransactions set
        void one_hop_output_threshold_consolidation_wrapper(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses, int hops = 0) {
            std::unordered_set<Transaction> visitedTransactions;
            one_hop_output_threshold_consolidation(tx, coinjoinTransactions, ds, collectedAddresses, hops,
                                                   visitedTransactions);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) const {
            one_output_consolidation(tx, coinjoinTransactions, ds, collectedAddresses);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) const {
            one_output_consolidation_with_change(tx, coinjoinTransactions, ds, collectedAddresses);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneInputConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) const {
            one_input_consolidation(tx, coinjoinTransactions, ds, collectedAddresses);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::None>::operator()(
            const Transaction&, const std::unordered_set<Transaction>&, AddressDisjointSets&,
            const std::unordered_map<Address, uint32_t>&) const {
            // Do nothing
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneHopOutputThresholdConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) const {
            one_hop_output_threshold_consolidation_wrapper(tx, coinjoinTransactions, ds, collectedAddresses, 0);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::TwoHopOutputThresholdConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) const {
            one_hop_output_threshold_consolidation_wrapper(tx, coinjoinTransactions, ds, collectedAddresses, 1);
        }
        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::ThreeHopOutputThresholdConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) const {
            one_hop_output_threshold_consolidation_wrapper(tx, coinjoinTransactions, ds, collectedAddresses, 2);
        }

    }  // namespace coinjoin_heuristics
}  // namespace blocksci