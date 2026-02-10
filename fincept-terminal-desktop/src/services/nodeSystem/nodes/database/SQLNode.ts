import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class SQLNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Database Query',
    name: 'sql',
    icon: 'fa:database',
    group: ['database'],
    version: 1,
    description: 'Execute SQL queries (PostgreSQL, MySQL, SQLite)',
    defaults: { name: 'Database Query' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Database Type',
        name: 'databaseType',
        type: NodePropertyType.Options,
        options: [
          { name: 'SQLite', value: 'sqlite' },
          { name: 'PostgreSQL', value: 'postgresql' },
          { name: 'MySQL', value: 'mysql' },
        ],
        default: 'sqlite',
      },
      {
        displayName: 'Connection String',
        name: 'connectionString',
        type: NodePropertyType.String,
        default: '',
        description: 'Database connection string',
        displayOptions: {
          show: {
            databaseType: ['postgresql', 'mysql'],
          },
        },
      },
      {
        displayName: 'Database File',
        name: 'databaseFile',
        type: NodePropertyType.String,
        default: 'fincept-terminal.db',
        description: 'SQLite database file in AppData',
        displayOptions: {
          show: {
            databaseType: ['sqlite'],
          },
        },
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Execute Query', value: 'query' },
          { name: 'Insert', value: 'insert' },
          { name: 'Update', value: 'update' },
          { name: 'Delete', value: 'delete' },
        ],
        default: 'query',
      },
      {
        displayName: 'SQL Query',
        name: 'sqlQuery',
        type: NodePropertyType.String,
        default: 'SELECT * FROM table_name',
        required: true,
        description: 'SQL query to execute',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const databaseType = this.getNodeParameter('databaseType', i) as string;
        const operation = this.getNodeParameter('operation', i) as string;
        const sqlQuery = this.getNodeParameter('sqlQuery', i) as string;

        if (databaseType === 'sqlite') {
          // Use Tauri SQL plugin
          const { invoke } = await import('@tauri-apps/api/core');
          const databaseFile = this.getNodeParameter('databaseFile', i) as string;

          const result = await invoke('execute_sql', {
            dbPath: databaseFile,
            query: sqlQuery,
          });

          returnData.push({ json: { result }, pairedItem: { item: i } });
        } else {
          // For PostgreSQL/MySQL, would need backend integration
          returnData.push({
            json: {
              error: 'PostgreSQL and MySQL require backend server integration',
              databaseType,
            },
            pairedItem: { item: i },
          });
        }
      } catch (error: any) {
        returnData.push({ json: { error: error.message }, pairedItem: { item: i } });
      }
    }

    return [returnData];
  }
}
