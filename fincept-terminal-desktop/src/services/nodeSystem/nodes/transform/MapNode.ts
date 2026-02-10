import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class MapNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Transform Data',
    name: 'map',
    icon: 'fa:exchange-alt',
    group: ['Transform'],
    version: 1,
    description: 'Transform and map fields',
    defaults: {
      name: 'Transform Data',
      color: '#6366f1',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mappings',
        name: 'mappings',
        type: NodePropertyType.Json,
        default: '{"newField": "oldField"}',
        description: 'Field mapping JSON',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const mappingsStr = this.getNodeParameter('mappings', 0) as string;
    const mappings = JSON.parse(mappingsStr);

    return [items.map((item, i) => {
      const mapped: any = {};
      for (const [newKey, oldKey] of Object.entries(mappings)) {
        mapped[newKey] = item.json[oldKey as string];
      }
      return { json: mapped, pairedItem: i };
    })];
  }
}
