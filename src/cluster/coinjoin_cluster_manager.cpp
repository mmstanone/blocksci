//
//  cluster_manager.cpp
//  blocksci
//
//  Created by Harry Kalodner on 7/6/17.
//
//

#include <dset/dset.h>
#include <wjfilesystem/path.h>

#include <blocksci/chain/blockchain.hpp>
#include <blocksci/chain/input.hpp>
#include <blocksci/chain/range_util.hpp>
#include <blocksci/cluster/cluster.hpp>
#include <blocksci/cluster/cluster_manager.hpp>
#include <blocksci/cluster/coinjoin_cluster_manager.hpp>
#include <blocksci/cluster/coinjoin_clustering_heuristics.hpp>
#include <blocksci/core/dedup_address.hpp>
#include <blocksci/heuristics/change_address.hpp>
#include <blocksci/scripts/scripthash_script.hpp>
#include <fstream>
#include <future>
#include <internal/address_info.hpp>
#include <internal/cluster_access.hpp>
#include <internal/data_access.hpp>
#include <internal/progress_bar.hpp>
#include <internal/script_access.hpp>
#include <map>
#include <nlohmann/json.hpp>
#include <range/v3/range_for.hpp>
#include <range/v3/view/iota.hpp>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace {
    template <typename Job>
    void segmentWork(uint32_t start, uint32_t end, uint32_t segmentCount, Job job) {
        uint32_t total = end - start;

        // Don't partition over threads if there are less items than segment count
        if (total < segmentCount) {
            for (uint32_t i = start; i < end; ++i) {
                job(i);
            }
            return;
        }

        auto segmentSize = total / segmentCount;
        auto segmentsRemaining = total % segmentCount;
        std::vector<std::pair<uint32_t, uint32_t>> segments;
        uint32_t i = 0;
        while (i < total) {
            uint32_t startSegment = i;
            i += segmentSize;
            if (segmentsRemaining > 0) {
                i += 1;
                segmentsRemaining--;
            }
            uint32_t endSegment = i;
            segments.emplace_back(startSegment + start, endSegment + start);
        }
        std::vector<std::thread> threads;
        for (uint32_t i = 0; i < segmentCount - 1; i++) {
            auto segment = segments[i];
            threads.emplace_back([segment, &job]() {
                for (uint32_t i = segment.first; i < segment.second; i++) {
                    job(i);
                }
            });
        }

        auto segment = segments.back();
        for (uint32_t i = segment.first; i < segment.second; i++) {
            job(i);
        }

        for (auto &thread : threads) {
            thread.join();
        }
    }
}  // namespace

namespace blocksci {

    void AddressDisjointSets::resolveAll() {
        segmentWork(0, disjoinSets.size(), 8, [&](uint32_t index) { disjoinSets.find(index); });
    }

    CoinjoinClusterManager::CoinjoinClusterManager(const std::string &baseDirectory, DataAccess &access_)
        : access(std::make_unique<ClusterAccess>(baseDirectory, access_)), clusterCount(access->clusterCount()) {}

    CoinjoinClusterManager::CoinjoinClusterManager(CoinjoinClusterManager &&other) = default;

    CoinjoinClusterManager &CoinjoinClusterManager::operator=(CoinjoinClusterManager &&other) = default;

    CoinjoinClusterManager::~CoinjoinClusterManager() = default;

    Cluster CoinjoinClusterManager::getCluster(const Address &address) const {
        auto clusterNum = access->getClusterNum(RawAddress{address.scriptNum, address.type});
        if (clusterNum == UINT32_MAX) {
            std::cerr << "Address not found in cluster manager" << std::endl;
            return Cluster(0, *access);
        }
        return Cluster(clusterNum + 1, *access);
    }

    ranges::any_view<Cluster, ranges::category::random_access | ranges::category::sized>
    CoinjoinClusterManager::getClusters() const {
        return ranges::views::ints(0u, clusterCount) |
               ranges::views::transform([&](uint32_t clusterNum) { return Cluster(clusterNum, *access); });
    }

    ranges::any_view<TaggedCluster> CoinjoinClusterManager::taggedClusters(
        const std::unordered_map<Address, std::string> &tags) const {
        return getClusters() | ranges::views::transform([tags](Cluster &&cluster) -> ranges::optional<TaggedCluster> {
                   return cluster.getTaggedUnsafe(tags);
               }) |
               flatMapOptionals();
    }

