import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class SortNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Sort',
    name: 'sort',
    icon: 'fa:sort',
    group: ['Transform'],
    version: 1,
    description: 'Sort items by field',
    defaults: {
      name: 'Sort',
      color: '#f59e0b',
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
        displayName: 'Order',
        name: 'order',
        type: NodePropertyType.Options,
        options: [
          { name: 'Ascending', value: 'asc' },
          { name: 'Descending', value: 'desc' },
        ],
        default: 'asc',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const field = this.getNodeParameter('field', 0) as string;
    const order = this.getNodeParameter('order', 0) as string;

    const sorted = [...items].sort((a, b) => {
      const aVal = a.json[field];
      const bVal = b.json[field];

      if (order === 'asc') {
        return (aVal ?? 0) > (bVal ?? 0) ? 1 : -1;
      } else {
        return (aVal ?? 0) < (bVal ?? 0) ? 1 : -1;
      }
    });

    return [sorted.map((item, i) => ({ ...item, pairedItem: i }))];
  }
}
