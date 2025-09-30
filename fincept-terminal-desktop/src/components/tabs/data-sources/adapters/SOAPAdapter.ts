// SOAP Web Service Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class SOAPAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const wsdlUrl = this.getConfig<string>('wsdlUrl');

      if (!wsdlUrl) {
        return this.createErrorResult('WSDL URL is required');
      }

      console.log('Testing SOAP service accessibility...');

      try {
        // Try to fetch the WSDL
        const response = await fetch(wsdlUrl);

        if (!response.ok) {
          throw new Error(`Cannot access WSDL: ${response.status} ${response.statusText}`);
        }

        const wsdlText = await response.text();

        // Basic WSDL validation
        if (!wsdlText.includes('wsdl:definitions') && !wsdlText.includes('<definitions')) {
          throw new Error('Invalid WSDL format');
        }

        // Parse WSDL to extract service info
        const parser = new DOMParser();
        const wsdlDoc = parser.parseFromString(wsdlText, 'text/xml');

        const serviceName = wsdlDoc.querySelector('service')?.getAttribute('name') || 'Unknown';
        const portCount = wsdlDoc.querySelectorAll('port').length;
        const operationCount = wsdlDoc.querySelectorAll('operation').length;

        return this.createSuccessResult('Successfully accessed SOAP service WSDL', {
          wsdlUrl,
          serviceName,
          ports: portCount,
          operations: operationCount,
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(operation: string, params?: Record<string, any>): Promise<any> {
    try {
      // SOAP operations need to go through backend with soap library
      const response = await fetch('/api/data-sources/soap', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'soap',
          wsdlUrl: this.getConfig('wsdlUrl'),
          operation,
          params,
          username: this.getConfig('username'),
          password: this.getConfig('password'),
        }),
      });

      if (!response.ok) {
        throw new Error(`SOAP operation failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`SOAP query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      wsdlUrl: this.getConfig('wsdlUrl'),
      hasCredentials: !!(this.getConfig('username') && this.getConfig('password')),
      provider: 'SOAP',
    };
  }

  /**
   * Call SOAP operation
   */
  async callOperation(operationName: string, params: Record<string, any>): Promise<any> {
    return await this.query(operationName, params);
  }

  /**
   * Get WSDL definition
   */
  async getWSDL(): Promise<any> {
    const wsdlUrl = this.getConfig<string>('wsdlUrl');
    const response = await fetch(wsdlUrl);
    const wsdlText = await response.text();
    return { wsdl: wsdlText };
  }

  /**
   * List operations
   */
  async listOperations(): Promise<any> {
    const wsdlUrl = this.getConfig<string>('wsdlUrl');
    const response = await fetch(wsdlUrl);
    const wsdlText = await response.text();

    const parser = new DOMParser();
    const wsdlDoc = parser.parseFromString(wsdlText, 'text/xml');

    const operations = Array.from(wsdlDoc.querySelectorAll('portType operation')).map((op) => ({
      name: op.getAttribute('name'),
      input: op.querySelector('input')?.getAttribute('message'),
      output: op.querySelector('output')?.getAttribute('message'),
    }));

    return { operations };
  }

  /**
   * Get operation parameters
   */
  async getOperationParams(operationName: string): Promise<any> {
    return await this.query('getOperationParams', { operationName });
  }

  /**
   * Get service endpoints
   */
  async getEndpoints(): Promise<any> {
    const wsdlUrl = this.getConfig<string>('wsdlUrl');
    const response = await fetch(wsdlUrl);
    const wsdlText = await response.text();

    const parser = new DOMParser();
    const wsdlDoc = parser.parseFromString(wsdlText, 'text/xml');

    const endpoints = Array.from(wsdlDoc.querySelectorAll('service port')).map((port) => ({
      name: port.getAttribute('name'),
      binding: port.getAttribute('binding'),
      address: port.querySelector('address')?.getAttribute('location'),
    }));

    return { endpoints };
  }
}
