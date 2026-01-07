import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class FilterTransformNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Filter',
    name: 'filterTransform',
    icon: 'fa:filter',
    group: ['Transform'],
    version: 1,
    description: 'Filter items based on conditions',
    defaults: {
      name: 'Filter',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Field',
        name: 'field',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Greater Than', value: 'gt' },
          { name: 'Less Than', value: 'lt' },
          { name: 'Equals', value: 'eq' },
          { name: 'Contains', value: 'contains' },
        ],
        default: 'gt',
      },
      {
        displayName: 'Value',
        name: 'value',
        type: NodePropertyType.String,
        default: '',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const field = this.getNodeParameter('field', 0) as string;
    const operation = this.getNodeParameter('operation', 0) as string;
    const value = this.getNodeParameter('value', 0);

    const filtered = items.filter((item, i) => {
      const fieldValue = getFieldValue(item.json, field);

      switch (operation) {
        case 'gt': return Number(fieldValue) > Number(value);
        case 'lt': return Number(fieldValue) < Number(value);
        case 'eq': return fieldValue == value;
        case 'contains': return String(fieldValue).includes(String(value));
        default: return true;
      }
    });

    return [filtered.map((item, i) => ({ ...item, pairedItem: i }))];
  }
}

function getFieldValue(obj: any, path: string): any {
  return path.split('.').reduce((o, k) => o?.[k], obj);
}
