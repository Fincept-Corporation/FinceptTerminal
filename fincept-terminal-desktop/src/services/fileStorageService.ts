/**
 * File Storage Service
 * Centralized file management for Fincept Terminal
 *
 * Stores uploaded files in terminal's data directory for access across all features
 */

import { BaseDirectory, mkdir, copyFile, exists, readDir, remove, readFile, writeFile } from '@tauri-apps/plugin-fs';
import { save, open } from '@tauri-apps/plugin-dialog';
import { appDataDir } from '@tauri-apps/api/path';

const STORAGE_DIR = 'fincept-files';

interface StoredFile {
  id: string;
  name: string;
  originalName: string;
  size: number;
  type: string;
  uploadedAt: string;
  path: string;
}

interface FileMetadata {
  files: StoredFile[];
}

class FileStorageService {
  private metadataPath = `${STORAGE_DIR}/metadata.json`;

  /**
   * Initialize storage directory
   */
  async initialize(): Promise<void> {
    try {
      const dirExists = await exists(STORAGE_DIR, { baseDir: BaseDirectory.AppData });
      if (!dirExists) {
        await mkdir(STORAGE_DIR, { baseDir: BaseDirectory.AppData, recursive: true });
        await this.saveMetadata({ files: [] });
      }
      console.log('[FileStorageService] Initialized');
    } catch (error) {
      console.error('[FileStorageService] Initialization failed:', error);
    }
  }

  /**
   * Upload file to terminal storage
   */
  async uploadFile(file: File): Promise<StoredFile> {
    await this.initialize();

    // Generate unique filename
    const timestamp = Date.now();
    const randomId = Math.random().toString(36).substring(7);
    const ext = file.name.split('.').pop();
    const storedName = `${timestamp}_${randomId}.${ext}`;
    const storedPath = `${STORAGE_DIR}/${storedName}`;

    // Read file content
    const arrayBuffer = await file.arrayBuffer();
    const uint8Array = new Uint8Array(arrayBuffer);

    // Write to storage
    await writeFile(storedPath, uint8Array, { baseDir: BaseDirectory.AppData });

    // Create metadata
    const storedFile: StoredFile = {
      id: `${timestamp}_${randomId}`,
      name: storedName,
      originalName: file.name,
      size: file.size,
      type: file.type || 'application/octet-stream',
      uploadedAt: new Date().toISOString(),
      path: storedPath,
    };

    // Update metadata
    const metadata = await this.loadMetadata();
    metadata.files.push(storedFile);
    await this.saveMetadata(metadata);

    console.log('[FileStorageService] File uploaded:', file.name);
    return storedFile;
  }

  /**
   * List all stored files
   */
  async listFiles(): Promise<StoredFile[]> {
    await this.initialize();
    const metadata = await this.loadMetadata();
    return metadata.files;
  }

  /**
   * Get file by ID
   */
  async getFile(fileId: string): Promise<StoredFile | null> {
    const metadata = await this.loadMetadata();
    return metadata.files.find(f => f.id === fileId) || null;
  }

  /**
   * Get file path for use in workflows
   */
  async getFilePath(fileId: string): Promise<string | null> {
    const file = await this.getFile(fileId);
    if (!file) return null;

    const appData = await appDataDir();
    return `${appData}${file.path}`;
  }

  /**
   * Delete file
   */
  async deleteFile(fileId: string): Promise<void> {
    const metadata = await this.loadMetadata();
    const file = metadata.files.find(f => f.id === fileId);

    if (!file) {
      throw new Error('File not found');
    }

    // Delete physical file
    try {
      await remove(file.path, { baseDir: BaseDirectory.AppData });
    } catch (error) {
      console.warn('[FileStorageService] File delete warning:', error);
    }

    // Update metadata
    metadata.files = metadata.files.filter(f => f.id !== fileId);
    await this.saveMetadata(metadata);

    console.log('[FileStorageService] File deleted:', file.originalName);
  }

  /**
   * Download file to user's chosen location
   */
  async downloadFile(fileId: string, defaultName?: string): Promise<void> {
    const file = await this.getFile(fileId);
    if (!file) {
      throw new Error('File not found');
    }

    // Ask user where to save
    const savePath = await save({
      defaultPath: defaultName || file.originalName,
      filters: [{
        name: 'All Files',
        extensions: ['*'],
      }],
    });

    if (!savePath) return; // User cancelled

    // Copy file to chosen location
    const fileContent = await readFile(file.path, { baseDir: BaseDirectory.AppData });
    await writeFile(savePath, fileContent);

    console.log('[FileStorageService] File downloaded:', file.originalName);
  }

  /**
   * Read file content
   */
  async readFileContent(fileId: string): Promise<Uint8Array> {
    const file = await this.getFile(fileId);
    if (!file) {
      throw new Error('File not found');
    }

    return await readFile(file.path, { baseDir: BaseDirectory.AppData });
  }

  /**
   * Get storage statistics
   */
  async getStats(): Promise<{ fileCount: number; totalSize: number }> {
    const files = await this.listFiles();
    return {
      fileCount: files.length,
      totalSize: files.reduce((sum, f) => sum + f.size, 0),
    };
  }

  /**
   * Load metadata
   */
  private async loadMetadata(): Promise<FileMetadata> {
    try {
      const content = await readFile(this.metadataPath, { baseDir: BaseDirectory.AppData });
      const text = new TextDecoder().decode(content);
      return JSON.parse(text);
    } catch {
      return { files: [] };
    }
  }

  /**
   * Save metadata
   */
  private async saveMetadata(metadata: FileMetadata): Promise<void> {
    const text = JSON.stringify(metadata, null, 2);
    const bytes = new TextEncoder().encode(text);
    await writeFile(this.metadataPath, bytes, { baseDir: BaseDirectory.AppData });
  }
}

export const fileStorageService = new FileStorageService();
export type { StoredFile };
