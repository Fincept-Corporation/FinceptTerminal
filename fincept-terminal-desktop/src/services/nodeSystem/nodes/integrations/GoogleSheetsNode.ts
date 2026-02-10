import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class GoogleSheetsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Google Sheets',
    name: 'googleSheets',
    icon: 'fa:table',
    group: ['integrations'],
    version: 1,
    description: 'Read and write data to Google Sheets',
    defaults: { name: 'Google Sheets' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Authentication',
        name: 'authentication',
        type: NodePropertyType.Options,
        options: [
          { name: 'API Key', value: 'apiKey' },
          { name: 'OAuth2', value: 'oauth2' },
        ],
        default: 'apiKey',
      },
      {
        displayName: 'API Key',
        name: 'apiKey',
        type: NodePropertyType.String,
        default: '',
        displayOptions: {
          show: {
            authentication: ['apiKey'],
          },
        },
      },
      {
        displayName: 'Spreadsheet ID',
        name: 'spreadsheetId',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'ID from the Google Sheets URL',
      },
      {
        displayName: 'Sheet Name',
        name: 'sheetName',
        type: NodePropertyType.String,
        default: 'Sheet1',
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Read', value: 'read' },
          { name: 'Append', value: 'append' },
          { name: 'Update', value: 'update' },
          { name: 'Clear', value: 'clear' },
        ],
        default: 'read',
      },
      {
        displayName: 'Range',
        name: 'range',
        type: NodePropertyType.String,
        default: 'A1:Z100',
        description: 'Range in A1 notation',
      },
      {
        displayName: 'Data Format',
        name: 'dataFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'First Row as Headers', value: 'headers' },
          { name: 'Raw Values', value: 'raw' },
        ],
        default: 'headers',
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
        const authentication = this.getNodeParameter('authentication', i) as string;
        const spreadsheetId = this.getNodeParameter('spreadsheetId', i) as string;
        const sheetName = this.getNodeParameter('sheetName', i) as string;
        const operation = this.getNodeParameter('operation', i) as string;
        const range = this.getNodeParameter('range', i) as string;

        if (authentication === 'apiKey') {
          const apiKey = this.getNodeParameter('apiKey', i) as string;
          const fullRange = `${sheetName}!${range}`;

          if (operation === 'read') {
            const dataFormat = this.getNodeParameter('dataFormat', i) as string;
            const url = `https://sheets.googleapis.com/v4/spreadsheets/${spreadsheetId}/values/${fullRange}?key=${apiKey}`;

            const response = await fetch(url);
            const data = await response.json();

            if (data.values) {
              if (dataFormat === 'headers') {
                const headers = data.values[0];
                const rows = data.values.slice(1);

                rows.forEach((row: any[]) => {
                  const obj: any = {};
                  headers.forEach((header: string, idx: number) => {
                    obj[header] = row[idx] || '';
                  });
                  returnData.push({ json: obj, pairedItem: { item: i } });
                });
              } else {
                data.values.forEach((row: any) => {
                  returnData.push({ json: { row }, pairedItem: { item: i } });
                });
              }
            }
          } else if (operation === 'append') {
            const url = `https://sheets.googleapis.com/v4/spreadsheets/${spreadsheetId}/values/${fullRange}:append?valueInputOption=USER_ENTERED&key=${apiKey}`;

            const values = [Object.values(items[i].json)];

            const response = await fetch(url, {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify({ values }),
            });

            const result = await response.json();
            returnData.push({ json: result, pairedItem: { item: i } });
          }
        } else {
          returnData.push({
            json: {
              error: 'OAuth2 authentication requires backend server integration',
              note: 'Use API Key authentication or implement OAuth2 flow',
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
