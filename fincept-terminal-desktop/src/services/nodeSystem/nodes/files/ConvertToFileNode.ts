import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';
import { writeFile, BaseDirectory } from '@tauri-apps/plugin-fs';

export class ConvertToFileNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Export to File',
    name: 'convertToFile',
    icon: 'fa:file-export',
    group: ['files'],
    version: 1,
    description: 'Convert data to PDF, CSV, or HTML file',
    defaults: { name: 'Export to File' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'File Format',
        name: 'fileFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'CSV', value: 'csv' },
          { name: 'HTML', value: 'html' },
          { name: 'JSON', value: 'json' },
          { name: 'Text', value: 'txt' },
        ],
        default: 'csv',
      },
      {
        displayName: 'File Name',
        name: 'fileName',
        type: NodePropertyType.String,
        default: 'output',
        required: true,
      },
      {
        displayName: 'Include Headers',
        name: 'includeHeaders',
        type: NodePropertyType.Boolean,
        default: true,
        displayOptions: {
          show: {
            fileFormat: ['csv'],
          },
        },
      },
      {
        displayName: 'HTML Template',
        name: 'htmlTemplate',
        type: NodePropertyType.Options,
        options: [
          { name: 'Table', value: 'table' },
          { name: 'List', value: 'list' },
          { name: 'Cards', value: 'cards' },
        ],
        default: 'table',
        displayOptions: {
          show: {
            fileFormat: ['html'],
          },
        },
      },
      {
        displayName: 'Pretty Print',
        name: 'prettyPrint',
        type: NodePropertyType.Boolean,
        default: true,
        displayOptions: {
          show: {
            fileFormat: ['json'],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];

    try {
      const fileFormat = this.getNodeParameter('fileFormat', 0) as string;
      const fileName = this.getNodeParameter('fileName', 0) as string;
      const fullFileName = `${fileName}.${fileFormat}`;

      let content = '';

      if (fileFormat === 'csv') {
        const includeHeaders = this.getNodeParameter('includeHeaders', 0) as boolean;
        const data = items.map((item) => item.json);

        if (data.length > 0) {
          const headers = Object.keys(data[0]);

          if (includeHeaders) {
            content += headers.join(',') + '\n';
          }

          data.forEach((row) => {
            const values = headers.map((header) => {
              const value = row[header];
              return typeof value === 'string' && value.includes(',') ? `"${value}"` : value;
            });
            content += values.join(',') + '\n';
          });
        }
      } else if (fileFormat === 'json') {
        const prettyPrint = this.getNodeParameter('prettyPrint', 0) as boolean;
        const data = items.map((item) => item.json);
        content = prettyPrint ? JSON.stringify(data, null, 2) : JSON.stringify(data);
      } else if (fileFormat === 'html') {
        const htmlTemplate = this.getNodeParameter('htmlTemplate', 0) as string;
        const data = items.map((item) => item.json);

        content = '<!DOCTYPE html><html><head><meta charset="UTF-8"><style>body{font-family:Arial,sans-serif;margin:20px;}table{border-collapse:collapse;width:100%;}th,td{border:1px solid #ddd;padding:8px;text-align:left;}th{background-color:#f2f2f2;}.card{border:1px solid #ddd;padding:15px;margin:10px 0;border-radius:5px;}</style></head><body>';

        if (htmlTemplate === 'table') {
          if (data.length > 0) {
            const headers = Object.keys(data[0]);
            content += '<table><thead><tr>';
            headers.forEach((h) => (content += `<th>${h}</th>`));
            content += '</tr></thead><tbody>';

            data.forEach((row) => {
              content += '<tr>';
              headers.forEach((h) => (content += `<td>${row[h]}</td>`));
              content += '</tr>';
            });

            content += '</tbody></table>';
          }
        } else if (htmlTemplate === 'list') {
          content += '<ul>';
          data.forEach((item) => {
            content += `<li>${JSON.stringify(item)}</li>`;
          });
          content += '</ul>';
        } else if (htmlTemplate === 'cards') {
          data.forEach((item) => {
            content += '<div class="card">';
            Object.entries(item).forEach(([key, value]) => {
              content += `<p><strong>${key}:</strong> ${value}</p>`;
            });
            content += '</div>';
          });
        }

        content += '</body></html>';
      } else if (fileFormat === 'txt') {
        const data = items.map((item) => item.json);
        content = data.map((item) => JSON.stringify(item)).join('\n');
      }

      // Write file
      const encoder = new TextEncoder();
      const bytes = encoder.encode(content);
      await writeFile(fullFileName, bytes, { baseDir: BaseDirectory.AppData });

      returnData.push({
        json: {
          success: true,
          fileName: fullFileName,
          size: bytes.length,
          itemCount: items.length,
        },
        pairedItem: { item: 0 },
      });
    } catch (error: any) {
      returnData.push({ json: { error: error.message }, pairedItem: { item: 0 } });
    }

    return [returnData];
  }
}
