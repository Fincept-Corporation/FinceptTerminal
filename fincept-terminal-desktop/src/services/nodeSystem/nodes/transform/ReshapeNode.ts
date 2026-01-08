import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class ReshapeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Reshape',
    name: 'reshape',
    icon: 'fa:th',
    group: ['Transform'],
    version: 1,
    description: 'Reshape data structure (flatten, nest, pivot)',
    defaults: {
      name: 'Reshape',
      color: '#f59e0b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Flatten', value: 'flatten' },
          { name: 'Nest', value: 'nest' },
          { name: 'Pivot', value: 'pivot' },
          { name: 'Unpivot', value: 'unpivot' },
        ],
        default: 'flatten',
      },
      {
        displayName: 'Field Path',
        name: 'fieldPath',
        type: NodePropertyType.String,
        default: '',
        description: 'Dot notation path for nested operations (e.g., user.address)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const operation = this.getNodeParameter('operation', 0) as string;
    const fieldPath = this.getNodeParameter('fieldPath', 0) as string;

    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      let resultJson: any;

      switch (operation) {
        case 'flatten':
          resultJson = flattenObject(item.json);
          break;

        case 'nest':
          resultJson = nestObject(item.json, fieldPath);
          break;

        case 'pivot':
          // Simple pivot - convert array to object
          if (Array.isArray(item.json)) {
            resultJson = Object.fromEntries(
              item.json.map((val: any, idx: number) => [idx, val])
            );
          } else {
            resultJson = item.json;
          }
          break;

        case 'unpivot':
          // Convert object to array of key-value pairs
          resultJson = Object.entries(item.json).map(([key, value]) => ({
            key,
            value,
          }));
          break;

        default:
          resultJson = item.json;
      }

      outputItems.push({
        json: resultJson,
        pairedItem: i,
      });
    }

    return [outputItems];
  }
}

function flattenObject(obj: any, prefix = ''): any {
  const flattened: any = {};

  for (const [key, value] of Object.entries(obj)) {
    const newKey = prefix ? `${prefix}.${key}` : key;

    if (value !== null && typeof value === 'object' && !Array.isArray(value)) {
      Object.assign(flattened, flattenObject(value, newKey));
    } else {
      flattened[newKey] = value;
    }
  }

  return flattened;
}

function nestObject(obj: any, path: string): any {
  if (!path) return obj;

  const parts = path.split('.');
  const nested: any = {};
  let current = nested;

  for (let i = 0; i < parts.length - 1; i++) {
    current[parts[i]] = {};
    current = current[parts[i]];
  }

  current[parts[parts.length - 1]] = obj;
  return nested;
}
