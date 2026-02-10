import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class JSONNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'JSON Parser',
    name: 'json',
    icon: 'fa:code',
    group: ['data'],
    version: 1,
    description: 'Parse, extract, and transform JSON data',
    defaults: {
      name: 'JSON Parser',
      color: '#f7df1e',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Parse', value: 'parse' },
          { name: 'Stringify', value: 'stringify' },
          { name: 'Extract Path', value: 'extract' },
          { name: 'Set Value', value: 'set' },
          { name: 'Delete Key', value: 'delete' },
        ],
        default: 'parse',
        required: true,
      },
      {
        displayName: 'JSON String',
        name: 'jsonString',
        type: NodePropertyType.String,
        default: '',
        placeholder: '{"key": "value"}',
        description: 'JSON string to parse',
      },
      {
        displayName: 'Path',
        name: 'path',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'data.items[0].name',
        description: 'JSON path to extract/set (dot notation)',
      },
      {
        displayName: 'Value',
        name: 'value',
        type: NodePropertyType.String,
        default: '',
        description: 'Value to set',
      },
      {
        displayName: 'Pretty Print',
        name: 'prettyPrint',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Format JSON with indentation',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const operation = this.getNodeParameter('operation', i) as string;
        const jsonString = this.getNodeParameter('jsonString', i, '') as string;
        const path = this.getNodeParameter('path', i, '') as string;
        const value = this.getNodeParameter('value', i, '') as string;
        const prettyPrint = this.getNodeParameter('prettyPrint', i, true) as boolean;

        let result: any;

        switch (operation) {
          case 'parse':
            result = JSON.parse(jsonString || JSON.stringify(items[i].json));
            break;

          case 'stringify':
            result = prettyPrint
              ? JSON.stringify(items[i].json, null, 2)
              : JSON.stringify(items[i].json);
            break;

          case 'extract':
            result = this.extractPath(items[i].json, path);
            break;

          case 'set':
            result = this.setPath(items[i].json, path, value);
            break;

          case 'delete':
            result = this.deletePath(items[i].json, path);
            break;
        }

        returnData.push({
          json: {
            json: result,
            ...items[i].json,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[JSONNode] Error:', error);
        if (this.continueOnFail()) {
          returnData.push({
            json: { error: error.message, ...items[i].json },
            pairedItem: i,
          });
        } else {
          throw error;
        }
      }
    }

    return [returnData];
  }

  private extractPath(obj: any, path: string): any {
    return path.split('.').reduce((current, key) => {
      const arrayMatch = key.match(/(.+)\[(\d+)\]/);
      if (arrayMatch) {
        return current?.[arrayMatch[1]]?.[parseInt(arrayMatch[2])];
      }
      return current?.[key];
    }, obj);
  }

  private setPath(obj: any, path: string, value: any): any {
    const copy = JSON.parse(JSON.stringify(obj));
    const keys = path.split('.');
    let current = copy;

    for (let i = 0; i < keys.length - 1; i++) {
      if (!current[keys[i]]) current[keys[i]] = {};
      current = current[keys[i]];
    }

    current[keys[keys.length - 1]] = value;
    return copy;
  }

  private deletePath(obj: any, path: string): any {
    const copy = JSON.parse(JSON.stringify(obj));
    const keys = path.split('.');
    let current = copy;

    for (let i = 0; i < keys.length - 1; i++) {
      if (!current[keys[i]]) return copy;
      current = current[keys[i]];
    }

    delete current[keys[keys.length - 1]];
    return copy;
  }
}
