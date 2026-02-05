#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>

namespace flex {
namespace chart {

struct AstNode {
    virtual ~AstNode() = default;
};

using AstValue = std::variant<double, std::string, bool>;

struct AstProperty : AstNode {
    std::string name;
    AstValue value;
    bool is_relative;
};

struct AstTransform : AstNode {
    std::string type;
    std::map<std::string, AstValue> properties;
};

struct AstData : AstNode {
    std::string name;
    std::string source;
    std::string format;
    std::vector<AstValue> inline_values;
    std::vector<std::shared_ptr<AstTransform>> transforms;
};

struct AstSignalEvent : AstNode {
    std::string event;
    std::string update_expr;
};

struct AstSignal : AstNode {
    std::string name;
    AstValue initial_value;
    std::vector<std::shared_ptr<AstSignalEvent>> handlers;
};

struct AstEncoding : AstNode {
    std::string channel;
    std::string field;
    std::map<std::string, AstValue> config;
};

struct AstState : AstNode {
    std::string name;
    std::vector<std::shared_ptr<AstEncoding>> encodings;
    std::map<std::string, AstValue> styles;
};

struct AstMark : AstNode {
    std::string type;
    std::string name;
    std::string data_ref;
    std::vector<std::shared_ptr<AstEncoding>> encodings;
    std::map<std::string, AstValue> styles;
    std::vector<std::shared_ptr<AstState>> states;
};

struct AstScale : AstNode {
    std::string name;
    std::map<std::string, AstValue> properties;
};

struct AstAxis : AstNode {
    std::string name;
    std::map<std::string, AstValue> properties;
};

struct AstView : AstNode {};

struct AstChart : AstView {
    std::string title;
    std::string width;
    std::string height;
    std::string margin;
    std::string theme;
    
    std::vector<std::shared_ptr<AstData>> datasets;
    std::vector<std::shared_ptr<AstSignal>> signals;
    std::vector<std::shared_ptr<AstScale>> scales;
    std::vector<std::shared_ptr<AstAxis>> axes;
    std::vector<std::shared_ptr<AstMark>> marks;
};

struct AstComposition : AstView {
    std::string type; // vconcat, hconcat, facet, repeat
    std::string name;
    std::vector<std::shared_ptr<AstView>> children;
    std::shared_ptr<AstView> spec;
    std::map<std::string, std::string> resolve;
};

struct AstProgram {
    std::vector<std::shared_ptr<AstView>> views;
    std::vector<std::shared_ptr<AstData>> global_datasets;
};

} // namespace chart
} // namespace flex
