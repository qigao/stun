#include <whiteboard/ddf/data_layer.h>

namespace whiteboard {
namespace ddf {

void DataLayer::add_node(const DataNode &node) { nodes_[node.id] = node; }

void DataLayer::remove_node(const std::string &id) { nodes_.erase(id); }

void DataLayer::update_node(const std::string &id,
                            const std::map<std::string, std::string> &properties) {
  auto it = nodes_.find(id);
  if (it != nodes_.end()) {
    for (const auto &[key, value] : properties) {
      it->second.properties[key] = value;
    }
  }
}

DataNode *DataLayer::get_node(const std::string &id) {
  auto it = nodes_.find(id);
  return it != nodes_.end() ? &it->second : nullptr;
}

const DataNode *DataLayer::get_node(const std::string &id) const {
  auto it = nodes_.find(id);
  return it != nodes_.end() ? &it->second : nullptr;
}

std::vector<DataNode *> DataLayer::get_all_nodes() {
  std::vector<DataNode *> result;
  result.reserve(nodes_.size());
  for (auto &[id, node] : nodes_) {
    result.push_back(&node);
  }
  return result;
}

std::vector<const DataNode *> DataLayer::get_all_nodes() const {
  std::vector<const DataNode *> result;
  result.reserve(nodes_.size());
  for (const auto &[id, node] : nodes_) {
    result.push_back(&node);
  }
  return result;
}

void DataLayer::add_relationship(const DataRelationship &rel) { relationships_[rel.id] = rel; }

void DataLayer::remove_relationship(const std::string &id) { relationships_.erase(id); }

DataRelationship *DataLayer::get_relationship(const std::string &id) {
  auto it = relationships_.find(id);
  return it != relationships_.end() ? &it->second : nullptr;
}

const DataRelationship *DataLayer::get_relationship(const std::string &id) const {
  auto it = relationships_.find(id);
  return it != relationships_.end() ? &it->second : nullptr;
}

std::vector<DataRelationship *> DataLayer::get_relationships_for_node(const std::string &node_id) {
  std::vector<DataRelationship *> result;
  for (auto &[id, rel] : relationships_) {
    if (rel.from_node_id == node_id || rel.to_node_id == node_id) {
      result.push_back(&rel);
    }
  }
  return result;
}

std::vector<const DataRelationship *>
DataLayer::get_relationships_for_node(const std::string &node_id) const {
  std::vector<const DataRelationship *> result;
  for (const auto &[id, rel] : relationships_) {
    if (rel.from_node_id == node_id || rel.to_node_id == node_id) {
      result.push_back(&rel);
    }
  }
  return result;
}

std::vector<DataNode *> DataLayer::query_nodes(const std::string &type) {
  std::vector<DataNode *> result;
  for (auto &[id, node] : nodes_) {
    if (node.type == type) {
      result.push_back(&node);
    }
  }
  return result;
}

std::vector<const DataNode *> DataLayer::query_nodes(const std::string &type) const {
  std::vector<const DataNode *> result;
  for (const auto &[id, node] : nodes_) {
    if (node.type == type) {
      result.push_back(&node);
    }
  }
  return result;
}

std::vector<DataNode *> DataLayer::query_nodes_by_property(const std::string &key,
                                                           const std::string &value) {
  std::vector<DataNode *> result;
  for (auto &[id, node] : nodes_) {
    auto it = node.properties.find(key);
    if (it != node.properties.end() && it->second == value) {
      result.push_back(&node);
    }
  }
  return result;
}

std::vector<const DataNode *> DataLayer::query_nodes_by_property(const std::string &key,
                                                                 const std::string &value) const {
  std::vector<const DataNode *> result;
  for (const auto &[id, node] : nodes_) {
    auto it = node.properties.find(key);
    if (it != node.properties.end() && it->second == value) {
      result.push_back(&node);
    }
  }
  return result;
}

void DataLayer::clear() {
  nodes_.clear();
  relationships_.clear();
}

} // namespace ddf
} // namespace whiteboard
