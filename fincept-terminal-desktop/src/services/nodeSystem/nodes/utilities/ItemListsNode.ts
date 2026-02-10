import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class ItemListsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Batch/Split Items',
    name: 'itemLists',
    icon: 'fa:list',
    group: ['utilities'],
    version: 1,
    description: 'Split or batch items into lists',
    defaults: { name: 'Batch/Split Items' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Split Out Items', value: 'splitOut' },
          { name: 'Batch Items', value: 'batch' },
          { name: 'Aggregate Items', value: 'aggregate' },
        ],
        default: 'splitOut',
      },
      {
        displayName: 'Field to Split',
        name: 'fieldToSplit',
        type: NodePropertyType.String,
        default: '',
        description: 'Field containing array to split',
        displayOptions: {
          show: {
            operation: ['splitOut'],
          },
        },
      },
      {
        displayName: 'Batch Size',
        name: 'batchSize',
        type: NodePropertyType.Number,
        default: 10,
        description: 'Number of items per batch',
        displayOptions: {
          show: {
            operation: ['batch'],
          },
        },
      },
      {
        displayName: 'Aggregate Into Field',
        name: 'aggregateField',
        type: NodePropertyType.String,
        default: 'items',
        displayOptions: {
          show: {
            operation: ['aggregate'],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const operation = this.getNodeParameter('operation', 0) as string;
    const returnData: any[] = [];

    try {
      if (operation === 'splitOut') {
        const fieldToSplit = this.getNodeParameter('fieldToSplit', 0) as string;

        items.forEach((item, idx) => {
          const arrayValue = item.json[fieldToSplit];
          if (Array.isArray(arrayValue)) {
            arrayValue.forEach((value) => {
              returnData.push({
                json: { ...item.json, [fieldToSplit]: value },
                pairedItem: { item: idx },
              });
            });
          } else {
            returnData.push({ json: item.json, pairedItem: { item: idx } });
          }
        });
      } else if (operation === 'batch') {
        const batchSize = this.getNodeParameter('batchSize', 0) as number;

        for (let i = 0; i < items.length; i += batchSize) {
          const batch = items.slice(i, i + batchSize).map((item) => item.json);
          returnData.push({
            json: { batch, batchIndex: Math.floor(i / batchSize), batchSize: batch.length },
            pairedItem: { item: i },
          });
        }
      } else if (operation === 'aggregate') {
        const aggregateField = this.getNodeParameter('aggregateField', 0) as string;
        const aggregated = items.map((item) => item.json);

        returnData.push({
          json: { [aggregateField]: aggregated, count: aggregated.length },
          pairedItem: { item: 0 },
        });
      }
    } catch (error: any) {
      returnData.push({ json: { error: error.message }, pairedItem: { item: 0 } });
    }

    return [returnData];
  }
}
