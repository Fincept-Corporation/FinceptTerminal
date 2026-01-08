import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class SplitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Split',
    name: 'split',
    icon: 'fa:code-branch',
    group: ['Control Flow'],
    version: 1,
    description: 'Split items to multiple outputs',
    defaults: {
      name: 'Split',
      color: '#ef4444',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [
      NodeConnectionType.Main,
      NodeConnectionType.Main,
    ],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Clone to All', value: 'clone' },
          { name: 'Distribute Round Robin', value: 'distribute' },
        ],
        default: 'clone',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const mode = this.getNodeParameter('mode', 0) as string;

    if (mode === 'clone') {
      return [items, items];
    } else {
      const output1: any[] = [];
      const output2: any[] = [];
      items.forEach((item, i) => {
        if (i % 2 === 0) output1.push({ ...item, pairedItem: i });
        else output2.push({ ...item, pairedItem: i });
      });
      return [output1, output2];
    }
  }
}
