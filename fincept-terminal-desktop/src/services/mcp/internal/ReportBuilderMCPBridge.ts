// Report Builder MCP Bridge - Connects MCP tools to Report Builder Service

import { terminalMCPProvider } from './TerminalMCPProvider';
import { ReportTemplate, ReportComponent, reportService } from '@/services/core/reportService';

class ReportBuilderMCPBridge {
  private template: ReportTemplate | null = null;
  private setTemplate: ((template: ReportTemplate) => void) | null = null;
  private addComponentFn: ((type: ReportComponent['type']) => ReportComponent) | null = null;
  private updateComponentFn: ((id: string, updates: Partial<ReportComponent>) => void) | null = null;
  private deleteComponentFn: ((id: string) => void) | null = null;
  private updateMetadataFn: ((updates: any) => void) | null = null;
  private saveTemplateFn: (() => Promise<void>) | null = null;
  private loadTemplateFn: ((path: string) => Promise<void>) | null = null;
  private exportReportFn: ((format: string) => Promise<string>) | null = null;
  private setPageTheme: ((theme: string) => void) | null = null;

  // Initialize global MCP contexts (works without tab being mounted)
  initializeGlobalContexts(): void {
    console.log('[ReportBuilderMCPBridge] Initializing global MCP contexts');
    terminalMCPProvider.setContexts({
      createReportTemplate: this.createReportTemplate.bind(this),
      addReportComponent: this.addReportComponent.bind(this),
      updateReportMetadata: this.updateReportMetadata.bind(this),
      saveReportTemplate: this.saveReportTemplate.bind(this),
      loadReportTemplate: this.loadReportTemplate.bind(this),
      exportReport: this.exportReport.bind(this),
      listReportComponents: this.listReportComponents.bind(this),
      deleteReportComponent: this.deleteReportComponent.bind(this),
      applyReportTheme: this.applyReportTheme.bind(this),
    });
  }

  updateTemplate(template: ReportTemplate): void {
    this.template = template;
  }

  async registerHandlers(handlers: {
    template: ReportTemplate;
    setTemplate: (template: ReportTemplate) => void;
    addComponent: (type: ReportComponent['type']) => ReportComponent;
    updateComponent: (id: string, updates: Partial<ReportComponent>) => void;
    deleteComponent: (id: string) => void;
    updateMetadata: (updates: any) => void;
    saveTemplate: () => Promise<void>;
    loadTemplate: (path: string) => Promise<void>;
    exportReport: (format: string) => Promise<string>;
    setPageTheme: (theme: string) => void;
  }): Promise<void> {
    console.log('[ReportBuilderMCPBridge] Registering handlers');
    this.setTemplate = handlers.setTemplate;
    this.addComponentFn = handlers.addComponent;
    this.updateComponentFn = handlers.updateComponent;
    this.deleteComponentFn = handlers.deleteComponent;
    this.updateMetadataFn = handlers.updateMetadata;
    this.saveTemplateFn = handlers.saveTemplate;
    this.loadTemplateFn = handlers.loadTemplate;
    this.exportReportFn = handlers.exportReport;
    this.setPageTheme = handlers.setPageTheme;

    // Check if there's a template in bridge (created via MCP)
    if (this.template && this.setTemplate) {
      console.log('[ReportBuilderMCPBridge] Syncing existing MCP template to UI:', this.template.name);
      this.setTemplate(this.template);
    } else {
      // Try to load the latest template from service
      try {
        const templates = await reportService.getTemplates();
        if (templates.length > 0) {
          // Get the most recently created template
          const latestTemplate = templates[templates.length - 1];
          console.log('[ReportBuilderMCPBridge] Loading latest template from service:', latestTemplate.name);
          this.template = latestTemplate;
          this.setTemplate(latestTemplate);
        } else {
          // No templates in service, use the UI's default empty template
          this.template = handlers.template;
        }
      } catch (error) {
        console.error('[ReportBuilderMCPBridge] Error loading templates from service:', error);
        // Fallback to UI's template
        this.template = handlers.template;
      }
    }
    // Contexts already registered globally via initializeGlobalContexts()
  }

