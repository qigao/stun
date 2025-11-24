#include "flexui/spatial_hash.h"

#include <algorithm>
#include <cmath>

namespace flexui {

SpatialHash::CellKey SpatialHash::getCellKey(float x, float y) const {
  return CellKey{
    static_cast<int>(std::floor(x / m_cell_size)),
    static_cast<int>(std::floor(y / m_cell_size))
  };
}

std::vector<SpatialHash::CellKey> SpatialHash::getCellKeys(const Bounds& bounds) const {
  std::vector<CellKey> keys;

  int min_x = static_cast<int>(std::floor(bounds.x / m_cell_size));
  int max_x = static_cast<int>(std::floor((bounds.x + bounds.width) / m_cell_size));
  int min_y = static_cast<int>(std::floor(bounds.y / m_cell_size));
  int max_y = static_cast<int>(std::floor((bounds.y + bounds.height) / m_cell_size));

  for (int gx = min_x; gx <= max_x; ++gx) {
    for (int gy = min_y; gy <= max_y; ++gy) {
      keys.push_back(CellKey{gx, gy});
    }
  }

  return keys;
}

void SpatialHash::insert(const std::string& id, const Bounds& bounds) {
  // Remove old entries if element already exists
  remove(id);

  // Get all cells this element spans
  auto cell_keys = getCellKeys(bounds);

  // Insert into each cell
  for (const auto& key : cell_keys) {
    m_grid[key].push_back(id);
  }

  // Remember which cells this element is in
  m_element_cells[id] = cell_keys;
}

void SpatialHash::remove(const std::string& id) {
  auto it = m_element_cells.find(id);
  if (it == m_element_cells.end()) {
    return;
  }

  // Remove from all cells
  for (const auto& key : it->second) {
    auto& cell = m_grid[key];
    cell.erase(std::remove(cell.begin(), cell.end(), id), cell.end());

    // Remove empty cells to save memory
    if (cell.empty()) {
      m_grid.erase(key);
    }
  }

  m_element_cells.erase(it);
}

std::vector<std::string> SpatialHash::query(float x, float y) const {
  auto key = getCellKey(x, y);
  auto it = m_grid.find(key);

  if (it == m_grid.end()) {
    return {};
  }

  return it->second;
}

void SpatialHash::clear() {
  m_grid.clear();
  m_element_cells.clear();
}

void SpatialHash::update(const std::string& id, const Bounds& bounds) {
  insert(id, bounds);
}

} // namespace flexui
