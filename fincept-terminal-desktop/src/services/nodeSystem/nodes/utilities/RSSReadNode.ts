import { INodeType, INodeTypeDescription, NodeConnectionType, IExecuteFunctions, NodePropertyType } from '../../types';

export class RSSReadNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'RSS Feed Reader',
    name: 'rssRead',
    icon: 'fa:rss',
    group: ['utilities'],
    version: 1,
    description: 'Read and parse RSS/Atom feeds',
    defaults: { name: 'RSS Feed Reader' },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Feed URL',
        name: 'feedUrl',
        type: NodePropertyType.String,
        default: '',
        required: true,
        description: 'URL of the RSS/Atom feed',
      },
      {
        displayName: 'Max Items',
        name: 'maxItems',
        type: NodePropertyType.Number,
        default: 20,
        description: 'Maximum number of items to return',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<any> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const feedUrl = this.getNodeParameter('feedUrl', i) as string;
        const maxItems = this.getNodeParameter('maxItems', i) as number;

        const response = await fetch(feedUrl);
        const xmlText = await response.text();

        const parser = new DOMParser();
        const xmlDoc = parser.parseFromString(xmlText, 'text/xml');

        // Check if RSS or Atom
        const isAtom = xmlDoc.querySelector('feed') !== null;
        const itemSelector = isAtom ? 'entry' : 'item';
        const itemElements = xmlDoc.querySelectorAll(itemSelector);

        let count = 0;
        itemElements.forEach((itemEl) => {
          if (count >= maxItems) return;

          const item: any = {};

          if (isAtom) {
            // Atom feed
            item.title = itemEl.querySelector('title')?.textContent || '';
            item.link = itemEl.querySelector('link')?.getAttribute('href') || '';
            item.description = itemEl.querySelector('summary')?.textContent || itemEl.querySelector('content')?.textContent || '';
            item.pubDate = itemEl.querySelector('updated')?.textContent || itemEl.querySelector('published')?.textContent || '';
            item.author = itemEl.querySelector('author name')?.textContent || '';
          } else {
            // RSS feed
            item.title = itemEl.querySelector('title')?.textContent || '';
            item.link = itemEl.querySelector('link')?.textContent || '';
            item.description = itemEl.querySelector('description')?.textContent || '';
            item.pubDate = itemEl.querySelector('pubDate')?.textContent || '';
            item.author = itemEl.querySelector('author')?.textContent || itemEl.querySelector('dc\\:creator')?.textContent || '';
            item.category = itemEl.querySelector('category')?.textContent || '';
            item.guid = itemEl.querySelector('guid')?.textContent || '';
          }

          returnData.push({ json: item, pairedItem: { item: i } });
          count++;
        });
      } catch (error: any) {
        returnData.push({ json: { error: error.message }, pairedItem: { item: i } });
      }
    }

    return [returnData];
  }
}
