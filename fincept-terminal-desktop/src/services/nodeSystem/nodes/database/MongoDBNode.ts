import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class MongoDBNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'MongoDB Query',
    name: 'mongodb',
    icon: 'fa:leaf',
    group: ['database'],
    version: 1,
    description: 'Interact with MongoDB collections',
    defaults: { name: 'MongoDB Query' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Connection String',
        name: 'connectionString',
        type: NodePropertyType.String,
        default: 'mongodb://localhost:27017',
        required: true,
      },
      {
        displayName: 'Database',
        name: 'database',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Collection',
        name: 'collection',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Find', value: 'find' },
          { name: 'Find One', value: 'findOne' },
          { name: 'Insert', value: 'insert' },
          { name: 'Update', value: 'update' },
          { name: 'Delete', value: 'delete' },
          { name: 'Aggregate', value: 'aggregate' },
        ],
        default: 'find',
      },
      {
        displayName: 'Query',
        name: 'query',
        type: NodePropertyType.Json,
        default: '{}',
        description: 'MongoDB query filter',
        displayOptions: {
          show: {
            operation: ['find', 'findOne', 'update', 'delete'],
          },
        },
      },
      {
        displayName: 'Document',
        name: 'document',
        type: NodePropertyType.Json,
        default: '{}',
        description: 'Document to insert/update',
        displayOptions: {
          show: {
            operation: ['insert', 'update'],
          },
        },
      },
      {
        displayName: 'Pipeline',
        name: 'pipeline',
        type: NodePropertyType.Json,
        default: '[]',
        description: 'Aggregation pipeline',
        displayOptions: {
          show: {
            operation: ['aggregate'],
          },
        },
      },
      {
        displayName: 'Limit',
        name: 'limit',
        type: NodePropertyType.Number,
        default: 100,
        displayOptions: {
          show: {
            operation: ['find'],
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
        const connectionString = this.getNodeParameter('connectionString', i) as string;
        const database = this.getNodeParameter('database', i) as string;
        const collection = this.getNodeParameter('collection', i) as string;
        const operation = this.getNodeParameter('operation', i) as string;

        // MongoDB requires backend integration or Tauri plugin
        returnData.push({
          json: {
            error: 'MongoDB integration requires backend API server',
            note: 'Use HTTP Request node to call MongoDB REST API or implement Tauri MongoDB plugin',
            operation,
            database,
            collection,
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
