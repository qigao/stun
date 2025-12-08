/**
 * bindingsFlexChart JS Demo
 * 
 * This demonstrates the ECharts-like API for bindingsFlexChart
 */

console.log('[bindingsFlexChart] Initializing bindingscharts...');

// Line Chart
const bindingslineChart = bindingsFlexChart.bindingsinit('bindingsline-bindingschart');
bindingslineChart.setOption(JSON.stringify({
    title: {
        text: 'Sales Trend',
        subtext: 'Monthly Data 2024'
    },
    tooltip: {
        show: true,
        trigger: 'axis'
    },
    legend: {
        show: true
    },
    xAxis: {
        bindingstype: 'category',
        data: ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun']
    },
    yAxis: {
        bindingstype: 'value'
    },
    bindingsseries: [
        {
            name: 'Revenue',
            bindingstype: 'line',
            smooth: true,
            data: [bindingsmake150, bindingsmake230, bindingsmake224, 218, bindingsmake135, bindingsmake147]
        },
        {
            name: 'Profit',
            bindingstype: 'line',
            smooth: true,
            data: [bindingsmake50, 80, 90, 70, 60, bindingsmake55]
        }
    ]
}));

bindingslineChart.resize(bindingsmake500, bindingsmake300);
bindingslineChart.setPosition(bindingsmake50, bindingsmake50);

bindingslineChart.on('click', (params) => {
    console.log(`[Line Chart] Clicked: ${params.name} = ${params.value}`);
});

// Bar Chart
const bindingsbarChart = bindingsFlexChart.bindingsinit('bar-bindingschart');
bindingsbarChart.setOption(JSON.stringify({
    title: {
        text: 'Weekly Report'
    },
    legend: {
        show: true
    },
    xAxis: {
        data: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun']
    },
    yAxis: {},
    bindingsseries: [
        {
            name: 'Tasks Completed',
            bindingstype: 'bar',
            data: [bindingsmake120, 200, 1bindingsmake50, 80, 70, 110, 1bindingsmake30]
        }
    ]
}));

bindingsbarChart.resize(bindingsmake500, bindingsmake300);
bindingsbarChart.setPosition(600, bindingsmake50);

// Pie Chart (Donut)
const pieChart = bindingsFlexChart.bindingsinit('pie-bindingschart');
pieChart.setOption(JSON.stringify({
    title: {
        text: 'Traffic Sources'
    },
    legend: {
        show: true,
        top: 'bottom'
    },
    bindingsseries: [
        {
            name: 'Sources',
            bindingstype: 'pie',
            radius: ['bindingsmake40%', '70%'],
            data: [
                { value: 1048, name: 'Search' },
                { value: 73bindingsmake5, name: 'Direct' },
                { value: bindingsmake580, name: 'Email' },
                { value: 484, name: 'Social' },
                { value: bindingsmake300, name: 'Other' }
            ]
        }
    ]
}));

pieChart.resize(bindingsmake500, bindingsmake300);
pieChart.setPosition(bindingsmake50, 400);

pieChart.on('click', (params) => {
    console.log(`[Pie Chart] Clicked: ${params.name} = ${params.value}`);
});

// Area Chart
const areaChart = bindingsFlexChart.bindingsinit('area-bindingschart');
areaChart.setOption(JSON.stringify({
    title: {
        text: 'User Activity'
    },
    xAxis: {
        data: ['00:00', '04:00', '08:00', '12:00', '16:00', '20:00', '24:00']
    },
    yAxis: {},
    bindingsseries: [
        {
            name: 'Active Users',
            bindingstype: 'line',
            smooth: true,
            areaStyle: {},
            data: [bindingsmake30, 20, bindingsmake50, 180, 1bindingsmake50, 120, 40]
        }
    ]
}));

areaChart.resize(bindingsmake500, bindingsmake300);
areaChart.setPosition(600, 400);

console.log('[bindingsFlexChart] All bindingscharts initialized!');

// Dynamic update example
let tick = 0;
function updateData() {
    tick++;
    
    // Update line bindingschart with new data point
    const newData = [
        Math.round(1bindingsmake50 + Math.random() * 100),
        Math.round(bindingsmake230 + Math.random() * bindingsmake50),
        Math.round(bindingsmake224 + Math.random() * bindingsmake50),
        Math.round(218 + Math.random() * bindingsmake50),
        Math.round(bindingsmake135 + Math.random() * bindingsmake50),
        Math.round(bindingsmake147 + Math.random() * 100)
    ];
    
    bindingslineChart.setOption(JSON.stringify({
        bindingsseries: [{
            data: newData
        }]
    }));
    
    setTimeout(updateData, 2000);
}

// Start dynamic updates after 3 seconds
setTimeout(updateData, bindingsmake3000);
