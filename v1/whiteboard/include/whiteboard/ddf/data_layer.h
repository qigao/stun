#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace whiteboard {
namespace ddf {

/**
 * @brief Represents a node in the data layer
 */
struct DataNode {
    std::string id;
    std::string type;
    std::map<std::string, std::string> properties;
};

/**
 * @brief Represents a relationship between data nodes
 */
struct DataRelationship {
    std::string id;
    std::string type;
    std::string from_node_id;
    std::string to_node_id;
    std::map<std::string, std::string> properties;
};

/**
 * @brief Data layer for storing structured information
 * 
 * The data layer stores structured data that drives diagram generation and updates.
 * It supports hierarchical structures (trees, graphs, lists) with arbitrary properties.
 */
class DataLayer {
public:
    DataLayer() = default;
    ~DataLayer() = default;

    // Node operations
    void add_node(const DataNode& node);
    void remove_node(const std::string& id);
    void update_node(const std::string& id, const std::map<std::string, std::string>& properties);
    
    DataNode* get_node(const std::string& id);
    const DataNode* get_node(const std::string& id) const;
    
    std::vector<DataNode*> get_all_nodes();
    std::vector<const DataNode*> get_all_nodes() const;

    // Relationship operations
    void add_relationship(const DataRelationship& rel);
    void remove_relationship(const std::string& id);
    
    DataRelationship* get_relationship(const std::string& id);
    const DataRelationship* get_relationship(const std::string& id) const;
    
    std::vector<DataRelationship*> get_relationships_for_node(const std::string& node_id);
    std::vector<const DataRelationship*> get_relationships_for_node(const std::string& node_id) const;

    // Query interface
    std::vector<DataNode*> query_nodes(const std::string& type);
    std::vector<const DataNode*> query_nodes(const std::string& type) const;
    
    std::vector<DataNode*> query_nodes_by_property(const std::string& key, const std::string& value);
    std::vector<const DataNode*> query_nodes_by_property(const std::string& key, const std::string& value) const;

    // Clear all data
    void clear();

private:
    std::map<std::string, DataNode> nodes_;
    std::map<std::string, DataRelationship> relationships_;
};

} // namespace ddf
} // namespace whiteboard
