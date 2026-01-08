import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData, IDataObject } from '../../types';

export class DiscordNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Discord',
    name: 'discord',
    icon: 'fa:discord',
    iconColor: '#5865f2',
    group: ['notifications'],
    version: 1,
    description: 'Send messages to Discord via webhooks',
    defaults: { name: 'Discord', color: '#5865f2' },
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
        typeOptions: {
          password: true,
        },
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
        placeholder: '📊 Trading Alert: $json.symbol at $json.price',
        description: 'Message content (supports expressions)',
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
        displayName: 'Use Embeds',
        name: 'useEmbeds',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Send as a rich embed instead of plain text',
      },
      {
        displayName: 'Embed Title',
        name: 'embedTitle',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'Price Alert',
        displayOptions: {
          show: {
            useEmbeds: [true],
          },
        },
        description: 'Title of the embed',
      },
      {
        displayName: 'Embed Color',
        name: 'embedColor',
        type: NodePropertyType.Color,
        default: '#5865f2',
        displayOptions: {
          show: {
            useEmbeds: [true],
          },
        },
        description: 'Color of the embed sidebar',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const webhookUrl = this.getNodeParameter('webhookUrl', i) as string;
        let message = this.getNodeParameter('message', i) as string;
        const username = this.getNodeParameter('username', i, '') as string;
        const useEmbeds = this.getNodeParameter('useEmbeds', i) as boolean;
        let embedTitle = this.getNodeParameter('embedTitle', i, '') as string;
        const embedColor = this.getNodeParameter('embedColor', i, '#5865f2') as string;

        // Evaluate expressions
        try {
          if (message.includes('$json')) message = this.evaluateExpression(message, i) as string;
          if (embedTitle.includes('$json')) embedTitle = this.evaluateExpression(embedTitle, i) as string;
        } catch {
          // Use literal values if expression evaluation fails
        }

        // Build Discord webhook payload
        const body: IDataObject = {};

        if (username) {
          body.username = username;
        }

        if (useEmbeds) {
          // Convert hex color to decimal
          const colorDecimal = parseInt(embedColor.replace('#', ''), 16);

          body.embeds = [
            {
              title: embedTitle || 'Notification',
              description: message,
              color: colorDecimal,
              timestamp: new Date().toISOString(),
            },
          ];
        } else {
          body.content = message;
        }

        // Send to Discord webhook
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
            message,
            useEmbeds,
            response: response || { status: 'sent' },
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              success: false,
              error: error.message || 'Discord message failed',
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
