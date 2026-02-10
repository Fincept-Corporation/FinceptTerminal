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
    displayName: 'Send Telegram',
    name: 'telegram',
    icon: 'fa:telegram',
    group: ['notifications'],
    version: 1,
    description: 'Send Telegram message via Bot API',
    defaults: {
      name: 'Send Telegram',
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
        placeholder: '123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11',
        description: 'Telegram Bot API token from @BotFather',
        typeOptions: {
          password: true,
        },
      },
      {
        displayName: 'Chat ID',
        name: 'chatId',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: '123456789 or @channel_name',
        description: 'Chat ID or channel username',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Your message here...',
        description: 'Message text to send',
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
        default: 'Markdown',
        description: 'Message formatting mode',
      },
      {
        displayName: 'Include Input Data',
        name: 'includeInputData',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether to include workflow input data in message',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const botToken = this.getNodeParameter('botToken', i) as string;
        const chatId = this.getNodeParameter('chatId', i) as string;
        let message = this.getNodeParameter('message', i) as string;
        const parseMode = this.getNodeParameter('parseMode', i) as string;
        const includeInputData = this.getNodeParameter('includeInputData', i, true) as boolean;

        // Include input data if requested
        if (includeInputData && items[i].json) {
          const dataPreview = JSON.stringify(items[i].json, null, 2);
          if (parseMode === 'Markdown') {
            message += `\n\n\`\`\`json\n${dataPreview}\n\`\`\``;
          } else if (parseMode === 'HTML') {
            message += `\n\n<pre>${dataPreview}</pre>`;
          } else {
            message += `\n\nData: ${dataPreview}`;
          }
        }

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

        console.log(`[TelegramNode] Message ${response.ok ? 'sent' : 'failed'} to chat ${chatId}`);

        outputItems.push({
          json: {
            ...items[i].json,
            notification: {
              platform: 'telegram',
              chatId,
              success: response.ok,
              messageId: result.result?.message_id,
              sentAt: new Date().toISOString(),
            },
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[TelegramNode] Error:', error);
        if (this.continueOnFail()) {
          outputItems.push({
            json: {
              ...items[i].json,
              notification: {
                platform: 'telegram',
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
