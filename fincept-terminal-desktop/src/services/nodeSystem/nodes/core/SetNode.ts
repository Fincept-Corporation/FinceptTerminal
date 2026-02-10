/**
 * Set Node
 * Add, edit, or remove fields from items
 *
 * Use cases:
 * - Add calculated fields (e.g., percentage change)
 * - Rename fields
 * - Remove sensitive fields
 * - Set default values
 * - Transform data structure
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
  IDataObject,
} from '../../types';

export class SetNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Set Values',
    name: 'set',
    icon: 'fa:pen',
    iconColor: '#3b82f6',
    group: ['transform'],
    version: 1,
    description: 'Add, edit, or remove fields from items',
    subtitle: 'Edit Fields',
    defaults: {
      name: 'Set Values',
      color: '#3b82f6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Fields to Set',
        name: 'fields',
        placeholder: 'Add Field',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: [],
        options: [
          {
            name: 'field',
            description: 'Field',
            values: [
              {
                displayName: 'Name',
                name: 'name',
                type: NodePropertyType.String,
                default: '',
                placeholder: 'e.g., percentage_change',
                description: 'Name of the field to set',
                required: true,
              },
              {
                displayName: 'Type',
                name: 'type',
                type: NodePropertyType.Options,
                options: [
                  {
                    name: 'String',
                    value: 'string',
                  },
                  {
                    name: 'Number',
                    value: 'number',
                  },
                  {
                    name: 'Boolean',
                    value: 'boolean',
                  },
                  {
                    name: 'Object',
                    value: 'object',
                  },
                  {
                    name: 'Array',
                    value: 'array',
                  },
                ],
                default: 'string',
              },
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
                description: 'The value to set. Can use expressions like {{$json.field}}',
              },
            ],
          },
        ],
      },
      {
        displayName: 'Include Other Fields',
        name: 'includeOtherFields',
        type: NodePropertyType.Boolean,
        default: true,
        description:
          'Whether to include all other fields from the input (true) or only the fields specified above (false)',
      },
      {
        displayName: 'Options',
        name: 'options',
        type: NodePropertyType.Collection,
        placeholder: 'Add Option',
        default: [],
        options: [
          {
            displayName: 'Dot Notation',
            name: 'dotNotation',
            type: NodePropertyType.Boolean,
            default: true,
            description:
              'Whether to support dot notation (e.g., "user.name") for nested fields',
          },
        ],
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: IDataObject[] = [];

    const includeOtherFields = this.getNodeParameter('includeOtherFields', 0) as boolean;
    const options = this.getNodeParameter('options', 0, {}) as IDataObject;
    const dotNotation = options.dotNotation !== false;

    for (let itemIndex = 0; itemIndex < items.length; itemIndex++) {
      const item = items[itemIndex];
      const fields = this.getNodeParameter('fields.field', itemIndex, []) as Array<{
        name: string;
        type: string;
        value: string;
      }>;

      let newItem: IDataObject = {};

      // Include other fields if specified
      if (includeOtherFields) {
        newItem = { ...item.json };
      }

      // Set/update fields
      for (const field of fields) {
        const { name, type, value } = field;

        // Evaluate value (simple implementation - will be enhanced with expression engine)
        let evaluatedValue: any = value;

        // Simple expression evaluation: {{$json.fieldName}}
        if (typeof value === 'string' && value.includes('{{$json.')) {
          const matches = value.match(/\{\{\$json\.([^}]+)\}\}/);
          if (matches && matches[1]) {
            evaluatedValue = getNestedValue(item.json, matches[1]);
          }
        }

        // Type conversion
        evaluatedValue = convertValue(evaluatedValue, type);

        // Set field (with dot notation support)
        if (dotNotation && name.includes('.')) {
          setNestedValue(newItem, name, evaluatedValue);
        } else {
          newItem[name] = evaluatedValue;
        }
      }

      returnData.push(newItem);
    }

    return [this.helpers.returnJsonArray(returnData)];
  }
}

/**
 * Convert value to specified type
 */
function convertValue(value: any, type: string): any {
  if (value === null || value === undefined) {
    return value;
  }

  switch (type) {
    case 'string':
      return String(value);

    case 'number':
      const num = Number(value);
      return isNaN(num) ? 0 : num;

    case 'boolean':
      if (typeof value === 'string') {
        return value.toLowerCase() === 'true';
      }
      return Boolean(value);

    case 'object':
      if (typeof value === 'string') {
        try {
          return JSON.parse(value);
        } catch {
          return {};
        }
      }
      return value;

    case 'array':
      if (Array.isArray(value)) {
        return value;
      }
      if (typeof value === 'string') {
        try {
          const parsed = JSON.parse(value);
          return Array.isArray(parsed) ? parsed : [value];
        } catch {
          return [value];
        }
      }
      return [value];

    default:
      return value;
  }
}

/**
 * Get nested value from object using dot notation
 */
function getNestedValue(obj: IDataObject, path: string): any {
  const keys = path.split('.');
  let current: any = obj;

  for (const key of keys) {
    if (current === null || current === undefined) {
      return undefined;
    }
    current = current[key];
  }

  return current;
}

/**
 * Set nested value in object using dot notation
 */
function setNestedValue(obj: IDataObject, path: string, value: any): void {
  const keys = path.split('.');
  let current: any = obj;

  for (let i = 0; i < keys.length - 1; i++) {
    const key = keys[i];

    if (!current[key] || typeof current[key] !== 'object') {
      current[key] = {};
    }

    current = current[key];
  }

  current[keys[keys.length - 1]] = value;
}
