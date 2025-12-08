/**
 * FlexChart JS Demo - All Chart Types
 */

console.log('[FlexChart] Initializing all chart types...');

// Line Chart
const lineChart = FlexChart.init('line-chart');
lineChart.setOption(JSON.stringify({
    title: { text: 'Line Chart' },
    legend: { show: true },
    xAxis: { data: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun'] },
    series: [
        { name: 'Revenue', type: 'line', smooth: true, data: [150, 230, 224, 218, 135, 147] },
        { name: 'Profit', type: 'line', smooth: true, data: [50, 80, 90, 70, 60, 55] }
    ]
}));
lineChart.resize(340, 260);
lineChart.setPosition(20, 20);

// Bar Chart
const barChart = FlexChart.init('bar-chart');
barChart.setOption(JSON.stringify({
    title: { text: 'Bar Chart' },
    legend: { show: true },
    xAxis: { data: ['Electronics', 'Clothing', 'Food', 'Books'] },
    series: [
        { name: 'Q1', type: 'bar', data: [320, 200, 150, 80] },
        { name: 'Q2', type: 'bar', data: [280, 240, 180, 120] }
    ]
}));
barChart.resize(340, 260);
barChart.setPosition(380, 20);

// Pie Chart
const pieChart = FlexChart.init('pie-chart');
pieChart.setOption(JSON.stringify({
    title: { text: 'Pie Chart (Donut)' },
    legend: { show: true, top: 'bottom' },
    series: [{
        name: 'Sources',
        type: 'pie',
        radius: ['40%', '70%'],
        data: [
            { value: 1048, name: 'Search' },
            { value: 735, name: 'Direct' },
            { value: 580, name: 'Email' },
            { value: 484, name: 'Social' },
            { value: 300, name: 'Other' }
        ]
    }]
}));
pieChart.resize(340, 260);
pieChart.setPosition(740, 20);

// Scatter Chart
const scatterChart = FlexChart.init('scatter-chart');
scatterChart.setOption(JSON.stringify({
    title: { text: 'Scatter Chart' },
    xAxis: { data: ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'] },
    series: [{
        name: 'Dataset',
        type: 'scatter',
        symbolSize: 8,
        data: [80, 120, 50, 180, 90, 150, 70, 200]
    }]
}));
scatterChart.resize(280, 260);
scatterChart.setPosition(1100, 20);

// Radar Chart
const radarChart = FlexChart.init('radar-chart');
radarChart.setOption(JSON.stringify({
    title: { text: 'Radar Chart' },
    legend: { show: true },
    series: [{
        name: 'Skills',
        type: 'radar',
        radarAreaStyle: true,
        radarIndicator: [
            { name: 'Sales', max: 100 },
            { name: 'Admin', max: 100 },
            { name: 'IT', max: 100 },
            { name: 'Support', max: 100 },
            { name: 'Dev', max: 100 },
            { name: 'Marketing', max: 100 }
        ],
        data: [
            { value: [80, 90, 70, 85, 95, 75], name: 'Team A' },
            { value: [60, 70, 80, 65, 75, 85], name: 'Team B' }
        ]
    }]
}));
radarChart.resize(340, 280);
radarChart.setPosition(20, 310);

// Gauge Chart
const gaugeChart = FlexChart.init('gauge-chart');
gaugeChart.setOption(JSON.stringify({
    title: { text: 'Gauge Chart' },
    series: [{
        name: 'Speed',
        type: 'gauge',
        min: 0,
        max: 100,
        data: [72.5],
        detail: { show: true }
    }]
}));
gaugeChart.resize(340, 280);
gaugeChart.setPosition(380, 310);

// Funnel Chart
const funnelChart = FlexChart.init('funnel-chart');
funnelChart.setOption(JSON.stringify({
    title: { text: 'Funnel Chart' },
    xAxis: { data: ['Visit', 'Inquiry', 'Order', 'Click', 'Show'] },
    series: [{
        name: 'Conversion',
        type: 'funnel',
        sort: 'descending',
        gap: 4,
        data: [
            { value: 100, name: 'Visit' },
            { value: 80, name: 'Inquiry' },
            { value: 60, name: 'Order' },
            { value: 40, name: 'Click' },
            { value: 20, name: 'Show' }
        ]
    }]
}));
funnelChart.resize(340, 280);
funnelChart.setPosition(740, 310);

// Candlestick Chart
const candleChart = FlexChart.init('candle-chart');
candleChart.setOption(JSON.stringify({
    title: { text: 'Candlestick' },
    xAxis: { data: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri'] },
    series: [{
        name: 'Stock',
        type: 'candlestick',
        data: [
            [20, 34, 10, 38],
            [40, 35, 30, 50],
            [31, 38, 28, 42],
            [38, 30, 25, 44],
            [30, 42, 26, 48]
        ]
    }]
}));
candleChart.resize(280, 280);
candleChart.setPosition(1100, 310);

// Area Chart
const areaChart = FlexChart.init('area-chart');
areaChart.setOption(JSON.stringify({
    title: { text: 'Area Chart' },
    legend: { show: true },
    xAxis: { data: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'] },
    series: [{
        name: 'Downloads',
        type: 'line',
        smooth: true,
        areaStyle: {},
        data: [820, 932, 901, 934, 1290, 1330, 1320]
    }]
}));
areaChart.resize(660, 260);
areaChart.setPosition(20, 620);

console.log('[FlexChart] All 9 chart types initialized!');

// Event handlers
lineChart.on('click', (params) => console.log(`Line: ${params.name} = ${params.value}`));
pieChart.on('click', (params) => console.log(`Pie: ${params.name} = ${params.value}`));
radarChart.on('click', (params) => console.log(`Radar: index ${params.dataIndex}`));
