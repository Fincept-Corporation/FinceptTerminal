import { invoke } from '@tauri-apps/api/core';
import Database from '@tauri-apps/plugin-sql';

interface NotebookHistory {
  id?: number;
  name: string;
  content: string;
  path: string | null;
  created_at: string;
  updated_at: string;
  cell_count: number;
  execution_count: number;
}

class NotebookService {
  private db: Database | null = null;
  private isInitialized = false;

  async initialize() {
    if (this.isInitialized) return;

    try {
      this.db = await Database.load('sqlite:notebooks.db');

      // Create notebooks history table
      await this.db.execute(`
        CREATE TABLE IF NOT EXISTS notebook_history (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          name TEXT NOT NULL,
          content TEXT NOT NULL,
          path TEXT,
          created_at TEXT NOT NULL,
          updated_at TEXT NOT NULL,
          cell_count INTEGER DEFAULT 0,
          execution_count INTEGER DEFAULT 0
        )
      `);

      // Create index on updated_at for faster queries
      await this.db.execute(`
        CREATE INDEX IF NOT EXISTS idx_notebook_updated
        ON notebook_history(updated_at DESC)
      `);

      this.isInitialized = true;
      console.log('Notebook service initialized successfully');
    } catch (error) {
      console.error('Failed to initialize notebook service:', error);
      throw error;
    }
  }

  async saveToHistory(notebook: NotebookHistory): Promise<number> {
    await this.initialize();

    try {
      const result = await this.db!.execute(
        `INSERT INTO notebook_history
         (name, content, path, created_at, updated_at, cell_count, execution_count)
         VALUES (?, ?, ?, ?, ?, ?, ?)`,
        [
          notebook.name,
          notebook.content,
          notebook.path,
          notebook.created_at,
          notebook.updated_at,
          notebook.cell_count,
          notebook.execution_count
        ]
      );

      return result.lastInsertId || 0;
    } catch (error) {
      console.error('Failed to save notebook to history:', error);
      throw error;
    }
  }

  async updateHistory(id: number, notebook: Partial<NotebookHistory>): Promise<void> {
    await this.initialize();

    try {
      const updates: string[] = [];
      const values: any[] = [];

      if (notebook.name !== undefined) {
        updates.push('name = ?');
        values.push(notebook.name);
      }
      if (notebook.content !== undefined) {
        updates.push('content = ?');
        values.push(notebook.content);
      }
      if (notebook.path !== undefined) {
        updates.push('path = ?');
        values.push(notebook.path);
      }
      if (notebook.cell_count !== undefined) {
        updates.push('cell_count = ?');
        values.push(notebook.cell_count);
      }
      if (notebook.execution_count !== undefined) {
        updates.push('execution_count = ?');
        values.push(notebook.execution_count);
      }

      updates.push('updated_at = ?');
      values.push(new Date().toISOString());

      values.push(id);

      await this.db!.execute(
        `UPDATE notebook_history SET ${updates.join(', ')} WHERE id = ?`,
        values
      );
    } catch (error) {
      console.error('Failed to update notebook history:', error);
      throw error;
    }
  }

  async getHistory(limit: number = 50): Promise<NotebookHistory[]> {
    await this.initialize();

    try {
      const result = await this.db!.select<NotebookHistory[]>(
        `SELECT * FROM notebook_history
         ORDER BY updated_at DESC
         LIMIT ?`,
        [limit]
      );

      return result;
    } catch (error) {
      console.error('Failed to get notebook history:', error);
      return [];
    }
  }

  async getNotebookById(id: number): Promise<NotebookHistory | null> {
    await this.initialize();

    try {
      const result = await this.db!.select<NotebookHistory[]>(
        'SELECT * FROM notebook_history WHERE id = ?',
        [id]
      );

      return result.length > 0 ? result[0] : null;
    } catch (error) {
      console.error('Failed to get notebook by ID:', error);
      return null;
    }
  }

  async deleteFromHistory(id: number): Promise<void> {
    await this.initialize();

    try {
      await this.db!.execute(
        'DELETE FROM notebook_history WHERE id = ?',
        [id]
      );
    } catch (error) {
      console.error('Failed to delete notebook from history:', error);
      throw error;
    }
  }

  async searchHistory(query: string): Promise<NotebookHistory[]> {
    await this.initialize();

    try {
      const result = await this.db!.select<NotebookHistory[]>(
        `SELECT * FROM notebook_history
         WHERE name LIKE ? OR path LIKE ?
         ORDER BY updated_at DESC
         LIMIT 50`,
        [`%${query}%`, `%${query}%`]
      );

      return result;
    } catch (error) {
      console.error('Failed to search notebook history:', error);
      return [];
    }
  }

  async clearHistory(): Promise<void> {
    await this.initialize();

    try {
      await this.db!.execute('DELETE FROM notebook_history');
    } catch (error) {
      console.error('Failed to clear notebook history:', error);
      throw error;
    }
  }
}

export const notebookService = new NotebookService();
