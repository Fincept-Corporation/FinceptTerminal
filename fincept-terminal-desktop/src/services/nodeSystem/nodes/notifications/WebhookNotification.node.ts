import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData, IDataObject } from '../../types';

export class WebhookNotificationNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Webhook',
    name: 'webhook',
    icon: 'fa:satellite-dish',
    iconColor: '#3b82f6',
    group: ['notifications'],
    version: 1,
    description: 'Send data to webhook URL via HTTP request',
    defaults: { name: 'Webhook', color: '#3b82f6' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Webhook URL',
        name: 'url',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'https://example.com/webhook',
        description: 'The webhook URL to send data to',
      },
      {
        displayName: 'Method',
        name: 'method',
        type: NodePropertyType.Options,
        options: [
          { name: 'POST', value: 'POST' },
          { name: 'GET', value: 'GET' },
          { name: 'PUT', value: 'PUT' },
          { name: 'PATCH', value: 'PATCH' },
        ],
        default: 'POST',
        description: 'HTTP method to use',
      },
      {
        displayName: 'Send Input Data',
        name: 'sendInputData',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether to send the input data as the request body',
      },
      {
        displayName: 'Custom Body',
        name: 'customBody',
        type: NodePropertyType.String,
        default: '',
        typeOptions: {
          rows: 4,
        },
        displayOptions: {
          show: {
            sendInputData: [false],
          },
        },
        description: 'Custom JSON body to send (can use $json.fieldName)',
        placeholder: '{"message": "$json.message", "price": $json.price}',
      },
      {
        displayName: 'Headers',
        name: 'headers',
        type: NodePropertyType.FixedCollection,
        typeOptions: {
          multipleValues: true,
        },
        default: [],
        placeholder: 'Add Header',
        options: [
          {
            name: 'header',
            displayName: 'Header',
            values: [
              {
                displayName: 'Name',
                name: 'name',
                type: NodePropertyType.String,
                default: '',
                placeholder: 'Content-Type',
              },
              {
                displayName: 'Value',
                name: 'value',
                type: NodePropertyType.String,
                default: '',
                placeholder: 'application/json',
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
      try {
        const url = this.getNodeParameter('url', i) as string;
        const method = this.getNodeParameter('method', i) as string;
        const sendInputData = this.getNodeParameter('sendInputData', i) as boolean;
        const customBody = this.getNodeParameter('customBody', i, '') as string;
        const headersData = this.getNodeParameter('headers', i, { header: [] }) as {
          header: Array<{ name: string; value: string }>;
        };

        // Build headers
        const headers: IDataObject = {
          'Content-Type': 'application/json',
        };

        if (headersData.header && Array.isArray(headersData.header)) {
          for (const h of headersData.header) {
            if (h.name && h.value) {
              headers[h.name] = h.value;
            }
          }
        }

        // Build body
        let body: any;
        if (sendInputData) {
          body = items[i].json;
        } else if (customBody) {
          try {
            // Try to evaluate as expression
            const evaluatedBody = this.evaluateExpression(customBody, i);
            body = typeof evaluatedBody === 'string' ? JSON.parse(evaluatedBody) : evaluatedBody;
          } catch {
            // If evaluation fails, try to parse as JSON
            try {
              body = JSON.parse(customBody);
            } catch {
              body = { message: customBody };
            }
          }
        }

        // Make HTTP request
        const response = await this.helpers.httpRequest({
          url,
          method: method as any,
          headers,
          body: method !== 'GET' ? body : undefined,
          json: true,
        });

        returnData.push({
          json: {
            success: true,
            url,
            method,
            statusCode: response.statusCode || 200,
            response: response,
            sentData: body,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              success: false,
              error: error.message || 'Webhook request failed',
            },
            pairedItem: i,
          });
        } else {
          throw error;
        }
      }
    }

    return [returnData];
  }
}
