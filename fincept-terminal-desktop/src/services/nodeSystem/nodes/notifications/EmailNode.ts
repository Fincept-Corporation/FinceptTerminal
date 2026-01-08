import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class EmailNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Email',
    name: 'email',
    icon: 'fa:envelope',
    group: ['Notifications'],
    version: 1,
    description: 'Send email notifications',
    defaults: {
      name: 'Email',
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
      },
      {
        displayName: 'Subject',
        name: 'subject',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Message',
        name: 'message',
        type: NodePropertyType.String,
        default: '',
        typeOptions: {
          rows: 4,
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const to = this.getNodeParameter('to', 0) as string;
    const subject = this.getNodeParameter('subject', 0) as string;
    const message = this.getNodeParameter('message', 0) as string;

    console.log(`[Email] Would send to ${to}: ${subject}`);

    return [items.map((item, i) => ({
      ...item,
      json: { ...item.json, emailSent: true, to, subject },
      pairedItem: i,
    }))];
  }
}
