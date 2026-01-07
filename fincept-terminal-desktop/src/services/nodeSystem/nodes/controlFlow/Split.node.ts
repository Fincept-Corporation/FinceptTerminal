import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData, IDataObject } from '../../types';

export class SplitNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Split Out',
    name: 'split',
    icon: 'fa:code-fork',
    iconColor: '#ef4444',
    group: ['controlFlow'],
    version: 1,
    description: 'Split arrays into individual items',
    defaults: { name: 'Split Out', color: '#ef4444' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Field to Split',
        name: 'fieldToSplit',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'items',
        description: 'Field containing array to split (leave empty to split the item itself)',
      },
      {
        displayName: 'Include Original Fields',
        name: 'includeOriginalFields',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether to include fields from the original item',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const fieldToSplit = this.getNodeParameter('fieldToSplit', 0) as string;
    const includeOriginalFields = this.getNodeParameter('includeOriginalFields', 0) as boolean;
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      if (fieldToSplit) {
        // Split specific field
        const arrayToSplit = items[i].json[fieldToSplit];

        if (Array.isArray(arrayToSplit)) {
          for (const arrayItem of arrayToSplit) {
            const newItem: IDataObject = {};

            if (includeOriginalFields) {
              // Include all fields except the split field
              for (const key of Object.keys(items[i].json)) {
                if (key !== fieldToSplit) {
                  newItem[key] = items[i].json[key];
                }
              }
            }

            // Add the split item
            if (typeof arrayItem === 'object' && arrayItem !== null) {
              Object.assign(newItem, arrayItem);
            } else {
              newItem.value = arrayItem;
            }

            returnData.push({ json: newItem, pairedItem: i });
          }
        } else {
          // Field is not an array, pass through
          returnData.push(items[i]);
        }
      } else {
        // Split the item itself if it's an array
        const itemData = items[i].json;
        if (Array.isArray(itemData)) {
          for (const element of itemData) {
            if (typeof element === 'object' && element !== null) {
              returnData.push({ json: element as IDataObject, pairedItem: i });
            } else {
              returnData.push({ json: { value: element }, pairedItem: i });
            }
          }
        } else {
          // Not an array, pass through
          returnData.push(items[i]);
        }
      }
    }

    return [returnData];
  }
}
