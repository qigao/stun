#pragma once

#include <lexbor/css/css.h>
#include <lexbor/html/html.h>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

namespace whiteboard {
namespace lexbor {

/**
 * @brief RAII wrapper for Lexbor CSS parser
 */
class CSSParser {
public:
    CSSParser() {
        parser_ = lxb_css_parser_create();
        if (!parser_) {
            throw std::runtime_error("Failed to create CSS parser");
        }
        
        lxb_status_t status = lxb_css_parser_init(parser_, nullptr);
        if (status != LXB_STATUS_OK) {
            lxb_css_parser_destroy(parser_, true);
            throw std::runtime_error("Failed to initialize CSS parser");
        }
    }
    
    ~CSSParser() {
        if (parser_) {
            lxb_css_parser_destroy(parser_, true);
        }
    }
    
    // Non-copyable
    CSSParser(const CSSParser&) = delete;
    CSSParser& operator=(const CSSParser&) = delete;
    
    // Movable
    CSSParser(CSSParser&& other) noexcept : parser_(other.parser_) {
        other.parser_ = nullptr;
    }
    
    CSSParser& operator=(CSSParser&& other) noexcept {
        if (this != &other) {
            if (parser_) {
                lxb_css_parser_destroy(parser_, true);
            }
            parser_ = other.parser_;
            other.parser_ = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Parse CSS string
     * @param css CSS text to parse
     * @return Parsed stylesheet (caller owns)
     */
    lxb_css_stylesheet_t* parse(const std::string& css) {
        if (!parser_) {
            throw std::runtime_error("Parser not initialized");
        }
        
        lxb_css_stylesheet_t* sheet = lxb_css_stylesheet_parse(
            parser_,
            reinterpret_cast<const lxb_char_t*>(css.c_str()),
            css.size()
        );
        
        if (!sheet) {
            throw std::runtime_error("Failed to parse CSS");
        }
        
        return sheet;
    }
    
    lxb_css_parser_t* get() { return parser_; }
    const lxb_css_parser_t* get() const { return parser_; }
    
private:
    lxb_css_parser_t* parser_;
};

/**
 * @brief RAII wrapper for Lexbor CSS stylesheet
 */
class CSSStyleSheet {
public:
    explicit CSSStyleSheet(lxb_css_stylesheet_t* sheet) : sheet_(sheet) {}
    
    ~CSSStyleSheet() {
        if (sheet_) {
            lxb_css_stylesheet_destroy(sheet_, true);
        }
    }
    
    // Non-copyable
    CSSStyleSheet(const CSSStyleSheet&) = delete;
    CSSStyleSheet& operator=(const CSSStyleSheet&) = delete;
    
    // Movable
    CSSStyleSheet(CSSStyleSheet&& other) noexcept : sheet_(other.sheet_) {
        other.sheet_ = nullptr;
    }
    
    CSSStyleSheet& operator=(CSSStyleSheet&& other) noexcept {
        if (this != &other) {
            if (sheet_) {
                lxb_css_stylesheet_destroy(sheet_, true);
            }
            sheet_ = other.sheet_;
            other.sheet_ = nullptr;
        }
        return *this;
    }
    
    // Note: Lexbor 2.5.0 doesn't expose rules directly via public API
    // Rules are accessed internally during selector matching
    
    lxb_css_stylesheet_t* get() { return sheet_; }
    const lxb_css_stylesheet_t* get() const { return sheet_; }
    
    lxb_css_stylesheet_t* release() {
        lxb_css_stylesheet_t* temp = sheet_;
        sheet_ = nullptr;
        return temp;
    }
    
private:
    lxb_css_stylesheet_t* sheet_;
};

/**
 * @brief RAII wrapper for Lexbor HTML parser
 */
class HTMLParser {
public:
    HTMLParser() {
        document_ = lxb_html_document_create();
        if (!document_) {
            throw std::runtime_error("Failed to create HTML document");
        }
    }
    
    ~HTMLParser() {
        if (document_) {
            lxb_html_document_destroy(document_);
        }
    }
    
    // Non-copyable
    HTMLParser(const HTMLParser&) = delete;
    HTMLParser& operator=(const HTMLParser&) = delete;
    
    // Movable
    HTMLParser(HTMLParser&& other) noexcept : document_(other.document_) {
        other.document_ = nullptr;
    }
    
    HTMLParser& operator=(HTMLParser&& other) noexcept {
        if (this != &other) {
            if (document_) {
                lxb_html_document_destroy(document_);
            }
            document_ = other.document_;
            other.document_ = nullptr;
        }
        return *this;
    }
    
    /**
     * @brief Parse HTML string
     * @param html HTML text to parse
     * @return Status code
     */
    lxb_status_t parse(const std::string& html) {
        if (!document_) {
            throw std::runtime_error("Document not initialized");
        }
        
        return lxb_html_document_parse(
            document_,
            reinterpret_cast<const lxb_char_t*>(html.c_str()),
            html.size()
        );
    }
    
    /**
     * @brief Get body element
     */
    lxb_dom_element_t* get_body() {
        if (!document_) return nullptr;
        
        lxb_html_body_element_t* body_html = lxb_html_document_body_element(document_);
        if (!body_html) return nullptr;
        
        // Cast: html_body_element -> html_element -> dom_element
        lxb_html_element_t* html_elem = lxb_html_interface_element(body_html);
        return lxb_dom_interface_element(html_elem);
    }
    
    /**
     * @brief Get document root
     */
    lxb_dom_node_t* get_root() {
        return document_ ? lxb_dom_interface_node(document_) : nullptr;
    }
    
    lxb_html_document_t* get() { return document_; }
    const lxb_html_document_t* get() const { return document_; }
    
private:
    lxb_html_document_t* document_;
};

/**
 * @brief Helper functions for Lexbor
 */
namespace utils {

/**
 * @brief Get text content from DOM node
 */
inline std::string get_text_content(lxb_dom_node_t* node) {
    if (!node) return "";
    
    // Get text content
    size_t len = 0;
    lxb_char_t* text = lxb_dom_node_text_content(node, &len);
    
    if (!text) return "";
    
    std::string result(reinterpret_cast<const char*>(text), len);
    
    // Free the text (Lexbor allocates it)
    lxb_dom_document_destroy_text(lxb_dom_interface_document(node), text);
    
    return result;
}

/**
 * @brief Get element tag name
 */
inline std::string get_tag_name(lxb_dom_element_t* element) {
    if (!element) return "";
    
    size_t len = 0;
    const lxb_char_t* name = lxb_dom_element_qualified_name(element, &len);
    
    if (!name) return "";
    
    return std::string(reinterpret_cast<const char*>(name), len);
}

/**
 * @brief Get element attribute
 */
inline std::string get_attribute(lxb_dom_element_t* element, const std::string& attr_name) {
    if (!element) return "";
    
    size_t len = 0;
    const lxb_char_t* value = lxb_dom_element_get_attribute(
        element,
        reinterpret_cast<const lxb_char_t*>(attr_name.c_str()),
        attr_name.size(),
        &len
    );
    
    if (!value) return "";
    
    return std::string(reinterpret_cast<const char*>(value), len);
}

/**
 * @brief Check if element has class
 */
inline bool has_class(lxb_dom_element_t* element, const std::string& class_name) {
    std::string classes = get_attribute(element, "class");
    return classes.find(class_name) != std::string::npos;
}

} // namespace utils

} // namespace lexbor
} // namespace whiteboard
