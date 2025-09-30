// gRPC Service Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class GRPCAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const host = this.getConfig<string>('host');
      const port = this.getConfig<number>('port', 50051);
      const tls = this.getConfig<boolean>('tls', false);

      if (!host) {
        return this.createErrorResult('Host is required');
      }

      console.log('Validating gRPC configuration (client-side only)');

      // gRPC requires specialized libraries (grpc-js, protobufjs) that need backend integration
      return this.createSuccessResult(`Configuration validated for gRPC at ${host}:${port}`, {
        validatedAt: new Date().toISOString(),
        host,
        port,
        tls,
        note: 'Client-side validation only. gRPC connections require backend integration with @grpc/grpc-js and proto definitions.',
      });
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async callMethod(method: string, request: any): Promise<any> {
    try {
      // gRPC calls need to go through backend
      const response = await fetch('/api/data-sources/grpc', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          connectionId: this.connection.id,
          type: 'grpc',
          method,
          request,
        }),
      });

      if (!response.ok) {
        throw new Error(`gRPC call failed: ${response.statusText}`);
      }

      return await response.json();
    } catch (error) {
      throw new Error(`gRPC query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      host: this.getConfig('host'),
      port: this.getConfig('port', 50051),
      tls: this.getConfig('tls', false),
      protoFile: this.getConfig('protoFile'),
    };
  }

  /**
   * Call a unary RPC method
   */
  async callUnary(serviceName: string, methodName: string, request: any): Promise<any> {
    return await this.query(`${serviceName}/${methodName}`, request);
  }

  /**
   * List available services
   */
  async listServices(): Promise<any> {
    return await this.query('reflection/listServices', {});
  }

  /**
   * Get service definition
   */
  async getServiceDefinition(serviceName: string): Promise<any> {
    return await this.query('reflection/getServiceDefinition', { serviceName });
  }

  /**
   * Get method signature
   */
  async getMethodSignature(serviceName: string, methodName: string): Promise<any> {
    return await this.query('reflection/getMethodSignature', { serviceName, methodName });
  }
}
