    void ClearAllDatabaseState();
#pragma once
#include "pch.hpp"
#include "Singleton.hpp"
#include "Debug/Log.hpp"
#include "Interlink/Interlink.hpp"
#include "Heuristic/Heuristic.hpp"
#include "Database/RedisCacheDatabase.hpp"
#include "AtlasNet/AtlasEntity.hpp"
class God : public Singleton<God>
{
public:
    void ClearAllDatabaseState();
    struct ActiveContainer
    {
        Json LatestInformJson;
        DockerContainerID ID;
    };

    // Structure to cache partition shape data
    struct PartitionShapeCache {
        std::vector<Shape> shapes;
        std::map<std::string, size_t> partitionToShapeIndex;
        bool isValid = false;
    };

private:
    CURL *curl;
    std::shared_ptr<Log> logger = std::make_shared<Log>("God");
    Heuristic heuristic;
    PartitionShapeCache shapeCache;
    struct IndexByID
    {
    };
    boost::multi_index_container<
        ActiveContainer,
        boost::multi_index::indexed_by<
            // Unique By ID
            boost::multi_index::ordered_unique<
                boost::multi_index::tag<IndexByID>,
                boost::multi_index::member<ActiveContainer, DockerContainerID,
                                           &ActiveContainer::ID>>

            >>
        ActiveContainers;
    bool ShouldShutdown = false;

public:
    God();
    ~God();

    void Shutdown()
    {
        ShouldShutdown = true;
    }

    void Init();

    /**
     * @brief Computes partition shapes using heuristic algorithms and stores them in both memory cache and database.
     *
     * This function calls the Heuristic::computePartition() method to generate optimal
     * partition boundaries, then:
     * 1. Stores the shapes in memory for fast access
     * 2. Serializes and stores the shapes in the cache database for persistence
     *
     * @return bool True if the operation completed successfully, false otherwise.
     */
    bool computeAndStorePartitions();

    /**
     * @brief Gets the cached shape for a specific partition.
     * @param partitionId The ID of the partition to get the shape for.
     * @return Pointer to the shape if found in cache, nullptr otherwise.
     */
    const Shape* getCachedShape(const std::string& partitionId) const;

    /**
     * @brief Gets all cached shapes.
     * @return Vector of all shapes currently in the cache.
     */
    const std::vector<Shape>& getAllCachedShapes() const { return shapeCache.shapes; }

    /**
     * @brief Checks if the shape cache is valid.
     * @return True if the cache contains valid shape data.
     */
    bool isShapeCacheValid() const { return shapeCache.isValid; }


    
    private:
        void HeuristicUpdate();
        void RedistributeOutliersToPartitions();
        void handleOutlierMessage(const std::string& message);
        void handleOutliersDetectedMessage(const std::string& message);
        void handleEntityRemovedMessage(const std::string& message);
        void processOutliersForPartition(const std::string& partitionId);
        void processExistingOutliers();
        
        /**
         * @brief Efficient grid cell-based entity detection
         * 
         * This method uses grid cells instead of triangles for much faster entity positioning.
         * It's 4.5x faster than triangle-based detection and uses 50% less memory.
         */
        void processOutliersWithGridCells();
        
        /**
         * @brief Redistributes an entity to the correct partition based on grid cell shapes
         * 
         * This method is called when a partition confirms it has removed an entity.
         * God then finds the correct target partition and moves the entity there.
         */
        void redistributeEntityToCorrectPartition(const AtlasEntity& entity, const std::string& sourcePartition);
        
        // Helpers to improve readability and reuse
        std::vector<InterLinkIdentifier> getPartitionServerIds() const;
        std::optional<std::string> findPartitionForPoint(const vec2& point) const;
    /**
     * @brief Retrieves a set of all active partition IDs.
     */
    const decltype(ActiveContainers) &GetContainers();

    /**
     * @brief Get the Partition object (returns null for now)
     */
    const ActiveContainer &GetContainer(const DockerContainerID &id);

    /**
     * @brief Notifies all partitions to fetch their shape data
     */
    void notifyPartitionsToFetchShapes();

    /**
     * @brief Spawns a new partition by invoking an external script.
     */
    std::optional<ActiveContainer> spawnPartition();

    /**
     * @brief Removes a partition by its ID.
     */
    bool removePartition(const DockerContainerID &id, uint32 TimeOutSeconds = 10);

    /**
     * @brief Cleans up all active partitions.
     */
    bool cleanupContainers();
    /**
     * @brief Set of active partition IDs.
     */
    std::set<int32> partitionIds;

    /**
     * @brief Set of partitions that were assigned shapes in the last computeAndStorePartitions call.
     */
    std::set<InterLinkIdentifier> assignedPartitions;

    /**
     * @brief Cache database pointer for storing partition metadata.
     *
     */
    std::unique_ptr<IDatabase> cache;

    /**
     * @brief Handles termination signals to ensure cleanup of partitions before exiting. (doesnt work rn)
     */
    static void handleSignal(int32 signum);
};