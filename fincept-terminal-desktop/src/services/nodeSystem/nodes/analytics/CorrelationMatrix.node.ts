import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class CorrelationMatrixNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Correlation Matrix',
    name: 'correlationMatrix',
    icon: 'fa:table',
    iconColor: '#a855f7',
    group: ['analytics'],
    version: 1,
    description: 'Calculate correlation matrix',
    defaults: { name: 'Correlation Matrix', color: '#a855f7' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Fields', name: 'fields', type: NodePropertyType.String, default: '', description: 'Comma-separated field names' },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
