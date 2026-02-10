import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class HTMLExtractNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Web Scraper',
    name: 'htmlExtract',
    icon: 'fa:html5',
    group: ['data'],
    version: 1,
    description: 'Extract data from HTML using CSS selectors',
    defaults: { name: 'Web Scraper' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'HTML Source',
        name: 'htmlSource',
        type: NodePropertyType.Options,
        options: [
          { name: 'From Input', value: 'input' },
          { name: 'From URL', value: 'url' },
        ],
        default: 'input',
      },
      {
        displayName: 'URL',
        name: 'url',
        type: NodePropertyType.String,
        default: '',
        displayOptions: {
          show: {
            htmlSource: ['url'],
          },
        },
      },
      {
        displayName: 'Extraction Mode',
        name: 'extractionMode',
        type: NodePropertyType.Options,
        options: [
          { name: 'CSS Selector', value: 'cssSelector' },
          { name: 'Extract Tables', value: 'tables' },
          { name: 'Extract Links', value: 'links' },
          { name: 'Extract Text', value: 'text' },
        ],
        default: 'cssSelector',
      },
      {
        displayName: 'CSS Selector',
        name: 'cssSelector',
        type: NodePropertyType.String,
        default: '',
        description: 'CSS selector to match elements',
        displayOptions: {
          show: {
            extractionMode: ['cssSelector'],
          },
        },
      },
      {
        displayName: 'Extract Attribute',
        name: 'extractAttribute',
        type: NodePropertyType.String,
        default: 'textContent',
        description: 'Attribute to extract (textContent, innerHTML, href, src, etc.)',
        displayOptions: {
          show: {
            extractionMode: ['cssSelector'],
          },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const htmlSource = this.getNodeParameter('htmlSource', i) as string;
        const extractionMode = this.getNodeParameter('extractionMode', i) as string;

        let html = '';
        if (htmlSource === 'url') {
          const url = this.getNodeParameter('url', i) as string;
          const response = await fetch(url);
          html = await response.text();
        } else {
          html = items[i].json.html as string;
        }

        const parser = new DOMParser();
        const doc = parser.parseFromString(html, 'text/html');

        if (extractionMode === 'cssSelector') {
          const cssSelector = this.getNodeParameter('cssSelector', i) as string;
          const extractAttribute = this.getNodeParameter('extractAttribute', i) as string;
          const elements = doc.querySelectorAll(cssSelector);
          const results: any[] = [];

          elements.forEach((el) => {
            if (extractAttribute === 'textContent') {
              results.push(el.textContent?.trim());
            } else if (extractAttribute === 'innerHTML') {
              results.push(el.innerHTML);
            } else {
              results.push(el.getAttribute(extractAttribute));
            }
          });

          returnData.push({ json: { results }, pairedItem: { item: i } });
        } else if (extractionMode === 'tables') {
          const tables = doc.querySelectorAll('table');
          const tablesData: any[] = [];

          tables.forEach((table) => {
            const rows: any[] = [];
            const tableRows = table.querySelectorAll('tr');

            tableRows.forEach((tr) => {
              const cells: string[] = [];
              tr.querySelectorAll('td, th').forEach((cell) => {
                cells.push(cell.textContent?.trim() || '');
              });
              if (cells.length > 0) rows.push(cells);
            });

            tablesData.push(rows);
          });

          returnData.push({ json: { tables: tablesData }, pairedItem: { item: i } });
        } else if (extractionMode === 'links') {
          const links = doc.querySelectorAll('a');
          const linksData: any[] = [];

          links.forEach((link) => {
            linksData.push({
              text: link.textContent?.trim(),
              href: link.getAttribute('href'),
            });
          });

          returnData.push({ json: { links: linksData }, pairedItem: { item: i } });
        } else if (extractionMode === 'text') {
          const text = doc.body.textContent?.trim() || '';
          returnData.push({ json: { text }, pairedItem: { item: i } });
        }
      } catch (error: any) {
        returnData.push({ json: { error: error.message }, pairedItem: { item: i } });
      }
    }

    return [returnData];
  }
}
