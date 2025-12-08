/**
 * FlexChart - ECharts-like charting library for cssbox
 * 
 * Usage:
 *   import FlexChart from 'flexchart';
 *   const chart = FlexChart.init('container-id');
 *   chart.setOption({ series: [{ type: 'line', data: [150, 230, 224] }] });
 */

export class Chart {
    constructor(containerId) {
        this._id = containerId;
        this._chart = FlexChart.init(containerId);
        this._option = null;
    }

    setOption(option, notMerge = false) {
        if (!this._chart) return this;
        
        if (notMerge || !this._option) {
            this._option = option;
        } else {
            this._option = this._mergeOption(this._option, option);
        }
        
        const jsonStr = JSON.stringify(this._option);
        this._chart.setOption(jsonStr);
        return this;
    }

    getOption() {
        return this._option;
    }

    resize(width, height) {
        if (this._chart) {
            this._chart.resize(width, height);
        }
        return this;
    }

    setPosition(x, y) {
        if (this._chart) {
            this._chart.setPosition(x, y);
        }
        return this;
    }

    on(eventName, callback) {
        if (this._chart) {
            this._chart.on(eventName, callback);
        }
        return this;
    }

    off(eventName) {
        return this;
    }

    dispose() {
        FlexChart.dispose(this._id);
        this._chart = null;
        this._option = null;
    }

    _mergeOption(base, update) {
        const result = { ...base };
        for (const key in update) {
            if (Array.isArray(update[key])) {
                result[key] = update[key];
            } else if (typeof update[key] === 'object' && update[key] !== null) {
                result[key] = this._mergeOption(base[key] || {}, update[key]);
            } else {
                result[key] = update[key];
            }
        }
        return result;
    }
}

export function init(containerId, theme, opts) {
    return new Chart(containerId);
}

export function getInstanceById(id) {
    const chart = FlexChart.getChart(id);
    if (!chart) return null;
    
    const instance = new Chart(id);
    instance._chart = chart;
    return instance;
}

export function dispose(containerId) {
    FlexChart.dispose(containerId);
}

export const version = '1.0.0';

export const SeriesTypes = {
    LINE: 'line',
    BAR: 'bar',
    PIE: 'pie',
    SCATTER: 'scatter',
    AREA: 'area'
};

export const defaultColors = [
    '#5B8FF9',
    '#10B981',
    '#F97316',
    '#8B5CF6',
    '#EC4899',
    '#EAB308',
    '#6366F1',
    '#14B8A6'
];

export default {
    init,
    getInstanceById,
    dispose,
    version,
    SeriesTypes,
    defaultColors,
    Chart
};
