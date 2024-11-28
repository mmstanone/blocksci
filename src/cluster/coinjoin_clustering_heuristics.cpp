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

        void one_hop_output_threshold_consolidation(const Transaction& tx,
                                                    const std::unordered_set<Transaction>& coinjoinTransactions,
                                                    AddressDisjointSets& ds,
                                                    const std::unordered_map<Address, uint32_t>& collectedAddresses,
                                                    int hops, std::unordered_set<Transaction>& visitedTransactions) {
            // Avoid processing the same transaction multiple times
            if (visitedTransactions.count(tx)) {
                return;
            }
            visitedTransactions.insert(tx);


            // Iterate through all outputs of the transaction
            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                auto spending_tx = output.getSpendingTx().value();

                if (coinjoinTransactions.count(spending_tx)) {
                    continue;
                }

                // Recursively process the spending transaction if hops remain
                if (hops > 0) {
                    one_hop_output_threshold_consolidation(spending_tx, coinjoinTransactions, ds, collectedAddresses,
                                                           hops - 1, visitedTransactions);
                }

                // Check if the spending transaction is a consolidation transaction
                if (1.5 * spending_tx.inputCount() <= spending_tx.outputCount()) {
                    continue;
                }

                auto spending_tx_output_address = spending_tx.outputs()[0].getAddress();
                auto maxOutputValue = spending_tx.outputs()[0].getValue();
                for (const auto &output: spending_tx.outputs()) {
                    if (output.getValue() > maxOutputValue) {
                        spending_tx_output_address = output.getAddress();
                        maxOutputValue = output.getValue();
                    }
                }


                if (collectedAddresses.find(spending_tx_output_address) == collectedAddresses.end()) {
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

                    ds.link_addresses(spending_tx_output_address, input_address);
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

        ClusteringHeuristic getClusteringHeuristic(const std::string& heuristicName) {
            if (heuristicName == "OneOutputConsolidation") {
                std::cout << "Using heuristic: OneOutputConsolidation" << std::endl;
                return ClusteringHeuristic(OneOutputConsolidation{});
            } else if (heuristicName == "OneInputConsolidation") {
                std::cout << "Using heuristic: OneInputConsolidation" << std::endl;
                return ClusteringHeuristic(OneInputConsolidation{});
            } else if (heuristicName == "None") {
                std::cout << "Using heuristic: None" << std::endl;
                return ClusteringHeuristic(NoClustering{});
            } else if (heuristicName == "OneHopOutputThresholdConsolidation") {
                std::cout << "Using heuristic: OneHopOutputThresholdConsolidation" << std::endl;
                return ClusteringHeuristic(OneHopOutputThresholdConsolidation{});
            } else if (heuristicName == "TwoHopOutputThresholdConsolidation") {
                std::cout << "Using heuristic: TwoHopOutputThresholdConsolidation" << std::endl;
                return ClusteringHeuristic(TwoHopOutputThresholdConsolidation{});
            } else if (heuristicName == "ThreeHopOutputThresholdConsolidation") {
                std::cout << "Using heuristic: ThreeHopOutputThresholdConsolidation" << std::endl;
                return ClusteringHeuristic(ThreeHopOutputThresholdConsolidation{});
            }

            else {
                throw std::invalid_argument("Invalid heuristic name");
            }
        }
    }  // namespace coinjoin_heuristics
}  // namespace blocksci