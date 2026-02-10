import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { getProvider } from '@/services/notifications/providerRegistry';

export class EmailNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Send Email',
    name: 'email',
    icon: 'fa:envelope',
    group: ['notifications'],
    version: 1,
    description: 'Send email notifications',
    defaults: {
      name: 'Send Email',
      color: '#10b981',
    },
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
        description: 'Email address of the recipient',
      },
      {
        displayName: 'Subject',
        name: 'subject',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Notification Subject',
        description: 'Email subject line',
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'Your message here...',
        typeOptions: {
          rows: 4,
        },
        description: 'Email body content',
      },
      {
        displayName: 'Include Input Data',
        name: 'includeInputData',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether to include input data in the email',
      },
      {
        displayName: 'SMTP Host',
        name: 'smtp_host',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'smtp.gmail.com',
        description: 'SMTP server host',
      },
      {
        displayName: 'SMTP Port',
        name: 'smtp_port',
        type: NodePropertyType.Number,
        default: 587,
        description: 'SMTP server port (587 for TLS, 465 for SSL)',
      },
      {
        displayName: 'SMTP Username',
        name: 'smtp_user',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'your-email@gmail.com',
        description: 'SMTP authentication username',
      },
      {
        displayName: 'SMTP Password',
        name: 'smtp_password',
        type: NodePropertyType.String,
        default: '',
        typeOptions: {
          password: true,
        },
        description: 'SMTP authentication password or app password',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const to = this.getNodeParameter('to', i) as string;
        const subject = this.getNodeParameter('subject', i) as string;
        const message = this.getNodeParameter('message', i) as string;
        const includeInputData = this.getNodeParameter('includeInputData', i) as boolean;
        const smtpHost = this.getNodeParameter('smtp_host', i) as string;
        const smtpPort = this.getNodeParameter('smtp_port', i) as number;
        const smtpUser = this.getNodeParameter('smtp_user', i) as string;
        const smtpPassword = this.getNodeParameter('smtp_password', i) as string;

        // Build email body
        let emailBody = message;
        if (includeInputData && items[i].json) {
          emailBody += '\n\n---\nInput Data:\n' + JSON.stringify(items[i].json, null, 2);
        }

        // Get email provider
        const emailProvider = getProvider('email');

        if (emailProvider) {
          // Prepare configuration
          const config: Record<string, string> = {
            smtp_host: smtpHost,
            smtp_port: String(smtpPort),
            smtp_user: smtpUser,
            smtp_password: smtpPassword,
            from: smtpUser, // Use SMTP user as sender
          };

          // Send email using provider
          const success = await emailProvider.send(config, {
            type: 'info',
            title: subject,
            body: emailBody,
            timestamp: new Date().toISOString(),
            metadata: {
              to,
              from: smtpUser,
            },
          });

          returnData.push({
            json: {
              success,
              to,
              subject,
              sentAt: new Date().toISOString(),
              ...(items[i].json || {}),
            },
            pairedItem: i,
          });

          console.log(`[EmailNode] Email ${success ? 'sent' : 'failed'} to ${to}: ${subject}`);
        } else {
          // Fallback: just return the data with metadata
          console.warn('[EmailNode] Email provider not available, logging only');
          returnData.push({
            json: {
              success: false,
              error: 'Email provider not configured',
              to,
              subject,
              message: emailBody,
              ...(items[i].json || {}),
            },
            pairedItem: i,
          });
        }
      } catch (error: any) {
        console.error('[EmailNode] Error:', error);
        returnData.push({
          json: {
            success: false,
            error: error.message,
            ...(items[i].json || {}),
          },
          pairedItem: i,
        });
      }
    }

    return [returnData];
  }
}
