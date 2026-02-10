import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';
import { readFile, writeFile, BaseDirectory } from '@tauri-apps/plugin-fs';
import pako from 'pako';

export class CompressNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Compress/Unzip',
    name: 'compress',
    icon: 'fa:file-archive',
    group: ['files'],
    version: 1,
    description: 'Compress and decompress data using GZIP',
    defaults: { name: 'Compress/Unzip' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Compress', value: 'compress' },
          { name: 'Decompress', value: 'decompress' },
        ],
        default: 'compress',
      },
      {
        displayName: 'Data Source',
        name: 'dataSource',
        type: NodePropertyType.Options,
        options: [
          { name: 'From Input', value: 'input' },
          { name: 'From File', value: 'file' },
        ],
        default: 'input',
      },
      {
        displayName: 'File Path',
        name: 'filePath',
        type: NodePropertyType.String,
        default: '',
        displayOptions: {
          show: {
            dataSource: ['file'],
          },
        },
      },
      {
        displayName: 'Input Field',
        name: 'inputField',
        type: NodePropertyType.String,
        default: 'data',
        description: 'Field containing data to compress/decompress',
        displayOptions: {
          show: {
            dataSource: ['input'],
          },
        },
      },
      {
        displayName: 'Output Format',
        name: 'outputFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'Base64', value: 'base64' },
          { name: 'Binary Array', value: 'binary' },
        ],
        default: 'base64',
        displayOptions: {
          show: {
            operation: ['compress'],
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
        const dataSource = this.getNodeParameter('dataSource', i) as string;

        let data: Uint8Array;

        if (dataSource === 'file') {
          const filePath = this.getNodeParameter('filePath', i) as string;
          data = await readFile(filePath, { baseDir: BaseDirectory.AppData });
        } else {
          const inputField = this.getNodeParameter('inputField', i) as string;
          const inputData = items[i].json[inputField];

          if (typeof inputData === 'string') {
            // Check if base64
            try {
              const binaryString = atob(inputData);
              data = new Uint8Array(binaryString.length);
              for (let j = 0; j < binaryString.length; j++) {
                data[j] = binaryString.charCodeAt(j);
              }
            } catch {
              // Not base64, treat as text
              data = new TextEncoder().encode(inputData);
            }
          } else if (Array.isArray(inputData)) {
            data = new Uint8Array(inputData as number[]);
          } else {
            data = new TextEncoder().encode(JSON.stringify(inputData));
          }
        }

        if (operation === 'compress') {
          const compressed = pako.gzip(data);
          const outputFormat = this.getNodeParameter('outputFormat', i) as string;

          let result: any;
          if (outputFormat === 'base64') {
            result = btoa(String.fromCharCode(...compressed));
          } else {
            result = Array.from(compressed);
          }

          returnData.push({
            json: {
              compressed: result,
              originalSize: data.length,
              compressedSize: compressed.length,
              compressionRatio: ((1 - compressed.length / data.length) * 100).toFixed(2) + '%',
            },
            pairedItem: { item: i },
          });
        } else if (operation === 'decompress') {
          const decompressed = pako.ungzip(data);
          const text = new TextDecoder().decode(decompressed);

          // Try to parse as JSON
          let result: any;
          try {
            result = JSON.parse(text);
          } catch {
            result = text;
          }

          returnData.push({
            json: { decompressed: result, size: decompressed.length },
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
