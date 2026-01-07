import { INodeType, INodeTypeDescription, IExecuteFunctions, NodeOutput, NodeConnectionType, NodePropertyType, INodeExecutionData } from '../../types';

export class EmailNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Email',
    name: 'email',
    icon: 'fa:envelope',
    iconColor: '#ef4444',
    group: ['notifications'],
    version: 1,
    description: 'Send email notifications via SMTP or email API',
    defaults: { name: 'Email', color: '#ef4444' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'To',
        name: 'to',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'recipient@example.com',
        description: 'Email recipient (can use $json.email for dynamic)',
      },
      {
        displayName: 'From',
        name: 'from',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'sender@example.com',
        description: 'Email sender (optional, uses default if not specified)',
      },
      {
        displayName: 'Subject',
        name: 'subject',
        type: NodePropertyType.String,
        default: 'Notification',
        placeholder: 'Trading Alert: $json.symbol',
        description: 'Email subject (supports expressions)',
      },
      {
        displayName: 'Body Type',
        name: 'bodyType',
        type: NodePropertyType.Options,
        options: [
          { name: 'Text', value: 'text' },
          { name: 'HTML', value: 'html' },
        ],
        default: 'text',
        description: 'Format of the email body',
      },
      {
        displayName: 'Body',
        name: 'body',
        type: NodePropertyType.String,
        default: '',
        typeOptions: {
          rows: 6,
        },
        placeholder: 'Stock $json.symbol reached price $json.price',
        description: 'Email body content (supports expressions)',
      },
      {
        displayName: 'Include Input Data',
        name: 'includeInputData',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Whether to append input data as JSON to the email body',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: INodeExecutionData[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        let to = this.getNodeParameter('to', i) as string;
        let from = this.getNodeParameter('from', i, '') as string;
        let subject = this.getNodeParameter('subject', i) as string;
        const bodyType = this.getNodeParameter('bodyType', i) as string;
        let body = this.getNodeParameter('body', i, '') as string;
        const includeInputData = this.getNodeParameter('includeInputData', i) as boolean;

        // Evaluate expressions
        try {
          if (to.includes('$json')) to = this.evaluateExpression(to, i) as string;
          if (from.includes('$json')) from = this.evaluateExpression(from, i) as string;
          if (subject.includes('$json')) subject = this.evaluateExpression(subject, i) as string;
          if (body.includes('$json')) body = this.evaluateExpression(body, i) as string;
        } catch (error) {
          // Use literal values if expression evaluation fails
        }

        // Append input data if requested
        if (includeInputData) {
          const dataString = JSON.stringify(items[i].json, null, 2);
          body += bodyType === 'html'
            ? `<br><br><pre>${dataString}</pre>`
            : `\n\n${dataString}`;
        }

        // In a real implementation, this would send via SMTP or email API
        // For now, we'll prepare the email data and mark as simulated
        const emailData = {
          to,
          from: from || 'noreply@fincept.com',
          subject,
          bodyType,
          body,
          timestamp: new Date().toISOString(),
        };

        // TODO: Integrate with actual email service (SMTP, SendGrid, etc.)
        // Example: await this.helpers.httpRequest({ ... })

        returnData.push({
          json: {
            success: true,
            simulated: true, // Remove when real email integration is added
            email: emailData,
            message: 'Email prepared (integration pending)',
          },
          pairedItem: i,
        });
      } catch (error: any) {
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              success: false,
              error: error.message || 'Email sending failed',
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
