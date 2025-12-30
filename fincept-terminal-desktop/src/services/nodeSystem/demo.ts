/**
 * Node System Demo
 * Demonstrates how to use the new node system
 *
 * This file shows:
 * - Creating a workflow
 * - Adding nodes
 * - Connecting nodes
 * - Executing the workflow
 */

import {
  IWorkflow,
  INode,
  NodeConnectionType,
} from './types';
import { initializeNodeSystem } from './NodeLoader';
import { WorkflowExecutor } from './WorkflowExecutor';
import { NodeRegistry } from './NodeRegistry';

/**
 * Demo: Simple Stock Data Workflow
 */
export async function demoStockDataWorkflow() {
  console.log('=== Node System Demo: Stock Data Workflow ===\n');

  // 1. Initialize the node system
  console.log('Step 1: Initializing node system...');
  await initializeNodeSystem();
  console.log('✓ Node system initialized\n');

  // 2. Create nodes
  console.log('Step 2: Creating nodes...');

  const stockDataNode: INode = {
    id: 'node-1',
    name: 'AAPL Stock Data',
    type: 'stockData',
    typeVersion: 1,
    position: [100, 100],
    parameters: {
      source: 'yfinance',
      symbol: 'AAPL',
      interval: '1d',
      period: '1mo',
      options: {
        maxResults: 10,
      },
    },
  };

  console.log('✓ Created Stock Data node\n');

  // 3. Create workflow
  console.log('Step 3: Creating workflow...');

  const workflow: IWorkflow = {
    id: 'demo-workflow-1',
    name: 'Stock Data Demo',
    active: true,
    nodes: [stockDataNode],
    connections: {},
    settings: {
      executionOrder: 'v1',
    },
  };

  console.log('✓ Workflow created\n');

  // 4. Execute workflow
  console.log('Step 4: Executing workflow...');

  const executor = new WorkflowExecutor(workflow);
  const result = await executor.execute();

  console.log('✓ Workflow executed\n');

  // 5. Display results
  console.log('Step 5: Results:');
  console.log('================\n');

  for (const [nodeName, taskDataArray] of Object.entries(result.resultData.runData)) {
    console.log(`Node: ${nodeName}`);
    console.log(`Status: ${taskDataArray[0].executionStatus}`);
    console.log(`Execution time: ${taskDataArray[0].executionTime}ms`);

    const outputData = taskDataArray[0].data.main?.[0] || [];
    console.log(`Output items: ${outputData.length}`);

    if (outputData.length > 0) {
      console.log('\nSample data (first 3 items):');
      outputData.slice(0, 3).forEach((item, i) => {
        console.log(`  Item ${i + 1}:`, {
          symbol: item.json.symbol,
          datetime: item.json.datetime,
          close: item.json.close,
          volume: item.json.volume,
        });
      });
    }

    console.log('');
  }

  console.log('=== Demo Complete ===\n');

  return result;
}

/**
 * Demo: Multi-Node Workflow
 * (Will be implemented after more nodes are created)
 */
export async function demoMultiNodeWorkflow() {
  console.log('=== Node System Demo: Multi-Node Workflow ===\n');
  console.log('Coming soon: This will demonstrate multiple nodes connected together');
  console.log('Examples:');
  console.log('  - Stock Data → Technical Indicators → Results Display');
  console.log('  - Stock Data → Portfolio Optimizer → Backtest → Results');
  console.log('  - Economic Data → AI Analysis → Report Generator');
  console.log('\n');
}

/**
 * Run all demos
 */
export async function runAllDemos() {
  try {
    await demoStockDataWorkflow();
    await demoMultiNodeWorkflow();
  } catch (error) {
    console.error('Demo failed:', error);
  }
}

// If run directly
if (require.main === module) {
  runAllDemos().catch(console.error);
}
