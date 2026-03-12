import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

async function readFileContent(path: string): Promise<string> {
  try {
    const { readTextFile } = await import('@tauri-apps/plugin-fs');
    return await readTextFile(path);
  } catch (e) {
    throw new Error(`Failed to read file: ${e}`);
  }
}

async function writeFileContent(path: string, content: string): Promise<void> {
  try {
    const { writeTextFile } = await import('@tauri-apps/plugin-fs');
    await writeTextFile(path, content);
  } catch (e) {
    throw new Error(`Failed to write file: ${e}`);
  }
}

function parseCSV(content: string, delimiter: string, hasHeaders: boolean): any[] {
  const lines = content.trim().split('\n');
  if (lines.length === 0) return [];

  const headers = hasHeaders ? lines[0].split(delimiter).map(h => h.trim()) : null;
  const dataLines = hasHeaders ? lines.slice(1) : lines;

  return dataLines.map(line => {
    const values = line.split(delimiter).map(v => v.trim());
    if (headers) {
      const obj: any = {};
      headers.forEach((header, i) => {
        obj[header] = values[i] || '';
      });
      return obj;
    } else {
      return { data: values };
    }
  });
}

function generateCSV(data: any[], delimiter: string, includeHeaders: boolean): string {
  if (data.length === 0) return '';

  const headers = Object.keys(data[0]);
  const rows: string[] = [];

  if (includeHeaders) {
    rows.push(headers.join(delimiter));
  }

  data.forEach(item => {
    const values = headers.map(h => {
      const val = item[h];
      return val !== undefined && val !== null ? String(val) : '';
    });
    rows.push(values.join(delimiter));
  });

  return rows.join('\n');
}

export class SpreadsheetFileNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Excel/CSV File',
    name: 'spreadsheetFile',
    icon: 'fa:table',
    group: ['files'],
    version: 1,
    description: 'Read/write CSV and Excel files',
    defaults: {
      name: 'Excel/CSV File',
      color: '#16a34a',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Read', value: 'read' },
          { name: 'Write', value: 'write' },
        ],
        default: 'read',
        required: true,
        description: 'Operation to perform',
      },
      {
        displayName: 'File Format',
        name: 'fileFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'CSV', value: 'csv' },
          { name: 'JSON', value: 'json' },
        ],
        default: 'csv',
        description: 'File format',
      },
      {
        displayName: 'File Path',
        name: 'filePath',
        type: NodePropertyType.String,
        default: '',
        placeholder: '/path/to/file.csv',
        description: 'Path to file (read) or output path (write)',
      },
      {
        displayName: 'CSV Delimiter',
        name: 'delimiter',
        type: NodePropertyType.String,
        default: ',',
        description: 'CSV delimiter character',
      },
      {
        displayName: 'Has Headers',
        name: 'hasHeaders',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Whether CSV has header row',
      },
      {
        displayName: 'Include Headers',
        name: 'includeHeaders',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include headers when writing CSV',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    const operation = this.getNodeParameter('operation', 0) as string;
    const fileFormat = this.getNodeParameter('fileFormat', 0) as string;
    const filePath = this.getNodeParameter('filePath', 0) as string;
    const delimiter = this.getNodeParameter('delimiter', 0, ',') as string;
    const hasHeaders = this.getNodeParameter('hasHeaders', 0, true) as boolean;
    const includeHeaders = this.getNodeParameter('includeHeaders', 0, true) as boolean;

    try {
      if (operation === 'read') {
        const content = await readFileContent(filePath);

        if (fileFormat === 'csv') {
          const rows = parseCSV(content, delimiter, hasHeaders);
          rows.forEach((row) => {
            returnData.push({
              json: row,
              pairedItem: 0,
            });
          });
        } else {
          const data = JSON.parse(content);
          const array = Array.isArray(data) ? data : [data];
          array.forEach((item) => {
            returnData.push({
              json: item,
              pairedItem: 0,
            });
          });
        }

        console.log(`[SpreadsheetFileNode] Read ${returnData.length} rows from ${filePath}`);
      } else {
        let content = '';

        if (fileFormat === 'csv') {
          content = generateCSV(items.map(i => i.json), delimiter, includeHeaders);
        } else {
          content = JSON.stringify(items.map(i => i.json), null, 2);
        }

        await writeFileContent(filePath, content);
        console.log(`[SpreadsheetFileNode] Wrote ${items.length} rows to ${filePath}`);

        returnData.push({
          json: {
            success: true,
            filePath,
            rowsWritten: items.length,
          },
          pairedItem: 0,
        });
      }
    } catch (error: any) {
      console.error('[SpreadsheetFileNode] Error:', error);
      if (this.continueOnFail()) {
        returnData.push({
          json: {
            error: error.message,
            success: false,
          },
          pairedItem: 0,
        });
      } else {
        throw error;
      }
    }

    return [returnData];
  }
}
