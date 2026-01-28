import { InternalTool } from '../types';

export const reportBuilderTools: InternalTool[] = [
  {
    name: 'create_report_template',
    description: 'Create a new report template with title and description. Works from any tab. After creation, you should tell the user the report was created and they can view it in the Report Builder tab.',
    inputSchema: {
      type: 'object',
      properties: {
        title: { type: 'string', description: 'Report title' },
        description: { type: 'string', description: 'Report description' },
        author: { type: 'string', description: 'Author name' },
        company: { type: 'string', description: 'Company name' },
      },
      required: ['title'],
    },
    handler: async (args, contexts) => {
      if (!contexts.createReportTemplate) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      const result = await contexts.createReportTemplate({
        title: args.title,
        description: args.description || '',
        author: args.author,
        company: args.company,
      });

      return {
        success: true,
        data: result,
        message: `Report template "${args.title}" created successfully with ID: ${result.id}`
      };
    },
  },
  {
    name: 'add_report_component',
    description: 'Add a component to the current report (heading, text, kpi, chart, table, etc.). Works from any tab.',
    inputSchema: {
      type: 'object',
      properties: {
        type: {
          type: 'string',
          description: 'Component type',
          enum: [
            'heading', 'subheading', 'text', 'quote', 'list',
            'chart', 'table', 'image', 'code',
            'kpi', 'sparkline', 'liveTable', 'dynamicChart',
            'toc', 'signature', 'disclaimer', 'qrcode', 'watermark',
            'section', 'columns', 'coverpage', 'divider', 'pagebreak'
          ],
        },
        content: { type: 'string', description: 'Component content (for text-based components)' },
        config: { type: 'string', description: 'JSON string of component configuration' },
      },
      required: ['type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addReportComponent) {
        return { success: false, error: 'Report Builder service unavailable' };
      }

      let config = {};
      if (args.config && typeof args.config === 'string') {
        try {
          config = JSON.parse(args.config);
        } catch (e) {
          return { success: false, error: 'Invalid JSON in config parameter' };
        }
      }

      const result = await contexts.addReportComponent({
        type: args.type,
        content: args.content,
        config,
      });
      return { success: true, data: result, message: `Added ${args.type} component to report` };
    },
  },
  {
    name: 'add_kpi_card',
    description: 'Add a KPI (Key Performance Indicator) card to the report',
    inputSchema: {
      type: 'object',
      properties: {
        label: { type: 'string', description: 'KPI label (e.g., Revenue, Profit, Growth)' },
        value: { type: 'string', description: 'KPI value (e.g., $1.2M, 25%, 340K)' },
        change: { type: 'string', description: 'Percentage change (e.g., 12.5, -5.2)' },
        trend: { type: 'string', description: 'Trend direction', enum: ['up', 'down'], default: 'up' },
      },
      required: ['label', 'value'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addReportComponent) {
        return { success: false, error: 'Report Builder service unavailable' };
      }

      const result = await contexts.addReportComponent({
        type: 'kpi',
        content: '',
        config: {
          kpis: [{
            label: args.label,
            value: args.value,
            change: args.change ? parseFloat(args.change) : 0,
            trend: args.trend || 'up',
          }],
        },
      });
      return { success: true, data: result, message: `Added KPI card "${args.label}" to report` };
    },
  },
  {
    name: 'add_report_table',
    description: 'Add a table to the report with specified columns',
    inputSchema: {
      type: 'object',
      properties: {
        columns: { type: 'string', description: 'Comma-separated column names (e.g., "Symbol, Price, Change")' },
        data_source: { type: 'string', description: 'Data source type', enum: ['portfolio', 'watchlist', 'market', 'static'], default: 'static' },
      },
      required: ['columns'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addReportComponent) {
        return { success: false, error: 'Report Builder service unavailable' };
      }

      const columns = args.columns.split(',').map((c: string) => c.trim());
      const isLiveTable = args.data_source && args.data_source !== 'static';

      const result = await contexts.addReportComponent({
        type: isLiveTable ? 'liveTable' : 'table',
        content: '',
        config: {
          columns,
          ...(isLiveTable && { dataSource: args.data_source }),
        },
      });
      return { success: true, data: result, message: `Added table with columns: ${columns.join(', ')}` };
    },
  },
  {
    name: 'add_report_chart',
    description: 'Add a chart to the report',
    inputSchema: {
      type: 'object',
      properties: {
        chart_type: { type: 'string', description: 'Chart type', enum: ['line', 'bar', 'pie', 'area'], default: 'line' },
        data: { type: 'string', description: 'JSON array of data points (e.g., [{"name":"Jan","value":100},{"name":"Feb","value":120}])' },
      },
      required: ['chart_type'],
    },
    handler: async (args, contexts) => {
      if (!contexts.addReportComponent) {
        return { success: false, error: 'Report Builder service unavailable' };
      }

      let chartData: any[] = [];
      if (args.data && typeof args.data === 'string') {
        try {
          chartData = JSON.parse(args.data);
        } catch (e) {
          return { success: false, error: 'Invalid JSON in data parameter' };
        }
      }

      const result = await contexts.addReportComponent({
        type: 'dynamicChart',
        content: '',
        config: {
          chartType: args.chart_type,
          data: chartData,
        },
      });
      return { success: true, data: result, message: `Added ${args.chart_type} chart to report` };
    },
  },
  {
    name: 'update_report_metadata',
    description: 'Update report metadata (title, author, company, date)',
    inputSchema: {
      type: 'object',
      properties: {
        title: { type: 'string', description: 'Report title' },
        author: { type: 'string', description: 'Author name' },
        company: { type: 'string', description: 'Company name' },
        date: { type: 'string', description: 'Report date (YYYY-MM-DD)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.updateReportMetadata) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      await contexts.updateReportMetadata(args);
      return { success: true, message: 'Report metadata updated' };
    },
  },
  {
    name: 'save_report_template',
    description: 'Save the current report template to disk',
    inputSchema: {
      type: 'object',
      properties: {
        filename: { type: 'string', description: 'Optional filename (without extension)' },
      },
    },
    handler: async (args, contexts) => {
      if (!contexts.saveReportTemplate) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      const result = await contexts.saveReportTemplate(args.filename);
      return { success: true, data: result, message: 'Report template saved successfully' };
    },
  },
  {
    name: 'load_report_template',
    description: 'Load a saved report template',
    inputSchema: {
      type: 'object',
      properties: {
        path: { type: 'string', description: 'Template file path' },
      },
      required: ['path'],
    },
    handler: async (args, contexts) => {
      if (!contexts.loadReportTemplate) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      const result = await contexts.loadReportTemplate(args.path);
      return { success: true, data: result, message: 'Report template loaded successfully' };
    },
  },
  {
    name: 'export_report',
    description: 'Export the current report to PDF, DOCX, or HTML',
    inputSchema: {
      type: 'object',
      properties: {
        format: { type: 'string', description: 'Export format', enum: ['pdf', 'docx', 'html'], default: 'pdf' },
        filename: { type: 'string', description: 'Optional filename (without extension)' },
      },
      required: ['format'],
    },
    handler: async (args, contexts) => {
      if (!contexts.exportReport) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      const result = await contexts.exportReport({
        format: args.format,
        filename: args.filename,
      });
      return { success: true, data: result, message: `Report exported as ${args.format.toUpperCase()}` };
    },
  },
  {
    name: 'list_report_components',
    description: 'List all components in the current report',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.listReportComponents) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      const components = await contexts.listReportComponents();
      return { success: true, data: components };
    },
  },
  {
    name: 'delete_report_component',
    description: 'Delete a component from the report by its ID',
    inputSchema: {
      type: 'object',
      properties: {
        component_id: { type: 'string', description: 'Component ID to delete' },
      },
      required: ['component_id'],
    },
    handler: async (args, contexts) => {
      if (!contexts.deleteReportComponent) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      await contexts.deleteReportComponent(args.component_id);
      return { success: true, message: 'Component deleted from report' };
    },
  },
  {
    name: 'apply_report_theme',
    description: 'Apply a page theme to the report',
    inputSchema: {
      type: 'object',
      properties: {
        theme: {
          type: 'string',
          description: 'Page theme',
          enum: ['classic', 'modern', 'geometric', 'gradient', 'minimal', 'pattern'],
          default: 'classic',
        },
      },
      required: ['theme'],
    },
    handler: async (args, contexts) => {
      if (!contexts.applyReportTheme) {
        return { success: false, error: 'Report Builder service unavailable' };
      }
      await contexts.applyReportTheme(args.theme);
      return { success: true, message: `Applied ${args.theme} theme to report` };
    },
  },
];
