import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class FilterTransformNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Filter Rows',
    name: 'filterTransform',
    icon: 'fa:filter',
    group: ['Transform'],
    version: 1,
    description: 'Filter items based on conditions',
    defaults: {
      name: 'Filter Rows',
      color: '#8b5cf6',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Filter Mode',
        name: 'filterMode',
        type: NodePropertyType.Options,
        options: [
          { name: 'Field vs Value', value: 'fieldValue' },
          { name: 'Field vs Field', value: 'fieldField' },
          { name: 'Preset Filter', value: 'preset' },
        ],
        default: 'fieldValue',
        description: 'How to compare: field against static value, or field against another field',
      },
      {
        displayName: 'Preset',
        name: 'preset',
        type: NodePropertyType.Options,
        options: [
          { name: 'Positive Days (close > open)', value: 'positiveDays' },
          { name: 'Negative Days (close < open)', value: 'negativeDays' },
          { name: 'High Volume (> average)', value: 'highVolume' },
          { name: 'Price Up (close > previous close)', value: 'priceUp' },
          { name: 'Price Down (close < previous close)', value: 'priceDown' },
        ],
        default: 'positiveDays',
        description: 'Common financial filters',
        displayOptions: {
          show: { filterMode: ['preset'] },
        },
      },
      {
        displayName: 'Field',
        name: 'field',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'close',
        description: 'Field to compare (left side)',
        displayOptions: {
          show: { filterMode: ['fieldValue', 'fieldField'] },
        },
      },
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Greater Than (>)', value: 'gt' },
          { name: 'Greater Than or Equal (>=)', value: 'gte' },
          { name: 'Less Than (<)', value: 'lt' },
          { name: 'Less Than or Equal (<=)', value: 'lte' },
          { name: 'Equals (==)', value: 'eq' },
          { name: 'Not Equals (!=)', value: 'neq' },
          { name: 'Contains', value: 'contains' },
        ],
        default: 'gt',
        displayOptions: {
          show: { filterMode: ['fieldValue', 'fieldField'] },
        },
      },
      {
        displayName: 'Compare To Field',
        name: 'compareField',
        type: NodePropertyType.String,
        default: '',
        placeholder: 'open',
        description: 'Field to compare against (right side)',
        displayOptions: {
          show: { filterMode: ['fieldField'] },
        },
      },
      {
        displayName: 'Value',
        name: 'value',
        type: NodePropertyType.String,
        default: '',
        placeholder: '0',
        description: 'Static value to compare against',
        displayOptions: {
          show: { filterMode: ['fieldValue'] },
        },
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const filterMode = this.getNodeParameter('filterMode', 0) as string;

    if (!items || items.length === 0) {
      return [[]];
    }

    let filtered: any[] = [];

    if (filterMode === 'preset') {
      const preset = this.getNodeParameter('preset', 0) as string;
      filtered = applyPresetFilter(items, preset);
    } else if (filterMode === 'fieldField') {
      const field = this.getNodeParameter('field', 0) as string;
      const operation = this.getNodeParameter('operation', 0) as string;
      const compareField = this.getNodeParameter('compareField', 0) as string;

      filtered = items.filter((item) => {
        const leftValue = getFieldValue(item.json, field);
        const rightValue = getFieldValue(item.json, compareField);
        return compareValues(leftValue, operation, rightValue);
      });
    } else {
      // fieldValue mode (default)
      const field = this.getNodeParameter('field', 0) as string;
      const operation = this.getNodeParameter('operation', 0) as string;
      const value = this.getNodeParameter('value', 0);

      filtered = items.filter((item) => {
        const fieldValue = getFieldValue(item.json, field);
        return compareValues(fieldValue, operation, value);
      });
    }

    return [filtered.map((item, i) => ({ ...item, pairedItem: i }))];
  }
}

function getFieldValue(obj: any, path: string): any {
  if (!obj || !path) return undefined;
  return path.split('.').reduce((o, k) => o?.[k], obj);
}

function compareValues(left: any, operation: string, right: any): boolean {
  const leftNum = Number(left);
  const rightNum = Number(right);
  const useNumbers = !isNaN(leftNum) && !isNaN(rightNum);

  switch (operation) {
    case 'gt':
      return useNumbers ? leftNum > rightNum : left > right;
    case 'gte':
      return useNumbers ? leftNum >= rightNum : left >= right;
    case 'lt':
      return useNumbers ? leftNum < rightNum : left < right;
    case 'lte':
      return useNumbers ? leftNum <= rightNum : left <= right;
    case 'eq':
      return left == right;
    case 'neq':
      return left != right;
    case 'contains':
      return String(left).includes(String(right));
    default:
      return true;
  }
}

function applyPresetFilter(items: any[], preset: string): any[] {
  switch (preset) {
    case 'positiveDays':
      // Close > Open (bullish/green candle)
      return items.filter((item) => {
        const close = Number(getFieldValue(item.json, 'close'));
        const open = Number(getFieldValue(item.json, 'open'));
        return !isNaN(close) && !isNaN(open) && close > open;
      });

    case 'negativeDays':
      // Close < Open (bearish/red candle)
      return items.filter((item) => {
        const close = Number(getFieldValue(item.json, 'close'));
        const open = Number(getFieldValue(item.json, 'open'));
        return !isNaN(close) && !isNaN(open) && close < open;
      });

    case 'highVolume': {
      // Volume > average volume
      const volumes = items.map((item) => Number(getFieldValue(item.json, 'volume'))).filter((v) => !isNaN(v));
      const avgVolume = volumes.length > 0 ? volumes.reduce((a, b) => a + b, 0) / volumes.length : 0;
      return items.filter((item) => {
        const volume = Number(getFieldValue(item.json, 'volume'));
        return !isNaN(volume) && volume > avgVolume;
      });
    }

    case 'priceUp':
    case 'priceDown': {
      // Compare with previous close - requires sorted data
      const sorted = [...items].sort((a, b) => {
        const dateA = new Date(getFieldValue(a.json, 'timestamp') || getFieldValue(a.json, 'date') || 0);
        const dateB = new Date(getFieldValue(b.json, 'timestamp') || getFieldValue(b.json, 'date') || 0);
        return dateA.getTime() - dateB.getTime();
      });

      return sorted.filter((item, index) => {
        if (index === 0) return false; // First item has no previous
        const currentClose = Number(getFieldValue(item.json, 'close'));
        const prevClose = Number(getFieldValue(sorted[index - 1].json, 'close'));
        if (isNaN(currentClose) || isNaN(prevClose)) return false;
        return preset === 'priceUp' ? currentClose > prevClose : currentClose < prevClose;
      });
    }

    default:
      return items;
  }
}
