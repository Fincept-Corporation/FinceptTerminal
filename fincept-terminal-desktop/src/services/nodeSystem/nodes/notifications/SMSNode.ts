import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class SMSNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Send SMS',
    name: 'sms',
    icon: 'fa:comment',
    group: ['notifications'],
    version: 1,
    description: 'Send SMS via Twilio',
    defaults: {
      name: 'Send SMS',
      color: '#f22f46',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Account SID',
        name: 'accountSid',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AC...',
        description: 'Twilio Account SID from console',
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
        placeholder: 'Your auth token',
        description: 'Twilio Auth Token from console',
        typeOptions: {
          password: true,
        },
      },
      {
        displayName: 'From Number',
        name: 'fromNumber',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: '+1234567890',
        description: 'Twilio phone number (E.164 format)',
      },
      {
        displayName: 'To Number',
        name: 'toNumber',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: '+1234567890',
        description: 'Recipient phone number (E.164 format)',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Your message here...',
        description: 'SMS message text (max 160 chars recommended)',
      },
      {
        displayName: 'Include Input Data',
        name: 'includeInputData',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Whether to include workflow input data (may exceed SMS limit)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const accountSid = this.getNodeParameter('accountSid', i) as string;
        const authToken = this.getNodeParameter('authToken', i) as string;
        const fromNumber = this.getNodeParameter('fromNumber', i) as string;
        const toNumber = this.getNodeParameter('toNumber', i) as string;
        let message = this.getNodeParameter('message', i) as string;
        const includeInputData = this.getNodeParameter('includeInputData', i, false) as boolean;

        // Include input data if requested (warn about SMS limits)
        if (includeInputData && items[i].json) {
          const dataPreview = JSON.stringify(items[i].json);
          message += `\n\nData: ${dataPreview.substring(0, 100)}...`;
        }

        const url = `https://api.twilio.com/2010-04-01/Accounts/${accountSid}/Messages.json`;

        const credentials = btoa(`${accountSid}:${authToken}`);

        const formData = new URLSearchParams({
          From: fromNumber,
          To: toNumber,
          Body: message.substring(0, 1600), // Twilio limit
        });

        const response = await fetch(url, {
          method: 'POST',
          headers: {
            'Authorization': `Basic ${credentials}`,
            'Content-Type': 'application/x-www-form-urlencoded',
          },
          body: formData.toString(),
        });

        const result = await response.json();

        console.log(`[SMSNode] SMS ${response.ok ? 'sent' : 'failed'} to ${toNumber}`);

        outputItems.push({
          json: {
            ...items[i].json,
            notification: {
              platform: 'sms',
              to: toNumber,
              from: fromNumber,
              success: response.ok,
              messageSid: result.sid,
              sentAt: new Date().toISOString(),
            },
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[SMSNode] Error:', error);
        if (this.continueOnFail()) {
          outputItems.push({
            json: {
              ...items[i].json,
              notification: {
                platform: 'sms',
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
