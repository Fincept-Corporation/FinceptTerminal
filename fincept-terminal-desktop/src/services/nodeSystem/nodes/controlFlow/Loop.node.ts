import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData } from '../../types';

export class LoopNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Loop',
    name: 'loop',
    icon: 'fa:sync',
    iconColor: '#f59e0b',
    group: ['controlFlow'],
    version: 1,
    description: 'Loop through items in batches',
    defaults: { name: 'Loop', color: '#f59e0b' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main, NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Batch Size',
        name: 'batchSize',
        type: NodePropertyType.Number,
        default: 1,
        description: 'Number of items to process per batch',
      },
      {
        displayName: 'Pause Between Batches (ms)',
        name: 'pauseBetweenBatches',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Milliseconds to wait between batches',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const batchSize = this.getNodeParameter('batchSize', 0) as number;
    const pauseBetweenBatches = this.getNodeParameter('pauseBetweenBatches', 0) as number;

    const batchedItems: INodeExecutionData[] = [];
    const completedItems: INodeExecutionData[] = [];

    // Process items in batches
    for (let i = 0; i < items.length; i += batchSize) {
      const batch = items.slice(i, i + batchSize);

      // First output: current batch for processing
      batchedItems.push(...batch);

      // Pause between batches if configured
      if (pauseBetweenBatches > 0 && i + batchSize < items.length) {
        await new Promise(resolve => setTimeout(resolve, pauseBetweenBatches));
      }
    }

    // Second output: all completed items
    completedItems.push(...items);

    // Return [current batch, all items]
    // In a real loop node, this would handle re-execution logic
    return [batchedItems, completedItems];
  }
}
