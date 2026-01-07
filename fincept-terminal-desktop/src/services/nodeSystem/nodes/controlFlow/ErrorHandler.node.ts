import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData } from '../../types';

export class ErrorHandlerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Error Handler',
    name: 'errorHandler',
    icon: 'fa:exclamation-triangle',
    iconColor: '#f97316',
    group: ['controlFlow'],
    version: 1,
    description: 'Handle errors gracefully and continue workflow',
    defaults: { name: 'Error Handler', color: '#f97316' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main, NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Continue on Error',
        name: 'continueOnError',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether to continue processing other items if one fails',
      },
      {
        displayName: 'Error Output Mode',
        name: 'errorOutputMode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Detailed', value: 'detailed', description: 'Include error details and stack trace' },
          { name: 'Simple', value: 'simple', description: 'Include only error message' },
        ],
        default: 'detailed',
      },
      {
        displayName: 'Default Value on Error',
        name: 'defaultValue',
        type: NodePropertyType.String,
        default: '',
        placeholder: '{"status": "error"}',
        description: 'JSON object to return when error occurs (optional)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const continueOnError = this.getNodeParameter('continueOnError', 0) as boolean;
    const errorOutputMode = this.getNodeParameter('errorOutputMode', 0) as string;
    const defaultValue = this.getNodeParameter('defaultValue', 0, '') as string;

    const successItems: INodeExecutionData[] = [];
    const errorItems: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        // Check if item has error from previous node
        if (items[i].json.error) {
          throw new Error(String(items[i].json.error));
        }

        // Item processed successfully
        successItems.push(items[i]);
      } catch (error: any) {
        if (continueOnError) {
          // Create error output
          const errorData: any = {
            error: true,
            message: error.message || 'Unknown error',
            originalData: items[i].json,
          };

          if (errorOutputMode === 'detailed') {
            errorData.stack = error.stack;
            errorData.timestamp = new Date().toISOString();
          }

          // Use default value if provided
          if (defaultValue) {
            try {
              const defaultObj = JSON.parse(defaultValue);
              Object.assign(errorData, defaultObj);
            } catch {
              errorData.defaultValue = defaultValue;
            }
          }

          errorItems.push({ json: errorData, pairedItem: i });
        } else {
          // Re-throw error if not continuing
          throw error;
        }
      }
    }

    // Output 0: Success items
    // Output 1: Error items
    return [successItems, errorItems];
  }
}
