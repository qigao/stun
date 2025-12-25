// Shapes Gallery - Demonstrates all shape types in Flex DSL
// rect, circle, ellipse, polygon, star, line, ring, path
// stroke, fill, rough (hand-drawn) style

const W = 900
const H = 700

scene shapes_gallery {
    width: W
    height: H

    rect bg { width: W, height: H, fill: #1a1a2e, position: absolute }

    group main {
        width: W
        height: H
        layout: flex
        flexDirection: column
        alignItems: center
        justifyContent: center
        gap: 20

        text title { content: "Flex DSL Shapes Gallery", fontSize: 24, color: #ffffff }

        // Row 1: Basic shapes
        group row1 {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40

            group item1 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                rect r1 { width: 60, height: 50, fill: #ff6b6b, cornerRadius: 8 }
                text t1 { content: "rect", fontSize: 12, color: #888888 }
            }
            group item2 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                circle c1 { radius: 30, fill: #4ecdc4 }
                text t2 { content: "circle", fontSize: 12, color: #888888 }
            }
            group item3 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                ellipse e1 { rx: 40, ry: 25, fill: #ffe66d }
                text t3 { content: "ellipse", fontSize: 12, color: #888888 }
            }
            group item4 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group lc1 { width: 60, height: 50
                    line l1 { x2: 55, y2: 40, stroke: #a855f7, strokeWidth: 3 }
                }
                text t4 { content: "line", fontSize: 12, color: #888888 }
            }
        }

        // Row 2: Polygons
        group row2 {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40

            group item5 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                polygon p3 { sides: 3, radius: 30, fill: #06b6d4 }
                text t5 { content: "polygon(3)", fontSize: 12, color: #888888 }
            }
            group item6 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                polygon p5 { sides: 5, radius: 30, fill: #8b5cf6 }
                text t6 { content: "polygon(5)", fontSize: 12, color: #888888 }
            }
            group item7 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                polygon p6 { sides: 6, radius: 30, fill: #ec4899 }
                text t7 { content: "polygon(6)", fontSize: 12, color: #888888 }
            }
            group item8 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                polygon p8 { sides: 8, radius: 30, fill: #f97316 }
                text t8 { content: "polygon(8)", fontSize: 12, color: #888888 }
            }
        }

        // Row 3: Stars and Ring
        group row3 {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40

            group item9 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                star s5 { points: 5, outerRadius: 30, innerRadius: 12, fill: #fbbf24 }
                text t9 { content: "star(5)", fontSize: 12, color: #888888 }
            }
            group item10 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                star s6 { points: 6, outerRadius: 30, innerRadius: 15, fill: #22c55e }
                text t10 { content: "star(6)", fontSize: 12, color: #888888 }
            }
            group item11 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                star s8 { points: 8, outerRadius: 30, innerRadius: 18, fill: #3b82f6 }
                text t11 { content: "star(8)", fontSize: 12, color: #888888 }
            }
            group item12 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                ring rg1 { outerRadius: 30, innerRadius: 18, fill: #ef4444 }
                text t12 { content: "ring", fontSize: 12, color: #888888 }
            }
        }

        // Row 4: Path triangles
        group row4 {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40

            group item13 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group pc1 { width: 50, height: 50
                    path pa1 { d: "M 0 0 L 40 25 L 0 50 Z", fill: #10b981 }
                }
                text t13 { content: "right", fontSize: 12, color: #888888 }
            }
            group item14 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group pc2 { width: 50, height: 50
                    path pa2 { d: "M 40 0 L 0 25 L 40 50 Z", fill: #f59e0b }
                }
                text t14 { content: "left", fontSize: 12, color: #888888 }
            }
            group item15 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group pc3 { width: 50, height: 50
                    path pa3 { d: "M 25 0 L 50 50 L 0 50 Z", fill: #6366f1 }
                }
                text t15 { content: "up", fontSize: 12, color: #888888 }
            }
            group item16 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group pc4 { width: 50, height: 50
                    path pa4 { d: "M 0 0 L 50 0 L 25 50 Z", fill: #ec4899 }
                }
                text t16 { content: "down", fontSize: 12, color: #888888 }
            }
        }

        // Row 5: Stroked shapes
        group row5 {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40

            group item17 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                rect sr1 { width: 50, height: 40, stroke: #ff6b6b, strokeWidth: 3 }
                text t17 { content: "stroke", fontSize: 12, color: #888888 }
            }
            group item18 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                circle sc1 { radius: 25, stroke: #4ecdc4, strokeWidth: 3 }
                text t18 { content: "stroke", fontSize: 12, color: #888888 }
            }
            group item19 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                polygon sp1 { sides: 6, radius: 28, fill: #ffe66d, stroke: #f97316, strokeWidth: 3 }
                text t19 { content: "fill+stroke", fontSize: 12, color: #888888 }
            }
            group item20 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                rect rr1 { width: 55, height: 40, fill: #a855f7, cornerRadius: 12 }
                text t20 { content: "rounded", fontSize: 12, color: #888888 }
            }
        }

        // Row 6: Rough/Hand-drawn style
        group row6 {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40

            group item21 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                rect rough1 { width: 50, height: 40, fill: #ff6b6b, roughness: 2, roughSeed: 42 }
                text t21 { content: "rough", fontSize: 12, color: #888888 }
            }
            group item22 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                circle rough2 { radius: 25, fill: #4ecdc4, roughness: 2.5, fillStyle: hachure, roughSeed: 123 }
                text t22 { content: "hachure", fontSize: 12, color: #888888 }
            }
            group item23 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                polygon rough3 { sides: 6, radius: 28, fill: #ffe66d, roughness: 3, fillStyle: crosshatch, roughSeed: 456 }
                text t23 { content: "crosshatch", fontSize: 12, color: #888888 }
            }
            group item24 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                star rough4 { points: 5, outerRadius: 28, innerRadius: 12, fill: #fbbf24, stroke: #f97316, strokeWidth: 2, roughness: 2, roughSeed: 789 }
                text t24 { content: "rough star", fontSize: 12, color: #888888 }
            }
        }

        // Row 7: Lines
        group row7 {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40

            group item25 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group lc2 { width: 60, height: 30
                    line lh1 { x2: 55, y2: 0, stroke: #06b6d4, strokeWidth: 3 }
                }
                text t25 { content: "horizontal", fontSize: 12, color: #888888 }
            }
            group item26 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group lc3 { width: 30, height: 50
                    line lv1 { x2: 0, y2: 45, stroke: #8b5cf6, strokeWidth: 3 }
                }
                text t26 { content: "vertical", fontSize: 12, color: #888888 }
            }
            group item27 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group lc4 { width: 50, height: 50
                    line ld1 { x2: 45, y2: 45, stroke: #ec4899, strokeWidth: 3 }
                }
                text t27 { content: "diagonal", fontSize: 12, color: #888888 }
            }
            group item28 { width: 80, height: 90, layout: flex, flexDirection: column, alignItems: center, gap: 5
                group lc5 { width: 60, height: 30
                    line lr1 { x2: 55, y2: 5, stroke: #22c55e, strokeWidth: 2, roughness: 2, roughSeed: 999 }
                }
                text t28 { content: "rough line", fontSize: 12, color: #888888 }
            }
        }
    }
}
