import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class RedisNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Redis Cache',
    name: 'redis',
    icon: 'fa:database',
    group: ['database'],
    version: 1,
    description: 'Interact with Redis cache',
    defaults: { name: 'Redis Cache' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Host',
        name: 'host',
        type: NodePropertyType.String,
        default: 'localhost',
      },
      {
        displayName: 'Port',
        name: 'port',
        type: NodePropertyType.Number,
        default: 6379,
      },
      {
        displayName: 'Password',
        name: 'password',
        type: NodePropertyType.String,
        default: '',
        description: 'Redis password (optional)',
      },
      {
        displayName: 'Database',
        name: 'database',
        type: NodePropertyType.Number,
        default: 0,
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Get', value: 'get' },
          { name: 'Set', value: 'set' },
          { name: 'Delete', value: 'del' },
          { name: 'Exists', value: 'exists' },
          { name: 'Increment', value: 'incr' },
          { name: 'Decrement', value: 'decr' },
          { name: 'Set With Expiry', value: 'setex' },
        ],
        default: 'get',
      },
      {
        displayName: 'Key',
        name: 'key',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Value',
        name: 'value',
        type: NodePropertyType.String,
        default: '',
        displayOptions: {
          show: {
            operation: ['set', 'setex'],
          },
        },
      },
      {
        displayName: 'Expiry (seconds)',
        name: 'expiry',
        type: NodePropertyType.Number,
        default: 3600,
        displayOptions: {
          show: {
            operation: ['setex'],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const host = this.getNodeParameter('host', i) as string;
        const port = this.getNodeParameter('port', i) as number;
        const password = this.getNodeParameter('password', i) as string;
        const database = this.getNodeParameter('database', i) as number;
        const operation = this.getNodeParameter('operation', i) as string;
        const key = this.getNodeParameter('key', i) as string;

        // Redis requires backend HTTP API or WebSocket connection
        // This is a placeholder implementation
        returnData.push({
          json: {
            error: 'Redis integration requires backend API server',
            note: 'Use HTTP Request node to call Redis REST API or implement Tauri Redis plugin',
            operation,
            key,
            host,
            port,
          },
          pairedItem: { item: i },
        });
      } catch (error: any) {
        returnData.push({ json: { error: error.message }, pairedItem: { item: i } });
      }
    }

    return [returnData];
  }
}
