import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class MergeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Merge',
    name: 'merge',
    icon: 'fa:object-group',
    group: ['Control Flow'],
    version: 1,
    description: 'Combine multiple inputs into one output',
    defaults: {
      name: 'Merge',
      color: '#10b981',
    },
    inputs: [
      NodeConnectionType.Main,
      NodeConnectionType.Main,
      NodeConnectionType.Main,
    ],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Append', value: 'append' },
          { name: 'Merge Fields', value: 'merge' },
          { name: 'Wait for All', value: 'waitAll' },
        ],
        default: 'append',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const mode = this.getNodeParameter('mode', 0) as string;
    const returnData: any[] = [];

    if (mode === 'append') {
      items.forEach((item, i) => returnData.push({ ...item, pairedItem: i }));
    } else if (mode === 'merge') {
      const merged: any = {};
      items.forEach(item => Object.assign(merged, item.json));
      returnData.push({ json: merged, pairedItem: 0 });
    } else {
      items.forEach((item, i) => returnData.push({ ...item, pairedItem: i }));
    }

    return [returnData];
  }
}
