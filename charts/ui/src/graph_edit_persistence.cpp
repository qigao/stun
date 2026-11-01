#include <charts/ui/graph_edit_document.h>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace charts::ui {
namespace {

using Json = nlohmann::json;
namespace gl = stun::graphlayout;

// Bound both encoded and decoded documents: no unlimited JSON input or output
// via the linked widget / future document service capability.
constexpr std::size_t kMaxDocumentBytes = 1024U * 1024U;
constexpr std::size_t kMaxNodes = 256;
constexpr std::size_t kMaxPorts = 4096;
constexpr std::size_t kMaxEdges = 1024;
constexpr std::size_t kMaxGroups = 256;

GraphEditStatus invalid(std::string message) {
  return {GraphEditError::InvalidInput, std::move(message)};
}

bool keys(const Json &value, std::initializer_list<const char *> names) {
  if (!value.is_object() || value.size() != names.size()) return false;
  for (const auto *name : names)
    if (!value.contains(name)) return false;
  return true;
}

const char *side_name(gl::Side side) noexcept {
  switch (side) {
  case gl::Side::North: return "north";
  case gl::Side::East: return "east";
  case gl::Side::South: return "south";
  case gl::Side::West: return "west";
  default: return nullptr;
  }
}

bool parse_side(const std::string &name, gl::Side &side) noexcept {
  if (name == "north") side = gl::Side::North;
  else if (name == "east") side = gl::Side::East;
  else if (name == "south") side = gl::Side::South;
  else if (name == "west") side = gl::Side::West;
  else return false;
  return true;
}

bool number(const Json &field, double &out) {
  if (!field.is_number()) return false;
  out = field.get<double>();
  return std::isfinite(out);
}

bool text(const Json &field, std::string &out) {
  if (!field.is_string()) return false;
  out = field.get<std::string>();
  return out.size() <= 128;
}

bool scalar_fields(const Json &value, std::string &id,
                   double &x, double &y, double &w, double &h) {
  return text(value.at("id"), id) &&
         number(value.at("x"), x) && number(value.at("y"), y) &&
         number(value.at("width"), w) &&
         number(value.at("height"), h);
}

bool fixed_array(const Json &parent, const char *key,
                 std::size_t maximum) {
  return parent.at(key).is_array() && parent.at(key).size() <= maximum;
}

} // namespace

GraphEditStatus GraphEditDocument::save_json(std::string &out) const {
  if (committed_.revision == 0)
    return invalid("cannot serialize an uninitialized graph");
  try {
    Json document = {
        {"format", "stun.graphcanvas.logical"},
        {"version", 1},
        {"nodes", Json::array()},
        {"ports", Json::array()},
        {"edges", Json::array()},
        {"groups", Json::array()}};
    // Stable order is already committed by the one GraphEditDocument adapter.
    // Original topology, not transient proxy routes or native indexes, is
    // persisted. Revisions/history/selection and GPU state are not portable.
    for (const auto &node : committed_.nodes) {
      document["nodes"].push_back({
          {"id", node.id},
          {"x", node.x}, {"y", node.y},
          {"width", node.width}, {"height", node.height},
          {"locked", node.locked}});
    }
    for (const auto &port : committed_.ports) {
      const char *side = side_name(port.side);
      if (!side) return invalid("unmeasured original port cannot be serialized");
      document["ports"].push_back({
          {"id", port.id}, {"node", port.node_id},
          {"side", side}, {"x", port.local_x}, {"y", port.local_y},
          {"output", port.output}});
    }
    for (const auto &edge : committed_.edges) {
      document["edges"].push_back({
          {"id", edge.id},
          {"source", edge.source_port},
          {"target", edge.target_port}});
    }
    for (const auto &group : committed_.groups) {
      Json record = {
          {"id", group.id},
          {"parent", group.parent_group_id},
          {"x", group.x}, {"y", group.y},
          {"width", group.width}, {"height", group.height},
          {"collapsed", group.collapsed},
          {"members", group.member_node_ids},
          {"proxy_ports", Json::array()}};
      for (const auto &proxy : group.proxy_ports) {
        const char *side = side_name(proxy.side);
        if (!side) return invalid("unmeasured proxy port cannot be serialized");
        record["proxy_ports"].push_back({
            {"id", proxy.id}, {"original", proxy.original_port_id},
            {"side", side},
            {"x", proxy.local_x}, {"y", proxy.local_y}});
      }
      document["groups"].push_back(std::move(record));
    }
    auto encoded = document.dump();
    if (encoded.size() > kMaxDocumentBytes)
      return {GraphEditError::CapacityExceeded,
              "serialized logical graph exceeds one-megabyte budget"};
    out = std::move(encoded); // Do not modify the caller's output on failure.
    return {};
  } catch (const std::exception &error) {
    return invalid(std::string("graph JSON encode failed: ") + error.what());
  }
}

