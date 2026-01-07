import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData } from '../../types';

export class TelegramNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Telegram',
    name: 'telegram',
    icon: 'fa:telegram',
    iconColor: '#0088cc',
    group: ['notifications'],
    version: 1,
    description: 'Send messages via Telegram Bot API',
    defaults: { name: 'Telegram', color: '#0088cc' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Bot Token',
        name: 'token',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: '123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11',
        description: 'Telegram bot token from @BotFather',
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
        placeholder: '123456789',
        description: 'Telegram chat ID or channel username (@channel)',
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
        placeholder: '🚨 Alert: $json.symbol price is $json.price',
        description: 'Message text (supports expressions with $json)',
      },
      {
        displayName: 'Parse Mode',
        name: 'parseMode',
        type: NodePropertyType.Options,
        options: [
          { name: 'None', value: '' },
          { name: 'Markdown', value: 'Markdown' },
          { name: 'MarkdownV2', value: 'MarkdownV2' },
          { name: 'HTML', value: 'HTML' },
        ],
        default: '',
        description: 'Format for the message text',
      },
      {
        displayName: 'Disable Notification',
        name: 'disableNotification',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Send message silently (users will receive without sound)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const token = this.getNodeParameter('token', i) as string;
        const chatId = this.getNodeParameter('chatId', i) as string;
        let message = this.getNodeParameter('message', i) as string;
        const parseMode = this.getNodeParameter('parseMode', i, '') as string;
        const disableNotification = this.getNodeParameter('disableNotification', i) as boolean;

        // Evaluate expressions in message
        try {
          if (message.includes('$json')) {
            message = this.evaluateExpression(message, i) as string;
          }
        } catch {
          // Use literal message if expression evaluation fails
        }

        // Build request body
        const body: any = {
          chat_id: chatId,
          text: message,
          disable_notification: disableNotification,
        };

        if (parseMode) {
          body.parse_mode = parseMode;
        }

        // Send to Telegram API
        const url = `https://api.telegram.org/bot${token}/sendMessage`;
        const response = await this.helpers.httpRequest({
          url,
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
            chatId,
            messageId: response.result?.message_id,
            message,
            response,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              success: false,
              error: error.message || 'Telegram message failed',
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
