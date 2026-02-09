#define CATCH_CONFIG_MAIN
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cmath>
#include "pdfgen.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// External data from rgb.c
extern "C" {
    extern unsigned char data_rgb[];
}

// Helper to read file to buffer (replacing the python generated array)
std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        // Try relative to build dir or source? 
        // We assume the test is run with correct CWD or data is copied next to executable
        return {};
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    if (file.read(reinterpret_cast<char*>(buffer.data()), size)) return buffer;
    return {};
}

TEST_CASE("PDF Generation Full Flow", "[pdfgen]") {
    struct pdf_info info = {
        "My software", // creator
        "My software", // producer
        "My document", // title
        "My name",     // author
        "My subject",  // subject
        "Today"        // date
    };

    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    REQUIRE(pdf != nullptr);

    SECTION("Validate Dimensions") {
        CHECK(pdf_width(pdf) == PDF_A4_WIDTH);
        CHECK(pdf_height(pdf) == PDF_A4_HEIGHT);
    }

    SECTION("Check Font Width") {
        float width;
        int err = pdf_get_font_text_width(pdf, "Times-BoldItalic", "foo", 14, &width);
        CHECK(err >= 0);
        CHECK(width >= 18.0f);
    }

    SECTION("Operations before adding page should fail") {
        // These calls should fail, since we haven't added a page yet
        CHECK(pdf_add_image_file(pdf, NULL, 10, 10, 20, 30, "data/teapot.ppm") < 0);
        CHECK(pdf_add_text(pdf, NULL, "Page One", 10, 20, 30, PDF_RGB(0xff, 0, 0)) < 0);
        
        int err_code;
        CHECK(pdf_get_err(pdf, &err_code) != nullptr);
        pdf_clear_err(pdf);
    }

    SECTION("Add Pages and Content") {
        // Page 1
        CHECK(pdf_set_font(pdf, "Times-BoldItalic") >= 0);
        REQUIRE(pdf_append_page(pdf) != nullptr);
        struct pdf_object *first_page = pdf_get_page(pdf, 1);
        REQUIRE(first_page != nullptr);

        float height;
        CHECK(pdf_add_text_wrap(
            pdf, NULL,
            "This is a great big long string that I hope will wrap properly "
            "around several lines.\nThere are some odd length "
            "linesthatincludelongwords to check the justification. "
            "I've put some embedded line breaks in to "
            "see how it copes with them. Hopefully it all works properly.\n\n\n"
            "We even include multiple breaks\n"
            "And special stuff \xE2\x82\xAC\xC3\x9C\xC5\xBD\xC5\xBE\xC5\xA0\xC5\xA1\xC3\x81 that \xC3\xA1\xC3\xBC\xC3\xB6\xC3\xA4 should \xC3\x84\xC3\x9C\xC3\x96\xC3\x9F\xE2\x80\x94 \xE2\x80\x9C\xE2\x80\x9D\xE2\x80\x98\xE2\x80\x99 break\n"
            "------------------------------------------------\n"
            "thisisanenourmouswordthatwillneverfitandwillhavetobecut",
            16, 60, 800, 0, PDF_RGB(0, 0, 0), 300, PDF_ALIGN_JUSTIFY, &height) >= 0);

        CHECK(pdf_add_rectangle(pdf, NULL, 58, 800 + 16, 304, -height, 2, PDF_RGB(0, 0, 0)) >= 0);

        // Images from files
        CHECK(pdf_add_image_file(pdf, NULL, 10, 10, 20, 30, "data/teapot.ppm") >= 0);
        CHECK(pdf_add_image_file(pdf, NULL, 50, 10, 30, 30, "data/coal.png") >= 0);
        // Assuming other images exist...
        
        // Image from memory (Penguin)
        auto penguinData = readFile("data/penguin.jpg");
        if (!penguinData.empty()) {
            CHECK(pdf_add_image_data(pdf, NULL, 100, 500, 50, 150, penguinData.data(), penguinData.size()) >= 0);
        } else {
            WARN("Skipping penguin image test - file not found");
        }

        // Shapes and Paths
        CHECK(pdf_add_text(pdf, NULL, "Page One", 10, 20, 30, PDF_RGB(0xff, 0, 0)) >= 0);
        CHECK(pdf_add_line(pdf, NULL, 10, 24, 100, 24, 4, PDF_RGB(0xff, 0, 0)) >= 0);
        
        // Cubic Bezier
        CHECK(pdf_add_cubic_bezier(pdf, NULL, 10, 100, 150, 100, 20, 30, 60, 30, 4,
                          PDF_RGB(0, 0xff, 0)) >= 0);

        // Quadratic Bezier
        CHECK(pdf_add_quadratic_bezier(pdf, NULL, 10, 140, 150, 140, 50, 160, 4,
                              PDF_RGB(0, 0, 0xff)) >= 0);

        struct pdf_path_operation operations[] = {
            {'m', 100, 100},
            {'l', 130, 100},
            {'c', 150, 150, 100, 100, 130, 130},
            {'l', 150, 120},
            {'h'}
        };
        CHECK(pdf_add_custom_path(pdf, NULL, operations, 5, 1, PDF_RGB(0xff, 0, 0), PDF_ARGB(0x80, 0xff, 0, 0)) >= 0);

        // Circle and Ellipse
        CHECK(pdf_add_circle(pdf, NULL, 100, 240, 50, 5, PDF_RGB(0xff, 0, 0),
                    PDF_TRANSPARENT) >= 0);
        CHECK(pdf_add_ellipse(pdf, NULL, 100, 240, 40, 30, 2, PDF_RGB(0xff, 0xff, 0),
                    PDF_RGB(0, 0, 0)) >= 0);

        // Filled Rectangle with Transparent Fill
        CHECK(pdf_add_filled_rectangle(pdf, NULL, 150, 450, 100, 100, 4,
                              PDF_RGB(0, 0xff, 0), PDF_TRANSPARENT) >= 0);

        // Text Rotate
        CHECK(pdf_add_text_rotate(pdf, NULL, "This should be transparent", 20, 160, 500,
                        M_PI / 4, PDF_ARGB(0x80, 0, 0, 0)) >= 0);

        // Polygons
        float p1X[] = {200, 200, 300, 300};
        float p1Y[] = {200, 300, 200, 300};
        CHECK(pdf_add_polygon(pdf, NULL, p1X, p1Y, 4, 4, PDF_RGB(0xaa, 0xff, 0xee)) >= 0);

        float p2X[] = {400, 400, 500, 500};
        float p2Y[] = {400, 500, 400, 500};
        CHECK(pdf_add_filled_polygon(pdf, NULL, p2X, p2Y, 4, 4,
                            PDF_RGB(0xff, 0x77, 0x77)) >= 0);

        // Bookmarks
        CHECK(pdf_add_bookmark(pdf, NULL, -1, "First page") >= 0);


        // Page 2
        REQUIRE(pdf_append_page(pdf) != nullptr);
        struct pdf_object *second_page = pdf_get_page(pdf, 2);
        CHECK(pdf_add_text(pdf, second_page, "Page Two", 10, 20, 30, PDF_RGB(0, 0, 0)) >= 0);

        // More Bookmarks
        int bm = pdf_add_bookmark(pdf, NULL, -1, "Another Page");
        CHECK(bm >= 0);
        CHECK(pdf_add_bookmark(pdf, NULL, bm, "Another Page again") >= 0);


        // Barcodes Page
        struct pdf_object *page3 = pdf_append_page(pdf);
        REQUIRE(page3 != nullptr);
        
        // Code 39
        CHECK(pdf_add_barcode(pdf, NULL, PDF_BARCODE_39, PDF_MM_TO_POINT(20), 
                     PDF_MM_TO_POINT(240), PDF_MM_TO_POINT(60), 
                     PDF_MM_TO_POINT(20), "CODE39", PDF_RGB(0, 0, 0)) >= 0);

        // Code 128A
        CHECK(pdf_add_barcode(pdf, NULL, PDF_BARCODE_128A, PDF_MM_TO_POINT(20),
                     PDF_MM_TO_POINT(210), PDF_MM_TO_POINT(60),
                     PDF_MM_TO_POINT(20), "Code128", PDF_RGB(0, 0, 0)) >= 0);

        // EAN13
        CHECK(pdf_add_barcode(pdf, NULL, PDF_BARCODE_EAN13, PDF_MM_TO_POINT(20),
                     PDF_MM_TO_POINT(160), PDF_MM_TO_POINT(60),
                     PDF_MM_TO_POINT(40), "4003994155486", PDF_BLACK) >= 0);

        // UPCA
         CHECK(pdf_add_barcode(pdf, NULL, PDF_BARCODE_UPCA, PDF_MM_TO_POINT(100),
                     PDF_MM_TO_POINT(160), PDF_MM_TO_POINT(60),
                     PDF_MM_TO_POINT(80), "003994155480", PDF_BLACK) >= 0);

        // EAN8
        CHECK(pdf_add_barcode(pdf, NULL, PDF_BARCODE_EAN8, PDF_MM_TO_POINT(20),
                     PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(60),
                     PDF_MM_TO_POINT(40), "95012346", PDF_BLACK) >= 0);

        // UPCE
        CHECK(pdf_add_barcode(pdf, NULL, PDF_BARCODE_UPCE, PDF_MM_TO_POINT(100),
                     PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(60),
                     PDF_MM_TO_POINT(80), "012345000058", PDF_BLACK) >= 0);


        // Page 4 (A3)
        REQUIRE(pdf_append_page(pdf) != nullptr);
        CHECK(pdf_page_set_size(pdf, NULL, PDF_A3_HEIGHT, PDF_A3_WIDTH) >= 0);
        CHECK(pdf_add_bookmark(pdf, NULL, -1, "Last Page") >= 0);

        // Links
        const char* a3_text = "This is an A3 landscape page";
        CHECK(pdf_add_text(pdf, NULL, a3_text, 10, 20, 30, PDF_RGB(0xff, 0, 0)) >= 0);
        float width_a3;
        if (pdf_get_font_text_width(pdf, NULL, a3_text, 10, &width_a3) == 0) {
            CHECK(pdf_add_link(pdf, NULL, 20, 30, width_a3, 10, first_page, 0,
                      pdf_page_height(first_page) / 2) >= 0);
        }

        CHECK(pdf_add_rgb24(pdf, NULL, 72, 72, 288, 144, data_rgb, 16, 8) >= 0);

        // Save
        CHECK(pdf_save(pdf, "output.pdf") >= 0);
    }

    pdf_destroy(pdf);
}

TEST_CASE("Massive File Generation", "[pdfgen][performance]") {
    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, NULL);
    REQUIRE(pdf != nullptr);

    int pagecount = 100; // Default from massive-file.c was 10, but let's test more
    
    pdf_set_font(pdf, "Times-Roman");
    for (int i = 0; i < pagecount; i++) {
        char str[64];
        REQUIRE(pdf_append_page(pdf) != nullptr);
        sprintf(str, "page %d", i);
        CHECK(pdf_add_text(pdf, NULL, str, 12, 50, 20, PDF_BLACK) >= 0);

        // We can skip the penguin image check if file doesn't exist to avoid noise,
        // but for a "massive" test we just care about page/object count.
        pdf_add_image_file(pdf, NULL, 100, 500, 50, 150, "data/penguin.jpg");
    }

    char filename[128];
    sprintf(filename, "massive-%d.pdf", pagecount);
    CHECK(pdf_save(pdf, filename) >= 0);
    
    pdf_destroy(pdf);
    
    // Cleanup generated file
    // std::remove(filename); 
}
