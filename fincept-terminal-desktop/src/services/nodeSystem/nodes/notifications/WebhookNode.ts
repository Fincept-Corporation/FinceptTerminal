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
    displayName: 'Webhook',
    name: 'webhookNotification',
    icon: 'fa:paper-plane',
    group: ['Notifications'],
    version: 1,
    description: 'Send HTTP webhook notifications',
    defaults: {
      name: 'Webhook',
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
      },
      {
        displayName: 'Method',
        name: 'method',
        type: NodePropertyType.Options,
        options: [
          { name: 'POST', value: 'POST' },
          { name: 'PUT', value: 'PUT' },
        ],
        default: 'POST',
      },
      {
        displayName: 'Headers',
        name: 'headers',
        type: NodePropertyType.Json,
        default: '{}',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const url = this.getNodeParameter('url', 0) as string;
    const method = this.getNodeParameter('method', 0) as string;
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const response = await fetch(url, {
          method,
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(items[i].json),
        });

        returnData.push({
          json: {
            ...items[i].json,
            webhookResponse: await response.json(),
            status: response.status,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: { ...items[i].json, error: error.message },
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
