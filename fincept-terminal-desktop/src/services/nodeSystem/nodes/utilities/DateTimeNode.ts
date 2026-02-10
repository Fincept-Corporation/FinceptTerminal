import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class DateTimeNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Date & Time',
    name: 'dateTime',
    icon: 'fa:clock',
    group: ['utilities'],
    version: 1,
    description: 'Date and time operations & formatting',
    defaults: {
      name: 'Date & Time',
      color: '#ff6b6b',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Action',
        name: 'action',
        type: NodePropertyType.Options,
        options: [
          { name: 'Get Current Time', value: 'getCurrent' },
          { name: 'Format Date', value: 'format' },
          { name: 'Parse Date', value: 'parse' },
          { name: 'Add Time', value: 'add' },
          { name: 'Subtract Time', value: 'subtract' },
          { name: 'Difference', value: 'diff' },
          { name: 'Extract Components', value: 'extract' },
        ],
        default: 'getCurrent',
        required: true,
        description: 'Operation to perform',
      },
      {
        displayName: 'Input Date',
        name: 'inputDate',
        type: NodePropertyType.String,
        default: '',
        placeholder: '2024-01-01 or ISO string',
        description: 'Date to operate on (leave empty for current)',
      },
      {
        displayName: 'Format',
        name: 'format',
        type: NodePropertyType.String,
        default: 'YYYY-MM-DD HH:mm:ss',
        placeholder: 'YYYY-MM-DD HH:mm:ss',
        description: 'Output format (YYYY, MM, DD, HH, mm, ss)',
      },
      {
        displayName: 'Amount',
        name: 'amount',
        type: NodePropertyType.Number,
        default: 1,
        description: 'Amount to add/subtract',
      },
      {
        displayName: 'Unit',
        name: 'unit',
        type: NodePropertyType.Options,
        options: [
          { name: 'Years', value: 'years' },
          { name: 'Months', value: 'months' },
          { name: 'Days', value: 'days' },
          { name: 'Hours', value: 'hours' },
          { name: 'Minutes', value: 'minutes' },
          { name: 'Seconds', value: 'seconds' },
        ],
        default: 'days',
        description: 'Time unit',
      },
      {
        displayName: 'Timezone',
        name: 'timezone',
        type: NodePropertyType.String,
        default: 'UTC',
        placeholder: 'UTC or America/New_York',
        description: 'Timezone identifier',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData: any[] = [];

    for (let i = 0; i < items.length; i++) {
      try {
        const action = this.getNodeParameter('action', i) as string;
        const inputDateStr = this.getNodeParameter('inputDate', i, '') as string;
        const format = this.getNodeParameter('format', i, 'YYYY-MM-DD HH:mm:ss') as string;
        const amount = this.getNodeParameter('amount', i, 1) as number;
        const unit = this.getNodeParameter('unit', i, 'days') as string;
        const timezone = this.getNodeParameter('timezone', i, 'UTC') as string;

        const inputDate = inputDateStr ? new Date(inputDateStr) : new Date();
        let result: any = {};

        switch (action) {
          case 'getCurrent':
            result = {
              timestamp: Date.now(),
              iso: new Date().toISOString(),
              formatted: this.formatDate(new Date(), format),
              unix: Math.floor(Date.now() / 1000),
            };
            break;

          case 'format':
            result = {
              formatted: this.formatDate(inputDate, format),
              iso: inputDate.toISOString(),
              timestamp: inputDate.getTime(),
            };
            break;

          case 'parse':
            result = {
              timestamp: inputDate.getTime(),
              iso: inputDate.toISOString(),
              formatted: this.formatDate(inputDate, format),
            };
            break;

          case 'add':
            const added = this.addTime(inputDate, amount, unit);
            result = {
              original: inputDate.toISOString(),
              result: added.toISOString(),
              formatted: this.formatDate(added, format),
            };
            break;

          case 'subtract':
            const subtracted = this.addTime(inputDate, -amount, unit);
            result = {
              original: inputDate.toISOString(),
              result: subtracted.toISOString(),
              formatted: this.formatDate(subtracted, format),
            };
            break;

          case 'diff':
            const now = new Date();
            const diffMs = now.getTime() - inputDate.getTime();
            result = {
              milliseconds: diffMs,
              seconds: Math.floor(diffMs / 1000),
              minutes: Math.floor(diffMs / 60000),
              hours: Math.floor(diffMs / 3600000),
              days: Math.floor(diffMs / 86400000),
            };
            break;

          case 'extract':
            result = {
              year: inputDate.getFullYear(),
              month: inputDate.getMonth() + 1,
              day: inputDate.getDate(),
              hour: inputDate.getHours(),
              minute: inputDate.getMinutes(),
              second: inputDate.getSeconds(),
              dayOfWeek: inputDate.getDay(),
              timestamp: inputDate.getTime(),
            };
            break;
        }

        returnData.push({
          json: {
            dateTime: result,
            ...items[i].json,
          },
          pairedItem: i,
        });
      } catch (error: any) {
        console.error('[DateTimeNode] Error:', error);
        if (this.continueOnFail()) {
          returnData.push({
            json: {
              error: error.message,
              ...items[i].json,
            },
            pairedItem: i,
          });
        } else {
          throw error;
        }
      }
    }

    return [returnData];
  }

  private formatDate(date: Date, format: string): string {
    const map: Record<string, string> = {
      YYYY: date.getFullYear().toString(),
      MM: String(date.getMonth() + 1).padStart(2, '0'),
      DD: String(date.getDate()).padStart(2, '0'),
      HH: String(date.getHours()).padStart(2, '0'),
      mm: String(date.getMinutes()).padStart(2, '0'),
      ss: String(date.getSeconds()).padStart(2, '0'),
    };

    return format.replace(/YYYY|MM|DD|HH|mm|ss/g, (match) => map[match] || match);
  }

  private addTime(date: Date, amount: number, unit: string): Date {
    const result = new Date(date);
    switch (unit) {
      case 'years':
        result.setFullYear(result.getFullYear() + amount);
        break;
      case 'months':
        result.setMonth(result.getMonth() + amount);
        break;
      case 'days':
        result.setDate(result.getDate() + amount);
        break;
      case 'hours':
        result.setHours(result.getHours() + amount);
        break;
      case 'minutes':
        result.setMinutes(result.getMinutes() + amount);
        break;
      case 'seconds':
        result.setSeconds(result.getSeconds() + amount);
        break;
    }
    return result;
  }
}
