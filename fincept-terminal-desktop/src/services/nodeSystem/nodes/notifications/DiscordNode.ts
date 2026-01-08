import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class DiscordNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Discord',
    name: 'discord',
    icon: 'fa:discord',
    group: ['Notification'],
    version: 1,
    description: 'Send Discord message via webhook',
    defaults: {
      name: 'Discord',
      color: '#5865f2',
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
        description: 'Discord webhook URL',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Message content',
      },
      {
        displayName: 'Username',
        name: 'username',
        type: NodePropertyType.String,
        default: '',
        description: 'Override webhook username',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const webhookUrl = this.getNodeParameter('webhookUrl', 0) as string;
    const message = this.getNodeParameter('message', 0) as string;
    const username = this.getNodeParameter('username', 0) as string;

    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const payload: any = {
          content: message,
        };

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
