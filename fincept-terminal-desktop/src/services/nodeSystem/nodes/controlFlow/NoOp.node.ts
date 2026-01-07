import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class NoOpNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'No Operation',
    name: 'noOp',
    icon: 'fa:circle',
    iconColor: '#9ca3af',
    group: ['controlFlow'],
    version: 1,
    description: 'Pass data through without changes (for visual organization)',
    defaults: { name: 'No Op', color: '#9ca3af' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Note',
        name: 'note',
        type: NodePropertyType.String,
        default: '',
        typeOptions: {
          rows: 3,
        },
        description: 'Optional note for documentation',
        placeholder: 'Data validation checkpoint',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    // Simply pass through all input data unchanged
    return [this.getInputData()];
  }
}
