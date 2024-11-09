

#ifndef coinjoin_cluster_manager_hpp
#define coinjoin_cluster_manager_hpp

#include "cluster_fwd.hpp"
#include "cluster.hpp"

#include <blocksci/blocksci_export.h>
#include <utility>
#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

namespace blocksci {
    namespace heuristics {
        struct ChangeHeuristic;
    }
    
    class ClusterAccess;

    class BLOCKSCI_EXPORT CoinjoinClusterManager {
        std::unique_ptr<ClusterAccess> access;
        uint32_t clusterCount;
        
        friend class blocksci::Cluster;
        
    public:
        CoinjoinClusterManager(const std::string &baseDirectory, DataAccess &access);
        CoinjoinClusterManager(CoinjoinClusterManager && other);
        CoinjoinClusterManager &operator=(CoinjoinClusterManager && other);
        ~CoinjoinClusterManager();
    
        static CoinjoinClusterManager createClustering(BlockRange &chain, const std::string &outputPath, bool overwrite = false, std::string coinjoinType = "None");
        
        Cluster getCluster(const Address &address) const;
        
        ranges::any_view<Cluster, ranges::category::random_access | ranges::category::sized> getClusters() const;
        
        ranges::any_view<TaggedCluster> taggedClusters(const std::unordered_map<Address, std::string> &tags) const;
    };
    
    using cluster_range = decltype(std::declval<CoinjoinClusterManager>().getClusters());
} // namespace blocksci


#endif /* coinjoin_cluster_manager_hpp */
