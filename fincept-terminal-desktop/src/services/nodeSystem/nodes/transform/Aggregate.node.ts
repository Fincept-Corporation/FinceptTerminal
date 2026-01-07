import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, IDataObject, INodeExecutionData } from '../../types';

export class AggregateNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Aggregate',
    name: 'aggregate',
    icon: 'fa:calculator',
    iconColor: '#f59e0b',
    group: ['transform'],
    version: 1,
    description: 'Perform aggregations on data (sum, average, count, min, max)',
    defaults: { name: 'Aggregate', color: '#f59e0b' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Aggregations',
        name: 'aggregations',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: [],
        placeholder: 'Add Aggregation',
        options: [
          {
            name: 'aggregation',
            displayName: 'Aggregation',
            values: [
              {
                displayName: 'Operation',
                name: 'operation',
                type: NodePropertyType.Options,
                options: [
                  { name: 'Sum', value: 'sum' },
                  { name: 'Average', value: 'average' },
                  { name: 'Count', value: 'count' },
                  { name: 'Min', value: 'min' },
                  { name: 'Max', value: 'max' },
                  { name: 'Median', value: 'median' },
                  { name: 'Standard Deviation', value: 'stdDev' },
                  { name: 'Variance', value: 'variance' },
                  { name: 'First', value: 'first' },
                  { name: 'Last', value: 'last' },
                ],
                default: 'sum',
              },
              {
                displayName: 'Field',
                name: 'field',
                type: NodePropertyType.String,
                default: '',
                description: 'Field to aggregate (not needed for count)',
                placeholder: 'price',
                displayOptions: {
                  hide: {
                    operation: ['count'],
                  },
                },
              },
              {
                displayName: 'Output Field Name',
                name: 'outputFieldName',
                type: NodePropertyType.String,
                default: '',
                description: 'Name for the output field',
                placeholder: 'totalPrice',
              },
            ],
          },
        ],
      },
      {
        displayName: 'Group By',
        name: 'groupBy',
        type: NodePropertyType.String,
        default: '',
        description: 'Field to group by (leave empty to aggregate all items)',
        placeholder: 'category',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    const aggregations = this.getNodeParameter('aggregations', 0, { aggregation: [] }) as {
      aggregation: Array<{ operation: string; field: string; outputFieldName: string }>;
    };
    const groupBy = this.getNodeParameter('groupBy', 0, '') as string;

    if (!aggregations.aggregation || aggregations.aggregation.length === 0) {
      // No aggregations defined, return original items
      return [items];
    }

    if (groupBy) {
      // Group items by field value
      const groups = new Map<string, INodeExecutionData[]>();

      for (const item of items) {
        const groupKey = String(item.json[groupBy] || 'undefined');
        if (!groups.has(groupKey)) {
          groups.set(groupKey, []);
        }
        groups.get(groupKey)!.push(item);
      }

      // Aggregate each group
      for (const [groupKey, groupItems] of groups) {
        const result: IDataObject = {
          [groupBy]: groupKey,
        };

        for (const agg of aggregations.aggregation) {
          const outputFieldName = agg.outputFieldName || `${agg.operation}_${agg.field}`;
          result[outputFieldName] = performAggregation(groupItems, agg.operation, agg.field);
        }

        returnData.push({ json: result });
      }
    } else {
      // Aggregate all items into a single result
      const result: IDataObject = {};

      for (const agg of aggregations.aggregation) {
        const outputFieldName = agg.outputFieldName || `${agg.operation}_${agg.field}`;
        result[outputFieldName] = performAggregation(items, agg.operation, agg.field);
      }

      returnData.push({ json: result });
    }

    return [returnData];
  }
}

function performAggregation(
  items: INodeExecutionData[],
  operation: string,
  field: string
): number | string | null {
    if (operation === 'count') {
      return items.length;
    }

    const values = items
      .map((item) => item.json[field])
      .filter((val) => val !== null && val !== undefined && val !== '')
      .map((val) => Number(val))
      .filter((val) => !isNaN(val));

    if (values.length === 0) {
      return null;
    }

    switch (operation) {
      case 'sum':
        return values.reduce((acc, val) => acc + val, 0);

      case 'average':
        return values.reduce((acc, val) => acc + val, 0) / values.length;

      case 'min':
        return Math.min(...values);

      case 'max':
        return Math.max(...values);

      case 'median': {
        const sorted = [...values].sort((a, b) => a - b);
        const mid = Math.floor(sorted.length / 2);
        return sorted.length % 2 === 0
          ? (sorted[mid - 1] + sorted[mid]) / 2
          : sorted[mid];
      }

      case 'stdDev': {
        const avg = values.reduce((acc, val) => acc + val, 0) / values.length;
        const variance = values.reduce((acc, val) => acc + Math.pow(val - avg, 2), 0) / values.length;
        return Math.sqrt(variance);
      }

      case 'variance': {
        const avg = values.reduce((acc, val) => acc + val, 0) / values.length;
        return values.reduce((acc, val) => acc + Math.pow(val - avg, 2), 0) / values.length;
      }

      case 'first':
        return values[0];

      case 'last':
        return values[values.length - 1];

      default:
        return null;
    }
}
