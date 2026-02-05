#include <catch2/catch_all.hpp>
#include "flexchart/flexchart.h"

using namespace flex::chart;

TEST_CASE("Chart Parser: Basic Properties", "[parser][chart]") {
    const char* source = R"(
        title: "Test Chart"
        width: 100%
        height: 500px
        margin: 5%
        theme: "dark"
    )";

    AstProgram program;
    std::string error;
    bool success = parse_chart(source, &program, error);

    REQUIRE(success);
    REQUIRE(error.empty());
    
    // Check root view
    REQUIRE(program.views.size() > 0);
    auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
    REQUIRE(chart != nullptr);
    CHECK(chart->title == "Test Chart");
    CHECK(chart->width == "100%");
    CHECK(chart->height == "500px");
    CHECK(chart->margin == "5%");
    CHECK(chart->theme == "dark");
}

TEST_CASE("Chart Parser: Data Blocks and Transforms", "[parser][chart]") {
    const char* source = R"(
        data sales {
            source: "sales.json"
            format: "json"
            transform: [
                { filter: "datum.value > 0" },
                { aggregate: "sum", field: "amount" }
            ]
        }
    )";

    AstProgram program;
    std::string error;
    bool success = parse_chart(source, &program, error);

    REQUIRE(success);
    REQUIRE(program.global_datasets.size() == 1);
    auto data = program.global_datasets[0];
    CHECK(data->name == "sales");
    CHECK(data->source == "sales.json");
    CHECK(data->format == "json");
    
    REQUIRE(data->transforms.size() == 2);
    CHECK(data->transforms[0]->type == "filter");
    CHECK(data->transforms[1]->type == "aggregate");
}

TEST_CASE("Chart Parser: Marks and Encodings", "[parser][chart]") {
    const char* source = R"(
        bar {
            data: sales
            x: month { type: "nominal" }
            y: revenue { scale: "linear" }
            mark.opacity: 0.8
        }
    )";

    AstProgram program;
    std::string error;
    bool success = parse_chart(source, &program, error);

    REQUIRE(success);
    auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
    REQUIRE(chart != nullptr);
    REQUIRE(chart->marks.size() == 1);
    auto mark = chart->marks[0];
    CHECK(mark->type == "bar");
    CHECK(mark->data_ref == "sales");
    
    REQUIRE(mark->encodings.size() == 2);
    CHECK(mark->encodings[0]->channel == "x");
    CHECK(mark->encodings[0]->field == "month");
    CHECK(mark->encodings[1]->channel == "y");
    CHECK(mark->encodings[1]->field == "revenue");
    
    CHECK(mark->styles.count("opacity") > 0);
}

TEST_CASE("Chart Parser: Signals and Event Handlers", "[parser][chart]") {
    const char* source = R"(
        signal hover_id {
            value: 0
            on: [
                { events: "rect:mouseover", update: "datum.id" },
                { events: "rect:mouseout", update: "0" }
            ]
        }
        signal active = true
    )";

    AstProgram program;
    std::string error;
    bool success = parse_chart(source, &program, error);

    REQUIRE(success);
    auto chart = std::dynamic_pointer_cast<AstChart>(program.views[0]);
    REQUIRE(chart != nullptr);
    REQUIRE(chart->signals.size() == 2);
    
    auto s1 = chart->signals[0];
    CHECK(s1->name == "hover_id");
    REQUIRE(s1->handlers.size() == 2);
    CHECK(s1->handlers[0]->event == "rect:mouseover");
    CHECK(s1->handlers[0]->update_expr == "datum.id");
    
    auto s2 = chart->signals[1];
    CHECK(s2->name == "active");
    CHECK(std::get<bool>(s2->initial_value) == true);
}
