// Database Verification Script
// Run with: bun run check-database

import { invoke } from '@tauri-apps/api/core';

console.log('🔍 Checking Paper Trading Database...\n');

async function checkDatabase() {
  try {
    // Check portfolios
    console.log('📊 Checking portfolios...');
    const portfolios = await invoke('db_list_portfolios');
    console.log(`Found ${portfolios.length} portfolio(s):`);
    console.log(JSON.stringify(portfolios, null, 2));

    if (portfolios.length > 0) {
      const portfolio = portfolios[0];
      console.log(`\n💼 Checking portfolio: ${portfolio.name} (${portfolio.id})`);

      // Check positions
      const positions = await invoke('db_get_portfolio_positions', {
        portfolioId: portfolio.id,
        status: null
      });
      console.log(`\n📈 Positions: ${positions.length}`);
      if (positions.length > 0) {
        console.log(JSON.stringify(positions, null, 2));
      }

      // Check orders
      const orders = await invoke('db_get_portfolio_orders', {
        portfolioId: portfolio.id,
        status: null
      });
      console.log(`\n📋 Orders: ${orders.length}`);
      if (orders.length > 0) {
        console.log(JSON.stringify(orders, null, 2));
      }

      // Check trades
      const trades = await invoke('db_get_portfolio_trades', {
        portfolioId: portfolio.id,
        limit: 10
      });
      console.log(`\n💱 Trades: ${trades.length}`);
      if (trades.length > 0) {
        console.log(JSON.stringify(trades, null, 2));
      }
    }

    console.log('\n✅ Database check complete!');
  } catch (error) {
    console.error('❌ Error checking database:', error);
    console.log('\n💡 This is expected if running outside Tauri app.');
    console.log('   Run this from the app console instead.');
  }
}

checkDatabase();
