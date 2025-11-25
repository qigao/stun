#include <flexui.h>
#include <iostream>

int main() {
    flexui::Screen screen(800, 600, "FlexUI SVG Demo");

    // Load CSS for styling
    screen.loadCSS(R"(
        .circle {
            fill: #4a90e2;
            stroke: #2e5c8a;
            stroke-width: 3px;
            transition: all 0.3s ease;
        }
        
        .circle:hover {
            fill: #5aa0f2;
            stroke-width: 5px;
        }
        
        .line {
            stroke: #e74c3c;
            stroke-width: 2px;
        }
        
        .dashed-line {
            stroke: #2ecc71;
            stroke-width: 3px;
            stroke-dasharray: 10 5;
        }
        
        .ellipse {
            fill: #f39c12;
            stroke: #e67e22;
            stroke-width: 2px;
        }
        
        .rect {
            fill: #9b59b6;
            stroke: #8e44ad;
            stroke-width: 2px;
        }
    )");

    // Create SVG elements with rough/hand-drawn rendering
    auto circle = screen.createCircle("circle1", 100, 100, 50);
    circle->setFill("#4a90e2");
    circle->setStroke("#2e5c8a", 5);
    circle->setInlineStyle("stroke-rendering", "rough");
    circle->setInlineStyle("roughness", "2.0");
    circle->setInlineStyle("bowing", "2.0");
    circle->setInlineStyle("stroke-count", "1");  // Single stroke
    circle->setInlineStyle("seed", "123");

    auto line = screen.createLine("line1", 200, 50, 350, 150);
    line->setStroke("#e74c3c", 2);
    line->setInlineStyle("stroke-rendering", "rough");
    line->setInlineStyle("roughness", "2.0");
    line->setInlineStyle("bowing", "2.0");
    line->setInlineStyle("stroke-count", "1");
    line->setInlineStyle("seed", "456");

    auto dashedLine = screen.createLine("line2", 200, 200, 350, 200);
    dashedLine->setStroke("#2ecc71", 3);
    dashedLine->setStrokeDash("10 5");
    dashedLine->setInlineStyle("stroke-rendering", "rough");
    dashedLine->setInlineStyle("roughness", "2.0");
    dashedLine->setInlineStyle("bowing", "2.0");
    dashedLine->setInlineStyle("stroke-count", "1");
    dashedLine->setInlineStyle("seed", "789");

    auto ellipse = screen.createEllipse("ellipse1", 500, 100, 60, 40);
    ellipse->setFill("#f39c12");
    ellipse->setStroke("#e67e22", 2);
    ellipse->setInlineStyle("stroke-rendering", "rough");
    ellipse->setInlineStyle("roughness", "2.0");
    ellipse->setInlineStyle("bowing", "2.0");
    ellipse->setInlineStyle("stroke-count", "1");  // Single stroke
    ellipse->setInlineStyle("seed", "321");

    auto rect = screen.createRect("rect1", 450, 200, 100, 80);
    rect->setFill("#9b59b6");
    rect->setStroke("#8e44ad", 2);
    rect->setHandDrawn(true, 987.0f);  // Rects still use old hand-drawn

    // Create a hand-drawn path
    auto path = screen.createPath("path1");
    path->addPathPoint(50, 350);
    path->addPathPoint(100, 320);
    path->addPathPoint(150, 340);
    path->addPathPoint(200, 310);
    path->addPathPoint(250, 350);
    path->setStroke("black", 3);
    path->setHandDrawn(true, 42.0f);  // Paths still use old hand-drawn

    // Create a smooth path
    auto smoothPath = screen.createPath("path2");
    smoothPath->addPathPoint(300, 350);
    smoothPath->addPathPoint(350, 320);
    smoothPath->addPathPoint(400, 340);
    smoothPath->addPathPoint(450, 310);
    smoothPath->addPathPoint(500, 350);
    smoothPath->setStroke("#3498db", 3);
    smoothPath->setStrokeLineCap("round");
    smoothPath->setStrokeLineJoin("round");

    // Create a polygon (triangle) with hand-drawn effect
    auto polygon = screen.createPolygon("polygon1", {
        {600.0f, 100.0f}, {650.0f, 150.0f}, {550.0f, 150.0f}
    });
    polygon->setStroke("#e74c3c", 3);
    polygon->setHandDrawn(true, 654.0f);  // Polygons still use old hand-drawn

    // Create a polyline (zigzag) - open path
    auto polyline = screen.createPolyline("polyline1", {
        {600.0f, 200.0f}, {625.0f, 220.0f}, {650.0f, 200.0f}, {675.0f, 220.0f}, {700.0f, 200.0f}
    });
    polyline->setStroke("#16a085", 3);
    polyline->setStrokeLineCap("round");
    polyline->setStrokeLineJoin("round");

    // Add title
    auto title = screen.addWidget("title", "div");
    title->setPosition(20, 20);
    title->setText("FlexUI SVG Demo - Hover over the circle!");
    title->setInlineStyle("font-size", "24px");
    title->setInlineStyle("font-weight", "bold");
    title->setInlineStyle("color", "#2c3e50");

    // Add labels
    auto label1 = screen.addWidget("label1", "div");
    label1->setPosition(70, 170);
    label1->setText("Circle");
    label1->setInlineStyle("font-size", "14px");

    auto label2 = screen.addWidget("label2", "div");
    label2->setPosition(250, 170);
    label2->setText("Lines");
    label2->setInlineStyle("font-size", "14px");

    auto label3 = screen.addWidget("label3", "div");
    label3->setPosition(470, 170);
    label3->setText("Ellipse & Rect");
    label3->setInlineStyle("font-size", "14px");

    auto label4 = screen.addWidget("label4", "div");
    label4->setPosition(50, 380);
    label4->setText("Hand-drawn path");
    label4->setInlineStyle("font-size", "14px");

    auto label5 = screen.addWidget("label5", "div");
    label5->setPosition(350, 380);
    label5->setText("Smooth path");
    label5->setInlineStyle("font-size", "14px");

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
