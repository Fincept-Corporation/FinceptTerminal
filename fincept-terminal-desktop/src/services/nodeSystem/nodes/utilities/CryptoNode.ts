import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class CryptoNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Encrypt/Hash',
    name: 'crypto',
    icon: 'fa:lock',
    group: ['utilities'],
    version: 1,
    description: 'Hash, encrypt, and decrypt data',
    defaults: {
      name: 'Encrypt/Hash',
      color: '#ff9500',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Action',
        name: 'action',
        type: NodePropertyType.Options,
        options: [
          { name: 'Hash (MD5)', value: 'md5' },
          { name: 'Hash (SHA-256)', value: 'sha256' },
          { name: 'Hash (SHA-512)', value: 'sha512' },
          { name: 'Base64 Encode', value: 'base64Encode' },
          { name: 'Base64 Decode', value: 'base64Decode' },
          { name: 'Generate UUID', value: 'uuid' },
          { name: 'Generate Random', value: 'random' },
        ],
        default: 'sha256',
        required: true,
      },
      {
        displayName: 'Value',
        name: 'value',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'Text to hash/encrypt',
        description: 'Input value',
      },
      {
        displayName: 'Encoding',
        name: 'encoding',
        type: NodePropertyType.Options,
        options: [
          { name: 'Hex', value: 'hex' },
          { name: 'Base64', value: 'base64' },
        ],
        default: 'hex',
        description: 'Output encoding for hashes',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const action = this.getNodeParameter('action', i) as string;
        const value = this.getNodeParameter('value', i, '') as string;
        const encoding = this.getNodeParameter('encoding', i, 'hex') as string;

        let result: any;

        switch (action) {
          case 'md5':
          case 'sha256':
          case 'sha512':
            result = await this.hash(value, action, encoding);
            break;
          case 'base64Encode':
            result = btoa(value);
            break;
          case 'base64Decode':
            result = atob(value);
            break;
          case 'uuid':
            result = crypto.randomUUID();
            break;
          case 'random': {
            const array = new Uint8Array(32);
            crypto.getRandomValues(array);
            result = encoding === 'hex'
              ? Array.from(array).map(b => b.toString(16).padStart(2, '0')).join('')
              : btoa(String.fromCharCode(...array));
            break;
          }
        }

        returnData.push({
          json: {
            crypto: {
              action,
              result,
              original: value.substring(0, 100),
            },
            ...items[i].json,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[CryptoNode] Error:', error);
        if (this.continueOnFail()) {
          returnData.push({
            json: { error: error.message, ...items[i].json },
            pairedItem: i,
          });
        } else {
          throw error;
        }
      }
    }

    return [returnData];
  }

  private async hash(value: string, algorithm: string, encoding: string): Promise<string> {
    const encoder = new TextEncoder();
    const data = encoder.encode(value);
    const hashBuffer = await crypto.subtle.digest(
      algorithm === 'md5' ? 'MD5' : algorithm.toUpperCase().replace('SHA', 'SHA-'),
      data
    );
    const hashArray = Array.from(new Uint8Array(hashBuffer));

    if (encoding === 'hex') {
      return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
    } else {
      return btoa(String.fromCharCode(...hashArray));
    }
  }
}
