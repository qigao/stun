/*
 * Flex Compiler Tool
 *
 * Command-line tool to compile .flex files to .flexb binary format.
 *
 * Usage:
 *   flex-compiler input.flex -o output.flexb [--compress] [--stats]
 *
 * Current Features:
 * - ✅ Parse .flex source
 * - ✅ Serialize to .flexb binary
 * - ✅ String deduplication
 * - ✅ CRC32 integrity check
 * - ✅ Compilation statistics
 * - ✅ zstd compression (--compress)
 *
 * Future Features (not yet implemented):
 * - ❌ Encryption (--encrypt)
 * - ❌ Optimization (--optimize)
 * - ❌ Asset embedding (--embed-assets)
 */

#include "flex/binary/compiler.h"
#include <cxxopts.hpp>
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    cxxopts::Options options("flex-compiler", "Flex Compiler - Compile .flex to .flexb binary format");

    options.add_options()
        ("i,input", "Input .flex file", cxxopts::value<std::string>())
        ("o,output", "Output .flexb file", cxxopts::value<std::string>())
        ("compress", "Enable zstd compression")
        ("stats", "Show compilation statistics")
        ("h,help", "Show this help message")
    ;

    options.parse_positional({"input"});
    options.positional_help("<input.flex>");

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << "\n";
            std::cout << "\nExamples:\n";
            std::cout << "  flex-compiler app.flex -o app.flexb\n";
            std::cout << "  flex-compiler app.flex -o app.flexb --compress --stats\n";
            return 0;
        }

        if (!result.count("input")) {
            std::cerr << "Error: No input file specified\n\n";
            std::cout << options.help() << "\n";
            return 1;
        }

        if (!result.count("output")) {
            std::cerr << "Error: No output file specified (use -o)\n";
            return 1;
        }

        std::string input_file = result["input"].as<std::string>();
        std::string output_file = result["output"].as<std::string>();
        bool compress = result.count("compress") > 0;
        bool show_stats = result.count("stats") > 0;

        // Compile
        std::cout << "Compiling: " << input_file << " → " << output_file;
        if (compress) {
            std::cout << " (compressed)";
        }
        std::cout << "\n";

        flex::binary::BinaryCompiler compiler;
        compiler.set_compress(compress);
        bool success = compiler.compile_to_file(input_file.c_str(), output_file.c_str());

        if (!success) {
            std::cerr << "Error: Compilation failed\n";
            std::cerr << compiler.error_message() << "\n";
            return 1;
        }

        // Show statistics
        if (show_stats) {
            const auto& stats = compiler.stats();
            std::cout << "\nCompilation Statistics:\n";
            std::cout << "  Source size:  " << stats.source_size << " bytes\n";
            std::cout << "  Binary size:  " << stats.binary_size << " bytes\n";
            std::cout << "  Nodes:        " << stats.node_count << "\n";
            std::cout << "  Strings:      " << stats.string_count << "\n";
            std::cout << "  Timelines:    " << stats.timeline_count << "\n";

            if (stats.source_size > 0) {
                float reduction = 100.0f * (1.0f - (float)stats.binary_size / (float)stats.source_size);
                std::cout << "  Reduction:    " << reduction << "%\n";
            }
        }

        std::cout << "Success! Output: " << output_file << "\n";
        return 0;

    } catch (const cxxopts::exceptions::exception& e) {
        std::cerr << "Error parsing options: " << e.what() << "\n";
        return 1;
    }
}
