// Excel Service - Now using high-performance Rust SQLite backend via Tauri commands

import { invoke } from '@tauri-apps/api/core';

export interface ExcelFile {
  id?: number;
  file_name: string;
  file_path: string;
  sheet_count: number;
  last_opened: string;
  last_modified: string;
  created_at: string;
}

export interface ExcelSnapshot {
  id?: number;
  file_id: number;
  snapshot_name: string;
  sheet_data: string; // JSON stringified sheet data
  created_at: string;
}

class ExcelService {
  // No initialization needed - Rust backend handles it

  async initialize() {
    console.log('[ExcelService] Using Rust SQLite backend');
  }

  async addOrUpdateFile(file: Omit<ExcelFile, 'id'>): Promise<number> {
    try {
      const fileData = {
        ...file,
        created_at: file.created_at || new Date().toISOString()
      };
      const id = await invoke<number>('db_add_or_update_excel_file', { file: fileData });
      console.log('[ExcelService] Added/updated excel file with ID:', id);
      return id;
    } catch (error) {
      console.error('[ExcelService] Failed to add/update file:', error);
      throw error;
    }
  }

  async getRecentFiles(limit: number = 10): Promise<ExcelFile[]> {
    try {
      return await invoke<ExcelFile[]>('db_get_recent_excel_files', { limit });
    } catch (error) {
      console.error('[ExcelService] Failed to get recent files:', error);
      throw error;
    }
  }

  async createSnapshot(fileId: number, snapshotName: string, sheetData: string): Promise<number> {
    try {
      const createdAt = new Date().toISOString();
      const id = await invoke<number>('db_create_excel_snapshot', {
        fileId,
        snapshotName,
        sheetData,
        createdAt
      });
      console.log('[ExcelService] Created snapshot with ID:', id);
      return id;
    } catch (error) {
      console.error('[ExcelService] Failed to create snapshot:', error);
      throw error;
    }
  }

  async getSnapshots(fileId: number): Promise<ExcelSnapshot[]> {
    try {
      return await invoke<ExcelSnapshot[]>('db_get_excel_snapshots', { fileId });
    } catch (error) {
      console.error('[ExcelService] Failed to get snapshots:', error);
      throw error;
    }
  }

  async deleteSnapshot(id: number): Promise<void> {
    try {
      await invoke('db_delete_excel_snapshot', { id });
      console.log('[ExcelService] Deleted snapshot:', id);
    } catch (error) {
      console.error('[ExcelService] Failed to delete snapshot:', error);
      throw error;
    }
  }
}

export const excelService = new ExcelService();
export default excelService;
