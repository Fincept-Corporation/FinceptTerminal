import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData, IDataObject } from '../../types';

export class SlackNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Slack',
    name: 'slack',
    icon: 'fa:slack',
    iconColor: '#4a154b',
    group: ['notifications'],
    version: 1,
    description: 'Send messages to Slack via webhooks',
    defaults: { name: 'Slack', color: '#4a154b' },
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
        description: 'Slack webhook URL from app settings',
        typeOptions: {
          password: true,
        },
      },
      {
        displayName: 'Channel',
        name: 'channel',
        type: NodePropertyType.String,
        default: '',
        placeholder: '#trading-alerts',
        description: 'Channel to post to (optional, uses webhook default)',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        typeOptions: {
          rows: 4,
        },
        placeholder: ':chart_with_upwards_trend: $json.symbol reached $json.price',
        description: 'Message text (supports Slack markdown and expressions)',
      },
      {
        displayName: 'Username',
        name: 'username',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'Trading Bot',
        description: 'Override default webhook username',
      },
      {
        displayName: 'Icon Emoji',
        name: 'iconEmoji',
        type: NodePropertyType.String,
        default: '',
        placeholder: ':robot_face:',
        description: 'Emoji to use as the icon (e.g., :chart:)',
      },
      {
        displayName: 'Use Blocks',
        name: 'useBlocks',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use Block Kit for rich formatting',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const webhookUrl = this.getNodeParameter('webhookUrl', i) as string;
        const channel = this.getNodeParameter('channel', i, '') as string;
        let message = this.getNodeParameter('message', i) as string;
        const username = this.getNodeParameter('username', i, '') as string;
        const iconEmoji = this.getNodeParameter('iconEmoji', i, '') as string;
        const useBlocks = this.getNodeParameter('useBlocks', i) as boolean;

        // Evaluate expressions
        try {
          if (message.includes('$json')) {
            message = this.evaluateExpression(message, i) as string;
          }
        } catch {
          // Use literal message if expression evaluation fails
        }

        // Build Slack payload
        const body: IDataObject = {};

        if (channel) {
          body.channel = channel;
        }

        if (username) {
          body.username = username;
        }

        if (iconEmoji) {
          body.icon_emoji = iconEmoji;
        }

        if (useBlocks) {
          body.blocks = [
            {
              type: 'section',
              text: {
                type: 'mrkdwn',
                text: message,
              },
            },
            {
              type: 'context',
              elements: [
                {
                  type: 'mrkdwn',
                  text: `_Sent at ${new Date().toLocaleString()}_`,
                },
              ],
            },
          ];
        } else {
          body.text = message;
        }

        // Send to Slack webhook
        const response = await this.helpers.httpRequest({
          url: webhookUrl,
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body,
          json: true,
        });

        returnData.push({
          json: {
            success: true,
            channel: channel || 'default',
            message,
            useBlocks,
            response: response || { status: 'ok' },
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              success: false,
              error: error.message || 'Slack message failed',
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
