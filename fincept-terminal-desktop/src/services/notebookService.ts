// Notebook Service - notebook history tracking

import { notebookLogger } from './loggerService';
import { saveSetting, getSetting } from './sqliteService';

const STORAGE_KEY = 'fincept_notebook_history';

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
  private isInitialized = false;
  private nextId = 1;

  async initialize() {
    if (this.isInitialized) return;

    try {
      // Ensure storage exists
      const existing = await getSetting(STORAGE_KEY);
      if (!existing) {
        await saveSetting(STORAGE_KEY, JSON.stringify([]), 'notebooks');
      }

      // Initialize next ID
      const history = await this.getHistoryFromStorage();
      if (history.length > 0) {
        const maxId = Math.max(...history.map(n => n.id || 0));
        this.nextId = maxId + 1;
      }

      this.isInitialized = true;
      notebookLogger.info('Notebook service initialized successfully');
    } catch (error) {
      notebookLogger.error('Failed to initialize notebook service:', error);
      throw error;
    }
  }

  async saveToHistory(notebook: NotebookHistory): Promise<number> {
    await this.initialize();

    try {
      const history = await this.getHistoryFromStorage();
      const id = this.nextId++;

      const newEntry: NotebookHistory = {
        ...notebook,
        id,
      };

      history.push(newEntry);
      await saveSetting(STORAGE_KEY, JSON.stringify(history), 'notebooks');

      return id;
    } catch (error) {
      notebookLogger.error('Failed to save notebook to history:', error);
      throw error;
    }
  }

  async updateHistory(id: number, notebook: Partial<NotebookHistory>): Promise<void> {
    await this.initialize();

    try {
      const history = await this.getHistoryFromStorage();
      const index = history.findIndex(n => n.id === id);

      if (index === -1) {
        throw new Error(`Notebook with id ${id} not found`);
      }

      history[index] = {
        ...history[index],
        ...notebook,
        id, // Preserve ID
        updated_at: new Date().toISOString(),
      };

      await saveSetting(STORAGE_KEY, JSON.stringify(history), 'notebooks');
    } catch (error) {
      notebookLogger.error('Failed to update notebook history:', error);
      throw error;
    }
  }

  async getHistory(limit: number = 50): Promise<NotebookHistory[]> {
    await this.initialize();

    try {
      const history = await this.getHistoryFromStorage();

      // Sort by updated_at DESC and limit
      return history
        .sort((a, b) => new Date(b.updated_at).getTime() - new Date(a.updated_at).getTime())
        .slice(0, limit);
    } catch (error) {
      notebookLogger.error('Failed to get notebook history:', error);
      return [];
    }
  }

  async getNotebookById(id: number): Promise<NotebookHistory | null> {
    await this.initialize();

    try {
      const history = await this.getHistoryFromStorage();
      const notebook = history.find(n => n.id === id);

      return notebook || null;
    } catch (error) {
      notebookLogger.error('Failed to get notebook by ID:', error);
      return null;
    }
  }

  async deleteFromHistory(id: number): Promise<void> {
    await this.initialize();

    try {
      const history = await this.getHistoryFromStorage();
      const filtered = history.filter(n => n.id !== id);

      await saveSetting(STORAGE_KEY, JSON.stringify(filtered), 'notebooks');
    } catch (error) {
      notebookLogger.error('Failed to delete notebook from history:', error);
      throw error;
    }
  }

  async searchHistory(query: string): Promise<NotebookHistory[]> {
    await this.initialize();

    try {
      const history = await this.getHistoryFromStorage();
      const lowerQuery = query.toLowerCase();

      const results = history.filter(n =>
        n.name.toLowerCase().includes(lowerQuery) ||
        (n.path && n.path.toLowerCase().includes(lowerQuery))
      );

      // Sort by updated_at DESC and limit to 50
      return results
        .sort((a, b) => new Date(b.updated_at).getTime() - new Date(a.updated_at).getTime())
        .slice(0, 50);
    } catch (error) {
      notebookLogger.error('Failed to search notebook history:', error);
      return [];
    }
  }

  async clearHistory(): Promise<void> {
    await this.initialize();

    try {
      await saveSetting(STORAGE_KEY, JSON.stringify([]), 'notebooks');
      this.nextId = 1;
    } catch (error) {
      notebookLogger.error('Failed to clear notebook history:', error);
      throw error;
    }
  }

  private async getHistoryFromStorage(): Promise<NotebookHistory[]> {
    const data = await getSetting(STORAGE_KEY);
    return data ? JSON.parse(data) : [];
  }
}

export const notebookService = new NotebookService();
