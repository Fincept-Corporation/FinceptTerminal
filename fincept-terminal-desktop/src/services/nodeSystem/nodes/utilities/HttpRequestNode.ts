import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class HttpRequestNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'API Request',
    name: 'httpRequest',
    icon: 'fa:globe',
    group: ['utilities'],
    version: 1,
    description: 'Make HTTP requests to any API endpoint',
    defaults: {
      name: 'API Request',
      color: '#0066cc',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
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
          { name: 'HEAD', value: 'HEAD' },
          { name: 'OPTIONS', value: 'OPTIONS' },
        ],
        default: 'GET',
        required: true,
        description: 'HTTP method to use',
      },
      {
        displayName: 'URL',
        name: 'url',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'https://api.example.com/endpoint',
        description: 'Full URL to send request to',
      },
      {
        displayName: 'Headers',
        name: 'headers',
        type: NodePropertyType.Json,
        default: '{"Content-Type": "application/json"}',
        description: 'HTTP headers as JSON object',
      },
      {
        displayName: 'Query Parameters',
        name: 'queryParams',
        type: NodePropertyType.Json,
        default: '{}',
        description: 'URL query parameters as JSON object',
      },
      {
        displayName: 'Body',
        name: 'body',
        type: NodePropertyType.Json,
        default: '',
        placeholder: '{"key": "value"}',
        description: 'Request body (JSON)',
      },
      {
        displayName: 'Response Format',
        name: 'responseFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'JSON', value: 'json' },
          { name: 'Text', value: 'text' },
          { name: 'Binary', value: 'binary' },
        ],
        default: 'json',
        description: 'Expected response format',
      },
      {
        displayName: 'Timeout (ms)',
        name: 'timeout',
        type: NodePropertyType.Number,
        default: 30000,
        description: 'Request timeout in milliseconds',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const method = this.getNodeParameter('method', i) as string;
        let url = this.getNodeParameter('url', i) as string;
        const headersJson = this.getNodeParameter('headers', i, '{}') as string;
        const queryParamsJson = this.getNodeParameter('queryParams', i, '{}') as string;
        const bodyJson = this.getNodeParameter('body', i, '') as string;
        const responseFormat = this.getNodeParameter('responseFormat', i, 'json') as string;
        const timeout = this.getNodeParameter('timeout', i, 30000) as number;

        // Parse headers
        let headers: Record<string, string> = {};
        try {
          headers = JSON.parse(headersJson);
        } catch {
          headers = { 'Content-Type': 'application/json' };
        }

        // Parse and append query parameters
        try {
          const queryParams = JSON.parse(queryParamsJson);
          const params = new URLSearchParams(queryParams);
          if (params.toString()) {
            url += (url.includes('?') ? '&' : '?') + params.toString();
          }
        } catch (e) {
          console.warn('[HttpRequestNode] Invalid query params:', e);
        }

        // Prepare request options
        const requestOptions: RequestInit = {
          method,
          headers,
        };

        // Add body for non-GET requests
        if (bodyJson && method !== 'GET' && method !== 'HEAD') {
          try {
            requestOptions.body = JSON.stringify(JSON.parse(bodyJson));
          } catch {
            requestOptions.body = bodyJson;
          }
        }

        // Make request with timeout
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), timeout);
        requestOptions.signal = controller.signal;

        const response = await fetch(url, requestOptions);
        clearTimeout(timeoutId);

        // Parse response
        let responseData: any;
        if (responseFormat === 'json') {
          const text = await response.text();
          try {
            responseData = JSON.parse(text);
          } catch {
            responseData = text;
          }
        } else if (responseFormat === 'binary') {
          responseData = await response.arrayBuffer();
        } else {
          responseData = await response.text();
        }

        console.log(`[HttpRequestNode] ${method} ${url} - Status: ${response.status}`);

        returnData.push({
          json: {
            statusCode: response.status,
            headers: Object.fromEntries(response.headers.entries()),
            body: responseData,
            ...items[i].json,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[HttpRequestNode] Error:', error);
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              error: error.message,
              success: false,
              ...items[i].json,
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
