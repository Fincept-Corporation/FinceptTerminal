import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, IDataObject, INodeExecutionData } from '../../types';

export class MapNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Map',
    name: 'map',
    icon: 'fa:code',
    iconColor: '#8b5cf6',
    group: ['transform'],
    version: 1,
    description: 'Transform each item using expressions or field mappings',
    defaults: { name: 'Map', color: '#8b5cf6' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Keep Only Set Fields',
        name: 'keepOnlySet',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Whether to keep only the fields set in the mapping',
      },
      {
        displayName: 'Fields to Map',
        name: 'fields',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: [],
        placeholder: 'Add Field',
        options: [
          {
            name: 'fieldMapping',
            displayName: 'Field Mapping',
            values: [
              {
                displayName: 'New Field Name',
                name: 'newFieldName',
                type: NodePropertyType.String,
                default: '',
                description: 'Name of the field to create',
                placeholder: 'totalPrice',
              },
              {
                displayName: 'Field Value',
                name: 'fieldValue',
                type: NodePropertyType.String,
                default: '',
                description: 'Value or expression for the field (use $json.fieldName to reference input data)',
                placeholder: '$json.price * $json.quantity',
              },
            ],
          },
        ],
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      const keepOnlySet = this.getNodeParameter('keepOnlySet', i) as boolean;
      const fields = this.getNodeParameter('fields', i, { fieldMapping: [] }) as {
        fieldMapping: Array<{ newFieldName: string; fieldValue: string }>;
      };

      const newItem: IDataObject = keepOnlySet ? {} : { ...items[i].json };

      // Process each field mapping
      if (fields.fieldMapping && Array.isArray(fields.fieldMapping)) {
        for (const mapping of fields.fieldMapping) {
          if (!mapping.newFieldName) continue;

          try {
            // Evaluate expression or use literal value
            let value: any;
            if (mapping.fieldValue.includes('$json') || mapping.fieldValue.includes('$item')) {
              value = this.evaluateExpression(mapping.fieldValue, i);
            } else {
              value = mapping.fieldValue;
            }

            newItem[mapping.newFieldName] = value;
          } catch (error) {
            // If expression evaluation fails, use literal value
            newItem[mapping.newFieldName] = mapping.fieldValue;
          }
        }
      }

      returnData.push({
        json: newItem,
        pairedItem: i,
      });
    }

    return [returnData];
  }
}
