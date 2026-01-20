import React, { useState, useEffect, useRef } from 'react';
import Handsontable from 'handsontable';
import 'handsontable/dist/handsontable.full.min.css';
import * as XLSX from 'xlsx';
import { HyperFormula } from 'hyperformula';
import { Upload, Download, Plus, Trash2, Save, FileSpreadsheet, History, Camera, RotateCcw, Clock, FolderOpen, X } from 'lucide-react';
import { open, save } from '@tauri-apps/plugin-dialog';
import { readFile as tauriReadFile, writeFile as tauriWriteFile } from '@tauri-apps/plugin-fs';
import { excelService, ExcelFile, ExcelSnapshot } from '@/services/core/excelService';
import { TabFooter } from '@/components/common/TabFooter';
import { useTranslation } from 'react-i18next';

interface SheetData {
  name: string;
  data: any[][];
}

const ExcelTab: React.FC = () => {
  const { t } = useTranslation('excel');
  const containerRef = useRef<HTMLDivElement>(null);
  const hotInstanceRef = useRef<Handsontable | null>(null);
  const [sheets, setSheets] = useState<SheetData[]>([
    { name: 'Sheet1', data: Array(100).fill(null).map(() => Array(26).fill('')) }
  ]);
  const [activeSheetIndex, setActiveSheetIndex] = useState(0);
  const [fileName, setFileName] = useState<string>('Untitled.xlsx');
  const [currentFileId, setCurrentFileId] = useState<number | null>(null);
  const [filePath, setFilePath] = useState<string>('');
  const [showHistory, setShowHistory] = useState(false);
  const [showSnapshots, setShowSnapshots] = useState(false);
  const [recentFiles, setRecentFiles] = useState<ExcelFile[]>([]);
  const [snapshots, setSnapshots] = useState<ExcelSnapshot[]>([]);

  // Initialize Handsontable
  useEffect(() => {
    if (containerRef.current && !hotInstanceRef.current) {
      const hyperformulaInstance = HyperFormula.buildEmpty({
        licenseKey: 'internal-use-in-handsontable',
      });

      hotInstanceRef.current = new Handsontable(containerRef.current, {
        data: sheets[activeSheetIndex].data,
        colHeaders: true,
        rowHeaders: true,
        width: '100%',
        height: '100%',
        licenseKey: 'non-commercial-and-evaluation',
        formulas: {
          engine: hyperformulaInstance,
        },
        contextMenu: true,
        manualColumnResize: true,
        manualRowResize: true,
        filters: true,
        dropdownMenu: true,
        autoWrapRow: true,
        autoWrapCol: true,
        stretchH: 'all',
        className: 'htDark',
        cell: [],
        cells: function () {
          return {
            className: 'excel-cell'
          };
        }
      });
    }

    return () => {
      if (hotInstanceRef.current) {
        hotInstanceRef.current.destroy();
        hotInstanceRef.current = null;
      }
    };
  }, []);

  // Update table when active sheet changes
  useEffect(() => {
    if (hotInstanceRef.current) {
      hotInstanceRef.current.loadData(sheets[activeSheetIndex].data);
    }
  }, [activeSheetIndex, sheets]);

  // Save current sheet data before switching
  const saveCurrentSheetData = () => {
    if (hotInstanceRef.current) {
      const data = hotInstanceRef.current.getData();
      setSheets(prev => {
        const updated = [...prev];
        updated[activeSheetIndex] = { ...updated[activeSheetIndex], data };
        return updated;
      });
    }
  };

  // Import Excel file
  const handleImport = async () => {
    try {
      const selected = await open({
        multiple: false,
        filters: [{
          name: 'Spreadsheet Files',
          extensions: ['xlsx', 'xls', 'xlsm', 'xlsb', 'csv', 'tsv', 'ods']
        }]
      });

      if (selected) {
        const filePath = typeof selected === 'string' ? selected : selected;
        const fileData = await tauriReadFile(filePath);
        const workbook = XLSX.read(fileData, { type: 'array' });

        const newSheets: SheetData[] = workbook.SheetNames.map(sheetName => {
          const worksheet = workbook.Sheets[sheetName];
          const data = XLSX.utils.sheet_to_json(worksheet, { header: 1, defval: '' }) as any[][];

          // Ensure minimum size
          const minRows = 100;
          const minCols = 26;
          while (data.length < minRows) {
            data.push(Array(minCols).fill(''));
          }
          data.forEach(row => {
            while (row.length < minCols) {
              row.push('');
            }
          });

          return { name: sheetName, data };
        });

        setSheets(newSheets);
        setActiveSheetIndex(0);
        const fName = filePath.split(/[\\/]/).pop() || 'Untitled.xlsx';
        setFileName(fName);
        setFilePath(filePath);

        // Save to history
        const fileId = await excelService.addOrUpdateFile({
          file_name: fName,
          file_path: filePath,
          sheet_count: newSheets.length,
          last_opened: new Date().toISOString(),
          last_modified: new Date().toISOString(),
          created_at: new Date().toISOString()
        });
        setCurrentFileId(fileId);
      }
    } catch (error) {
      console.error('Error importing Excel file:', error);
      alert('Failed to import Excel file');
    }
  };

  // Export Excel file
  const handleExport = async () => {
    try {
      saveCurrentSheetData();

      const filePath = await save({
        filters: [{
          name: 'Excel Files',
          extensions: ['xlsx']
        }],
        defaultPath: fileName
      });

      if (filePath) {
        const workbook = XLSX.utils.book_new();

        sheets.forEach(sheet => {
          const worksheet = XLSX.utils.aoa_to_sheet(sheet.data);
          XLSX.utils.book_append_sheet(workbook, worksheet, sheet.name);
        });

        const excelBuffer = XLSX.write(workbook, { bookType: 'xlsx', type: 'array' });
        await tauriWriteFile(filePath, new Uint8Array(excelBuffer));

        setFileName(filePath.split(/[\\/]/).pop() || 'Untitled.xlsx');
        alert('Excel file exported successfully!');
      }
    } catch (error) {
      console.error('Error exporting Excel file:', error);
      alert('Failed to export Excel file');
    }
  };

  // Add new sheet
  const handleAddSheet = () => {
    const newSheetName = `Sheet${sheets.length + 1}`;
    setSheets(prev => [
      ...prev,
      { name: newSheetName, data: Array(100).fill(null).map(() => Array(26).fill('')) }
    ]);
    setActiveSheetIndex(sheets.length);
  };

  // Delete sheet
  const handleDeleteSheet = () => {
    if (sheets.length === 1) {
      alert('Cannot delete the last sheet');
      return;
    }

    if (confirm(`Delete "${sheets[activeSheetIndex].name}"?`)) {
      setSheets(prev => prev.filter((_, idx) => idx !== activeSheetIndex));
      setActiveSheetIndex(Math.max(0, activeSheetIndex - 1));
    }
  };

  // Rename sheet
  const handleRenameSheet = (index: number) => {
    const newName = prompt('Enter new sheet name:', sheets[index].name);
    if (newName && newName.trim()) {
      setSheets(prev => {
        const updated = [...prev];
        updated[index] = { ...updated[index], name: newName.trim() };
        return updated;
      });
    }
  };

  // Load recent files
  const loadRecentFiles = async () => {
    const files = await excelService.getRecentFiles(20);
    setRecentFiles(files);
  };

  // Load snapshots for current file
  const loadSnapshots = async () => {
    if (currentFileId) {
      const snaps = await excelService.getSnapshots(currentFileId);
      setSnapshots(snaps);
    }
  };

  // Create snapshot
  const handleCreateSnapshot = async () => {
    if (!currentFileId) {
      alert('Please save the file first before creating a snapshot');
      return;
    }

    const snapshotName = prompt('Enter snapshot name:', `Restore Point ${new Date().toLocaleString()}`);
    if (!snapshotName) return;

    try {
      saveCurrentSheetData();
      await excelService.createSnapshot(currentFileId, snapshotName, JSON.stringify({ sheets }));
      await loadSnapshots();
      alert('Snapshot created successfully!');
    } catch (error) {
      console.error('Failed to create snapshot:', error);
      alert('Failed to create snapshot');
    }
  };

  // Restore from snapshot
  const handleRestoreSnapshot = async (snapshot: ExcelSnapshot) => {
    if (!confirm(`Restore to: ${snapshot.snapshot_name}?\n\nCurrent changes will be lost.`)) return;

    try {
      const data = JSON.parse(snapshot.sheet_data);
      setSheets(data.sheets);
      setActiveSheetIndex(0);
      alert('Snapshot restored successfully!');
    } catch (error) {
      console.error('Failed to restore snapshot:', error);
      alert('Failed to restore snapshot');
    }
  };

  // Open file from history
  const handleOpenFromHistory = async (file: ExcelFile) => {
    try {
      const fileData = await tauriReadFile(file.file_path);
      const workbook = XLSX.read(fileData, { type: 'array' });

      const newSheets: SheetData[] = workbook.SheetNames.map(sheetName => {
        const worksheet = workbook.Sheets[sheetName];
        const data = XLSX.utils.sheet_to_json(worksheet, { header: 1, defval: '' }) as any[][];

        const minRows = 100;
        const minCols = 26;
        while (data.length < minRows) {
          data.push(Array(minCols).fill(''));
        }
        data.forEach(row => {
          while (row.length < minCols) {
            row.push('');
          }
        });

        return { name: sheetName, data };
      });

      setSheets(newSheets);
      setActiveSheetIndex(0);
      setFileName(file.file_name);
      setFilePath(file.file_path);
      setCurrentFileId(file.id!);
      setShowHistory(false);

      // Update last opened
      await excelService.addOrUpdateFile({
        file_name: file.file_name,
        file_path: file.file_path,
        sheet_count: newSheets.length,
        last_opened: new Date().toISOString(),
        last_modified: file.last_modified,
        created_at: file.created_at
      });
    } catch (error) {
      console.error('Error opening file from history:', error);
      alert('Failed to open file');
    }
  };

  // Update import to save to history
  useEffect(() => {
    if (showHistory) {
      loadRecentFiles();
    }
  }, [showHistory]);

  useEffect(() => {
    if (showSnapshots) {
      loadSnapshots();
    }
  }, [showSnapshots, currentFileId]);

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: '#000000',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: 'Consolas, "Courier New", monospace',
      overflow: 'hidden'
    }}>
      {/* Toolbar */}
      <div style={{
        backgroundColor: '#2d2d2d',
        borderBottom: '1px solid #404040',
        padding: '8px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        flexShrink: 0
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          color: '#ea580c',
          fontSize: '11px',
          fontWeight: 'bold',
          marginRight: '12px'
        }}>
          <FileSpreadsheet size={16} />
          <span>{t('title')}</span>
        </div>

        <button
          onClick={handleImport}
          style={{
            backgroundColor: '#404040',
            color: '#fff',
            border: 'none',
            padding: '6px 12px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
          onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#525252'}
          onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#404040'}
        >
          <Upload size={14} />
          {t('toolbar.import')}
        </button>

        <button
          onClick={handleExport}
          style={{
            backgroundColor: '#404040',
            color: '#fff',
            border: 'none',
            padding: '6px 12px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
          onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#525252'}
          onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#404040'}
        >
          <Download size={14} />
          {t('toolbar.export')}
        </button>

        <div style={{ width: '1px', height: '20px', backgroundColor: '#525252' }}></div>

        <button
          onClick={() => saveCurrentSheetData()}
          style={{
            backgroundColor: '#404040',
            color: '#fff',
            border: 'none',
            padding: '6px 12px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
          onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#525252'}
          onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#404040'}
        >
          <Save size={14} />
          {t('toolbar.save')}
        </button>

        <div style={{ width: '1px', height: '20px', backgroundColor: '#525252' }}></div>

        <button
          onClick={() => setShowHistory(!showHistory)}
          style={{
            backgroundColor: showHistory ? '#ea580c' : '#404040',
            color: '#fff',
            border: 'none',
            padding: '6px 12px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
          onMouseEnter={(e) => { if (!showHistory) e.currentTarget.style.backgroundColor = '#525252' }}
          onMouseLeave={(e) => { if (!showHistory) e.currentTarget.style.backgroundColor = '#404040' }}
        >
          <FolderOpen size={14} />
          {t('toolbar.history')}
        </button>

        <button
          onClick={handleCreateSnapshot}
          style={{
            backgroundColor: '#404040',
            color: '#fff',
            border: 'none',
            padding: '6px 12px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
          onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#525252'}
          onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#404040'}
          title="Create Restore Point"
        >
          <Camera size={14} />
          {t('toolbar.snapshot')}
        </button>

        <button
          onClick={() => setShowSnapshots(!showSnapshots)}
          style={{
            backgroundColor: showSnapshots ? '#ea580c' : '#404040',
            color: '#fff',
            border: 'none',
            padding: '6px 12px',
            fontSize: '10px',
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}
          onMouseEnter={(e) => { if (!showSnapshots) e.currentTarget.style.backgroundColor = '#525252' }}
          onMouseLeave={(e) => { if (!showSnapshots) e.currentTarget.style.backgroundColor = '#404040' }}
          title="View and restore from saved snapshots - rollback to any previous state"
        >
          <RotateCcw size={14} />
          {t('toolbar.revert')}
        </button>

        <div style={{ flex: 1 }}></div>

        <span style={{ color: '#a3a3a3', fontSize: '10px' }}>
          {fileName}
        </span>
      </div>

      {/* History Panel */}
      {showHistory && (
        <div style={{
          position: 'absolute',
          top: '60px',
          right: '12px',
          width: '400px',
          maxHeight: '500px',
          backgroundColor: '#1a1a1a',
          border: '1px solid #404040',
          borderRadius: '4px',
          zIndex: 1000,
          overflow: 'auto'
        }}>
          <div style={{
            padding: '12px',
            borderBottom: '1px solid #404040',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: '#ea580c', fontWeight: 'bold' }}>
              <History size={16} />
              <span>RECENT FILES</span>
            </div>
            <button onClick={() => setShowHistory(false)} style={{ background: 'none', border: 'none', color: '#a3a3a3', cursor: 'pointer' }}>
              <X size={16} />
            </button>
          </div>
          <div style={{ maxHeight: '400px', overflow: 'auto' }}>
            {recentFiles.length === 0 ? (
              <div style={{ padding: '20px', textAlign: 'center', color: '#666', fontSize: '11px' }}>
                No recent files
              </div>
            ) : (
              recentFiles.map(file => (
                <div
                  key={file.id}
                  onClick={() => handleOpenFromHistory(file)}
                  style={{
                    padding: '12px',
                    borderBottom: '1px solid #2d2d2d',
                    cursor: 'pointer',
                    fontSize: '11px'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#2d2d2d'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                >
                  <div style={{ color: '#fff', fontWeight: 600, marginBottom: '4px' }}>{file.file_name}</div>
                  <div style={{ color: '#666', fontSize: '10px', marginBottom: '4px' }}>{file.file_path}</div>
                  <div style={{ display: 'flex', gap: '12px', color: '#999', fontSize: '9px' }}>
                    <span>{file.sheet_count} sheet(s)</span>
                    <span>•</span>
                    <span>Last opened: {new Date(file.last_opened).toLocaleString()}</span>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>
      )}

      {/* Snapshots Panel */}
      {showSnapshots && (
        <div style={{
          position: 'absolute',
          top: '60px',
          right: '12px',
          width: '400px',
          maxHeight: '500px',
          backgroundColor: '#1a1a1a',
          border: '1px solid #404040',
          borderRadius: '4px',
          zIndex: 1000,
          overflow: 'auto'
        }}>
          <div style={{
            padding: '12px',
            borderBottom: '1px solid #404040',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', color: '#ea580c', fontWeight: 'bold' }}>
              <Clock size={16} />
              <span>RESTORE POINTS</span>
            </div>
            <button onClick={() => setShowSnapshots(false)} style={{ background: 'none', border: 'none', color: '#a3a3a3', cursor: 'pointer' }}>
              <X size={16} />
            </button>
          </div>
          <div style={{ maxHeight: '400px', overflow: 'auto' }}>
            {!currentFileId ? (
              <div style={{ padding: '20px', textAlign: 'center', color: '#666', fontSize: '11px' }}>
                Please save the file first
              </div>
            ) : snapshots.length === 0 ? (
              <div style={{ padding: '20px', textAlign: 'center', color: '#666', fontSize: '11px' }}>
                No snapshots created yet
              </div>
            ) : (
              snapshots.map(snapshot => (
                <div
                  key={snapshot.id}
                  style={{
                    padding: '12px',
                    borderBottom: '1px solid #2d2d2d',
                    fontSize: '11px'
                  }}
                >
                  <div style={{ color: '#fff', fontWeight: 600, marginBottom: '4px' }}>{snapshot.snapshot_name}</div>
                  <div style={{ color: '#999', fontSize: '10px', marginBottom: '8px' }}>
                    Created: {new Date(snapshot.created_at).toLocaleString()}
                  </div>
                  <div style={{ display: 'flex', gap: '8px' }}>
                    <button
                      onClick={() => handleRestoreSnapshot(snapshot)}
                      style={{
                        backgroundColor: '#10b981',
                        color: '#fff',
                        border: 'none',
                        padding: '4px 8px',
                        fontSize: '10px',
                        cursor: 'pointer',
                        borderRadius: '2px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <RotateCcw size={12} />
                      Restore
                    </button>
                    <button
                      onClick={async () => {
                        if (confirm('Delete this snapshot?')) {
                          await excelService.deleteSnapshot(snapshot.id!);
                          await loadSnapshots();
                        }
                      }}
                      style={{
                        backgroundColor: '#ef4444',
                        color: '#fff',
                        border: 'none',
                        padding: '4px 8px',
                        fontSize: '10px',
                        cursor: 'pointer',
                        borderRadius: '2px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <Trash2 size={12} />
                      Delete
                    </button>
                  </div>
                </div>
              ))
            )}
          </div>
        </div>
      )}

      {/* Spreadsheet Container */}
      <div style={{
        flex: 1,
        backgroundColor: '#1a1a1a',
        padding: '0',
        overflow: 'hidden',
        position: 'relative'
      }}>
        <div
          ref={containerRef}
          style={{
            width: '100%',
            height: 'calc(100% - 32px)',
            backgroundColor: '#1a1a1a'
          }}
        />

        {/* Sheet Tabs */}
        <div style={{
          position: 'absolute',
          bottom: 0,
          left: 0,
          right: 0,
          height: '32px',
          backgroundColor: '#2d2d2d',
          borderTop: '1px solid #404040',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          padding: '0 8px',
          overflowX: 'auto',
          overflowY: 'hidden'
        }}>
          <button
            onClick={handleAddSheet}
            style={{
              backgroundColor: 'transparent',
              color: '#a3a3a3',
              border: '1px solid #404040',
              padding: '4px 8px',
              fontSize: '10px',
              cursor: 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              gap: '2px',
              marginRight: '8px'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.backgroundColor = '#404040';
              e.currentTarget.style.color = '#fff';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.backgroundColor = 'transparent';
              e.currentTarget.style.color = '#a3a3a3';
            }}
            title="Add Sheet"
          >
            <Plus size={12} />
          </button>

          {sheets.map((sheet, index) => (
            <div
              key={index}
              onClick={() => {
                saveCurrentSheetData();
                setActiveSheetIndex(index);
              }}
              onDoubleClick={() => handleRenameSheet(index)}
              style={{
                backgroundColor: activeSheetIndex === index ? '#ea580c' : '#404040',
                color: activeSheetIndex === index ? '#fff' : '#a3a3a3',
                padding: '4px 12px',
                fontSize: '10px',
                cursor: 'pointer',
                borderRadius: '2px',
                whiteSpace: 'nowrap',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                border: activeSheetIndex === index ? '1px solid #ea580c' : '1px solid #525252'
              }}
              onMouseEnter={(e) => {
                if (activeSheetIndex !== index) {
                  e.currentTarget.style.backgroundColor = '#525252';
                }
              }}
              onMouseLeave={(e) => {
                if (activeSheetIndex !== index) {
                  e.currentTarget.style.backgroundColor = '#404040';
                }
              }}
            >
              <span>{sheet.name}</span>
              {activeSheetIndex === index && sheets.length > 1 && (
                <Trash2
                  size={12}
                  onClick={(e) => {
                    e.stopPropagation();
                    handleDeleteSheet();
                  }}
                  style={{ cursor: 'pointer' }}
                  onMouseEnter={(e) => {
                    (e.target as SVGElement).style.color = '#ff6b6b';
                  }}
                  onMouseLeave={(e) => {
                    (e.target as SVGElement).style.color = '#fff';
                  }}
                />
              )}
            </div>
          ))}
        </div>
      </div>

      {/* Custom CSS for Handsontable dark theme */}
      <style>{`
        .handsontable {
          color: #d4d4d4 !important;
          background: #1a1a1a !important;
          font-family: Consolas, "Courier New", monospace !important;
          font-size: 11px !important;
        }

        .handsontable td {
          background: #1a1a1a !important;
          color: #d4d4d4 !important;
          border-color: #404040 !important;
        }

        .handsontable th {
          background: #2d2d2d !important;
          color: #a3a3a3 !important;
          border-color: #404040 !important;
        }

        .handsontable td.current {
          background: #2d2d2d !important;
        }

        .handsontable td.area {
          background: #3f3f3f !important;
        }

        .handsontable .wtBorder {
          background: #ea580c !important;
        }

        .handsontable .currentRow {
          background: #262626 !important;
        }

        .handsontable .currentCol {
          background: #262626 !important;
        }

        .htContextMenu {
          background: #2d2d2d !important;
          border: 1px solid #404040 !important;
        }

        .htContextMenu .ht_clone_top {
          background: #2d2d2d !important;
        }

        .htContextMenu tbody td {
          background: #2d2d2d !important;
          color: #d4d4d4 !important;
          border-color: #404040 !important;
        }

        .htContextMenu tbody td:hover {
          background: #404040 !important;
        }

        .htDropdownMenu {
          background: #2d2d2d !important;
          border: 1px solid #404040 !important;
        }

        .htDropdownMenu tbody td {
          background: #2d2d2d !important;
          color: #d4d4d4 !important;
        }

        .htDropdownMenu tbody td:hover {
          background: #404040 !important;
        }
      `}</style>

      <TabFooter
        tabName="EXCEL SPREADSHEET"
        leftInfo={[
          { label: `File: ${fileName}`, color: '#a3a3a3' },
          { label: `Sheets: ${sheets.length}`, color: '#a3a3a3' },
          { label: `Active: ${sheets[activeSheetIndex]?.name}`, color: '#a3a3a3' },
        ]}
        statusInfo={`${currentFileId ? 'Saved' : 'Unsaved'} | Snapshots: ${snapshots.length} | Recent: ${recentFiles.length}`}
        backgroundColor="#1a1a1a"
        borderColor="#2d2d2d"
      />
    </div>
  );
};

export default ExcelTab;
