import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class GroupByNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Group Rows',
    name: 'groupBy',
    icon: 'fa:object-group',
    group: ['Transform'],
    version: 1,
    description: 'Group items by field value',
    defaults: {
      name: 'Group Rows',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Group By Field',
        name: 'groupByField',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Field to group by',
      },
      {
        displayName: 'Aggregate Fields',
        name: 'aggregateFields',
        type: NodePropertyType.FixedCollection,
        default: [],
        description: 'Fields to aggregate within each group',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const groupByField = this.getNodeParameter('groupByField', 0) as string;

    // Group items by field value
    const groups = new Map<any, any[]>();

    for (const item of items) {
      const groupValue = item.json[groupByField];
      if (!groups.has(groupValue)) {
        groups.set(groupValue, []);
      }
      groups.get(groupValue)!.push(item);
    }

    // Create output items for each group
    const outputItems = Array.from(groups.entries()).map(([groupValue, groupItems], i) => ({
      json: {
        [groupByField]: groupValue,
        count: groupItems.length,
        items: groupItems.map(item => item.json),
      },
      pairedItem: i,
    }));

    return [outputItems];
  }
}
