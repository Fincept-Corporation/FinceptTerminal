import { useState, useCallback, useEffect } from 'react';
import { useEditor } from '@tiptap/react';
import StarterKit from '@tiptap/starter-kit';
import Underline from '@tiptap/extension-underline';
import TextAlign from '@tiptap/extension-text-align';
import { TextStyle } from '@tiptap/extension-text-style';
import Color from '@tiptap/extension-color';
import Highlight from '@tiptap/extension-highlight';
import FontFamily from '@tiptap/extension-font-family';
import Image from '@tiptap/extension-image';
import { Table } from '@tiptap/extension-table';
import TableRow from '@tiptap/extension-table-row';
import TableHeader from '@tiptap/extension-table-header';
import TableCell from '@tiptap/extension-table-cell';
import Link from '@tiptap/extension-link';
import Placeholder from '@tiptap/extension-placeholder';
import Typography from '@tiptap/extension-typography';
import SuperscriptExt from '@tiptap/extension-superscript';
import SubscriptExt from '@tiptap/extension-subscript';
import { open, save } from '@tauri-apps/plugin-dialog';
import { toast } from '@/components/ui/terminal-toast';
import { docxService, DocxSnapshot } from '@/services/core/docxService';
import { DocxEditorState, DocxViewMode, DocxDocumentMetadata, DocxPageSettings } from './types';
import { showPrompt, showConfirm } from '@/utils/notifications';

const DEFAULT_STATE: DocxEditorState = {
  viewMode: 'edit',
  filePath: null,
  fileName: 'Untitled.docx',
  originalArrayBuffer: null,
  htmlContent: '',
  isDirty: false,
  fileId: null,
  metadata: {
    title: '',
    author: '',
    company: '',
    date: new Date().toISOString().split('T')[0],
  },
};

const DEFAULT_PAGE_SETTINGS: DocxPageSettings = {
  watermarkEnabled: false,
  watermarkText: 'DRAFT',
  headerEnabled: false,
  footerEnabled: false,
  headerText: '',
  footerText: '',
  lineSpacing: '1.6',
  margins: { top: 20, right: 20, bottom: 20, left: 20 },
};

