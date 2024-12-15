#include <blocksci/cluster/coinjoin_clustering_heuristics.hpp>
#include <blocksci/heuristics/tx_identification.hpp>
#include <unordered_set>
#include <vector>

namespace blocksci {
    namespace coinjoin_heuristics {
        static void oneInputConsolidation(const Transaction& tx,
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

        static void oneOutputConsolidation(const Transaction& tx,
                                           const std::unordered_set<Transaction>& coinjoinTransactions,
                                           AddressDisjointSets& ds,
                                           const std::unordered_map<Address, uint32_t>& collectedAddresses, int hops) {
            // --> go this way
            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                auto nextTx = output.getSpendingTx().value();

                if (nextTx.outputCount() != 1) {
                    continue;
                }

                if (hops > 0) {
                    oneOutputConsolidation(nextTx, coinjoinTransactions, ds, collectedAddresses, hops - 1);
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
                    if (collectedAddresses.find(nextLevelInputAddress) == collectedAddresses.end()) {
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
        static void oneOutputConsolidationWithChange(const Transaction& tx,
                                                     const std::unordered_set<Transaction>& coinjoinTransactions,
                                                     AddressDisjointSets& ds,
                                                     const std::unordered_map<Address, uint32_t>& collectedAddresses,
                                                     int hops) {
            // --> go this way
            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                auto nextTx = output.getSpendingTx().value();

                if (hops > 0) {
                    oneOutputConsolidationWithChange(nextTx, coinjoinTransactions, ds, collectedAddresses, hops - 1);
                }

                if (nextTx.outputCount() != 2) {
                    continue;
                }

                auto nextTxOutputAddress = nextTx.outputs()[0].getValue() < nextTx.outputs()[1].getValue()
                                               ? nextTx.outputs()[0].getAddress()
                                               : nextTx.outputs()[1].getAddress();

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
         * Since there can be more outputs and we are not sure which one belongs to the same entity, we link only the
         * inputs.
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
        void outputThresholdConsolidation(const Transaction& tx,
                                          const std::unordered_set<Transaction>& coinjoinTransactions,
                                          AddressDisjointSets& ds,
                                          const std::unordered_map<Address, uint32_t>& collectedAddresses, int hops,
                                          std::unordered_set<Transaction>& visitedTransactions) {
            if (visitedTransactions.count(tx)) {
                return;
            }
            visitedTransactions.insert(tx);

            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                auto spendingTx = output.getSpendingTx().value();

                if (coinjoinTransactions.count(spendingTx)) {
                    continue;
                }

                if (hops > 0) {
                    outputThresholdConsolidation(spendingTx, coinjoinTransactions, ds, collectedAddresses, hops - 1,
                                                 visitedTransactions);
                }

                if (spendingTx.outputCount() == 1) {
                    auto outputAddress = output.getAddress();
                    if (collectedAddresses.find(outputAddress) == collectedAddresses.end()) {
                        continue;
                    }

                    for (const auto& input : spendingTx.inputs()) {
                        auto inputAddress = input.getAddress();
                        if (collectedAddresses.find(inputAddress) == collectedAddresses.end()) {
                            continue;
                        }

                        ds.link_addresses(outputAddress, inputAddress);
                    }

                    continue;
                }

                if (spendingTx.outputCount() > 1 && spendingTx.inputCount() < 2 * spendingTx.outputCount()) {
                    continue;
                }

                auto baseAddress = spendingTx.inputs()[0].getAddress();
                if (collectedAddresses.find(baseAddress) == collectedAddresses.end()) {
                    continue;
                }

                for (const auto& input : spendingTx.inputs()) {
                    auto inputTx = input.getSpentTx();
                    if (coinjoinTransactions.count(inputTx)) {
                        continue;
                    }

                    auto inputAddress = input.getAddress();
                    if (collectedAddresses.find(inputAddress) == collectedAddresses.end()) {
                        continue;
                    }

                    ds.link_addresses(baseAddress, inputAddress);
                }
            }
        }

        void fakeOutputsConsolidation(const Transaction& tx,
                                      const std::unordered_set<Transaction>& coinjoinTransactions,
                                      AddressDisjointSets& ds,
                                      const std::unordered_map<Address, uint32_t>& collectedAddresses, int hops) {
            for (const auto& output : tx.outputs()) {
                if (!output.isSpent()) continue;

                const auto& optNextTx = output.getSpendingTx();

                const auto& nextTx = optNextTx.value();

                if (hops > 0) {
                    fakeOutputsConsolidation(nextTx, coinjoinTransactions, ds, collectedAddresses, hops - 1);
                }

                if (nextTx.inputCount() < 5 || nextTx.outputCount() > 6 || nextTx.outputCount() < 2 || coinjoinTransactions.count(nextTx)) {
                    continue;
                }

                std::vector<std::pair<Address, int64_t>> outputValues;
                int64_t sum = 0;
                outputValues.reserve(nextTx.outputs().size());

                // Collect only relevant outputs and compute sum
                for (const auto& nextOutput : nextTx.outputs()) {
                    sum += nextOutput.getValue();
                    auto addr = nextOutput.getAddress();
                    if (collectedAddresses.find(addr) != collectedAddresses.end()) {
                        outputValues.emplace_back(addr, nextOutput.getValue());
                    }
                }

                std::sort(outputValues.begin(), outputValues.end(), [](const auto& a, const auto& b) {
                    return a.second > b.second;
                });

                auto largest = outputValues[0];
                auto secondLargest = outputValues[1];

                if (std::abs(largest.second - secondLargest.second) > largest.second * 0.01) {
                    continue;
                }
                
                int64_t bigParts = largest.second + secondLargest.second;
                if (bigParts < sum * 0.75) {
                    continue;
                }

                for (const auto& nextInput : nextTx.inputs()) {
                    auto inAddr = nextInput.getAddress();
                    if (collectedAddresses.find(inAddr) == collectedAddresses.end()) {
                        continue;
                    }

                    ds.link_addresses(largest.first, inAddr);
                    ds.link_addresses(secondLargest.first, inAddr);
                }
            }
        }

        // Wrapper function to initialize the visitedTransactions set
        void outputThresholdConsolidationWrapper(const Transaction& tx,
                                                 const std::unordered_set<Transaction>& coinjoinTransactions,
                                                 AddressDisjointSets& ds,
                                                 const std::unordered_map<Address, uint32_t>& collectedAddresses,
                                                 int hops = 0) {
            std::unordered_set<Transaction> visitedTransactions;
            outputThresholdConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, hops, visitedTransactions);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 0);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation1Hop>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 1);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation2Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 2);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidation3Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 3);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidationWithChange(tx, coinjoinTransactions, ds, collectedAddresses, 0);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange1Hop>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidationWithChange(tx, coinjoinTransactions, ds, collectedAddresses, 1);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange2Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidationWithChange(tx, coinjoinTransactions, ds, collectedAddresses, 2);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneOutputConsolidationWithChange3Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneOutputConsolidationWithChange(tx, coinjoinTransactions, ds, collectedAddresses, 3);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OneInputConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            oneInputConsolidation(tx, coinjoinTransactions, ds, collectedAddresses);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::None>::operator()(
            const Transaction&, const std::unordered_set<Transaction>&, AddressDisjointSets&,
            const std::unordered_map<Address, uint32_t>&) {
            // Do nothing
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            outputThresholdConsolidationWrapper(tx, coinjoinTransactions, ds, collectedAddresses, 0);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation1Hop>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            outputThresholdConsolidationWrapper(tx, coinjoinTransactions, ds, collectedAddresses, 1);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation2Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            outputThresholdConsolidationWrapper(tx, coinjoinTransactions, ds, collectedAddresses, 2);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::OutputThresholdConsolidation3Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            outputThresholdConsolidationWrapper(tx, coinjoinTransactions, ds, collectedAddresses, 3);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            fakeOutputsConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 0);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation1Hop>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            fakeOutputsConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 1);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation2Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            fakeOutputsConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 2);
        }

        template <>
        void ClusteringHeuristicImpl<ClusteringHeuristicsType::FakeOutputConsolidation3Hops>::operator()(
            const Transaction& tx, const std::unordered_set<Transaction>& coinjoinTransactions, AddressDisjointSets& ds,
            const std::unordered_map<Address, uint32_t>& collectedAddresses) {
            fakeOutputsConsolidation(tx, coinjoinTransactions, ds, collectedAddresses, 3);
        }

    }  // namespace coinjoin_heuristics
}  // namespace blocksci