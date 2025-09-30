// Excel File Adapter (XLSX)
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class ExcelAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const filePath = this.getConfig<string>('filePath');
      const url = this.getConfig<string>('url');

      if (!filePath && !url) {
        return this.createErrorResult('Either file path or URL is required');
      }

      console.log('Validating Excel file configuration (client-side only)');

      // If URL is provided, we can try to validate it
      if (url) {
        try {
          const response = await fetch(url, { method: 'HEAD' });

          if (!response.ok) {
            throw new Error(`URL not accessible: ${response.status} ${response.statusText}`);
          }

          const contentType = response.headers.get('content-type');
          const contentLength = response.headers.get('content-length');

          // Check if it's an Excel file
          const isExcel =
            contentType?.includes('spreadsheet') ||
            contentType?.includes('excel') ||
            url.toLowerCase().endsWith('.xlsx') ||
            url.toLowerCase().endsWith('.xls');

          if (!isExcel) {
            return this.createErrorResult(
              'URL does not appear to point to an Excel file. Expected .xlsx or .xls file.'
            );
          }

          return this.createSuccessResult('Excel file accessible at URL', {
            url,
            contentType,
            size: contentLength ? `${(parseInt(contentLength) / 1024).toFixed(2)} KB` : 'Unknown',
            timestamp: new Date().toISOString(),
          });
        } catch (fetchError) {
          return this.createErrorResult(
            `Failed to access Excel file at URL: ${fetchError instanceof Error ? fetchError.message : String(fetchError)}`
          );
        }
      }

      // For file path, perform basic validation
      return this.createSuccessResult(`Configuration validated for Excel file "${filePath}"`, {
        validatedAt: new Date().toISOString(),
        filePath,
        note: 'Client-side validation only. Full file parsing requires backend integration with xlsx library.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/excel', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'excel',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`Excel operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filePath: this.getConfig('filePath'),
      url: this.getConfig('url'),
    };
  }

  /**
   * Get list of sheet names
   */
  async getSheets(): Promise<string[]> {
    const result = await this.query('getSheets');
    return result.sheets || [];
  }

  /**
   * Read data from a specific sheet
   */
  async readSheet(sheetName: string, options?: { range?: string; header?: number }): Promise<any> {
    return await this.query('readSheet', { sheetName, ...options });
  }

  /**
   * Read all sheets
   */
  async readAll(): Promise<Record<string, any[]>> {
    return await this.query('readAll');
  }

  /**
   * Get sheet info (row count, column count)
   */
  async getSheetInfo(sheetName: string): Promise<any> {
    return await this.query('getSheetInfo', { sheetName });
  }

  /**
   * Read range from sheet (e.g., "A1:D10")
   */
  async readRange(sheetName: string, range: string): Promise<any> {
    return await this.query('readRange', { sheetName, range });
  }

  /**
   * Search for value in sheet
   */
  async search(sheetName: string, searchValue: string): Promise<any> {
    return await this.query('search', { sheetName, searchValue });
  }

  /**
   * Get cell value
   */
  async getCellValue(sheetName: string, cell: string): Promise<any> {
    return await this.query('getCellValue', { sheetName, cell });
  }

  /**
   * Convert sheet to JSON
   */
  async sheetToJSON(sheetName: string, options?: { header?: number; range?: string }): Promise<any[]> {
    return await this.query('sheetToJSON', { sheetName, ...options });
  }
}
