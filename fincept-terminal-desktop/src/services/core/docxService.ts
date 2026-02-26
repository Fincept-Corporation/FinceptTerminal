// DOCX Service - Import, export, and history management for DOCX files
// Uses mammoth for DOCX→HTML, html-to-docx for HTML→DOCX, and sqliteService for persistence

import mammoth from 'mammoth';
import JSZip from 'jszip';
import { readFile as tauriReadFile, writeFile as tauriWriteFile } from '@tauri-apps/plugin-fs';
import { saveSetting, getSetting } from './sqliteService';

export interface DocxFileRecord {
  id: string;
  file_name: string;
  file_path: string;
  page_count: number;
  last_opened: string;
  last_modified: string;
  created_at: string;
}

export interface DocxSnapshot {
  id: string;
  file_id: string;
  snapshot_name: string;
  html_content: string;
  created_at: string;
}

export interface DocxMetadata {
  title?: string;
  author?: string;
  company?: string;
  date?: string;
}

export interface DocxHeaderFooter {
  headerText: string;
  footerText: string;
  hasHeader: boolean;
  hasFooter: boolean;
}

export interface DocxImportResult {
  html: string;
  messages: Array<{ type: string; message: string }>;
  arrayBuffer: ArrayBuffer;
  headerFooter: DocxHeaderFooter;
}

class DocxService {
  private readonly RECENT_FILES_KEY = 'docx_recent_files';
  private readonly SNAPSHOTS_KEY_PREFIX = 'docx_snapshots_';

  /**
   * Extract plain text from OOXML body content (word/header*.xml, word/footer*.xml).
   * Parses <w:t> text runs from the XML.
   */
  private extractTextFromOoxml(xml: string): string {
    // Extract all <w:t ...>text</w:t> runs
    const textRuns: string[] = [];
    const regex = /<w:t[^>]*>([\s\S]*?)<\/w:t>/g;
    let match: RegExpExecArray | null;
    while ((match = regex.exec(xml)) !== null) {
      textRuns.push(match[1]);
    }
    return textRuns.join('').trim();
  }

  /**
   * Extract header and footer text from DOCX ZIP archive.
   * DOCX files are ZIP archives; headers are in word/header*.xml, footers in word/footer*.xml.
   */
  async extractHeaderFooter(arrayBuffer: ArrayBuffer): Promise<DocxHeaderFooter> {
    const result: DocxHeaderFooter = {
      headerText: '',
      footerText: '',
      hasHeader: false,
      hasFooter: false,
    };

    try {
      const zip = await JSZip.loadAsync(arrayBuffer);

      // Collect all header XML files (header1.xml, header2.xml, header3.xml)
      const headerFiles = Object.keys(zip.files).filter(
        name => /^word\/header\d*\.xml$/i.test(name)
      );
      // Collect all footer XML files
      const footerFiles = Object.keys(zip.files).filter(
        name => /^word\/footer\d*\.xml$/i.test(name)
      );

      // Extract text from headers — use the first one with content (default header)
      for (const hf of headerFiles) {
        const xml = await zip.files[hf].async('text');
        const text = this.extractTextFromOoxml(xml);
        if (text) {
          result.headerText = text;
          result.hasHeader = true;
          break; // Use first non-empty header
        }
      }

      // Extract text from footers
      for (const ff of footerFiles) {
        const xml = await zip.files[ff].async('text');
        const text = this.extractTextFromOoxml(xml);
        if (text) {
          result.footerText = text;
          result.hasFooter = true;
          break; // Use first non-empty footer
        }
      }

      console.log('[DocxService] Extracted header/footer:', {
        hasHeader: result.hasHeader,
        headerText: result.headerText.substring(0, 100),
        hasFooter: result.hasFooter,
        footerText: result.footerText.substring(0, 100),
        headerFiles: headerFiles.length,
        footerFiles: footerFiles.length,
      });
    } catch (err) {
      console.warn('[DocxService] Failed to extract header/footer from DOCX ZIP:', err);
    }

    return result;
  }

