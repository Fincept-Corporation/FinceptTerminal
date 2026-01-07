import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData } from '../../types';

export class FilterTransformNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Filter Data',
    name: 'filterTransform',
    icon: 'fa:filter',
    iconColor: '#60a5fa',
    group: ['transform'],
    version: 1,
    description: 'Filter items based on conditions',
    defaults: { name: 'Filter Data', color: '#60a5fa' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Conditions',
        name: 'conditions',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: [],
        placeholder: 'Add Condition',
        options: [
          {
            name: 'condition',
            displayName: 'Condition',
            values: [
              {
                displayName: 'Field',
                name: 'field',
                type: NodePropertyType.String,
                default: '',
                description: 'Field name to check',
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
                  { name: 'Greater Than or Equal', value: 'greaterThanOrEqual' },
                  { name: 'Less Than or Equal', value: 'lessThanOrEqual' },
                  { name: 'Contains', value: 'contains' },
                  { name: 'Not Contains', value: 'notContains' },
                  { name: 'Starts With', value: 'startsWith' },
                  { name: 'Ends With', value: 'endsWith' },
                  { name: 'Is Empty', value: 'isEmpty' },
                  { name: 'Is Not Empty', value: 'isNotEmpty' },
                ],
                default: 'equals',
              },
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
                description: 'Value to compare against (not needed for isEmpty/isNotEmpty)',
                displayOptions: {
                  hide: {
                    operation: ['isEmpty', 'isNotEmpty'],
                  },
                },
              },
            ],
          },
        ],
      },
      {
        displayName: 'Combine Conditions',
        name: 'combineOperation',
        type: NodePropertyType.Options,
        options: [
          { name: 'AND (all conditions must match)', value: 'and' },
          { name: 'OR (any condition must match)', value: 'or' },
        ],
        default: 'and',
        description: 'How to combine multiple conditions',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      const conditions = this.getNodeParameter('conditions', i, { condition: [] }) as {
        condition: Array<{ field: string; operation: string; value: string }>;
      };
      const combineOperation = this.getNodeParameter('combineOperation', i, 'and') as string;

      if (!conditions.condition || conditions.condition.length === 0) {
        // No conditions, pass through all items
        returnData.push(items[i]);
        continue;
      }

      const results = conditions.condition.map((cond) => {
        const fieldValue = items[i].json[cond.field];
        return evaluateCondition(fieldValue, cond.operation, cond.value);
      });

      const shouldInclude =
        combineOperation === 'and'
          ? results.every((r) => r === true)
          : results.some((r) => r === true);

      if (shouldInclude) {
        returnData.push(items[i]);
      }
    }

    return [returnData];
  }
}

function evaluateCondition(fieldValue: any, operation: string, compareValue: string): boolean {
    switch (operation) {
      case 'equals':
        return String(fieldValue) === compareValue;
      case 'notEquals':
        return String(fieldValue) !== compareValue;
      case 'greaterThan':
        return Number(fieldValue) > Number(compareValue);
      case 'lessThan':
        return Number(fieldValue) < Number(compareValue);
      case 'greaterThanOrEqual':
        return Number(fieldValue) >= Number(compareValue);
      case 'lessThanOrEqual':
        return Number(fieldValue) <= Number(compareValue);
      case 'contains':
        return String(fieldValue).includes(compareValue);
      case 'notContains':
        return !String(fieldValue).includes(compareValue);
      case 'startsWith':
        return String(fieldValue).startsWith(compareValue);
      case 'endsWith':
        return String(fieldValue).endsWith(compareValue);
      case 'isEmpty':
        return fieldValue === null || fieldValue === undefined || fieldValue === '';
      case 'isNotEmpty':
        return fieldValue !== null && fieldValue !== undefined && fieldValue !== '';
      default:
        return false;
    }
}
