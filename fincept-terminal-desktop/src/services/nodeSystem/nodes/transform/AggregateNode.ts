import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class AggregateNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Aggregate',
    name: 'aggregate',
    icon: 'fa:calculator',
    group: ['Transform'],
    version: 1,
    description: 'Aggregate data (sum, average, count)',
    defaults: {
      name: 'Aggregate',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Field',
        name: 'field',
        type: NodePropertyType.String,
        default: '',
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Sum', value: 'sum' },
          { name: 'Average', value: 'avg' },
          { name: 'Count', value: 'count' },
          { name: 'Min', value: 'min' },
          { name: 'Max', value: 'max' },
        ],
        default: 'sum',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const field = this.getNodeParameter('field', 0) as string;
    const operation = this.getNodeParameter('operation', 0) as string;

    const values = items.map(item => Number(item.json[field]) || 0);
    let result = 0;

    switch (operation) {
      case 'sum': result = values.reduce((a, b) => a + b, 0); break;
      case 'avg': result = values.reduce((a, b) => a + b, 0) / values.length; break;
      case 'count': result = values.length; break;
      case 'min': result = Math.min(...values); break;
      case 'max': result = Math.max(...values); break;
    }

    return [[{
      json: { [operation]: result, field, count: values.length },
      pairedItem: 0,
    }]];
  }
}
