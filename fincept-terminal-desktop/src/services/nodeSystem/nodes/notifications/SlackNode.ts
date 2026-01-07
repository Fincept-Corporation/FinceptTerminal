import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class SlackNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Slack',
    name: 'slack',
    icon: 'fa:slack',
    group: ['Notification'],
    version: 1,
    description: 'Send Slack message',
    defaults: {
      name: 'Slack',
      color: '#4a154b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Webhook URL',
        name: 'webhookUrl',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Slack incoming webhook URL',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Message text',
      },
      {
        displayName: 'Channel',
        name: 'channel',
        type: NodePropertyType.String,
        default: '',
        description: 'Override default channel (e.g., #general)',
      },
      {
        displayName: 'Username',
        name: 'username',
        type: NodePropertyType.String,
        default: '',
        description: 'Override username',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const webhookUrl = this.getNodeParameter('webhookUrl', 0) as string;
    const message = this.getNodeParameter('message', 0) as string;
    const channel = this.getNodeParameter('channel', 0) as string;
    const username = this.getNodeParameter('username', 0) as string;

    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const payload: any = {
          text: message,
        };

        if (channel) {
          payload.channel = channel;
        }

        if (username) {
          payload.username = username;
        }

        const response = await fetch(webhookUrl, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(payload),
        });

        outputItems.push({
          json: {
            ...items[i].json,
            success: response.ok,
            status: response.status,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          outputItems.push({
            json: {
              ...items[i].json,
              error: error.message,
              success: false,
            },
            pairedItem: i,
          });
        } else {
          throw error;
        }
      }
    }

    return [outputItems];
  }
}