export const useDocxEditor = () => {
  const [docxState, setDocxState] = useState<DocxEditorState>(DEFAULT_STATE);
  const [snapshots, setSnapshots] = useState<DocxSnapshot[]>([]);
  const [pageSettings, setPageSettings] = useState<DocxPageSettings>(DEFAULT_PAGE_SETTINGS);
  const [zoom, setZoom] = useState<number>(100);

  // TipTap editor instance
  const editor = useEditor({
    extensions: [
      StarterKit.configure({
        heading: { levels: [1, 2, 3, 4, 5, 6] },
      }),
      Underline,
      TextAlign.configure({
        types: ['heading', 'paragraph'],
      }),
      TextStyle,
      Color,
      Highlight.configure({ multicolor: true }),
      FontFamily,
      Image.configure({
        inline: true,
        allowBase64: true,
      }),
      Table.configure({
        resizable: true,
      }),
      TableRow,
      TableHeader,
      TableCell,
      Link.configure({
        openOnClick: false,
        HTMLAttributes: {
          rel: 'noopener noreferrer',
        },
      }),
      Placeholder.configure({
        placeholder: 'Start typing or import a DOCX file...',
      }),
      Typography,
      SuperscriptExt,
      SubscriptExt,
    ],
    content: '',
    onUpdate: ({ editor: ed }) => {
      setDocxState(prev => ({
        ...prev,
        htmlContent: ed.getHTML(),
        isDirty: true,
      }));
    },
  });

  // Import DOCX file via Tauri dialog
  const importDocx = useCallback(async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [{
          name: 'Word Documents',
          extensions: ['docx'],
        }],
      });

      if (!selected) return;

      const filePath = typeof selected === 'string' ? selected : selected;
      toast.info('Importing DOCX file...');

      const result = await docxService.importDocx(filePath);

      // Set editor content
      if (editor) {
        editor.commands.setContent(result.html);
      }

      const fileName = filePath.split(/[\\/]/).pop() || 'Untitled.docx';

      // Save to file history
      const fileId = await docxService.addOrUpdateFile({
        file_name: fileName,
        file_path: filePath,
        page_count: 0,
        last_opened: new Date().toISOString(),
        last_modified: new Date().toISOString(),
        created_at: new Date().toISOString(),
      });

      setDocxState({
        viewMode: 'edit',
        filePath,
        fileName,
        originalArrayBuffer: result.arrayBuffer,
        htmlContent: result.html,
        isDirty: false,
        fileId,
        metadata: {
          title: fileName.replace(/\.docx$/i, ''),
          author: '',
          company: '',
          date: new Date().toISOString().split('T')[0],
        },
      });

      // Use real header/footer extracted from the DOCX file
      const hf = result.headerFooter;
      setPageSettings(prev => ({
        ...prev,
        headerEnabled: hf.hasHeader,
        footerEnabled: hf.hasFooter,
        headerText: hf.headerText || '',
        footerText: hf.footerText || '',
      }));

      // Load snapshots for this file
      const snaps = await docxService.getSnapshots(fileId);
      setSnapshots(snaps);

      if (result.messages.length > 0) {
        // Log each warning so user can see exactly what's happening
        console.warn(`[DocxEditor] Import warnings for "${fileName}" (${result.messages.length}):`);
        result.messages.forEach((msg, i) => {
          console.warn(`  [${i + 1}] [${msg.type}] ${msg.message}`);
        });

        // Group warnings by type for a useful toast
        const warningsByType: Record<string, number> = {};
        result.messages.forEach((msg) => {
          warningsByType[msg.type] = (warningsByType[msg.type] || 0) + 1;
        });
        const summary = Object.entries(warningsByType)
          .map(([type, count]) => `${count} ${type}`)
          .join(', ');

        // Show first 3 unique warning messages in toast for visibility
        const uniqueMessages = [...new Set(result.messages.map(m => m.message))];
        const preview = uniqueMessages.slice(0, 3).join(' | ');
        const moreCount = uniqueMessages.length > 3 ? ` (+${uniqueMessages.length - 3} more)` : '';

        toast.warning(`Import: ${summary}${moreCount}\n${preview}`);
      } else {
        toast.success(`Imported: ${fileName}`);
      }
    } catch (error) {
      console.error('[DocxEditor] Import failed:', error);
      toast.error('Failed to import DOCX file');
    }
  }, [editor]);

  // Export to DOCX
  const exportDocx = useCallback(async () => {
    try {
      const html = editor?.getHTML() || docxState.htmlContent;
      if (!html.trim()) {
        toast.error('Nothing to export');
        return;
      }

      const filePath = await save({
        filters: [{
          name: 'Word Documents',
          extensions: ['docx'],
        }],
        defaultPath: docxState.fileName,
      });

      if (!filePath) return;

      toast.info('Exporting DOCX...');

      const docxBuffer = await docxService.exportToDocx(html, {
        title: docxState.metadata.title,
        author: docxState.metadata.author,
        company: docxState.metadata.company,
        date: docxState.metadata.date,
      });

      await docxService.writeDocxFile(filePath, docxBuffer);

      setDocxState(prev => ({
        ...prev,
        isDirty: false,
        fileName: filePath.split(/[\\/]/).pop() || prev.fileName,
        filePath,
      }));

      toast.success('DOCX exported successfully');
    } catch (error) {
      console.error('[DocxEditor] Export failed:', error);
      toast.error('Failed to export DOCX');
    }
  }, [editor, docxState]);

  // Save (quick save to existing path or export)
  const saveDocx = useCallback(async () => {
    if (docxState.filePath) {
      try {
        const html = editor?.getHTML() || docxState.htmlContent;
        const docxBuffer = await docxService.exportToDocx(html, {
          title: docxState.metadata.title,
          author: docxState.metadata.author,
          company: docxState.metadata.company,
          date: docxState.metadata.date,
        });
        await docxService.writeDocxFile(docxState.filePath, docxBuffer);
        setDocxState(prev => ({ ...prev, isDirty: false }));
        toast.success('Saved');
      } catch (error) {
        console.error('[DocxEditor] Save failed:', error);
        toast.error('Failed to save');
      }
    } else {
      await exportDocx();
    }
  }, [editor, docxState, exportDocx]);

  // View mode
  const setViewMode = useCallback((mode: DocxViewMode) => {
    setDocxState(prev => ({ ...prev, viewMode: mode }));
  }, []);

  // Metadata
  const updateMetadata = useCallback((updates: Partial<DocxDocumentMetadata>) => {
    setDocxState(prev => ({
      ...prev,
      metadata: { ...prev.metadata, ...updates },
      isDirty: true,
    }));
  }, []);

  // Image insert
  const insertImage = useCallback(async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [{
          name: 'Images',
          extensions: ['png', 'jpg', 'jpeg', 'gif', 'bmp', 'webp', 'svg'],
        }],
      });

      if (selected && editor) {
        // For Tauri, we need to use file:// protocol
        editor.chain().focus().setImage({ src: `file://${selected}` }).run();
      }
    } catch (error) {
      console.error('[DocxEditor] Image insert failed:', error);
      toast.error('Failed to insert image');
    }
  }, [editor]);

  // Snapshots
  const createSnapshot = useCallback(async () => {
    if (!docxState.fileId) {
      toast.warning('Save the file first to create snapshots');
      return;
    }

    const name = await showPrompt('Enter snapshot name:', {
      title: 'Create Snapshot',
      defaultValue: `Snapshot ${new Date().toLocaleString()}`,
    });
    if (!name) return;

    try {
      const html = editor?.getHTML() || docxState.htmlContent;
      await docxService.createSnapshot(docxState.fileId, name, html);
      const snaps = await docxService.getSnapshots(docxState.fileId);
      setSnapshots(snaps);
      toast.success('Snapshot created');
    } catch (error) {
      console.error('[DocxEditor] Snapshot creation failed:', error);
      toast.error('Failed to create snapshot');
    }
  }, [editor, docxState]);

  const restoreSnapshot = useCallback(async (snapshotId: string) => {
    const snapshot = snapshots.find(s => s.id === snapshotId);
    if (!snapshot) return;

    const confirmed = await showConfirm(
      'Current changes will be lost.',
      { title: `Restore: ${snapshot.snapshot_name}?`, type: 'warning' }
    );
    if (!confirmed) return;

    if (editor) {
      editor.commands.setContent(snapshot.html_content);
    }
    setDocxState(prev => ({
      ...prev,
      htmlContent: snapshot.html_content,
      isDirty: true,
    }));
    toast.success('Snapshot restored');
  }, [editor, snapshots]);

  const deleteSnapshot = useCallback(async (snapshotId: string) => {
    if (!docxState.fileId) return;

    const confirmed = await showConfirm(
      'This action cannot be undone.',
      { title: 'Delete this snapshot?', type: 'danger' }
    );
    if (!confirmed) return;

    try {
      await docxService.deleteSnapshot(docxState.fileId, snapshotId);
      const snaps = await docxService.getSnapshots(docxState.fileId);
      setSnapshots(snaps);
      toast.success('Snapshot deleted');
    } catch (error) {
      toast.error('Failed to delete snapshot');
    }
  }, [docxState.fileId]);

  // New blank document
  const newDocument = useCallback(() => {
    if (editor) {
      editor.commands.setContent('');
    }
    setDocxState(DEFAULT_STATE);
    setPageSettings(DEFAULT_PAGE_SETTINGS);
    setSnapshots([]);
  }, [editor]);

  // Page settings toggles
  const toggleWatermark = useCallback(() => {
    setPageSettings(prev => ({ ...prev, watermarkEnabled: !prev.watermarkEnabled }));
  }, []);

  const toggleHeader = useCallback(() => {
    setPageSettings(prev => ({ ...prev, headerEnabled: !prev.headerEnabled }));
  }, []);

  const toggleFooter = useCallback(() => {
    setPageSettings(prev => ({ ...prev, footerEnabled: !prev.footerEnabled }));
  }, []);

  const updatePageSettings = useCallback((updates: Partial<DocxPageSettings>) => {
    setPageSettings(prev => ({ ...prev, ...updates }));
  }, []);

  return {
    editor,
    docxState,
    snapshots,
    pageSettings,
    zoom,
    setZoom,
    importDocx,
    exportDocx,
    saveDocx,
    newDocument,
    setViewMode,
    updateMetadata,
    insertImage,
    createSnapshot,
    restoreSnapshot,
    deleteSnapshot,
    toggleWatermark,
    toggleHeader,
    toggleFooter,
    updatePageSettings,
  };
};