  /**
   * Extract paragraph alignment info from DOCX document.xml.
   * Returns an ordered list of alignment values ('center', 'right', 'both')
   * for each <w:p> in the document body, in order.
   */
  private async extractAlignments(arrayBuffer: ArrayBuffer): Promise<string[]> {
    const alignments: string[] = [];
    try {
      const zip = await JSZip.loadAsync(arrayBuffer);
      const docXml = zip.files['word/document.xml'];
      if (!docXml) return alignments;

      const xml = await docXml.async('text');
      // Match each <w:p ...> ... </w:p> block
      const paraRegex = /<w:p[ >][\s\S]*?<\/w:p>/g;
      let paraMatch: RegExpExecArray | null;
      while ((paraMatch = paraRegex.exec(xml)) !== null) {
        const para = paraMatch[0];
        // Look for <w:jc w:val="center|right|both|left"/>
        const jcMatch = para.match(/<w:jc\s+w:val="([^"]+)"/);
        if (jcMatch) {
          const val = jcMatch[1];
          // Map Word alignment to CSS
          if (val === 'center') alignments.push('center');
          else if (val === 'right') alignments.push('right');
          else if (val === 'both') alignments.push('justify');
          else alignments.push(''); // left or unknown
        } else {
          alignments.push(''); // no explicit alignment (left default)
        }
      }
    } catch (err) {
      console.warn('[DocxService] Failed to extract alignments:', err);
    }
    return alignments;
  }

  /**
   * Post-process mammoth HTML to inject text-align styles based on DOCX alignment data.
   * Mammoth outputs block elements (p, h1-h6, li, blockquote>p) in document order.
   * We match them 1:1 with the alignment array extracted from the DOCX XML.
   */
  private applyAlignments(html: string, alignments: string[]): string {
    if (alignments.length === 0) return html;

    let alignIdx = 0;
    // Match opening tags of block elements that mammoth generates
    const blockTagRegex = /(<(?:p|h[1-6]|li)(?:\s[^>]*)?>)/gi;

    return html.replace(blockTagRegex, (match) => {
      if (alignIdx >= alignments.length) return match;
      const align = alignments[alignIdx];
      alignIdx++;

      if (!align) return match;

      // If tag already has a style attribute, append to it
      if (match.includes('style="')) {
        return match.replace('style="', `style="text-align: ${align}; `);
      }
      // Otherwise inject style before the closing >
      return match.replace(/>$/, ` style="text-align: ${align};">`);
    });
  }

  /**
   * Import a DOCX file: read bytes, convert to HTML via mammoth, extract header/footer
   */
  async importDocx(filePath: string): Promise<DocxImportResult> {
    try {
      const fileData = await tauriReadFile(filePath);
      const arrayBuffer = fileData.buffer.slice(
        fileData.byteOffset,
        fileData.byteOffset + fileData.byteLength
      );

      // Extract header/footer, alignments, and convert HTML in parallel
      const [headerFooter, alignments, result] = await Promise.all([
        this.extractHeaderFooter(arrayBuffer),
        this.extractAlignments(arrayBuffer),
        mammoth.convertToHtml(
        { arrayBuffer },
        {
          convertImage: mammoth.images.imgElement(function (image: any) {
            return image.read('base64').then(function (imageBuffer: string) {
              return {
                src: 'data:' + image.contentType + ';base64,' + imageBuffer,
              };
            });
          }),
          styleMap: [
            // ── Headings ──────────────────────────────────
            "p[style-name='Heading 1'] => h1:fresh",
            "p[style-name='Heading 2'] => h2:fresh",
            "p[style-name='Heading 3'] => h3:fresh",
            "p[style-name='Heading 4'] => h4:fresh",
            "p[style-name='Heading 5'] => h5:fresh",
            "p[style-name='Heading 6'] => h6:fresh",
            "p[style-name='heading 1'] => h1:fresh",
            "p[style-name='heading 2'] => h2:fresh",
            "p[style-name='heading 3'] => h3:fresh",
            "p[style-name='heading 4'] => h4:fresh",
            "p[style-name='heading 5'] => h5:fresh",
            "p[style-name='heading 6'] => h6:fresh",

            // ── Title / Subtitle ──────────────────────────
            "p[style-name='Title'] => h1.doc-title:fresh",
            "p[style-name='Subtitle'] => h2.doc-subtitle:fresh",

            // ── Lists ─────────────────────────────────────
            "p[style-name='List Paragraph'] => p:fresh",
            "p[style-name='List Bullet'] => ul > li:fresh",
            "p[style-name='List Bullet 2'] => ul > li:fresh",
            "p[style-name='List Bullet 3'] => ul > li:fresh",
            "p[style-name='List Number'] => ol > li:fresh",
            "p[style-name='List Number 2'] => ol > li:fresh",
            "p[style-name='List Number 3'] => ol > li:fresh",
            "p[style-name='List Continue'] => p:fresh",
            "p[style-name='List Continue 2'] => p:fresh",

            // ── Character styles ──────────────────────────
            "r[style-name='Strong'] => strong",
            "r[style-name='Emphasis'] => em",
            "r[style-name='Intense Emphasis'] => em > strong",
            "r[style-name='Book Title'] => em.book-title",
            "r[style-name='Subtle Emphasis'] => em",
            "r[style-name='Intense Reference'] => strong",
            "r[style-name='Subtle Reference'] => span",
            "r[style-name='Hyperlink'] => a",

            // ── Paragraph styles ──────────────────────────
            "p[style-name='Quote'] => blockquote > p:fresh",
            "p[style-name='Intense Quote'] => blockquote > p:fresh",
            "p[style-name='Block Text'] => blockquote > p:fresh",
            "p[style-name='No Spacing'] => p:fresh",
            "p[style-name='Normal'] => p:fresh",
            "p[style-name='Normal (Web)'] => p:fresh",
            "p[style-name='Body Text'] => p:fresh",
            "p[style-name='Body Text 2'] => p:fresh",
            "p[style-name='Body Text 3'] => p:fresh",
            "p[style-name='Body Text Indent'] => p:fresh",
            "p[style-name='Body Text First Indent'] => p:fresh",
            "p[style-name='Plain Text'] => p:fresh",

            // ── Table of Contents ─────────────────────────
            "p[style-name='TOC Heading'] => h2.toc-heading:fresh",
            "p[style-name='toc 1'] => p.toc-1:fresh",
            "p[style-name='toc 2'] => p.toc-2:fresh",
            "p[style-name='toc 3'] => p.toc-3:fresh",

            // ── Header / Footer ───────────────────────────
            "p[style-name='Header'] => p.doc-header:fresh",
            "p[style-name='Footer'] => p.doc-footer:fresh",
            "p[style-name='header'] => p.doc-header:fresh",
            "p[style-name='footer'] => p.doc-footer:fresh",

            // ── Captions / Annotations ────────────────────
            "p[style-name='Caption'] => p.caption:fresh",
            "p[style-name='Footnote Text'] => p.footnote:fresh",
            "p[style-name='Endnote Text'] => p.endnote:fresh",
            "p[style-name='annotation text'] => p.comment:fresh",
            "p[style-name='Balloon Text'] => p:fresh",
            "r[style-name='annotation reference'] => span",
            "r[style-name='Footnote Reference'] => sup",
            "r[style-name='Endnote Reference'] => sup",

            // ── Table styles ──────────────────────────────
            "p[style-name='Table Paragraph'] => p:fresh",

            // ── Catch-all: map any unknown paragraph style to p:fresh
            // (reduces noise from custom/legacy styles)
          ],
        }
      ),
      ]);

      // Post-process HTML to inject text alignment from DOCX
      const alignedHtml = this.applyAlignments(result.value, alignments);

      return {
        html: alignedHtml,
        messages: result.messages,
        arrayBuffer,
        headerFooter,
      };
    } catch (error) {
      console.error('[DocxService] Failed to import DOCX:', error);
      throw error;
    }
  }

  /**
   * Read raw DOCX file bytes for docx-preview rendering
   */
  async getDocxArrayBuffer(filePath: string): Promise<ArrayBuffer> {
    try {
      const fileData = await tauriReadFile(filePath);
      return fileData.buffer.slice(
        fileData.byteOffset,
        fileData.byteOffset + fileData.byteLength
      );
    } catch (error) {
      console.error('[DocxService] Failed to read DOCX file:', error);
      throw error;
    }
  }

  /**
   * Export HTML content to DOCX binary using html-to-docx
   */
  async exportToDocx(htmlContent: string, metadata?: DocxMetadata): Promise<Uint8Array> {
    try {
      const { default: HTMLtoDOCX } = await import('html-to-docx');

      const fullHtml = `
        <!DOCTYPE html>
        <html>
        <head>
          <meta charset="UTF-8">
          <style>
            body { font-family: Arial, sans-serif; font-size: 11pt; line-height: 1.6; color: #000; }
            h1 { font-size: 24pt; font-weight: bold; margin: 16pt 0 8pt; }
            h2 { font-size: 18pt; font-weight: bold; margin: 14pt 0 6pt; }
            h3 { font-size: 14pt; font-weight: bold; margin: 12pt 0 6pt; }
            h4 { font-size: 12pt; font-weight: bold; margin: 10pt 0 4pt; }
            p { margin: 0 0 6pt; }
            table { border-collapse: collapse; width: 100%; margin: 8pt 0; }
            td, th { border: 1px solid #999; padding: 4pt 8pt; }
            th { background: #f0f0f0; font-weight: bold; }
            img { max-width: 100%; }
            ul, ol { margin: 6pt 0; padding-left: 24pt; }
            li { margin: 2pt 0; }
            blockquote { border-left: 3pt solid #ccc; padding-left: 12pt; margin: 8pt 0; color: #555; }
            pre { background: #f5f5f5; padding: 8pt; font-family: Courier New, monospace; font-size: 10pt; }
            hr { border: none; border-top: 1pt solid #ccc; margin: 12pt 0; }
          </style>
        </head>
        <body>
          ${htmlContent}
        </body>
        </html>
      `;

      const docxBlob = await HTMLtoDOCX(fullHtml, null, {
        table: { row: { cantSplit: true } },
        footer: true,
        pageNumber: true,
        title: metadata?.title || 'Document',
        creator: metadata?.author || '',
        description: metadata?.company || '',
      });

      // html-to-docx returns a Blob in browser
      if (docxBlob instanceof Blob) {
        const arrayBuffer = await docxBlob.arrayBuffer();
        return new Uint8Array(arrayBuffer);
      }
      // In case it returns a Buffer (Node.js environment)
      return new Uint8Array(docxBlob as ArrayBuffer);
    } catch (error) {
      console.error('[DocxService] Failed to export to DOCX:', error);
      throw error;
    }
  }

  /**
   * Write DOCX binary to a file path via Tauri FS
   */
  async writeDocxFile(filePath: string, data: Uint8Array): Promise<void> {
    try {
      await tauriWriteFile(filePath, data);
    } catch (error) {
      console.error('[DocxService] Failed to write DOCX file:', error);
      throw error;
    }
  }

  // ---- File History (persisted via sqliteService settings) ----

  async addOrUpdateFile(file: Omit<DocxFileRecord, 'id'>): Promise<string> {
    try {
      const files = await this.getRecentFiles();
      const existing = files.find(f => f.file_path === file.file_path);
      const id = existing?.id || `docx-${Date.now()}`;

      const record: DocxFileRecord = {
        id,
        ...file,
      };

      // Update or add
      const updated = existing
        ? files.map(f => (f.id === id ? record : f))
        : [record, ...files];

      // Keep only last 50 files
      const trimmed = updated.slice(0, 50);
      await saveSetting(this.RECENT_FILES_KEY, JSON.stringify(trimmed), 'docx');
      return id;
    } catch (error) {
      console.error('[DocxService] Failed to add/update file record:', error);
      throw error;
    }
  }

  async getRecentFiles(limit: number = 20): Promise<DocxFileRecord[]> {
    try {
      const data = await getSetting(this.RECENT_FILES_KEY);
      if (!data) return [];
      const files: DocxFileRecord[] = JSON.parse(data);
      return files.slice(0, limit);
    } catch (error) {
      console.error('[DocxService] Failed to get recent files:', error);
      return [];
    }
  }

  // ---- Snapshots (persisted via sqliteService settings) ----

  async createSnapshot(fileId: string, snapshotName: string, htmlContent: string): Promise<string> {
    try {
      const snapshots = await this.getSnapshots(fileId);
      const snapshot: DocxSnapshot = {
        id: `snap-${Date.now()}`,
        file_id: fileId,
        snapshot_name: snapshotName,
        html_content: htmlContent,
        created_at: new Date().toISOString(),
      };
      const updated = [snapshot, ...snapshots].slice(0, 20); // Keep max 20 snapshots
      await saveSetting(`${this.SNAPSHOTS_KEY_PREFIX}${fileId}`, JSON.stringify(updated), 'docx');
      return snapshot.id;
    } catch (error) {
      console.error('[DocxService] Failed to create snapshot:', error);
      throw error;
    }
  }

  async getSnapshots(fileId: string): Promise<DocxSnapshot[]> {
    try {
      const data = await getSetting(`${this.SNAPSHOTS_KEY_PREFIX}${fileId}`);
      if (!data) return [];
      return JSON.parse(data);
    } catch (error) {
      console.error('[DocxService] Failed to get snapshots:', error);
      return [];
    }
  }

  async deleteSnapshot(fileId: string, snapshotId: string): Promise<void> {
    try {
      const snapshots = await this.getSnapshots(fileId);
      const filtered = snapshots.filter(s => s.id !== snapshotId);
      await saveSetting(`${this.SNAPSHOTS_KEY_PREFIX}${fileId}`, JSON.stringify(filtered), 'docx');
    } catch (error) {
      console.error('[DocxService] Failed to delete snapshot:', error);
      throw error;
    }
  }
}

export const docxService = new DocxService();
export default docxService;
