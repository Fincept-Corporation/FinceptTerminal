import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class IfElseNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'If/Else',
    name: 'ifElse',
    icon: 'fa:code-branch',
    iconColor: '#10b981',
    group: ['controlFlow'],
    version: 1,
    description: 'Route data based on conditions',
    defaults: { name: 'If/Else', color: '#10b981' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main, NodeConnectionType.Main],
    properties: [
      { displayName: 'Condition', name: 'condition', type: NodePropertyType.String, default: '', description: 'Expression to evaluate' },
    ],
  };
  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const condition = this.getNodeParameter('condition', 0) as string;
    const trueItems: any[] = [];
    const falseItems: any[] = [];
    
    items.forEach(item => {
      try {
        const result = this.evaluateExpression(condition, 0);
        if (result) trueItems.push(item);
        else falseItems.push(item);
      } catch {
        falseItems.push(item);
      }
    });
    
    return [this.helpers.returnJsonArray(trueItems.map(i => i.json)), this.helpers.returnJsonArray(falseItems.map(i => i.json))];
  }
}
