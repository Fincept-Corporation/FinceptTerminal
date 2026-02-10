import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class XMLNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'XML Parser',
    name: 'xml',
    icon: 'fa:code',
    group: ['data'],
    version: 1,
    description: 'Parse, extract, and transform XML data',
    defaults: { name: 'XML Parser' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Mode',
        name: 'mode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Parse to JSON', value: 'parseToJson' },
          { name: 'JSON to XML', value: 'jsonToXml' },
          { name: 'Extract Value', value: 'extractValue' },
        ],
        default: 'parseToJson',
        description: 'Operation to perform',
      },
      {
        displayName: 'XML String',
        name: 'xmlString',
        type: NodePropertyType.String,
        default: '',
        description: 'XML string to parse (for parseToJson mode)',
        displayOptions: {
          show: {
            mode: ['parseToJson'],
          },
        },
      },
      {
        displayName: 'XPath',
        name: 'xpath',
        type: NodePropertyType.String,
        default: '',
        description: 'XPath expression to extract value',
        displayOptions: {
          show: {
            mode: ['extractValue'],
          },
        },
      },
      {
        displayName: 'Root Element Name',
        name: 'rootElementName',
        type: NodePropertyType.String,
        default: 'root',
        description: 'Root element name for JSON to XML conversion',
        displayOptions: {
          show: {
            mode: ['jsonToXml'],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];
    const mode = this.getNodeParameter('mode', 0) as string;

    for (let i = 0; i < items.length; i++) {
      try {
        if (mode === 'parseToJson') {
          const xmlString = (this.getNodeParameter('xmlString', i) as string || items[i].json.xml) as string;
          const parser = new DOMParser();
          const xmlDoc = parser.parseFromString(xmlString, 'text/xml');
          const json = this.xmlToJson(xmlDoc.documentElement);
          returnData.push({ json, pairedItem: { item: i } });
        } else if (mode === 'jsonToXml') {
          const rootElementName = this.getNodeParameter('rootElementName', i) as string;
          const json = items[i].json;
          const xml = this.jsonToXml(json, rootElementName);
          returnData.push({ json: { xml }, pairedItem: { item: i } });
        } else if (mode === 'extractValue') {
          const xmlString = items[i].json.xml as string;
          const xpath = this.getNodeParameter('xpath', i) as string;
          const parser = new DOMParser();
          const xmlDoc = parser.parseFromString(xmlString, 'text/xml');
          const result = xmlDoc.evaluate(xpath, xmlDoc, null, XPathResult.STRING_TYPE, null);
          returnData.push({ json: { value: result.stringValue }, pairedItem: { item: i } });
        }
      } catch (error: any) {
        returnData.push({ json: { error: error.message }, pairedItem: { item: i } });
      }
    }

    return [returnData];
  }

  private xmlToJson(xml: Element): any {
    const obj: any = {};

    if (xml.nodeType === 1) {
      if (xml.attributes.length > 0) {
        obj['@attributes'] = {};
        for (let j = 0; j < xml.attributes.length; j++) {
          const attribute = xml.attributes.item(j);
          if (attribute) {
            obj['@attributes'][attribute.nodeName] = attribute.nodeValue;
          }
        }
      }
    } else if (xml.nodeType === 3) {
      obj.value = xml.nodeValue;
    }

    if (xml.hasChildNodes()) {
      for (let i = 0; i < xml.childNodes.length; i++) {
        const item = xml.childNodes.item(i);
        if (!item) continue;
        const nodeName = item.nodeName;
        if (typeof obj[nodeName] === 'undefined') {
          if (item.nodeType === 3) {
            const text = item.nodeValue?.trim();
            if (text) return text;
          } else {
            obj[nodeName] = this.xmlToJson(item as Element);
          }
        } else {
          if (!Array.isArray(obj[nodeName])) {
            const old = obj[nodeName];
            obj[nodeName] = [old];
          }
          obj[nodeName].push(this.xmlToJson(item as Element));
        }
      }
    }
    return obj;
  }

  private jsonToXml(obj: any, rootName: string): string {
    let xml = `<?xml version="1.0" encoding="UTF-8"?>\n<${rootName}>`;
    xml += this.objToXml(obj);
    xml += `</${rootName}>`;
    return xml;
  }

  private objToXml(obj: any): string {
    let xml = '';
    for (const key in obj) {
      if (obj.hasOwnProperty(key)) {
        if (Array.isArray(obj[key])) {
          for (const item of obj[key]) {
            xml += `<${key}>${typeof item === 'object' ? this.objToXml(item) : item}</${key}>`;
          }
        } else if (typeof obj[key] === 'object') {
          xml += `<${key}>${this.objToXml(obj[key])}</${key}>`;
        } else {
          xml += `<${key}>${obj[key]}</${key}>`;
        }
      }
    }
    return xml;
  }
}
