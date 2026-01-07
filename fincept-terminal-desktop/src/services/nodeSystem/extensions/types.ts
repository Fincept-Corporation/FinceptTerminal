/**
 * Extension System Types
 * Based on n8n's extension architecture
 */

export interface ExtensionMap {
  typeName: string;
  functions: Record<string, ExtensionFunction>;
}

export type ExtensionFunction = (value: any, args: any[]) => any;

/**
 * Extension metadata for documentation
 */
export interface ExtensionDoc {
  name: string;
  description: string;
  returnType: string;
  args?: Array<{
    name: string;
    type: string;
    optional?: boolean;
    description?: string;
  }>;
  examples?: Array<{
    example: string;
    evaluated: string;
  }>;
}