  unregisterHandlers(): void {
    console.log('[ReportBuilderMCPBridge] Unregistering handlers');
    this.template = null;
    this.setTemplate = null;
    this.addComponentFn = null;
    this.updateComponentFn = null;
    this.deleteComponentFn = null;
    this.updateMetadataFn = null;
    this.saveTemplateFn = null;
    this.loadTemplateFn = null;
    this.exportReportFn = null;
    this.setPageTheme = null;

    // Clear contexts
    terminalMCPProvider.setContexts({
      createReportTemplate: undefined,
      addReportComponent: undefined,
      updateReportMetadata: undefined,
      saveReportTemplate: undefined,
      loadReportTemplate: undefined,
      exportReport: undefined,
      listReportComponents: undefined,
      deleteReportComponent: undefined,
      applyReportTheme: undefined,
    });
  }

  private async createReportTemplate(params: {
    title: string;
    description: string;
    author?: string;
    company?: string;
  }): Promise<ReportTemplate> {
    const newTemplate: ReportTemplate = {
      id: `report-${Date.now()}`,
      name: params.title,
      description: params.description,
      components: [],
      metadata: {
        title: params.title,
        author: params.author || '',
        company: params.company || '',
        date: new Date().toISOString().split('T')[0],
      },
      styles: {
        pageSize: 'A4',
        orientation: 'portrait',
        margins: '20mm',
        headerFooter: true,
      },
    };

    // Save to service
    await reportService.saveTemplate(newTemplate);
    this.template = newTemplate;

    // Update UI if tab is mounted
    if (this.setTemplate) {
      this.setTemplate(newTemplate);
    }

    return newTemplate;
  }

  private async addReportComponent(params: {
    type: string;
    content?: string;
    config?: any;
  }): Promise<ReportComponent> {
    if (!this.template) {
      throw new Error('No active report template. Create a template first.');
    }

    const newComponent: ReportComponent = {
      id: `component-${Date.now()}`,
      type: params.type as ReportComponent['type'],
      content: params.content || '',
      config: params.config || {},
    };

    // Add to template
    this.template.components.push(newComponent);
    await reportService.saveTemplate(this.template);

    // Update UI if tab is mounted - sync the entire template instead of adding a new component
    if (this.setTemplate) {
      this.setTemplate({ ...this.template });
    }

    return newComponent;
  }

  private async updateReportMetadata(metadata: Record<string, any>): Promise<void> {
    if (!this.template) {
      throw new Error('No active report template. Create a template first.');
    }

    // Update metadata in template
    this.template.metadata = { ...this.template.metadata, ...metadata };
    await reportService.saveTemplate(this.template);

    // Update UI if tab is mounted
    if (this.setTemplate) {
      this.setTemplate({ ...this.template });
    }
  }

  private async saveReportTemplate(filename?: string): Promise<{ path: string; filename: string }> {
    if (!this.saveTemplateFn) {
      throw new Error('Report builder not initialized');
    }
    await this.saveTemplateFn();
    return { path: 'saved', filename: filename || this.template?.name || 'report' };
  }

  private async loadReportTemplate(path: string): Promise<ReportTemplate> {
    if (!this.loadTemplateFn) {
      throw new Error('Report builder not initialized');
    }
    await this.loadTemplateFn(path);
    if (!this.template) {
      throw new Error('Failed to load template');
    }
    return this.template;
  }

  private async exportReport(params: { format: string; filename?: string }): Promise<{ path: string; format: string }> {
    if (!this.exportReportFn) {
      throw new Error('Report builder not initialized');
    }
    const path = await this.exportReportFn(params.format);
    return { path, format: params.format };
  }

  private async listReportComponents(): Promise<ReportComponent[]> {
    if (!this.template) {
      return [];
    }
    return this.template.components;
  }

  private async deleteReportComponent(id: string): Promise<void> {
    if (!this.template) {
      throw new Error('No active report template. Create a template first.');
    }

    // Remove component from template
    this.template.components = this.template.components.filter(c => c.id !== id);
    await reportService.saveTemplate(this.template);

    // Update UI if tab is mounted
    if (this.setTemplate) {
      this.setTemplate({ ...this.template });
    }
  }

  private async applyReportTheme(theme: string): Promise<void> {
    if (!this.template) {
      throw new Error('No active report template. Create a template first.');
    }

    // Update theme in template styles
    if (!this.template.styles) {
      this.template.styles = {
        pageSize: 'A4',
        orientation: 'portrait',
        margins: '20mm',
        headerFooter: true,
      };
    }

    // Store theme preference (you may want to add a theme field to styles)
    await reportService.saveTemplate(this.template);

    // Update UI if tab is mounted
    if (this.setTemplate) {
      this.setTemplate({ ...this.template });
    }
  }
}

export const reportBuilderMCPBridge = new ReportBuilderMCPBridge();
