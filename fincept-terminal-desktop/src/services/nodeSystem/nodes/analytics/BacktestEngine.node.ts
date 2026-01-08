import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class BacktestEngineNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Backtest Engine',
    name: 'backtestEngine',
    icon: 'fa:history',
    iconColor: '#f59e0b',
    group: ['analytics'],
    version: 1,
    description: 'Backtest trading strategies',
    defaults: { name: 'Backtest Engine', color: '#f59e0b' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      { displayName: 'Initial Capital', name: 'initialCapital', type: NodePropertyType.Number, default: 100000 },
      { displayName: 'Commission', name: 'commission', type: NodePropertyType.Number, default: 0.001 },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    return [this.getInputData()];
  }
}