    uint32_t remapClusterIdsForCoinJoins(std::vector<uint32_t> &clusterIDs) {
        uint32_t placeholder = UINT32_MAX;
        std::unordered_map<uint32_t, uint32_t> clusterIDMap;
        uint32_t clusterCount = 0;

        for (uint32_t &clusterID : clusterIDs) {
            if (clusterID == placeholder) {
                continue;
            }
            auto it = clusterIDMap.find(clusterID);
            if (it == clusterIDMap.end()) {
                clusterIDMap[clusterID] = clusterCount++;
            }
            clusterID = clusterIDMap[clusterID];
        }

        return clusterCount;
    }

    std::unordered_set<Transaction> identifyCoinjoinTransactions(BlockRange &chain, const std::string &coinjoinType) {
        auto mapFunc = [&](const BlockRange &blocks) {
            std::unordered_set<Transaction> localCoinjoinTransactions;
            for (const auto &block : blocks) {
                for (const auto &tx : block) {
                    if (heuristics::isCoinjoinOfGivenType(tx, coinjoinType)) {
                        localCoinjoinTransactions.insert(tx);
                    }
                }
            }
            return localCoinjoinTransactions;
        };

        auto reduceFunc = [](std::unordered_set<Transaction> &set1,
                             std::unordered_set<Transaction> &set2) -> std::unordered_set<Transaction> & {
            set1.insert(set2.begin(), set2.end());
            return set1;
        };

        return chain.mapReduce<std::unordered_set<Transaction>>(mapFunc, reduceFunc);
    }

    /**
     * Create clusters from a set of transactions and a heuristic.
     * Skips CoinJoin and Coinbase transactions.
     * 
     * @param chain The blocks to process
     * @param collectedAddresses A map of addresses to their index in the disjoint set
     * @param coinjoinTransactions A set of transactions that are coinjoins
     * @param heuristic The heuristic to use to cluster the transactions
     * 
     * @return A pair of the cluster IDs and the number of clusters
     */
    std::pair<std::vector<uint32_t>, uint32_t> createClusters(
        BlockRange &chain, std::unordered_map<Address, uint32_t> collectedAddresses,
        std::unordered_set<Transaction> coinjoinTransactions,
        const blocksci::coinjoin_heuristics::ClusteringHeuristic &heuristic) {
        std::cout << "Creating disjoint sets of size " << collectedAddresses.size() << std::endl;
        blocksci::AddressDisjointSets ds(collectedAddresses.size(), collectedAddresses);

        std::cout << "Created disjoint sets of size " << ds.size() << std::endl;

        // push the heuristics into the map function and link addresses in them
        auto mapFunc = [&](const BlockRange &blocks) {
            for (const auto &block : blocks) {
                for (const auto &tx : block) {
                    if (tx.isCoinbase()) continue;
                    if (!coinjoinTransactions.count(tx)) {
                        continue;
                    }
                    heuristic(tx, coinjoinTransactions, ds, collectedAddresses);
                }
            }
            return 0;
        };

        // noop reduce function
        auto reduceFunc = [](int a, int b) -> int { return a + b; };

        chain.mapReduce<int>(mapFunc, reduceFunc);

        ds.resolveAll();

        std::vector<uint32_t> parents(ds.size());
        for (uint32_t i = 0; i < ds.size(); ++i) {
            parents[i] = ds.find(i);
        }

        uint32_t clusterCount = ClusterManager::remapClusterIds(parents);
        return std::make_pair(parents, clusterCount);
    }

    /**
     * Collect addresses within `maxHops` from a set of transactions `startTransactions`.
     * This is used to collect addresses around CoinJoin transactions gathered in a different step.
     * 
     * This method also assigns a global index to the addresses.
     */
    std::unordered_map<Address, uint32_t> collectAddressesWithinHops(
        const std::unordered_set<Transaction> &startTransactions, int maxHops) {
        std::unordered_map<Address, uint32_t> collectedAddresses;
        uint32_t index = 0;
        std::unordered_set<Transaction> transactionsToProcess = startTransactions;
        std::unordered_set<Transaction> processedTransactions;

        for (int hop = 0; hop < maxHops; ++hop) {
            std::unordered_set<Transaction> nextTransactions;

            for (const auto &tx : transactionsToProcess) {
                if (processedTransactions.count(tx)) continue;
                processedTransactions.insert(tx);

                for (const auto &input : tx.inputs()) {
                    // don't increment the index if the address is already in the map
                    auto [_, res] = collectedAddresses.insert(std::make_pair(input.getAddress(), index));
                    if (res) index++;

                    auto prevTx = input.getSpentTx();
                    if (processedTransactions.count(prevTx) == 0) {
                        nextTransactions.insert(prevTx);
                    }
                }

                for (const auto &output : tx.outputs()) {
                    auto [_, res] = collectedAddresses.insert(std::make_pair(output.getAddress(), index));
                    if (res) index++;

                    if (output.isSpent()) {
                        auto nextTx = output.getSpendingTx().value();
                        if (processedTransactions.count(nextTx) == 0) {
                            nextTransactions.insert(nextTx);
                        }
                    }
                }
            }

            transactionsToProcess = std::move(nextTransactions);
        }

        return collectedAddresses;
    }

