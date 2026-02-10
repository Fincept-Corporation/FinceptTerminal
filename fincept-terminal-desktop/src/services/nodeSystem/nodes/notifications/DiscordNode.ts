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
    displayName: 'Send Discord',
    name: 'discord',
    icon: 'fa:discord',
    group: ['notifications'],
    version: 1,
    description: 'Send Discord message via webhook',
    defaults: {
      name: 'Send Discord',
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
        placeholder: 'https://discord.com/api/webhooks/...',
        description: 'Discord webhook URL from channel settings',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Your message here...',
        description: 'Message content (supports Markdown)',
      },
      {
        displayName: 'Username',
        name: 'username',
        type: NodePropertyType.String,
        default: 'Fincept Terminal',
        placeholder: 'Bot Name',
        description: 'Override webhook username',
      },
      {
        displayName: 'Include Input Data',
        name: 'includeInputData',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether to include workflow input data',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const webhookUrl = this.getNodeParameter('webhookUrl', i) as string;
        let message = this.getNodeParameter('message', i) as string;
        const username = this.getNodeParameter('username', i, 'Fincept Terminal') as string;
        const includeInputData = this.getNodeParameter('includeInputData', i, true) as boolean;

        // Include input data if requested
        if (includeInputData && items[i].json) {
          const dataPreview = JSON.stringify(items[i].json, null, 2);
          message += `\n\`\`\`json\n${dataPreview}\n\`\`\``;
        }

        const payload: any = {
          content: message,
          username: username,
        };

        const response = await fetch(webhookUrl, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(payload),
        });

        console.log(`[DiscordNode] Message ${response.ok ? 'sent' : 'failed'}, status: ${response.status}`);

        outputItems.push({
          json: {
            ...items[i].json,
            notification: {
              platform: 'discord',
              success: response.ok,
              status: response.status,
              sentAt: new Date().toISOString(),
            },
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[DiscordNode] Error:', error);
        if (this.continueOnFail()) {
          outputItems.push({
            json: {
              ...items[i].json,
              notification: {
                platform: 'discord',
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

    return [outputItems];
  }
}
