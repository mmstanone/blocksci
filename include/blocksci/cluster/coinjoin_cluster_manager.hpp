

#ifndef coinjoin_cluster_manager_hpp
#define coinjoin_cluster_manager_hpp

#include <blocksci/blocksci_export.h>
#include "../external/dset/dset.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#include "cluster.hpp"
#include "cluster_fwd.hpp"

namespace blocksci {
    namespace coinjoin_heuristics {
        struct ClusteringHeuristic;
    }

    struct BLOCKSCI_EXPORT AddressDisjointSets {
        DisjointSets disjoinSets;
        std::unordered_map<Address, uint32_t> addressIndices;

        AddressDisjointSets(uint32_t size, std::unordered_map<Address, uint32_t> addressIndices_)
            : disjoinSets(size), addressIndices(addressIndices_) {}

        uint32_t size() const { return disjoinSets.size(); }

        void link_addresses(const Address &address1, const Address &address2) {
            auto firstAddressIndex = addressIndices.at(address1);
            auto secondAddressIndex = addressIndices.at(address2);
            disjoinSets.unite(firstAddressIndex, secondAddressIndex);
        }

        void resolveAll();

        uint32_t find(uint32_t index) { return disjoinSets.find(index); }
    };

    class ClusterAccess;

    class BLOCKSCI_EXPORT CoinjoinClusterManager {
        std::unique_ptr<ClusterAccess> access;
        uint32_t clusterCount;

        friend class blocksci::Cluster;

       public:
        CoinjoinClusterManager(const std::string &baseDirectory, DataAccess &access);
        CoinjoinClusterManager(CoinjoinClusterManager &&other);
        CoinjoinClusterManager &operator=(CoinjoinClusterManager &&other);
        ~CoinjoinClusterManager();

        static CoinjoinClusterManager createClustering(
            BlockRange &chain, const blocksci::coinjoin_heuristics::ClusteringHeuristic &clusteringFunc,
            const std::string &outputPath, bool overwrite = false, std::string coinjoinType = "None");

        Cluster getCluster(const Address &address) const;

        ranges::any_view<Cluster, ranges::category::random_access | ranges::category::sized> getClusters() const;

        ranges::any_view<TaggedCluster> taggedClusters(const std::unordered_map<Address, std::string> &tags) const;
    };

    using cluster_range = decltype(std::declval<CoinjoinClusterManager>().getClusters());
}  // namespace blocksci

#endif /* coinjoin_cluster_manager_hpp */
