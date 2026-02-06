#include <flexmaid.h>
#include <turbo_parser.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>

using namespace flex::modules::flexmaid;

std::string read_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void write_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create file: " + filename);
    }
    file << content;
}

Theme get_theme(int64_t idx) {
    switch (idx) {
        case 1: return Theme::dark();
        case 2: return Theme::modern();
        case 3: return Theme::official();
        default: return Theme::light();
    }
}

int main(int argc, char* argv[]) {
    turbo_cmd_parser_t* cmd = turbo_cmd_create("flexmaid", "1.0.0");
    
    // Required arguments with short options
    char* input_file = nullptr;
    char* output_file = nullptr;
    turbo_cmd_add_string(cmd, &input_file, "input", "i", "Input Mermaid diagram file (.mmd)");
    turbo_cmd_set_required(cmd, turbo_cmd_last_index(cmd));
    turbo_cmd_add_string(cmd, &output_file, "output", "o", "Output SVG file");
    turbo_cmd_set_required(cmd, turbo_cmd_last_index(cmd));
    
    // Optional arguments
    int64_t theme_idx = 0;
    bool verbose = false;
    bool benchmark = false;
    
    turbo_cmd_enum_t themes[] = {
        {"light",    "Light theme (default)", 0},
        {"dark",     "Dark theme",            1},
        {"modern",   "Modern theme",          2},
        {"official", "Official Mermaid theme", 3},
    };
    turbo_cmd_add_enum(cmd, &theme_idx, "theme", "t", "Theme name", themes, 4);
    turbo_cmd_add_flag(cmd, &verbose, "verbose", "v", "Print detailed information");
    turbo_cmd_add_flag(cmd, &benchmark, "benchmark", "b", "Show performance metrics");
    
    turbo_cmd_parse(cmd, argc, argv, true);
    
    try {
        // Read input
        if (verbose) std::cout << "Reading: " << input_file << "\n";
        
        auto t0 = std::chrono::high_resolution_clock::now();
        std::string mermaid_code = read_file(input_file);
        auto t1 = std::chrono::high_resolution_clock::now();
        
        if (verbose) std::cout << "Input size: " << mermaid_code.size() << " bytes\n";
        
        // Initialize FlexMaid
        FlexMaid maid;
        maid.set_theme(get_theme(theme_idx));
        
        const char* theme_names[] = {"light", "dark", "modern", "official"};
        if (verbose) std::cout << "Theme: " << theme_names[theme_idx] << "\n";
        
        // Parse
        auto t2 = std::chrono::high_resolution_clock::now();
        auto result = maid.parse(mermaid_code);
        auto t3 = std::chrono::high_resolution_clock::now();
        
        if (!result.success) {
            std::cerr << "Parse error: " << result.get_error() << "\n";
            turbo_cmd_destroy(cmd);
            return 1;
        }
        
        // Render
        auto t4 = std::chrono::high_resolution_clock::now();
        std::string svg = maid.mermaid_to_svg(mermaid_code);
        auto t5 = std::chrono::high_resolution_clock::now();
        
        // Write output
        auto t6 = std::chrono::high_resolution_clock::now();
        write_file(output_file, svg);
        auto t7 = std::chrono::high_resolution_clock::now();
        
        std::cout << "✓ Generated " << output_file << "\n";
        
        // Benchmark
        if (benchmark) {
            auto us = [](auto a, auto b) { 
                return std::chrono::duration_cast<std::chrono::microseconds>(b - a).count(); 
            };
            std::cout << "\nPerformance:\n";
            std::cout << "  Read:   " << us(t0, t1) / 1000.0 << " ms\n";
            std::cout << "  Parse:  " << us(t2, t3) / 1000.0 << " ms\n";
            std::cout << "  Render: " << us(t4, t5) / 1000.0 << " ms\n";
            std::cout << "  Write:  " << us(t6, t7) / 1000.0 << " ms\n";
            std::cout << "  Total:  " << us(t0, t7) / 1000.0 << " ms\n";
            std::cout << "Output: " << svg.size() << " bytes\n";
        }
        
        turbo_cmd_destroy(cmd);
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        turbo_cmd_destroy(cmd);
        return 1;
    }
}
