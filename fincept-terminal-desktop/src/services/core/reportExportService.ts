import { invoke } from '@tauri-apps/api/core';
import { save } from '@tauri-apps/plugin-dialog';
import { writeTextFile } from '@tauri-apps/plugin-fs';
import { ReportTemplate, ReportComponent } from './reportService';
import { BrandKit } from './brandKitService';

export type ExportFormat = 'pdf' | 'html' | 'docx' | 'markdown';

export interface ExportOptions {
  format: ExportFormat;
  includeStyles?: boolean;
  includeHeader?: boolean;
  includeFooter?: boolean;
}

export interface BatchExportOptions {
  formats: ExportFormat[];
  basePath?: string;
}

class ReportExportService {
  // Generate HTML from template
  generateHTML(template: ReportTemplate, brandKit?: BrandKit | null): string {
    const styles = this.generateStyles(brandKit);
    const content = this.renderComponents(template.components, 'html');

    return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>${template.metadata.title || 'Report'}</title>
  <style>${styles}</style>
</head>
<body>
  <div class="report-container">
    <header class="report-header">
      <h1>${template.metadata.title || 'Report'}</h1>
      ${template.metadata.company ? `<p class="company">${template.metadata.company}</p>` : ''}
      ${template.metadata.author ? `<p class="author">By ${template.metadata.author}</p>` : ''}
      <p class="date">${template.metadata.date || new Date().toLocaleDateString()}</p>
    </header>
    <main class="report-content">
      ${content}
    </main>
    <footer class="report-footer">
      <p>&copy; ${new Date().getFullYear()} ${template.metadata.company || ''}</p>
    </footer>
  </div>
</body>
</html>`;
  }

  // Generate Markdown from template
  generateMarkdown(template: ReportTemplate): string {
    let md = `# ${template.metadata.title || 'Report'}\n\n`;

    if (template.metadata.company) md += `**${template.metadata.company}**\n\n`;
    if (template.metadata.author) md += `*By ${template.metadata.author}*\n\n`;
    if (template.metadata.date) md += `Date: ${template.metadata.date}\n\n`;

    md += '---\n\n';
    md += this.renderComponents(template.components, 'markdown');

    return md;
  }

  // Generate DOCX XML structure
  generateDocxContent(template: ReportTemplate): string {
    const content = this.renderComponents(template.components, 'docx');

    return `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:p>
      <w:pPr><w:pStyle w:val="Title"/></w:pPr>
      <w:r><w:t>${template.metadata.title || 'Report'}</w:t></w:r>
    </w:p>
    ${template.metadata.company ? `<w:p><w:r><w:t>${template.metadata.company}</w:t></w:r></w:p>` : ''}
    ${template.metadata.author ? `<w:p><w:r><w:t>By ${template.metadata.author}</w:t></w:r></w:p>` : ''}
    <w:p><w:r><w:t>${template.metadata.date || ''}</w:t></w:r></w:p>
    ${content}
  </w:body>
</w:document>`;
  }

  // Render components to different formats
  private renderComponents(components: ReportComponent[], format: 'html' | 'markdown' | 'docx'): string {
    return components.map(comp => this.renderComponent(comp, format)).join('\n');
  }

  private renderComponent(comp: ReportComponent, format: 'html' | 'markdown' | 'docx'): string {
    const content = comp.content || '';

    switch (format) {
      case 'html':
        return this.renderComponentHTML(comp, content);
      case 'markdown':
        return this.renderComponentMarkdown(comp, content);
      case 'docx':
        return this.renderComponentDocx(comp, content);
      default:
        return '';
    }
  }

  private renderComponentHTML(comp: ReportComponent, content: string): string {
    switch (comp.type) {
      case 'heading':
        return `<h2 class="heading" style="text-align:${comp.config.alignment || 'left'}">${content}</h2>`;
      case 'subheading':
        return `<h3 class="subheading" style="text-align:${comp.config.alignment || 'left'}">${content}</h3>`;
      case 'text':
        return `<p style="text-align:${comp.config.alignment || 'left'}">${content}</p>`;
      case 'divider':
        return '<hr class="divider"/>';
      case 'code':
        return `<pre class="code-block"><code>${this.escapeHtml(content)}</code></pre>`;
      case 'table':
        return this.renderTableHTML(comp);
      case 'image':
        return comp.config.imageUrl ? `<img src="file://${comp.config.imageUrl}" alt="Report Image" class="report-image"/>` : '';
      case 'section':
        return `<section class="section" style="border-left:4px solid ${comp.config.borderColor || '#FFA500'};background:${comp.config.backgroundColor || '#f8f9fa'};padding:${comp.config.padding || '15px'}">
          <h4>${comp.config.sectionTitle || 'Section'}</h4>
          ${comp.children ? this.renderComponents(comp.children, 'html') : ''}
        </section>`;
      case 'columns':
        return `<div class="columns columns-${comp.config.columnCount || 2}">
          ${comp.children ? comp.children.map(c => `<div class="column">${this.renderComponent(c, 'html')}</div>`).join('') : ''}
        </div>`;
      case 'pagebreak':
        return '<div class="page-break"></div>';
      case 'coverpage':
        return `<div class="cover-page" style="background:${comp.config.backgroundColor || '#0a0a0a'};color:#FFA500;text-align:center;padding:100px 50px;">
          <h1>${content}</h1>
          ${comp.config.subtitle ? `<p class="subtitle">${comp.config.subtitle}</p>` : ''}
        </div>`;
      default:
        return `<div>${content}</div>`;
    }
  }

