/**
 * Webhook Trigger Node
 * Triggers workflow from external HTTP webhook
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class WebhookTriggerNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'API Webhook',
    name: 'webhookTrigger',
    icon: 'file:webhook.svg',
    group: ['trigger'],
    version: 1,
    subtitle: '={{$parameter["httpMethod"]}} {{$parameter["path"]}}',
    description: 'Triggers workflow from external HTTP webhook calls',
    defaults: {
      name: 'Webhook',
      color: '#f59e0b',
    },
    inputs: [],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'HTTP Method',
        name: 'httpMethod',
        type: NodePropertyType.Options,
        default: 'POST',
        options: [
          { name: 'GET', value: 'GET' },
          { name: 'POST', value: 'POST' },
          { name: 'PUT', value: 'PUT' },
          { name: 'PATCH', value: 'PATCH' },
          { name: 'DELETE', value: 'DELETE' },
        ],
        description: 'HTTP method to accept',
      },
      {
        displayName: 'Webhook Path',
        name: 'path',
        type: NodePropertyType.String,
        default: '/webhook',
        required: true,
        placeholder: '/my-webhook',
        description: 'Path for the webhook URL',
      },
      {
        displayName: 'Authentication',
        name: 'authentication',
        type: NodePropertyType.Options,
        default: 'none',
        options: [
          { name: 'None', value: 'none', description: 'No authentication required' },
          { name: 'API Key (Header)', value: 'headerAuth', description: 'API key in header' },
          { name: 'API Key (Query)', value: 'queryAuth', description: 'API key in query string' },
          { name: 'Basic Auth', value: 'basicAuth', description: 'Basic HTTP authentication' },
        ],
        description: 'Authentication method for webhook',
      },
      {
        displayName: 'Header Name',
        name: 'headerAuthName',
        type: NodePropertyType.String,
        default: 'X-API-Key',
        description: 'Header name for API key',
        displayOptions: {
          show: { authentication: ['headerAuth'] },
        },
      },
      {
        displayName: 'API Key',
        name: 'apiKey',
        type: NodePropertyType.String,
        default: '',
        description: 'Expected API key value',
        typeOptions: { password: true },
        displayOptions: {
          show: { authentication: ['headerAuth', 'queryAuth'] },
        },
      },
      {
        displayName: 'Query Parameter Name',
        name: 'queryAuthName',
        type: NodePropertyType.String,
        default: 'api_key',
        description: 'Query parameter name for API key',
        displayOptions: {
          show: { authentication: ['queryAuth'] },
        },
      },
      {
        displayName: 'Response Mode',
        name: 'responseMode',
        type: NodePropertyType.Options,
        default: 'onReceived',
        options: [
          { name: 'Immediately', value: 'onReceived', description: 'Respond immediately when webhook is received' },
          { name: 'When Last Node Finishes', value: 'lastNode', description: 'Respond after workflow completes' },
        ],
        description: 'When to send webhook response',
      },
      {
        displayName: 'Response Code',
        name: 'responseCode',
        type: NodePropertyType.Number,
        default: 200,
        description: 'HTTP status code to return',
      },
      {
        displayName: 'Response Data',
        name: 'responseData',
        type: NodePropertyType.Options,
        default: 'allEntries',
        options: [
          { name: 'All Entries', value: 'allEntries', description: 'Return all workflow data' },
          { name: 'First Entry JSON', value: 'firstEntryJson', description: 'Return first entry as JSON' },
          { name: 'Custom', value: 'custom', description: 'Custom response body' },
          { name: 'No Response Body', value: 'noData', description: 'Return empty body' },
        ],
        description: 'Data to include in response',
        displayOptions: {
          show: { responseMode: ['lastNode'] },
        },
      },
      {
        displayName: 'Custom Response',
        name: 'customResponse',
        type: NodePropertyType.Json,
        default: '{"status": "ok"}',
        description: 'Custom JSON response body',
        displayOptions: {
          show: { responseData: ['custom'] },
        },
      },
      {
        displayName: 'IP Whitelist',
        name: 'ipWhitelist',
        type: NodePropertyType.String,
        default: '',
        placeholder: '192.168.1.1, 10.0.0.0/8',
        description: 'Comma-separated list of allowed IPs/CIDRs (leave empty for all)',
      },
      {
        displayName: 'Rate Limit',
        name: 'rateLimit',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Max requests per minute (0 for unlimited)',
      },
    ],
    hints: [
      {
        message: 'Webhook URL will be available after deployment',
        type: 'info',
        location: 'outputPane',
        whenToDisplay: 'always',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const httpMethod = this.getNodeParameter('httpMethod', 0) as string;
    const path = this.getNodeParameter('path', 0) as string;
    const authentication = this.getNodeParameter('authentication', 0) as string;
    const responseCode = this.getNodeParameter('responseCode', 0) as number;

    // Generate webhook URL (in production, this would come from the server)
    const webhookId = `wh_${Date.now().toString(36)}`;
    const webhookUrl = `https://hooks.fincept.app${path.startsWith('/') ? path : '/' + path}`;

    return [[{
      json: {
        trigger: 'webhook',
        webhookId,
        webhookUrl,
        httpMethod,
        path,
        authentication,
        responseCode,
        timestamp: new Date().toISOString(),
        executionId: this.getExecutionId(),
        status: 'listening',
        message: `Webhook listening at ${webhookUrl}`,
        // In real execution, these would contain the actual webhook data:
        headers: {},
        body: {},
        query: {},
      },
    }]];
  }
}
