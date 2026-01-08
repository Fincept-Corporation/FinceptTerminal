import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData, IDataObject } from '../../types';

export class CodeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Code',
    name: 'code',
    icon: 'fa:code',
    iconColor: '#9333ea',
    group: ['controlFlow'],
    version: 1,
    description: 'Execute custom JavaScript code',
    defaults: { name: 'Code', color: '#9333ea' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Run Once for All Items', value: 'runOnceForAllItems' },
          { name: 'Run Once for Each Item', value: 'runOnceForEachItem' },
        ],
        default: 'runOnceForEachItem',
        description: 'How to execute the code',
      },
      {
        displayName: 'JavaScript Code',
        name: 'jsCode',
        type: NodePropertyType.String,
        default: '// Access input data via $input\n// Return output data\nreturn $input.item.json;',
        typeOptions: {
          rows: 10,
          editor: 'code',
        },
        description: 'JavaScript code to execute',
        displayOptions: {
          show: {
            mode: ['runOnceForEachItem'],
          },
        },
      },
      {
        displayName: 'JavaScript Code',
        name: 'jsCodeAllItems',
        type: NodePropertyType.String,
        default: '// Access all items via $input.all()\n// Return array of items\nreturn $input.all();',
        typeOptions: {
          rows: 10,
          editor: 'code',
        },
        description: 'JavaScript code to execute',
        displayOptions: {
          show: {
            mode: ['runOnceForAllItems'],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const mode = this.getNodeParameter('mode', 0) as string;
    const returnData: INodeExecutionData[] = [];

    if (mode === 'runOnceForAllItems') {
      const jsCode = this.getNodeParameter('jsCodeAllItems', 0) as string;

      // Create execution context
      const $input = {
        all: () => items.map(item => item.json),
        first: () => items[0]?.json,
        last: () => items[items.length - 1]?.json,
      };

      try {
        // Execute code
        const func = new Function('$input', '$json', jsCode);
        const result = func($input, items[0]?.json || {});

        // Handle result
        if (Array.isArray(result)) {
          for (const item of result) {
            returnData.push({ json: typeof item === 'object' ? item : { value: item } });
          }
        } else if (result !== undefined && result !== null) {
          returnData.push({ json: typeof result === 'object' ? result : { value: result } });
        }
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              error: true,
              message: error.message,
              code: jsCode,
            },
          });
        } else {
          throw new Error(`Code execution failed: ${error.message}`);
        }
      }
    } else {
      // Run once for each item
      const jsCode = this.getNodeParameter('jsCode', 0) as string;

      for (let i = 0; i < items.length; i++) {
        const $input = {
          item: items[i],
          all: () => items.map(item => item.json),
          first: () => items[0]?.json,
          last: () => items[items.length - 1]?.json,
        };

        const $json = items[i].json;

        try {
          // Execute code
          const func = new Function('$input', '$json', '$item', jsCode);
          const result = func($input, $json, items[i]);

          // Handle result
          if (result !== undefined && result !== null) {
            returnData.push({
              json: typeof result === 'object' ? result : { value: result },
              pairedItem: i,
            });
          } else {
            returnData.push(items[i]);
          }
        } catch (error: any) {
          if (this.continueOnFail()) {
            returnData.push({
              json: {
                error: true,
                message: error.message,
                originalData: items[i].json,
              },
              pairedItem: i,
            });
          } else {
            throw new Error(`Code execution failed at item ${i}: ${error.message}`);
          }
        }
      }
    }

    return [returnData];
  }
}
