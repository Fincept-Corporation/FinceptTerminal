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
    displayName: 'SMS',
    name: 'sms',
    icon: 'fa:comment',
    group: ['Notification'],
    version: 1,
    description: 'Send SMS via Twilio',
    defaults: {
      name: 'SMS',
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
        description: 'Twilio Account SID',
      },
      {
        displayName: 'Auth Token',
        name: 'authToken',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Twilio Auth Token',
      },
      {
        displayName: 'From Number',
        name: 'fromNumber',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Twilio phone number (e.g., +1234567890)',
      },
      {
        displayName: 'To Number',
        name: 'toNumber',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Recipient phone number (e.g., +1234567890)',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'SMS message text',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const accountSid = this.getNodeParameter('accountSid', 0) as string;
    const authToken = this.getNodeParameter('authToken', 0) as string;
    const fromNumber = this.getNodeParameter('fromNumber', 0) as string;
    const toNumber = this.getNodeParameter('toNumber', 0) as string;
    const message = this.getNodeParameter('message', 0) as string;

    const outputItems: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const url = `https://api.twilio.com/2010-04-01/Accounts/${accountSid}/Messages.json`;

        const credentials = btoa(`${accountSid}:${authToken}`);

        const formData = new URLSearchParams({
          From: fromNumber,
          To: toNumber,
          Body: message,
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

        outputItems.push({
          json: {
            ...items[i].json,
            smsResponse: result,
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
