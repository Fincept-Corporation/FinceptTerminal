// XML File Adapter
import { BaseAdapter } from './BaseAdapter';
import { TestConnectionResult } from '../types';

export class XMLAdapter extends BaseAdapter {
  async testConnection(): Promise<TestConnectionResult> {
    try {
      const validation = this.validateConfig();
      if (!validation.valid) {
        return this.createErrorResult(validation.errors.join(', '));
      }

      const filepath = this.getConfig<string>('filepath');

      if (!filepath) {
        return this.createErrorResult('File path is required');
      }

      console.log('Testing XML file accessibility...');

      try {
        // Try to fetch and parse the XML file
        const response = await fetch(filepath);

        if (!response.ok) {
          throw new Error(`Cannot access XML file: ${response.status} ${response.statusText}`);
        }

        const xmlText = await response.text();

        // Basic XML validation
        const parser = new DOMParser();
        const xmlDoc = parser.parseFromString(xmlText, 'text/xml');

        const parseError = xmlDoc.querySelector('parsererror');
        if (parseError) {
          throw new Error('Invalid XML format');
        }

        const rootElement = xmlDoc.documentElement;

        return this.createSuccessResult('Successfully loaded and parsed XML file', {
          filepath,
          rootElement: rootElement.tagName,
          childNodes: rootElement.childNodes.length,
          fileSize: xmlText.length,
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

  async query(xpath?: string): Promise<any> {
    try {
      const filepath = this.getConfig<string>('filepath');

      const response = await fetch(filepath);

      if (!response.ok) {
        throw new Error(`Cannot access XML file: ${response.statusText}`);
      }

      const xmlText = await response.text();
      const parser = new DOMParser();
      const xmlDoc = parser.parseFromString(xmlText, 'text/xml');

      const parseError = xmlDoc.querySelector('parsererror');
      if (parseError) {
        throw new Error('Invalid XML format');
      }

      if (xpath) {
        // If XPath is provided, use it to extract specific data
        const result = xmlDoc.evaluate(xpath, xmlDoc, null, XPathResult.ANY_TYPE, null);
        const nodes = [];
        let node = result.iterateNext();
        while (node) {
          nodes.push(this.xmlNodeToObject(node));
          node = result.iterateNext();
        }
        return nodes;
      }

      // Return the entire document structure
      return this.xmlToJson(xmlDoc.documentElement);
    } catch (error) {
      throw new Error(`XML query error: ${error instanceof Error ? error.message : String(error)}`);
    }
  }

  async getMetadata(): Promise<Record<string, any>> {
    const base = await super.getMetadata();
    return {
      ...base,
      filepath: this.getConfig('filepath'),
      xpath: this.getConfig('xpath'),
      provider: 'XML File',
    };
  }

  /**
   * Convert XML node to JavaScript object
   */
  private xmlNodeToObject(node: Node): any {
    if (node.nodeType === Node.TEXT_NODE) {
      return node.nodeValue?.trim() || '';
    }

    if (node.nodeType === Node.ELEMENT_NODE) {
      const element = node as Element;
      const obj: any = {};

      // Add attributes
      if (element.attributes.length > 0) {
        obj['@attributes'] = {};
        for (let i = 0; i < element.attributes.length; i++) {
          const attr = element.attributes[i];
          obj['@attributes'][attr.name] = attr.value;
        }
      }

      // Add child nodes
      if (element.childNodes.length > 0) {
        for (let i = 0; i < element.childNodes.length; i++) {
          const child = element.childNodes[i];

          if (child.nodeType === Node.TEXT_NODE) {
            const text = child.nodeValue?.trim();
            if (text) {
              obj['#text'] = text;
            }
          } else if (child.nodeType === Node.ELEMENT_NODE) {
            const childElement = child as Element;
            const childObj = this.xmlNodeToObject(childElement);

            if (obj[childElement.tagName]) {
              if (!Array.isArray(obj[childElement.tagName])) {
                obj[childElement.tagName] = [obj[childElement.tagName]];
              }
              obj[childElement.tagName].push(childObj);
            } else {
              obj[childElement.tagName] = childObj;
            }
          }
        }
      }

      return obj;
    }

    return null;
  }

  /**
   * Convert entire XML document to JSON
   */
  private xmlToJson(element: Element): any {
    const obj: any = {};
    obj[element.tagName] = this.xmlNodeToObject(element);
    return obj;
  }

  /**
   * Get XML data
   */
  async getData(xpath?: string): Promise<any> {
    return await this.query(xpath);
  }

  /**
   * Get root element
   */
  async getRootElement(): Promise<any> {
    const filepath = this.getConfig<string>('filepath');
    const response = await fetch(filepath);
    const xmlText = await response.text();
    const parser = new DOMParser();
    const xmlDoc = parser.parseFromString(xmlText, 'text/xml');

    return {
      tagName: xmlDoc.documentElement.tagName,
      attributes: Array.from(xmlDoc.documentElement.attributes).map((attr) => ({
        name: attr.name,
        value: attr.value,
      })),
      childCount: xmlDoc.documentElement.childNodes.length,
    };
  }

  /**
   * Query with XPath
   */
  async queryXPath(xpath: string): Promise<any> {
    return await this.query(xpath);
  }
}
