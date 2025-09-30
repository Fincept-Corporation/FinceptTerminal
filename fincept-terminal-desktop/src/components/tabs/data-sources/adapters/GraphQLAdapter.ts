// GraphQL API Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class GraphQLAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const endpoint = this.getConfig<string>('endpoint');
      const apiKey = this.getConfig<string>('apiKey');
      const authHeader = this.getConfig<string>('authHeader', 'Authorization');
      const authPrefix = this.getConfig<string>('authPrefix', 'Bearer');

      if (!endpoint) {
        return this.createErrorResult('Endpoint URL is required');
      }

      console.log('Testing GraphQL endpoint...');

      try {
        const headers: Record<string, string> = {
          'Content-Type': 'application/json',
        };

        if (apiKey) {
          headers[authHeader] = `${authPrefix} ${apiKey}`;
        }

        // Test with introspection query
        const introspectionQuery = `
          {
            __schema {
              queryType { name }
              mutationType { name }
              subscriptionType { name }
            }
          }
        `;

        const response = await fetch(endpoint, {
          method: 'POST',
          headers,
          body: JSON.stringify({ query: introspectionQuery }),
        });

        if (!response.ok) {
          if (response.status === 401 || response.status === 403) {
            throw new Error('Authentication failed. Check your API key or credentials.');
          }
          throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const data = await response.json();

        if (data.errors) {
          // Some endpoints disable introspection, which is fine
          if (data.errors[0]?.message?.includes('introspection')) {
            return this.createSuccessResult('Successfully connected to GraphQL endpoint', {
              endpoint,
              note: 'Introspection is disabled on this endpoint, but connection is valid',
              timestamp: new Date().toISOString(),
            });
          }
          throw new Error(data.errors[0].message);
        }

        return this.createSuccessResult('Successfully connected to GraphQL endpoint', {
          endpoint,
          queryType: data.data?.__schema?.queryType?.name,
          mutationType: data.data?.__schema?.mutationType?.name,
          subscriptionType: data.data?.__schema?.subscriptionType?.name,
          timestamp: new Date().toISOString(),
        });
      } catch (fetchError) {
        if (fetchError instanceof Error) {
          if (fetchError.message.includes('Failed to fetch')) {
            return this.createErrorResult(`Cannot reach GraphQL endpoint at ${endpoint}.`);
          }
          return this.createErrorResult(fetchError.message);
        }
        throw fetchError;
      }
    } catch (error) {
      return this.createErrorResult(error);
    }
  }

  async query(graphqlQuery: string, variables?: Record<string, any>): Promise<any> {
    try {
      const endpoint = this.getConfig<string>('endpoint');
      const apiKey = this.getConfig<string>('apiKey');
      const authHeader = this.getConfig<string>('authHeader', 'Authorization');
      const authPrefix = this.getConfig<string>('authPrefix', 'Bearer');

      const headers: Record<string, string> = {
        'Content-Type': 'application/json',
      };

      if (apiKey) {
        headers[authHeader] = `${authPrefix} ${apiKey}`;
      }

      const response = await fetch(endpoint, {
        method: 'POST',
        headers,
        body: JSON.stringify({
          query: graphqlQuery,
          variables,
        }),
      });

      if (!response.ok) {
        throw new Error(`Query failed: ${response.status} ${response.statusText}`);
      }

      const data = await response.json();

      if (data.errors) {
        throw new Error(data.errors.map((e: any) => e.message).join(', '));
      }

      return data.data;
    } catch (error) {
      throw new Error(`GraphQL query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      endpoint: this.getConfig('endpoint'),
      hasApiKey: !!this.getConfig('apiKey'),
      authHeader: this.getConfig('authHeader', 'Authorization'),
      authPrefix: this.getConfig('authPrefix', 'Bearer'),
    };
  }

  /**
   * Get schema via introspection
   */
  async getSchema(): Promise<any> {
    const introspectionQuery = `
      {
        __schema {
          types {
            name
            kind
            description
            fields {
              name
              type {
                name
                kind
              }
            }
          }
        }
      }
    `;

    return await this.query(introspectionQuery);
  }

  /**
   * Get query type
   */
  async getQueryType(): Promise<any> {
    const introspectionQuery = `
      {
        __schema {
          queryType {
            name
            fields {
              name
              description
              args {
                name
                type {
                  name
                  kind
                }
              }
              type {
                name
                kind
              }
            }
          }
        }
      }
    `;

    return await this.query(introspectionQuery);
  }

  /**
   * Get mutation type
   */
  async getMutationType(): Promise<any> {
    const introspectionQuery = `
      {
        __schema {
          mutationType {
            name
            fields {
              name
              description
              args {
                name
                type {
                  name
                  kind
                }
              }
              type {
                name
                kind
              }
            }
          }
        }
      }
    `;

    return await this.query(introspectionQuery);
  }

  /**
   * Execute mutation
   */
  async mutate(mutation: string, variables?: Record<string, any>): Promise<any> {
    return await this.query(mutation, variables);
  }
}
