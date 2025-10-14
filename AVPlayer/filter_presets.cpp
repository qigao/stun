#include "filter_presets.h"

const std::vector<FilterPreset> FilterPresets::presets_ = {
	// Transform
	{"None", "No filter", "", "Transform"},
	{"Horizontal Flip", "Mirror horizontally", "hflip", "Transform"},
	{"Vertical Flip", "Mirror vertically", "vflip", "Transform"},
	{"Rotate 180°", "Rotate 180 degrees", "hflip,vflip", "Transform"},
	// Note: 90° rotation filters disabled - they change dimensions and may crash
	// {"Rotate 90° CW", "Rotate 90 degrees clockwise", "transpose=1", "Transform"},
	// {"Rotate 90° CCW", "Rotate 90 degrees counter-clockwise", "transpose=2", "Transform"},
	
	// Color Adjustments (using curves instead of eq for compatibility)
	{"Brightness +20%", "Increase brightness", "curves=all='0/0.2 1/1'", "Color"},
	{"Brightness -20%", "Decrease brightness", "curves=all='0/0 1/0.8'", "Color"},
	{"Contrast +30%", "Increase contrast", "curves=all='0/0 0.5/0.4 1/1'", "Color"},
	{"High Contrast", "Dramatic contrast", "curves=strong_contrast", "Color"},
	{"Saturation +50%", "More vivid colors", "hue=s=1.5", "Color"},
	{"Saturation -50%", "Muted colors", "hue=s=0.5", "Color"},
	{"Grayscale", "Remove all color", "hue=s=0", "Color"},
	{"Sepia", "Vintage sepia tone", "colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131", "Color"},
	{"Negative", "Invert colors", "negate", "Color"},
	{"Warm Tone", "Warmer colors", "colortemperature=temperature=7000", "Color"},
	{"Cool Tone", "Cooler colors", "colortemperature=temperature=3000", "Color"},
	
	// Effects
	{"Blur Light", "Slight blur", "boxblur=2:1", "Effect"},
	{"Blur Medium", "Medium blur", "boxblur=5:2", "Effect"},
	{"Blur Heavy", "Strong blur", "boxblur=10:3", "Effect"},
	{"Sharpen", "Enhance sharpness", "unsharp=5:5:1.0:5:5:0.0", "Effect"},
	{"Sharpen Strong", "Strong sharpening", "unsharp=7:7:2.0:7:7:0.0", "Effect"},
	{"Edge Detect", "Show edges only", "edgedetect", "Effect"},
	{"Emboss", "3D emboss effect", "convolution='0 -1 0 -1 4 -1 0 -1 0:0 -1 0 -1 4 -1 0 -1 0:0 -1 0 -1 4 -1 0 -1 0:0 -1 0 -1 4 -1 0 -1 0'", "Effect"},
	
	// Noise & Quality
	{"Denoise Light", "Remove slight noise", "hqdn3d=1.5:1.5:6:6", "Quality"},
	{"Denoise Medium", "Remove moderate noise", "hqdn3d=4:3:6:4.5", "Quality"},
	{"Denoise Strong", "Remove heavy noise", "hqdn3d=8:6:12:9", "Quality"},
	{"Deinterlace", "Remove interlacing", "yadif", "Quality"},
	{"Deflicker", "Stabilize brightness", "deflicker", "Quality"},
	
	// 3D & Stereo
	{"Stereo3D: Side-by-Side", "Convert SBS 3D to anaglyph", "stereo3d=sbsl:arcd", "3D"},
	{"Stereo3D: Top-Bottom", "Convert top-bottom 3D to anaglyph", "stereo3d=abl:arcd", "3D"},
	{"Stereo3D: Anaglyph Red-Cyan", "Create red-cyan 3D", "stereo3d=al:arcd", "3D"},
	{"Stereo3D: Anaglyph Green-Magenta", "Create green-magenta 3D", "stereo3d=al:agmc", "3D"},
	
	// Artistic
	{"Cartoon", "Cartoon-like effect", "edgedetect=mode=colormix:high=0.1,negate", "Artistic"},
	{"Oil Painting", "Oil painting effect", "boxblur=5:1,unsharp=5:5:1.5", "Artistic"},
	{"Vintage", "Old film look", "curves=vintage,vignette", "Artistic"},
	{"Vignette", "Darken edges", "vignette", "Artistic"},
	{"Pixelate", "Pixelated look", "scale=iw/8:ih/8,scale=iw*8:ih*8:flags=neighbor", "Artistic"},
	
	// Corrections
	{"Auto Levels", "Automatic color correction", "eq=eval=frame", "Correction"},
	{"White Balance Warm", "Warmer color temperature", "eq=gamma_r=0.9:gamma_b=1.1", "Correction"},
	{"White Balance Cool", "Cooler color temperature", "eq=gamma_r=1.1:gamma_b=0.9", "Correction"},
	{"Stabilize", "Video stabilization", "deshake", "Correction"},
	
	// Performance
	{"Fast Deinterlace", "Quick deinterlacing", "bwdif", "Performance"},
	{"Scale 720p", "Downscale to 720p", "scale=1280:720", "Performance"},
	{"Scale 480p", "Downscale to 480p", "scale=854:480", "Performance"},
};

std::vector<FilterPreset> FilterPresets::get_all() {
	return presets_;
}

std::vector<FilterPreset> FilterPresets::get_by_category(const std::string& category) {
	std::vector<FilterPreset> result;
	for (const auto& preset : presets_) {
		if (preset.category == category) {
			result.push_back(preset);
		}
	}
	return result;
}

std::vector<std::string> FilterPresets::get_categories() {
	std::vector<std::string> categories;
	std::map<std::string, bool> seen;
	
	for (const auto& preset : presets_) {
		if (!seen[preset.category]) {
			categories.push_back(preset.category);
			seen[preset.category] = true;
		}
	}
	
	return categories;
}

const FilterPreset* FilterPresets::find_by_name(const std::string& name) {
	for (const auto& preset : presets_) {
		if (preset.name == name) {
			return &preset;
		}
	}
	return nullptr;
}