GraphEditStatus GraphEditDocument::load_json(std::string_view bytes) {
  if (bytes.empty()) return invalid("empty logical graph JSON");
  if (bytes.size() > kMaxDocumentBytes)
    return {GraphEditError::CapacityExceeded,
            "logical graph JSON exceeds one-megabyte budget"};
  // Parse to a new snapshot and make all error checks *before* initialize,
  // which independently validates and routes the one canonical graph.
  GraphEditSnapshot candidate;
  try {
    const auto root = Json::parse(bytes.begin(), bytes.end());
    if (!keys(root, {"format", "version", "nodes", "ports", "edges", "groups"}) ||
        !root.at("format").is_string() ||
        root.at("format").get<std::string>() != "stun.graphcanvas.logical" ||
        !root.at("version").is_number_integer() ||
        root.at("version").get<int>() != 1)
      return invalid("unknown or unsupported logical graph schema");
    if (!fixed_array(root, "nodes", kMaxNodes) ||
        !fixed_array(root, "ports", kMaxPorts) ||
        !fixed_array(root, "edges", kMaxEdges) ||
        !fixed_array(root, "groups", kMaxGroups))
      return invalid("logical graph collection exceeds native schema bounds");

    for (const auto &item : root.at("nodes")) {
      if (!keys(item, {"id", "x", "y", "width", "height", "locked"}) ||
          !item.at("locked").is_boolean())
        return invalid("malformed original node");
      GraphEditNode node;
      if (!scalar_fields(item, node.id, node.x, node.y,
                         node.width, node.height))
        return invalid("nonfinite or malformed original node");
      node.locked = item.at("locked").get<bool>();
      candidate.nodes.push_back(std::move(node));
    }
    for (const auto &item : root.at("ports")) {
      if (!keys(item, {"id", "node", "side", "x", "y", "output"}) ||
          !item.at("output").is_boolean())
        return invalid("malformed original port");
      GraphEditPort port;
      std::string direction;
      if (!text(item.at("id"), port.id) ||
          !text(item.at("node"), port.node_id) ||
          !text(item.at("side"), direction) ||
          !parse_side(direction, port.side) ||
          !number(item.at("x"), port.local_x) ||
          !number(item.at("y"), port.local_y))
        return invalid("invalid measured original port");
      port.output = item.at("output").get<bool>();
      candidate.ports.push_back(std::move(port));
    }
    for (const auto &item : root.at("edges")) {
      if (!keys(item, {"id", "source", "target"}))
        return invalid("malformed logical edge");
      GraphEditEdge edge;
      if (!text(item.at("id"), edge.id) ||
          !text(item.at("source"), edge.source_port) ||
          !text(item.at("target"), edge.target_port))
        return invalid("invalid logical edge identity");
      candidate.edges.push_back(std::move(edge));
    }
    for (const auto &item : root.at("groups")) {
      if (!keys(item, {"id", "parent", "x", "y", "width", "height",
                       "collapsed", "members", "proxy_ports"}) ||
          !item.at("collapsed").is_boolean() ||
          !item.at("members").is_array() ||
          item.at("members").size() > kMaxNodes ||
          !item.at("proxy_ports").is_array() ||
          item.at("proxy_ports").size() > kMaxPorts)
        return invalid("malformed compound group");
      GraphEditGroup group;
      if (!scalar_fields(item, group.id, group.x, group.y,
                         group.width, group.height) ||
          !text(item.at("parent"), group.parent_group_id))
        return invalid("nonfinite or malformed group transform");
      group.collapsed = item.at("collapsed").get<bool>();
      for (const auto &member : item.at("members")) {
        std::string id;
        if (!text(member, id)) return invalid("invalid group member identity");
        group.member_node_ids.push_back(std::move(id));
      }
      for (const auto &entry : item.at("proxy_ports")) {
        if (!keys(entry, {"id", "original", "side", "x", "y"}))
          return invalid("malformed group proxy port");
        GraphEditGroupProxyPort port;
        std::string direction;
        if (!text(entry.at("id"), port.id) ||
            !text(entry.at("original"), port.original_port_id) ||
            !text(entry.at("side"), direction) ||
            !parse_side(direction, port.side) ||
            !number(entry.at("x"), port.local_x) ||
            !number(entry.at("y"), port.local_y))
          return invalid("invalid measured group proxy port");
        group.proxy_ports.push_back(std::move(port));
      }
      candidate.groups.push_back(std::move(group));
    }
  } catch (const std::exception &error) {
    return invalid(std::string("graph JSON decode failed: ") + error.what());
  }
  // initialize() atomically performs native GraphOrthogonal validation,
  // rebuilds the dual-axis route indexes, resets the old history only on
  // success and advances revision monotonically across document replacement.
  return initialize(std::move(candidate));
}

} // namespace charts::ui
