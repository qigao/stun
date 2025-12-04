// main.js - Dashboard entry point
console.log('[Dashboard] Starting to load...');

import { FlexUI } from 'flexui';
console.log('[Dashboard] FlexUI imported');

import { DashboardController } from './controller.js';
console.log('[Dashboard] DashboardController imported');

// Create dashboard instance
const dashboard = new DashboardController();
console.log('[Dashboard] Dashboard instance created');

// Initialize on load
dashboard.init();

// Export handlers for XML onclick
globalThis.navOverview = () => dashboard.selectNav('overview');
globalThis.navCustomers = () => dashboard.selectNav('customers');
globalThis.navProducts = () => dashboard.selectNav('products');
globalThis.navSettings = () => dashboard.selectNav('settings');
globalThis.refreshDashboard = () => dashboard.refresh();

FlexUI.log('Dashboard application loaded');
FlexUI.log('Navigation: Overview, Customers, Products, Settings');
