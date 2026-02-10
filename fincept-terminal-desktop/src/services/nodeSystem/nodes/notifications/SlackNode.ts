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
    displayName: 'Send Slack',
    name: 'slack',
    icon: 'fa:slack',
    group: ['notifications'],
    version: 1,
    description: 'Send Slack message via webhook',
    defaults: {
      name: 'Send Slack',
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
        placeholder: 'https://hooks.slack.com/services/...',
        description: 'Slack incoming webhook URL',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Your message here...',
        description: 'Message text (supports Slack formatting)',
      },
      {
        displayName: 'Channel',
        name: 'channel',
        type: NodePropertyType.String,
        default: '',
        placeholder: '#general',
        description: 'Override default channel',
      },
      {
        displayName: 'Username',
        name: 'username',
        type: NodePropertyType.String,
        default: 'Fincept Terminal',
        placeholder: 'Bot Name',
        description: 'Override username',
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
        const channel = this.getNodeParameter('channel', i) as string;
        const username = this.getNodeParameter('username', i, 'Fincept Terminal') as string;
        const includeInputData = this.getNodeParameter('includeInputData', i, true) as boolean;

        // Include input data if requested
        if (includeInputData && items[i].json) {
          const dataPreview = JSON.stringify(items[i].json, null, 2);
          message += `\n\`\`\`\n${dataPreview}\n\`\`\``;
        }

        const payload: any = {
          text: message,
          username: username,
        };

        if (channel) {
          payload.channel = channel;
        }

        const response = await fetch(webhookUrl, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(payload),
        });

        console.log(`[SlackNode] Message ${response.ok ? 'sent' : 'failed'}, status: ${response.status}`);

        outputItems.push({
          json: {
            ...items[i].json,
            notification: {
              platform: 'slack',
              channel: channel || 'default',
              success: response.ok,
              status: response.status,
              sentAt: new Date().toISOString(),
            },
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[SlackNode] Error:', error);
        if (this.continueOnFail()) {
          outputItems.push({
            json: {
              ...items[i].json,
              notification: {
                platform: 'slack',
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
