/**
 * \file filter_presets.h
 * \brief Predefined video filter presets
 */

#pragma once
#include <string>
#include <vector>
#include <map>

/**
 * \struct FilterPreset
 * \brief A predefined video filter configuration
 */
struct FilterPreset {
	std::string name;           ///< Display name
	std::string description;    ///< Short description
	std::string filter_string;  ///< FFmpeg filter string
	std::string category;       ///< Category (Transform, Color, Effect, 3D, etc.)
};

/**
 * \class FilterPresets
 * \brief Collection of predefined video filters
 */
class FilterPresets {
public:
	/**
	 * \brief Gets all available filter presets
	 * \return Vector of filter presets
	 */
	static std::vector<FilterPreset> get_all();
	
	/**
	 * \brief Gets filter presets by category
	 * \param category Category name
	 * \return Vector of filter presets in that category
	 */
	static std::vector<FilterPreset> get_by_category(const std::string& category);
	
	/**
	 * \brief Gets all available categories
	 * \return Vector of category names
	 */
	static std::vector<std::string> get_categories();
	
	/**
	 * \brief Finds a preset by name
	 * \param name Preset name
	 * \return Pointer to preset or nullptr if not found
	 */
	static const FilterPreset* find_by_name(const std::string& name);

private:
	static const std::vector<FilterPreset> presets_;
};
