import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class FTPNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'FTP Transfer',
    name: 'ftp',
    icon: 'fa:server',
    group: ['integrations'],
    version: 1,
    description: 'Transfer files via FTP or SFTP',
    defaults: { name: 'FTP Transfer' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Protocol',
        name: 'protocol',
        type: NodePropertyType.Options,
        options: [
          { name: 'FTP', value: 'ftp' },
          { name: 'SFTP', value: 'sftp' },
        ],
        default: 'sftp',
      },
      {
        displayName: 'Host',
        name: 'host',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Port',
        name: 'port',
        type: NodePropertyType.Number,
        default: 22,
        description: 'Port (21 for FTP, 22 for SFTP)',
      },
      {
        displayName: 'Username',
        name: 'username',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Password',
        name: 'password',
        type: NodePropertyType.String,
        default: '',
        required: true,
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Upload', value: 'upload' },
          { name: 'Download', value: 'download' },
          { name: 'List Files', value: 'list' },
          { name: 'Delete', value: 'delete' },
        ],
        default: 'upload',
      },
      {
        displayName: 'Remote Path',
        name: 'remotePath',
        type: NodePropertyType.String,
        default: '/',
        description: 'Remote directory or file path',
      },
      {
        displayName: 'Local Path',
        name: 'localPath',
        type: NodePropertyType.String,
        default: '',
        description: 'Local file path',
        displayOptions: {
          show: {
            operation: ['upload', 'download'],
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
        const protocol = this.getNodeParameter('protocol', i) as string;
        const host = this.getNodeParameter('host', i) as string;
        const port = this.getNodeParameter('port', i) as number;
        const username = this.getNodeParameter('username', i) as string;
        const operation = this.getNodeParameter('operation', i) as string;
        const remotePath = this.getNodeParameter('remotePath', i) as string;

        // FTP/SFTP requires Tauri plugin or backend integration
        returnData.push({
          json: {
            error: 'FTP/SFTP integration requires Tauri plugin or backend server',
            note: 'Implement Tauri FTP plugin or use HTTP Request node to call FTP proxy API',
            protocol,
            host,
            port,
            username,
            operation,
            remotePath,
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
