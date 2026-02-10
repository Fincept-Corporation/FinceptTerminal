import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';
import { copyFile, rename, remove, exists, mkdir, BaseDirectory } from '@tauri-apps/plugin-fs';

export class FileOperationsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Manage Files',
    name: 'fileOperations',
    icon: 'fa:folder-open',
    group: ['files'],
    version: 1,
    description: 'Move, copy, delete files and folders',
    defaults: { name: 'Manage Files' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Copy', value: 'copy' },
          { name: 'Move', value: 'move' },
          { name: 'Delete', value: 'delete' },
          { name: 'Create Directory', value: 'mkdir' },
          { name: 'Check Exists', value: 'exists' },
        ],
        default: 'copy',
      },
      {
        displayName: 'Source Path',
        name: 'sourcePath',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'Source file/folder path',
        displayOptions: {
          hide: {
            operation: ['mkdir'],
          },
        },
      },
      {
        displayName: 'Destination Path',
        name: 'destinationPath',
        type: NodePropertyType.String,
        default: '',
        description: 'Destination path',
        displayOptions: {
          show: {
            operation: ['copy', 'move', 'mkdir'],
          },
        },
      },
      {
        displayName: 'Use AppData Directory',
        name: 'useAppData',
        type: NodePropertyType.Boolean,
        default: true,
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const operation = this.getNodeParameter('operation', i) as string;
        const useAppData = this.getNodeParameter('useAppData', i) as boolean;
        const baseDir = useAppData ? BaseDirectory.AppData : undefined;

        if (operation === 'copy') {
          const sourcePath = this.getNodeParameter('sourcePath', i) as string;
          const destinationPath = this.getNodeParameter('destinationPath', i) as string;

          await copyFile(sourcePath, destinationPath, { fromPathBaseDir: baseDir, toPathBaseDir: baseDir });

          returnData.push({
            json: { success: true, operation: 'copy', from: sourcePath, to: destinationPath },
            pairedItem: { item: i },
          });
        } else if (operation === 'move') {
          const sourcePath = this.getNodeParameter('sourcePath', i) as string;
          const destinationPath = this.getNodeParameter('destinationPath', i) as string;

          await rename(sourcePath, destinationPath, { oldPathBaseDir: baseDir, newPathBaseDir: baseDir });

          returnData.push({
            json: { success: true, operation: 'move', from: sourcePath, to: destinationPath },
            pairedItem: { item: i },
          });
        } else if (operation === 'delete') {
          const sourcePath = this.getNodeParameter('sourcePath', i) as string;

          await remove(sourcePath, { baseDir });

          returnData.push({
            json: { success: true, operation: 'delete', path: sourcePath },
            pairedItem: { item: i },
          });
        } else if (operation === 'mkdir') {
          const destinationPath = this.getNodeParameter('destinationPath', i) as string;

          await mkdir(destinationPath, { baseDir, recursive: true });

          returnData.push({
            json: { success: true, operation: 'mkdir', path: destinationPath },
            pairedItem: { item: i },
          });
        } else if (operation === 'exists') {
          const sourcePath = this.getNodeParameter('sourcePath', i) as string;

          const fileExists = await exists(sourcePath, { baseDir });

          returnData.push({
            json: { exists: fileExists, path: sourcePath },
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
