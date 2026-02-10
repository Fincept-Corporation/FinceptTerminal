import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class JoinNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Merge Tables',
    name: 'join',
    icon: 'fa:link',
    group: ['Transform'],
    version: 1,
    description: 'Join two data sets',
    defaults: {
      name: 'Merge Tables',
      color: '#06b6d4',
    },
    inputs: [
      NodeConnectionType.Main,
      NodeConnectionType.Main,
    ],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Join Type',
        name: 'joinType',
        type: NodePropertyType.Options,
        options: [
          { name: 'Inner Join', value: 'inner' },
          { name: 'Left Join', value: 'left' },
          { name: 'Right Join', value: 'right' },
          { name: 'Full Outer Join', value: 'outer' },
        ],
        default: 'inner',
      },
      {
        displayName: 'Left Key Field',
        name: 'leftKeyField',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Right Key Field',
        name: 'rightKeyField',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const leftItems = this.getInputData(0);
    const rightItems = this.getInputData(1);
    const joinType = this.getNodeParameter('joinType', 0) as string;
    const leftKeyField = this.getNodeParameter('leftKeyField', 0) as string;
    const rightKeyField = this.getNodeParameter('rightKeyField', 0) as string;

    const outputItems: any[] = [];

    // Create index of right items
    const rightIndex = new Map<any, any[]>();
    for (const rightItem of rightItems) {
      const key = rightItem.json[rightKeyField];
      if (!rightIndex.has(key)) {
        rightIndex.set(key, []);
      }
      rightIndex.get(key)!.push(rightItem.json);
    }

    // Process left items
    const matchedRightKeys = new Set<any>();

    for (let i = 0; i < leftItems.length; i++) {
      const leftItem = leftItems[i];
      const leftKey = leftItem.json[leftKeyField];
      const matchingRightItems = rightIndex.get(leftKey) || [];

      if (matchingRightItems.length > 0) {
        matchedRightKeys.add(leftKey);
        for (const rightData of matchingRightItems) {
          outputItems.push({
            json: { ...leftItem.json, ...rightData },
            pairedItem: i,
          });
        }
      } else if (joinType === 'left' || joinType === 'outer') {
        outputItems.push({
          json: { ...leftItem.json },
          pairedItem: i,
        });
      }
    }

    // Add unmatched right items for right/outer joins
    if (joinType === 'right' || joinType === 'outer') {
      for (const [key, rightItemsData] of rightIndex.entries()) {
        if (!matchedRightKeys.has(key)) {
          for (const rightData of rightItemsData) {
            outputItems.push({
              json: { ...rightData },
              pairedItem: outputItems.length,
            });
          }
        }
      }
    }

    return [outputItems];
  }
}
