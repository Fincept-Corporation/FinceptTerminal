import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class CompareDatasetsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Compare Tables',
    name: 'compareDatasets',
    icon: 'fa:not-equal',
    group: ['data'],
    version: 1,
    description: 'Compare two datasets and find differences',
    defaults: { name: 'Compare Tables' },
    inputs: [NodeConnectionType.Main, NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Compare Mode',
        name: 'compareMode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Find Differences', value: 'differences' },
          { name: 'Find Common', value: 'common' },
          { name: 'Only in First', value: 'onlyFirst' },
          { name: 'Only in Second', value: 'onlySecond' },
        ],
        default: 'differences',
      },
      {
        displayName: 'Compare By Field',
        name: 'compareByField',
        type: NodePropertyType.String,
        default: '',
        description: 'Field to use for comparison (leave empty to compare entire objects)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items1 = this.getInputData(0);
    const items2 = this.getInputData(1);
    const returnData: any[] = [];

    const compareMode = this.getNodeParameter('compareMode', 0) as string;
    const compareByField = this.getNodeParameter('compareByField', 0) as string;

    try {
      if (compareByField) {
        // Compare by specific field
        const set1 = new Map(items1.map((item) => [item.json[compareByField], item]));
        const set2 = new Map(items2.map((item) => [item.json[compareByField], item]));

        if (compareMode === 'differences') {
          // Items that exist in both but have different values
          for (const [key, item1] of set1) {
            if (set2.has(key)) {
              const item2 = set2.get(key);
              if (JSON.stringify(item1.json) !== JSON.stringify(item2?.json)) {
                returnData.push({
                  json: {
                    key,
                    dataset1: item1.json,
                    dataset2: item2?.json,
                  },
                  pairedItem: { item: 0 },
                });
              }
            }
          }
        } else if (compareMode === 'common') {
          // Items that exist in both
          for (const [key, item1] of set1) {
            if (set2.has(key)) {
              returnData.push({ json: item1.json, pairedItem: { item: 0 } });
            }
          }
        } else if (compareMode === 'onlyFirst') {
          // Items only in first dataset
          for (const [key, item1] of set1) {
            if (!set2.has(key)) {
              returnData.push({ json: item1.json, pairedItem: { item: 0 } });
            }
          }
        } else if (compareMode === 'onlySecond') {
          // Items only in second dataset
          for (const [key, item2] of set2) {
            if (!set1.has(key)) {
              returnData.push({ json: item2.json, pairedItem: { item: 0 } });
            }
          }
        }
      } else {
        // Compare entire objects
        const set1Strings = new Set(items1.map((item) => JSON.stringify(item.json)));
        const set2Strings = new Set(items2.map((item) => JSON.stringify(item.json)));

        if (compareMode === 'onlyFirst') {
          items1.forEach((item, idx) => {
            const str = JSON.stringify(item.json);
            if (!set2Strings.has(str)) {
              returnData.push({ json: item.json, pairedItem: { item: idx } });
            }
          });
        } else if (compareMode === 'onlySecond') {
          items2.forEach((item, idx) => {
            const str = JSON.stringify(item.json);
            if (!set1Strings.has(str)) {
              returnData.push({ json: item.json, pairedItem: { item: idx } });
            }
          });
        } else if (compareMode === 'common') {
          items1.forEach((item, idx) => {
            const str = JSON.stringify(item.json);
            if (set2Strings.has(str)) {
              returnData.push({ json: item.json, pairedItem: { item: idx } });
            }
          });
        }
      }
    } catch (error: any) {
      returnData.push({ json: { error: error.message }, pairedItem: { item: 0 } });
    }

    return [returnData];
  }
}
