import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData, IDataObject } from '../../types';

export class SMSNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'SMS',
    name: 'sms',
    icon: 'fa:mobile',
    iconColor: '#10b981',
    group: ['notifications'],
    version: 1,
    description: 'Send SMS notifications via Twilio or SMS API',
    defaults: { name: 'SMS', color: '#10b981' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'SMS Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        options: [
          { name: 'Twilio', value: 'twilio' },
          { name: 'Custom API', value: 'custom' },
        ],
        default: 'twilio',
        description: 'SMS service provider',
      },
      {
        displayName: 'Account SID',
        name: 'accountSid',
        type: NodePropertyType.String,
        default: '',
        required: true,
        displayOptions: {
          show: {
            provider: ['twilio'],
          },
        },
        placeholder: 'ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx',
        description: 'Twilio Account SID',
        typeOptions: {
          password: true,
        },
      },
      {
        displayName: 'Auth Token',
        name: 'authToken',
        type: NodePropertyType.String,
        default: '',
        required: true,
        displayOptions: {
          show: {
            provider: ['twilio'],
          },
        },
        placeholder: 'your_auth_token',
        description: 'Twilio Auth Token',
        typeOptions: {
          password: true,
        },
      },
      {
        displayName: 'API URL',
        name: 'apiUrl',
        type: NodePropertyType.String,
        default: '',
        required: true,
        displayOptions: {
          show: {
            provider: ['custom'],
          },
        },
        placeholder: 'https://api.example.com/sms',
        description: 'Custom SMS API endpoint',
      },
      {
        displayName: 'From Number',
        name: 'fromNumber',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: '+1234567890',
        description: 'Sender phone number (with country code)',
      },
      {
        displayName: 'To Number',
        name: 'toNumber',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: '+1234567890 or $json.phone',
        description: 'Recipient phone number (supports expressions)',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        typeOptions: {
          rows: 3,
        },
        placeholder: 'Alert: $json.symbol at $json.price',
        description: 'SMS message text (max 160 chars recommended, supports expressions)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const provider = this.getNodeParameter('provider', i) as string;
        const fromNumber = this.getNodeParameter('fromNumber', i) as string;
        let toNumber = this.getNodeParameter('toNumber', i) as string;
        let message = this.getNodeParameter('message', i) as string;

        // Evaluate expressions
        try {
          if (toNumber.includes('$json')) toNumber = this.evaluateExpression(toNumber, i) as string;
          if (message.includes('$json')) message = this.evaluateExpression(message, i) as string;
        } catch {
          // Use literal values if expression evaluation fails
        }

        let response: any;

        if (provider === 'twilio') {
          const accountSid = this.getNodeParameter('accountSid', i) as string;
          const authToken = this.getNodeParameter('authToken', i) as string;

          // Twilio API
          const url = `https://api.twilio.com/2010-04-01/Accounts/${accountSid}/Messages.json`;

          const body = new URLSearchParams({
            From: fromNumber,
            To: toNumber,
            Body: message,
          });

          response = await this.helpers.httpRequest({
            url,
            method: 'POST',
            headers: {
              'Content-Type': 'application/x-www-form-urlencoded',
            },
            auth: {
              username: accountSid,
              password: authToken,
            },
            body: body.toString(),
          });
        } else {
          // Custom API
          const apiUrl = this.getNodeParameter('apiUrl', i) as string;

          response = await this.helpers.httpRequest({
            url: apiUrl,
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
            },
            body: {
              from: fromNumber,
              to: toNumber,
              message,
            },
            json: true,
          });
        }

        returnData.push({
          json: {
            success: true,
            provider,
            to: toNumber,
            from: fromNumber,
            message,
            messageId: response.sid || response.messageId || 'unknown',
            response,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              success: false,
              error: error.message || 'SMS sending failed',
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
