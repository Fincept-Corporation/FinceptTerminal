import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class TelegramNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Telegram',
    name: 'telegram',
    icon: 'fa:telegram',
    group: ['Notification'],
    version: 1,
    description: 'Send Telegram message',
    defaults: {
      name: 'Telegram',
      color: '#0088cc',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Bot Token',
        name: 'botToken',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Telegram bot token',
      },
      {
        displayName: 'Chat ID',
        name: 'chatId',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Telegram chat ID',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Message to send',
      },
      {
        displayName: 'Parse Mode',
        name: 'parseMode',
        type: NodePropertyType.Options,
        options: [
          { name: 'None', value: '' },
          { name: 'Markdown', value: 'Markdown' },
          { name: 'HTML', value: 'HTML' },
        ],
        default: '',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const botToken = this.getNodeParameter('botToken', 0) as string;
    const chatId = this.getNodeParameter('chatId', 0) as string;
    const message = this.getNodeParameter('message', 0) as string;
    const parseMode = this.getNodeParameter('parseMode', 0) as string;

    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const url = `https://api.telegram.org/bot${botToken}/sendMessage`;

        const payload: any = {
          chat_id: chatId,
          text: message,
        };

        if (parseMode) {
          payload.parse_mode = parseMode;
        }

        const response = await fetch(url, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
          },
          body: JSON.stringify(payload),
        });

        const result = await response.json();

        outputItems.push({
          json: {
            ...items[i].json,
            telegramResponse: result,
            success: response.ok,
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
