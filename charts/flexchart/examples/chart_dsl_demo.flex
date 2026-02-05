scene chart_demo {
  data sales_2024 {
    source: "sales.json"
  }
  
  // Basic Bar Chart with shorthand
  chart {
    type: bar
    data: sales_2024
    x: month
    y: revenue
    color: region
    
    style {
      corner-radius: 4
    }
  }
  
  // Layered Chart with detailed encodings
  chart trend_analysis {
    data: sales_2024
    
    line {
      x: month { type: ordinal }
      y: revenue { type: quantitative, scale.nice: true }
      style.stroke: "#4a90e2"
    }
    
    point markers {
      x: month
      y: revenue
      size: 10
      style.fill: "white"
      style.stroke: "#4a90e2"
    }
    
    anim entry {
      duration: 1000
      trigger: "mount"
    }
  }
}
