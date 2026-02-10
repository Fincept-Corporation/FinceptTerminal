import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';
import { readFile, writeFile, BaseDirectory } from '@tauri-apps/plugin-fs';

export class BinaryFileNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Binary File',
    name: 'binaryFile',
    icon: 'fa:file',
    group: ['files'],
    version: 1,
    description: 'Read and write binary files',
    defaults: { name: 'Binary File' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Read File', value: 'read' },
          { name: 'Write File', value: 'write' },
        ],
        default: 'read',
      },
      {
        displayName: 'File Path',
        name: 'filePath',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Path to the file',
      },
      {
        displayName: 'Use AppData Directory',
        name: 'useAppData',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Use application data directory as base',
      },
      {
        displayName: 'Encoding',
        name: 'encoding',
        type: NodePropertyType.Options,
        options: [
          { name: 'Base64', value: 'base64' },
          { name: 'Binary', value: 'binary' },
        ],
        default: 'base64',
        description: 'How to encode/decode the file data',
        displayOptions: {
          show: {
            operation: ['read'],
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
        const operation = this.getNodeParameter('operation', i) as string;
        const filePath = this.getNodeParameter('filePath', i) as string;
        const useAppData = this.getNodeParameter('useAppData', i) as boolean;

        if (operation === 'read') {
          const encoding = this.getNodeParameter('encoding', i) as string;
          const baseDir = useAppData ? BaseDirectory.AppData : undefined;
          const data = await readFile(filePath, { baseDir });

          let result: any;
          if (encoding === 'base64') {
            result = btoa(String.fromCharCode(...data));
          } else {
            result = Array.from(data);
          }

          returnData.push({
            json: { data: result, filePath, encoding },
            pairedItem: { item: i },
          });
        } else if (operation === 'write') {
          const data = items[i].json.data;
          let uint8Array: Uint8Array;

          if (typeof data === 'string') {
            // Assume base64
            const binaryString = atob(data);
            uint8Array = new Uint8Array(binaryString.length);
            for (let j = 0; j < binaryString.length; j++) {
              uint8Array[j] = binaryString.charCodeAt(j);
            }
          } else if (Array.isArray(data)) {
            uint8Array = new Uint8Array(data as number[]);
          } else {
            throw new Error('Invalid data format for write operation');
          }

          const baseDir = useAppData ? BaseDirectory.AppData : undefined;
          await writeFile(filePath, uint8Array, { baseDir });

          returnData.push({
            json: { success: true, filePath, size: uint8Array.length },
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