  private renderComponentMarkdown(comp: ReportComponent, content: string): string {
    switch (comp.type) {
      case 'heading':
        return `## ${content}\n\n`;
      case 'subheading':
        return `### ${content}\n\n`;
      case 'text':
        return `${content}\n\n`;
      case 'divider':
        return '---\n\n';
      case 'code':
        return `\`\`\`\n${content}\n\`\`\`\n\n`;
      case 'table':
        return this.renderTableMarkdown(comp);
      case 'image':
        return comp.config.imageUrl ? `![Image](${comp.config.imageUrl})\n\n` : '';
      case 'section':
        return `> **${comp.config.sectionTitle || 'Section'}**\n>\n> ${comp.children ? this.renderComponents(comp.children, 'markdown').replace(/\n/g, '\n> ') : ''}\n\n`;
      case 'pagebreak':
        return '\n---\n<div style="page-break-after: always;"></div>\n\n';
      case 'coverpage':
        return `# ${content}\n\n${comp.config.subtitle ? `*${comp.config.subtitle}*\n\n` : ''}---\n\n`;
      default:
        return `${content}\n\n`;
    }
  }

  private renderComponentDocx(comp: ReportComponent, content: string): string {
    const escaped = this.escapeXml(content);
    switch (comp.type) {
      case 'heading':
        return `<w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:t>${escaped}</w:t></w:r></w:p>`;
      case 'subheading':
        return `<w:p><w:pPr><w:pStyle w:val="Heading2"/></w:pPr><w:r><w:t>${escaped}</w:t></w:r></w:p>`;
      case 'text':
        return `<w:p><w:r><w:t>${escaped}</w:t></w:r></w:p>`;
      case 'divider':
        return `<w:p><w:pPr><w:pBdr><w:bottom w:val="single" w:sz="6" w:space="1" w:color="auto"/></w:pBdr></w:pPr></w:p>`;
      case 'code':
        return `<w:p><w:pPr><w:pStyle w:val="Code"/></w:pPr><w:r><w:rPr><w:rFonts w:ascii="Courier New"/></w:rPr><w:t>${escaped}</w:t></w:r></w:p>`;
      default:
        return `<w:p><w:r><w:t>${escaped}</w:t></w:r></w:p>`;
    }
  }

  private renderTableHTML(comp: ReportComponent): string {
    const columns = comp.config.columns || ['Column 1', 'Column 2', 'Column 3'];
    const rows = (comp.content as any)?.rows || [columns.map(() => 'Sample data')];

    return `<table class="report-table">
      <thead><tr>${columns.map(c => `<th>${c}</th>`).join('')}</tr></thead>
      <tbody>${rows.map((row: string[]) => `<tr>${row.map(cell => `<td>${cell}</td>`).join('')}</tr>`).join('')}</tbody>
    </table>`;
  }

  private renderTableMarkdown(comp: ReportComponent): string {
    const columns = comp.config.columns || ['Column 1', 'Column 2', 'Column 3'];
    const rows = (comp.content as any)?.rows || [columns.map(() => 'Sample data')];

    let md = `| ${columns.join(' | ')} |\n`;
    md += `| ${columns.map(() => '---').join(' | ')} |\n`;
    rows.forEach((row: string[]) => {
      md += `| ${row.join(' | ')} |\n`;
    });
    return md + '\n';
  }

