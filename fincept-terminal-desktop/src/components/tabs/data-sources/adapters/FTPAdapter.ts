// FTP File Transfer Protocol Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class FTPAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 21);
      const username = this.getConfig<string>('username');
      const password = this.getConfig<string>('password');

      if (!host || !username || !password) {
        return this.createErrorResult('Host, username, and password are required');
      }

      console.log('Validating FTP configuration (client-side only)');

      return this.createSuccessResult(`Configuration validated for FTP at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        note: 'Client-side validation only. FTP requires backend integration with basic-ftp or similar library.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      const response = await fetch('/api/data-sources/ftp', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'ftp',
          operation,
          params,
        }),
      });

      if (!response.ok) {
        throw new Error(`Operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`FTP operation error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 21),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
    };
  }

  async list(path: string = '/'): Promise<any> {
    return await this.query('list', { path });
  }

  async get(remotePath: string): Promise<any> {
    return await this.query('get', { remotePath });
  }

  async put(localPath: string, remotePath: string): Promise<any> {
    return await this.query('put', { localPath, remotePath });
  }

  async delete(remotePath: string): Promise<any> {
    return await this.query('delete', { remotePath });
  }

  async mkdir(path: string): Promise<any> {
    return await this.query('mkdir', { path });
  }

  async rmdir(path: string): Promise<any> {
    return await this.query('rmdir', { path });
  }
}
