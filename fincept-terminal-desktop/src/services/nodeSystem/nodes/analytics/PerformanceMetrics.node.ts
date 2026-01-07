import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class PerformanceMetricsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Performance Metrics',
    name: 'performanceMetrics',
    icon: 'fa:chart-bar',
    iconColor: '#06b6d4',
    group: ['analytics'],
    version: 1,
    description: 'Calculate performance metrics',
    defaults: { name: 'Performance Metrics', color: '#06b6d4' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
