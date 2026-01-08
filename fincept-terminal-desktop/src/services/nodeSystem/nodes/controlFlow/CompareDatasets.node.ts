import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData, IDataObject } from '../../types';

export class CompareDatasetsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Compare Datasets',
    name: 'compareDatasets',
    icon: 'fa:exchange-alt',
    iconColor: '#06b6d4',
    group: ['controlFlow'],
    version: 1,
    description: 'Compare two datasets and find differences',
    defaults: { name: 'Compare Datasets', color: '#06b6d4' },
    inputs: [NodeConnectionType.Main, NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main, NodeConnectionType.Main, NodeConnectionType.Main, NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Compare Field',
        name: 'compareField',
        type: NodePropertyType.String,
        default: 'id',
        description: 'Field to use for comparison',
        placeholder: 'id',
      },
      {
        displayName: 'Fuzzy Match',
        name: 'fuzzyMatch',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Enable fuzzy matching for string comparisons',
      },
      {
        displayName: 'Ignore Case',
        name: 'ignoreCase',
        type: NodePropertyType.Boolean,
        default: false,
        displayOptions: {
          show: {
            fuzzyMatch: [true],
          },
        },
        description: 'Ignore case when comparing strings',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const input1 = this.getInputData(0);
    const input2 = this.getInputData(1);
    const compareField = this.getNodeParameter('compareField', 0) as string;
    const fuzzyMatch = this.getNodeParameter('fuzzyMatch', 0) as boolean;
    const ignoreCase = this.getNodeParameter('ignoreCase', 0, false) as boolean;

    const onlyInInput1: INodeExecutionData[] = [];
    const onlyInInput2: INodeExecutionData[] = [];
    const inBoth: INodeExecutionData[] = [];
    const different: INodeExecutionData[] = [];

    // Create map from input2
    const input2Map = new Map<string, IDataObject>();
    for (const item of input2) {
      let key = String(item.json[compareField]);
      if (fuzzyMatch && ignoreCase && typeof key === 'string') {
        key = key.toLowerCase();
      }
      input2Map.set(key, item.json);
    }

    // Compare input1 with input2
    for (const item of input1) {
      let key = String(item.json[compareField]);
      if (fuzzyMatch && ignoreCase && typeof key === 'string') {
        key = key.toLowerCase();
      }

      if (input2Map.has(key)) {
        const item2 = input2Map.get(key)!;

        // Check if items are identical
        const identical = JSON.stringify(item.json) === JSON.stringify(item2);

        if (identical) {
          inBoth.push(item);
        } else {
          // Items have same key but different data
          different.push({
            json: {
              key,
              input1: item.json,
              input2: item2,
              differences: findDifferences(item.json, item2),
            },
          });
        }

        // Remove from map so we can find items only in input2
        input2Map.delete(key);
      } else {
        // Only in input1
        onlyInInput1.push(item);
      }
    }

    // Remaining items are only in input2
    for (const [key, value] of Array.from(input2Map.entries())) {
      onlyInInput2.push({ json: value });
    }

    // Output 0: Only in input1
    // Output 1: Only in input2
    // Output 2: In both (identical)
    // Output 3: Different (same key, different data)
    return [onlyInInput1, onlyInInput2, inBoth, different];
  }
}

function findDifferences(obj1: IDataObject, obj2: IDataObject): string[] {
  const differences: string[] = [];
  const allKeys = new Set([...Object.keys(obj1), ...Object.keys(obj2)]);

  for (const key of Array.from(allKeys)) {
    if (!(key in obj2)) {
      differences.push(`${key}: only in input1`);
    } else if (!(key in obj1)) {
      differences.push(`${key}: only in input2`);
    } else if (JSON.stringify(obj1[key]) !== JSON.stringify(obj2[key])) {
      differences.push(`${key}: ${JSON.stringify(obj1[key])} vs ${JSON.stringify(obj2[key])}`);
    }
  }

  return differences;
}
