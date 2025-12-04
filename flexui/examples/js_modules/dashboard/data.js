// data.js - Dashboard data management
export class DashboardData {
    constructor() {
        this.stats = {
            totalRevenue: 45231.89,
            subscriptions: 2350,
            sales: 12234,
            activeNow: 573
        };
        
        this.recentSales = [
            { name: 'Olivia Martin', email: 'olivia@example.com', amount: 1999.00 },
            { name: 'Jackson Lee', email: 'jackson@example.com', amount: 39.00 },
            { name: 'Isabella Nguyen', email: 'isabella@example.com', amount: 299.00 },
            { name: 'William Kim', email: 'will@example.com', amount: 99.00 },
            { name: 'Sofia Davis', email: 'sofia@example.com', amount: 39.00 }
        ];
    }
    
    getStats() {
        return this.stats;
    }
    
    getRecentSales() {
        return this.recentSales;
    }
    
    // Simulate real-time updates
    updateStats() {
        this.stats.activeNow = 500 + Math.floor(Math.random() * 200);
        this.stats.totalRevenue += Math.random() * 100 - 50;
        return this.stats;
    }
}
