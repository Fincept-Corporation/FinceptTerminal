import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData } from '../../types';

export class SwitchNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Switch',
    name: 'switch',
    icon: 'fa:random',
    iconColor: '#3b82f6',
    group: ['controlFlow'],
    version: 1,
    description: 'Route items to different outputs based on rules',
    defaults: { name: 'Switch', color: '#3b82f6' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main, NodeConnectionType.Main, NodeConnectionType.Main, NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Rules', value: 'rules' },
          { name: 'Expression', value: 'expression' },
        ],
        default: 'rules',
        description: 'How to determine routing',
      },
      {
        displayName: 'Rules',
        name: 'rules',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: [],
        displayOptions: {
          show: {
            mode: ['rules'],
          },
        },
        placeholder: 'Add Rule',
        options: [
          {
            name: 'rule',
            displayName: 'Rule',
            values: [
              {
                displayName: 'Output',
                name: 'output',
                type: NodePropertyType.Number,
                default: 0,
                description: 'Output index (0-3)',
              },
              {
                displayName: 'Field',
                name: 'field',
                type: NodePropertyType.String,
                default: '',
                placeholder: 'price',
              },
              {
                displayName: 'Operation',
                name: 'operation',
                type: NodePropertyType.Options,
                options: [
                  { name: 'Equals', value: 'equals' },
                  { name: 'Not Equals', value: 'notEquals' },
                  { name: 'Greater Than', value: 'greaterThan' },
                  { name: 'Less Than', value: 'lessThan' },
                  { name: 'Contains', value: 'contains' },
                ],
                default: 'equals',
              },
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
              },
            ],
          },
        ],
      },
      {
        displayName: 'Expression',
        name: 'expression',
        type: NodePropertyType.String,
        default: '',
        displayOptions: {
          show: {
            mode: ['expression'],
          },
        },
        placeholder: '$json.price > 100 ? 0 : 1',
        description: 'Expression that returns output index (0-3)',
      },
      {
        displayName: 'Fallback Output',
        name: 'fallbackOutput',
        type: NodePropertyType.Number,
        default: 3,
        description: 'Output for items that match no rules',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const mode = this.getNodeParameter('mode', 0) as string;
    const fallbackOutput = this.getNodeParameter('fallbackOutput', 0) as number;

    const outputs: INodeExecutionData[][] = [[], [], [], []];

    for (let i = 0; i < items.length; i++) {
      let outputIndex = fallbackOutput;

      if (mode === 'rules') {
        const rules = this.getNodeParameter('rules', i, { rule: [] }) as {
          rule: Array<{ output: number; field: string; operation: string; value: string }>;
        };

        if (rules.rule && Array.isArray(rules.rule)) {
          for (const rule of rules.rule) {
            const fieldValue = items[i].json[rule.field];
            let matches = false;

            switch (rule.operation) {
              case 'equals':
                matches = String(fieldValue) === rule.value;
                break;
              case 'notEquals':
                matches = String(fieldValue) !== rule.value;
                break;
              case 'greaterThan':
                matches = Number(fieldValue) > Number(rule.value);
                break;
              case 'lessThan':
                matches = Number(fieldValue) < Number(rule.value);
                break;
              case 'contains':
                matches = String(fieldValue).includes(rule.value);
                break;
            }

            if (matches) {
              outputIndex = Math.min(Math.max(rule.output, 0), 3);
              break;
            }
          }
        }
      } else {
        const expression = this.getNodeParameter('expression', i) as string;
        try {
          const result = this.evaluateExpression(expression, i);
          outputIndex = Math.min(Math.max(Number(result), 0), 3);
        } catch {
          outputIndex = fallbackOutput;
        }
      }

      outputs[outputIndex].push(items[i]);
    }

    return outputs;
  }
}
