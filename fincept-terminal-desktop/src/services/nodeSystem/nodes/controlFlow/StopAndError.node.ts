import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType } from '../../types';

export class StopAndErrorNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Stop and Error',
    name: 'stopAndError',
    icon: 'fa:hand-paper',
    iconColor: '#dc2626',
    group: ['controlFlow'],
    version: 1,
    description: 'Halt workflow execution with an error',
    defaults: { name: 'Stop and Error', color: '#dc2626' },
    inputs: [NodeConnectionType.Main],
    outputs: [],
    properties: [
      {
        displayName: 'Error Message',
        name: 'errorMessage',
        type: NodePropertyType.String,
        default: 'Workflow stopped',
        typeOptions: {
          rows: 3,
        },
        description: 'Error message to display (supports expressions)',
        placeholder: 'Invalid data: $json.error',
      },
      {
        displayName: 'Include Input Data',
        name: 'includeInputData',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Whether to include input data in error',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    let errorMessage = this.getNodeParameter('errorMessage', 0) as string;
    const includeInputData = this.getNodeParameter('includeInputData', 0) as boolean;

    // Evaluate expression in error message
    try {
      if (errorMessage.includes('$json')) {
        errorMessage = this.evaluateExpression(errorMessage, 0) as string;
      }
    } catch {
      // Use literal message if expression evaluation fails
    }

    // Build error object
    const errorData: any = {
      message: errorMessage,
      timestamp: new Date().toISOString(),
      nodeType: 'Stop and Error',
    };

    if (includeInputData && items.length > 0) {
      errorData.inputData = items.map(item => item.json);
    }

    // Throw error to stop workflow
    throw new Error(JSON.stringify(errorData, null, 2));
  }
}