    void recordOrderedAddresses(const std::vector<uint32_t> &clusterIDs, std::vector<uint32_t> &clusterPositions,
                                const std::unordered_map<DedupAddressType::Enum, uint32_t> &scriptStarts,
                                const ScriptAccess &scripts, std::ofstream &clusterAddressesFile) {
        size_t totalAddresses = clusterIDs.size();
        std::vector<DedupAddress> orderedScripts;

        // Build clusterPositions
        for (uint32_t clusterID : clusterIDs) {
            if (clusterID != UINT32_MAX) {
                clusterPositions[clusterID + 1]++;
            }
        }

        for (size_t i = 1; i < clusterPositions.size(); ++i) {
            clusterPositions[i] += clusterPositions[i - 1];
        }

        // Prepare orderedScripts
        orderedScripts.resize(clusterPositions.back());

        // For tracking current position in each cluster
        std::vector<uint32_t> currentPositions = clusterPositions;

        // Iterate over all addresses
        for (size_t i = 0; i < totalAddresses; ++i) {
            uint32_t clusterID = clusterIDs[i];
            if (clusterID == UINT32_MAX) {
                continue;  // Address not in any cluster
            }

            uint32_t &position = currentPositions[clusterID];
            // Reconstruct DedupAddress from global index
            DedupAddressType::Enum addressType = DedupAddressType::NULL_DATA;
            uint32_t scriptNum = 0;

            // Determine addressType and scriptNum based on global index 'i'
            for (const auto &pair : scriptStarts) {
                DedupAddressType::Enum type = pair.first;
                uint32_t startIndex = pair.second;
                uint32_t endIndex = startIndex + scripts.scriptCount(type);

                if (i >= startIndex && i < endIndex) {
                    addressType = type;
                    scriptNum = static_cast<uint32_t>((i - startIndex) + 1);  // +1 to adjust back to 1-based scriptNum
                    break;
                }
            }

            orderedScripts[position] = DedupAddress(scriptNum, addressType);
            position++;
        }

        // Write orderedScripts to file
        clusterAddressesFile.write(reinterpret_cast<char *>(orderedScripts.data()),
                                   static_cast<std::streamsize>(sizeof(DedupAddress) * orderedScripts.size()));
    }

    void serializeCoinjoinClusterData(const ScriptAccess &scripts, const std::string &outputPath,
                                      const std::vector<uint32_t> &clusterIDs,
                                      const std::unordered_map<DedupAddressType::Enum, uint32_t> &scriptStarts,
                                      uint32_t clusterCount) {
        std::vector<uint32_t> clusterPositions(clusterCount + 1, 0);
        std::string addressesFile = ClusterAccess::addressesFilePath(outputPath);
        std::ofstream clusterAddressesFile(addressesFile, std::ios::binary);

        // This both with cluster index file writing can be done asynchronously
        auto recordOrdered =
            std::async(std::launch::async, recordOrderedAddresses, clusterIDs, std::ref(clusterPositions),
                       std::ref(scriptStarts), std::ref(scripts), std::ref(clusterAddressesFile));

        std::string offsetFile = ClusterAccess::offsetFilePath(outputPath);
        std::ofstream clusterOffsetFile(offsetFile, std::ios::binary);

        recordOrdered.get();

        clusterOffsetFile.write(reinterpret_cast<char *>(clusterPositions.data()),
                                sizeof(uint32_t) * clusterPositions.size());

        std::vector<std::future<void>> indexFileTasks;
        for (auto dedupType : DedupAddressType::allArray()) {
            indexFileTasks.push_back(std::async(std::launch::async, [&, dedupType]() {
                uint32_t startIndex = scriptStarts.at(dedupType);
                uint32_t totalCount = scripts.scriptCount(dedupType);
                std::string indexFilePath = ClusterAccess::typeIndexFilePath(outputPath, dedupType);
                std::ofstream indexFile(indexFilePath, std::ios::binary);

                indexFile.write(reinterpret_cast<const char *>(clusterIDs.data() + startIndex),
                                sizeof(uint32_t) * totalCount);
            }));
        }

        for (auto &task : indexFileTasks) {
            task.get();
        }
    }

