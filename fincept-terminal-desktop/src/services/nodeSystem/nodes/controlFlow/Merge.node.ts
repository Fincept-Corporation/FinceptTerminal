import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData } from '../../types';

export class MergeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Merge',
    name: 'merge',
    icon: 'fa:code-merge',
    iconColor: '#06b6d4',
    group: ['controlFlow'],
    version: 1,
    description: 'Merge data from multiple inputs',
    defaults: { name: 'Merge', color: '#06b6d4' },
    inputs: [NodeConnectionType.Main, NodeConnectionType.Main, NodeConnectionType.Main, NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Append', value: 'append', description: 'Combine all items into one list' },
          { name: 'Pass-through', value: 'passThrough', description: 'Only pass through first input' },
          { name: 'Wait', value: 'wait', description: 'Wait for all inputs before continuing' },
          { name: 'Combine by Key', value: 'combineByKey', description: 'Merge items with matching keys' },
        ],
        default: 'append',
        description: 'How to merge inputs',
      },
      {
        displayName: 'Join Field',
        name: 'joinField',
        type: NodePropertyType.String,
        default: 'id',
        displayOptions: {
          show: {
            mode: ['combineByKey'],
          },
        },
        description: 'Field name to match items',
        placeholder: 'id',
      },
      {
        displayName: 'Output Combination',
        name: 'outputCombination',
        type: NodePropertyType.Options,
        options: [
          { name: 'Merge Objects', value: 'merge', description: 'Merge matching objects' },
          { name: 'Keep Both', value: 'keepBoth', description: 'Keep both as separate fields' },
        ],
        default: 'merge',
        displayOptions: {
          show: {
            mode: ['combineByKey'],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const mode = this.getNodeParameter('mode', 0) as string;
    const returnData: INodeExecutionData[] = [];

    // Get data from all inputs
    const input1 = this.getInputData(0);
    const input2 = this.getInputData(1);
    const input3 = this.getInputData(2);
    const input4 = this.getInputData(3);

    switch (mode) {
      case 'append':
        // Combine all inputs into one list
        returnData.push(...input1, ...input2, ...input3, ...input4);
        break;

      case 'passThrough':
        // Only pass through first input
        returnData.push(...input1);
        break;

      case 'wait':
        // Wait for all inputs and pass through first
        // (In n8n this waits until all inputs have data)
        returnData.push(...input1);
        break;

      case 'combineByKey': {
        const joinField = this.getNodeParameter('joinField', 0) as string;
        const outputCombination = this.getNodeParameter('outputCombination', 0) as string;

        // Create map from first input
        const itemMap = new Map<string, any>();
        for (const item of input1) {
          const key = String(item.json[joinField]);
          itemMap.set(key, { ...item.json });
        }

        // Merge with other inputs
        const allOtherInputs = [...input2, ...input3, ...input4];
        for (const item of allOtherInputs) {
          const key = String(item.json[joinField]);
          if (itemMap.has(key)) {
            if (outputCombination === 'merge') {
              // Merge objects
              itemMap.set(key, { ...itemMap.get(key), ...item.json });
            } else {
              // Keep both
              itemMap.set(key, {
                ...itemMap.get(key),
                input2: item.json,
              });
            }
          } else {
            // New key, add to map
            itemMap.set(key, { ...item.json });
          }
        }

        // Convert map back to array
        for (const value of Array.from(itemMap.values())) {
          returnData.push({ json: value });
        }
        break;
      }

      default:
        returnData.push(...input1);
    }

    return [returnData];
  }
}
