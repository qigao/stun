#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct Bounds {
  float x, y, width, height;
};

/**
 * @brief Spatial hash for fast hit testing
 *
 * Divides 2D space into a grid and indexes elements by which cells they occupy.
 * This enables O(1) hit testing by only checking elements in the cell containing
 * the query point, instead of checking all elements O(n).
 */
class SpatialHash {
public:
  explicit SpatialHash(float cell_size = 100.0f) : m_cell_size(cell_size) {}

  /**
   * @brief Insert an element with its bounding box
   */
  void insert(const std::string& id, const Bounds& bounds);

  /**
   * @brief Remove an element from the spatial hash
   */
  void remove(const std::string& id);

  /**
   * @brief Query which elements could contain the point (x, y)
   * @return List of element IDs in the same cell as the query point
   */
  std::vector<std::string> query(float x, float y) const;

  /**
   * @brief Clear all elements
   */
  void clear();

  /**
   * @brief Update an element's position (remove + insert)
   */
  void update(const std::string& id, const Bounds& bounds);

private:
  struct CellKey {
    int grid_x;
    int grid_y;

    bool operator==(const CellKey& other) const {
      return grid_x == other.grid_x && grid_y == other.grid_y;
    }
  };

  struct CellKeyHash {
    std::size_t operator()(const CellKey& key) const {
      // Cantor pairing function for unique hash
      return (key.grid_x + key.grid_y) * (key.grid_x + key.grid_y + 1) / 2 + key.grid_y;
    }
  };

  CellKey getCellKey(float x, float y) const;
  std::vector<CellKey> getCellKeys(const Bounds& bounds) const;

  float m_cell_size;
  std::unordered_map<CellKey, std::vector<std::string>, CellKeyHash> m_grid;
  std::unordered_map<std::string, std::vector<CellKey>> m_element_cells;
};

} // namespace flexui