    std::vector<uint32_t> collectClusterIDs(std::unordered_map<Address, uint32_t> collectedAddresses, uint32_t totalAddressCount,
                                            const std::vector<uint32_t> &parents, std::unordered_map<DedupAddressType::Enum, uint32_t> scriptStarts) {
        // Create a vector of addresses ordered by their index in `collectedAddresses`
        std::vector<Address> addresses(collectedAddresses.size());
        for (const auto &pair : collectedAddresses) {
            const Address &address = pair.first;
            uint32_t index = pair.second;
            addresses[index] = address;
        }

        // Prepare clusterIDs array
        std::vector<uint32_t> clusterIDs(totalAddressCount, UINT32_MAX);

        // Assign cluster IDs to global indices
        for (size_t i = 0; i < addresses.size(); ++i) {
            const Address &address = addresses[i];
            DedupAddressType::Enum addressType = dedupType(address.type);
            uint32_t scriptNum = address.scriptNum;
            uint32_t globalIndex = scriptStarts[addressType] + (scriptNum - 1);
            clusterIDs[globalIndex] = parents[i];
        }

        return clusterIDs;
    }

    /**
     * Create a clustering around CoinJoin transactions of given type.
     *
     * First, find the CoinJoin transactions and collect addresses within 2 hops of them.
     * Then, create clusters using the provided clustering heuristic function.
     * Finally, serialize the cluster data to the output directory.
     *
     * @param chain BlockRange to cluster
     * @param clusteringFunc Clustering heuristic function
     * @param outputPath Path to output directory
     * @param overwrite Overwrite existing cluster data
     * @param coinjoinType Type of CoinJoin transactions to cluster (wasabi1, wasabi2, whirlpool)
     * @return CoinjoinClusterManager instance
     */
    CoinjoinClusterManager CoinjoinClusterManager::createClustering(
        BlockRange &chain, const blocksci::coinjoin_heuristics::ClusteringHeuristic &clusteringFunc,
        const std::string &outputPath, bool overwrite, std::string coinjoinType) {
        ClusterManager::prepareClusterDataLocation(outputPath, overwrite);

        auto &scripts = chain.getAccess().getScripts();
        auto coinjoinTransactions = identifyCoinjoinTransactions(chain, coinjoinType);
        auto collectedAddresses = collectAddressesWithinHops(coinjoinTransactions, 2);

        std::cout << "Collected " << collectedAddresses.size() << " addresses" << std::endl;
        // Create clusters
        auto [parents, clusterCount] = createClusters(chain, collectedAddresses, coinjoinTransactions, clusteringFunc);

        std::cout << "Preparing to serialize cluster data" << std::endl;

        size_t totalAddressCount = scripts.totalAddressCount();

        std::unordered_map<DedupAddressType::Enum, uint32_t> scriptStarts;
        {
            std::vector<uint32_t> starts(DedupAddressType::size);

            for (size_t i = 0; i < DedupAddressType::size; i++) {
                DedupAddressType::Enum type = static_cast<DedupAddressType::Enum>(i);
                if (i > 0) {
                    starts[i] = scripts.scriptCount(static_cast<DedupAddressType::Enum>(i - 1)) + starts[i - 1];
                }
                scriptStarts[type] = starts[i];
            }
        }

        auto clusterIDs = collectClusterIDs(collectedAddresses, totalAddressCount, parents, scriptStarts);

        uint32_t remappedClusterCount = remapClusterIdsForCoinJoins(clusterIDs);

        std::cout << "Serializing cluster data" << std::endl;

        serializeCoinjoinClusterData(scripts, outputPath, clusterIDs, scriptStarts, remappedClusterCount);

        std::cout << "Serialized" << std::endl;
        return {filesystem::path{outputPath}.str(), chain.getAccess()};
    }

}  // namespace blocksci
