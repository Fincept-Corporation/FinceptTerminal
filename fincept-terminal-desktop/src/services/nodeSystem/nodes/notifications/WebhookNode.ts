import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class WebhookNotificationNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Send Webhook',
    name: 'webhookNotification',
    icon: 'fa:paper-plane',
    group: ['notifications'],
    version: 1,
    description: 'Send HTTP webhook notifications',
    defaults: {
      name: 'Send Webhook',
      color: '#6366f1',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'URL',
        name: 'url',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'https://your-webhook-url.com/endpoint',
        description: 'HTTP endpoint to send webhook to',
      },
      {
        displayName: 'Method',
        name: 'method',
        type: NodePropertyType.Options,
        options: [
          { name: 'GET', value: 'GET' },
          { name: 'POST', value: 'POST' },
          { name: 'PUT', value: 'PUT' },
          { name: 'PATCH', value: 'PATCH' },
          { name: 'DELETE', value: 'DELETE' },
        ],
        default: 'POST',
        description: 'HTTP method to use',
      },
      {
        displayName: 'Headers',
        name: 'headers',
        type: NodePropertyType.Json,
        default: '{"Content-Type": "application/json"}',
        description: 'Custom HTTP headers as JSON object',
      },
      {
        displayName: 'Send Input Data',
        name: 'sendInputData',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether to send workflow input data as request body',
      },
      {
        displayName: 'Custom Body',
        name: 'customBody',
        type: NodePropertyType.Json,
        default: '',
        placeholder: '{"message": "Workflow completed"}',
        description: 'Custom JSON body (overrides input data if provided)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const url = this.getNodeParameter('url', i) as string;
        const method = this.getNodeParameter('method', i) as string;
        const headersJson = this.getNodeParameter('headers', i, '{}') as string;
        const sendInputData = this.getNodeParameter('sendInputData', i, true) as boolean;
        const customBodyJson = this.getNodeParameter('customBody', i, '') as string;

        // Parse headers
        let headers: Record<string, string> = {};
        try {
          headers = JSON.parse(headersJson);
        } catch {
          headers = { 'Content-Type': 'application/json' };
        }

        // Prepare body
        let body: any = null;
        if (customBodyJson) {
          try {
            body = JSON.parse(customBodyJson);
          } catch {
            body = { data: customBodyJson };
          }
        } else if (sendInputData) {
          body = items[i].json;
        }

        // Make request
        const requestOptions: RequestInit = {
          method,
          headers,
        };

        if (body && method !== 'GET' && method !== 'DELETE') {
          requestOptions.body = JSON.stringify(body);
        }

        const response = await fetch(url, requestOptions);

        let responseData: any;
        const contentType = response.headers.get('content-type');
        if (contentType?.includes('application/json')) {
          responseData = await response.json();
        } else {
          responseData = await response.text();
        }

        console.log(`[WebhookNode] ${method} request to ${url}, status: ${response.status}`);

        returnData.push({
          json: {
            ...items[i].json,
            notification: {
              platform: 'webhook',
              url,
              method,
              success: response.ok,
              status: response.status,
              response: responseData,
              sentAt: new Date().toISOString(),
            },
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[WebhookNode] Error:', error);
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              ...items[i].json,
              notification: {
                platform: 'webhook',
                success: false,
                error: error.message,
              },
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