  private generateStyles(brandKit?: BrandKit | null): string {
    const primary = brandKit?.colors.find(c => c.usage === 'primary')?.hex || '#FFA500';
    const text = brandKit?.colors.find(c => c.usage === 'text')?.hex || '#1a1a1a';
    const bg = brandKit?.colors.find(c => c.usage === 'background')?.hex || '#ffffff';
    const bodyFont = brandKit?.fonts.find(f => f.usage === 'body')?.family || 'Arial, sans-serif';
    const headingFont = brandKit?.fonts.find(f => f.usage === 'heading')?.family || 'Georgia, serif';

    return `
      * { margin: 0; padding: 0; box-sizing: border-box; }
      body { font-family: ${bodyFont}; color: ${text}; background: ${bg}; line-height: 1.6; }
      .report-container { max-width: 210mm; margin: 0 auto; padding: 20mm; }
      .report-header { border-bottom: 2px solid ${primary}; padding-bottom: 20px; margin-bottom: 30px; }
      .report-header h1 { font-family: ${headingFont}; font-size: 28pt; color: ${text}; }
      .report-header .company { font-size: 14pt; color: #666; }
      .report-header .author, .report-header .date { font-size: 11pt; color: #888; }
      .heading { font-family: ${headingFont}; font-size: 18pt; margin: 20px 0 10px; color: ${text}; }
      .subheading { font-family: ${headingFont}; font-size: 14pt; margin: 15px 0 8px; color: ${text}; }
      p { margin-bottom: 12px; }
      .divider { border: none; border-top: 1px solid #ddd; margin: 20px 0; }
      .code-block { background: #1a1a1a; color: #00ff00; padding: 15px; border-radius: 4px; overflow-x: auto; font-family: monospace; }
      .report-table { width: 100%; border-collapse: collapse; margin: 15px 0; }
      .report-table th, .report-table td { border: 1px solid #ddd; padding: 10px; text-align: left; }
      .report-table th { background: #f5f5f5; font-weight: bold; }
      .report-image { max-width: 100%; height: auto; margin: 15px 0; }
      .section { margin: 20px 0; border-radius: 4px; }
      .columns { display: flex; gap: 20px; margin: 15px 0; }
      .columns-2 .column { flex: 1; }
      .columns-3 .column { flex: 1; }
      .page-break { page-break-after: always; height: 0; }
      .cover-page { min-height: 500px; display: flex; flex-direction: column; justify-content: center; align-items: center; }
      .cover-page h1 { font-size: 36pt; margin-bottom: 20px; }
      .cover-page .subtitle { font-size: 18pt; }
      .report-footer { margin-top: 40px; padding-top: 20px; border-top: 1px solid #ddd; text-align: center; color: #888; font-size: 10pt; }
      @media print { .page-break { page-break-after: always; } }
      ${brandKit?.customCSS || ''}
    `;
  }

  private escapeHtml(text: string): string {
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
  }

  private escapeXml(text: string): string {
    return text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&apos;');
  }

  // Export to file
  async exportToFile(template: ReportTemplate, format: ExportFormat, brandKit?: BrandKit | null): Promise<string | null> {
    const extensions: Record<ExportFormat, string> = {
      pdf: 'pdf', html: 'html', docx: 'docx', markdown: 'md'
    };

    const filePath = await save({
      filters: [{ name: format.toUpperCase(), extensions: [extensions[format]] }],
      defaultPath: `${template.metadata.title || 'report'}.${extensions[format]}`
    });

    if (!filePath) return null;

    switch (format) {
      case 'html':
        await writeTextFile(filePath, this.generateHTML(template, brandKit));
        break;
      case 'markdown':
        await writeTextFile(filePath, this.generateMarkdown(template));
        break;
      case 'docx':
        // For DOCX, we'll generate HTML and let Rust handle conversion
        await invoke('export_report_docx', {
          htmlContent: this.generateHTML(template, brandKit),
          outputPath: filePath
        });
        break;
      case 'pdf':
        await invoke('generate_report_pdf', {
          templateJson: JSON.stringify(template),
          outputPath: filePath
        });
        break;
    }

    return filePath;
  }

  // Batch export to multiple formats
  async batchExport(template: ReportTemplate, formats: ExportFormat[], brandKit?: BrandKit | null): Promise<Record<ExportFormat, string | null>> {
    const results: Record<ExportFormat, string | null> = {
      pdf: null, html: null, docx: null, markdown: null
    };

    for (const format of formats) {
      try {
        results[format] = await this.exportToFile(template, format, brandKit);
      } catch (error) {
        console.error(`Failed to export ${format}:`, error);
        results[format] = null;
      }
    }

    return results;
  }

  // Get print-ready HTML for preview
  getPrintPreviewHTML(template: ReportTemplate, brandKit?: BrandKit | null): string {
    return this.generateHTML(template, brandKit);
  }
}

export const reportExportService = new ReportExportService();
