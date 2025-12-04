// controller.js - Dashboard UI controller
import { WidgetHelper, FlexUI } from 'flexui';
import { DashboardData } from './data.js';

export class DashboardController {
    constructor() {
        this.data = new DashboardData();
        this.currentView = 'overview';
        this.updateInterval = null;
    }
    
    init() {
        this.renderStats();
        this.selectNav('overview');  // This will call renderView which includes renderRecentSales
        this.startAutoUpdate();
        FlexUI.log('Dashboard initialized');
    }
    
    renderStats() {
        const stats = this.data.getStats();
        
        WidgetHelper.setText('stat-revenue-value', '$' + stats.totalRevenue.toFixed(2));
        WidgetHelper.setText('stat-subs-value', '+' + stats.subscriptions);
        WidgetHelper.setText('stat-sales-value', '+' + stats.sales);
        WidgetHelper.setText('stat-active-value', '+' + stats.activeNow);
    }
    
    renderRecentSales() {
        const sales = this.data.getRecentSales();
        let text = 'Recent Sales:\n';
        
        sales.forEach((sale, index) => {
            text += sale.name + ' - $' + sale.amount.toFixed(2);
            if (index < sales.length - 1) text += '\n';
        });
        
        WidgetHelper.setText('recent-sales-list', text);
    }
    
    selectNav(view) {
        FlexUI.log('selectNav called:', view);
        const oldView = this.currentView;
        this.currentView = view;
        
        // Update page header
        FlexUI.log('Updating page title to:', this.getViewTitle(view));
        WidgetHelper.setText('page-title', this.getViewTitle(view));
        WidgetHelper.setText('page-subtitle', this.getViewSubtitle(view));
        
        // Update content area based on view
        this.renderView(view);
        
        // Update status bar
        WidgetHelper.setText('status-bar', 'Viewing: ' + this.getViewTitle(view));
        
        FlexUI.log('Navigated from', oldView, 'to', view);
    }
    
    getViewTitle(view) {
        const titles = {
            overview: 'Dashboard',
            customers: 'Customers',
            products: 'Products',
            settings: 'Settings'
        };
        return titles[view] || 'Dashboard';
    }
    
    getViewSubtitle(view) {
        const subtitles = {
            overview: 'Welcome back! Here\'s what\'s happening.',
            customers: 'Manage your customer relationships.',
            products: 'View and manage your products.',
            settings: 'Configure your dashboard settings.'
        };
        return subtitles[view] || '';
    }
    
    startAutoUpdate() {
        this.updateInterval = FlexUI.setTimeout(() => {
            this.autoUpdate();
        }, 3000);
    }
    
    autoUpdate() {
        this.data.updateStats();
        this.renderStats();
        
        // Schedule next update
        this.updateInterval = FlexUI.setTimeout(() => {
            this.autoUpdate();
        }, 3000);
    }
    
    stopAutoUpdate() {
        if (this.updateInterval) {
            FlexUI.clearTimeout(this.updateInterval);
            this.updateInterval = null;
        }
    }
    
    renderView(view) {
        FlexUI.log('renderView called with:', view);
        
        switch(view) {
            case 'overview':
                FlexUI.log('Rendering overview view');
                WidgetHelper.setText('content-title', 'Overview');
                WidgetHelper.setText('content-description', 'Monthly revenue and growth metrics');
                WidgetHelper.setText('content-body', 
                    '📈 Revenue Chart\n\n' +
                    'Jan: $32,450\n' +
                    'Feb: $38,920\n' +
                    'Mar: $45,231 (current)\n\n' +
                    'Growth: +20.1%'
                );
                
                WidgetHelper.setText('side-title', 'Recent Sales');
                WidgetHelper.setText('side-description', 'You made 265 sales this month');
                this.renderRecentSales();
                break;
                
            case 'customers':
                FlexUI.log('Rendering customers view');
                WidgetHelper.setText('content-title', 'Customer Management');
                WidgetHelper.setText('content-description', 'View and manage your customer database');
                WidgetHelper.setText('content-body', 
                    '👥 Customer Database\n\n' +
                    'Total: 2,350\n' +
                    'Active: 2,100\n' +
                    'Inactive: 250\n\n' +
                    'Recent:\n' +
                    '- John Doe\n' +
                    '- Jane Smith\n' +
                    '- Bob Johnson'
                );
                
                WidgetHelper.setText('side-title', 'Customer Stats');
                WidgetHelper.setText('side-description', 'Monthly customer metrics');
                WidgetHelper.setText('recent-sales-list',
                    'New this month: +180\n' +
                    'Churn rate: 2.3%\n' +
                    'Avg. lifetime value: $1,234\n\n' +
                    'Top Customers:\n' +
                    '1. Acme Corp - $12,450\n' +
                    '2. TechStart Inc - $9,870\n' +
                    '3. Global Ltd - $8,320'
                );
                break;
                
            case 'products':
                FlexUI.log('Rendering products view');
                WidgetHelper.setText('content-title', 'Product Catalog');
                WidgetHelper.setText('content-description', 'Manage your product inventory');
                WidgetHelper.setText('content-body',
                    '📦 Inventory Status\n\n' +
                    'Total Products: 234\n' +
                    'In Stock: 189\n' +
                    'Low Stock: 12\n' +
                    'Out of Stock: 45\n\n' +
                    'Restock needed:\n' +
                    '- Widget Pro\n' +
                    '- Gadget Plus'
                );
                
                WidgetHelper.setText('side-title', 'Top Products');
                WidgetHelper.setText('side-description', 'Best sellers this month');
                WidgetHelper.setText('recent-sales-list',
                    '1. Premium Widget\n' +
                    '   $99.99 - 234 sold\n\n' +
                    '2. Standard Widget\n' +
                    '   $49.99 - 567 sold\n\n' +
                    '3. Basic Widget\n' +
                    '   $19.99 - 891 sold'
                );
                break;
                
            case 'settings':
                FlexUI.log('Rendering settings view');
                WidgetHelper.setText('content-title', 'Settings');
                WidgetHelper.setText('content-description', 'Configure your dashboard');
                WidgetHelper.setText('content-body',
                    '⚙️ Configuration\n\n' +
                    '✓ Account Settings\n' +
                    '✓ Notifications\n' +
                    '✓ Display Options\n' +
                    '✓ API Keys\n' +
                    '✓ Integrations\n\n' +
                    'Last updated:\n' +
                    new Date().toLocaleDateString()
                );
                
                WidgetHelper.setText('side-title', 'Quick Actions');
                WidgetHelper.setText('side-description', 'Common tasks');
                WidgetHelper.setText('recent-sales-list',
                    '• Export Data\n' +
                    '• Import Customers\n' +
                    '• Generate Report\n' +
                    '• Backup Database\n' +
                    '• Clear Cache\n' +
                    '• Reset Preferences'
                );
                break;
        }
    }
    
    refresh() {
        this.data.updateStats();
        this.renderStats();
        this.renderView(this.currentView);
        WidgetHelper.setText('status-bar', 'Refreshed at ' + new Date().toLocaleTimeString());
        FlexUI.log('Dashboard refreshed');
    }
}
