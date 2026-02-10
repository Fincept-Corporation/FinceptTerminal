import React, { useState, useEffect } from 'react';
import { Upload, File, Folder, Trash2, Download, Search, RefreshCw, FileText, FileSpreadsheet, Image as ImageIcon, Film, Music, Archive } from 'lucide-react';
import { fileStorageService } from '@/services/fileStorageService';
import { showSuccess, showError, showConfirm } from '@/utils/notifications';

interface StoredFile {
  id: string;
  name: string;
  originalName: string;
  size: number;
  type: string;
  uploadedAt: string;
  path: string;
}

export const FileManagerTab: React.FC = () => {
  const [files, setFiles] = useState<StoredFile[]>([]);
  const [loading, setLoading] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedFiles, setSelectedFiles] = useState<Set<string>>(new Set());

  useEffect(() => {
    loadFiles();
  }, []);

  const loadFiles = async () => {
    setLoading(true);
    try {
      const fileList = await fileStorageService.listFiles();
      setFiles(fileList);
    } catch (error: any) {
      showError('Failed to load files', [{ label: 'ERROR', value: error.message }]);
    } finally {
      setLoading(false);
    }
  };

  const handleFileUpload = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const uploadedFiles = event.target.files;
    if (!uploadedFiles || uploadedFiles.length === 0) return;

    setLoading(true);
    try {
      for (const file of Array.from(uploadedFiles)) {
        await fileStorageService.uploadFile(file);
      }
      showSuccess(`Uploaded ${uploadedFiles.length} file(s)`);
      loadFiles();
    } catch (error: any) {
      showError('Upload failed', [{ label: 'ERROR', value: error.message }]);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (fileId: string) => {
    const confirmed = await showConfirm('Delete this file? This action cannot be undone.', {
      title: 'Confirm Delete',
      type: 'danger',
    });

    if (!confirmed) return;

    try {
      await fileStorageService.deleteFile(fileId);
      showSuccess('File deleted');
      loadFiles();
    } catch (error: any) {
      showError('Delete failed', [{ label: 'ERROR', value: error.message }]);
    }
  };

  const handleDownload = async (file: StoredFile) => {
    try {
      await fileStorageService.downloadFile(file.id, file.originalName);
      showSuccess('File downloaded');
    } catch (error: any) {
      showError('Download failed', [{ label: 'ERROR', value: error.message }]);
    }
  };

  const getFileIcon = (type: string) => {
    if (type.includes('spreadsheet') || type.includes('csv') || type.includes('excel')) {
      return <FileSpreadsheet size={20} className="text-green-500" />;
    }
    if (type.includes('image')) {
      return <ImageIcon size={20} className="text-blue-500" />;
    }
    if (type.includes('video')) {
      return <Film size={20} className="text-purple-500" />;
    }
    if (type.includes('audio')) {
      return <Music size={20} className="text-pink-500" />;
    }
    if (type.includes('zip') || type.includes('compressed')) {
      return <Archive size={20} className="text-yellow-500" />;
    }
    return <FileText size={20} className="text-gray-400" />;
  };

  const formatFileSize = (bytes: number): string => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  };

  const filteredFiles = files.filter(file =>
    file.originalName.toLowerCase().includes(searchQuery.toLowerCase()) ||
    file.type.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: '#0a0a0a',
      color: '#fff',
    }}>
      {/* Header */}
      <div style={{
        padding: '16px 20px',
        borderBottom: '1px solid #2d2d2d',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
      }}>
        <div>
          <h1 style={{ fontSize: '18px', fontWeight: 'bold', color: '#ea580c', marginBottom: '4px' }}>
            FILE MANAGER
          </h1>
          <p style={{ fontSize: '12px', color: '#6b7280' }}>
            Centralized file storage for all terminal operations
          </p>
        </div>
        <div style={{ display: 'flex', gap: '8px' }}>
          <button
            onClick={loadFiles}
            disabled={loading}
            style={{
              padding: '8px 16px',
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
              borderRadius: '4px',
              color: '#fff',
              cursor: loading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontSize: '12px',
            }}
          >
            <RefreshCw size={14} />
            Refresh
          </button>
          <label style={{
            padding: '8px 16px',
            backgroundColor: '#ea580c',
            border: 'none',
            borderRadius: '4px',
            color: '#000',
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontSize: '12px',
            fontWeight: 'bold',
          }}>
            <Upload size={14} />
            Upload Files
            <input
              type="file"
              multiple
              onChange={handleFileUpload}
              style={{ display: 'none' }}
            />
          </label>
        </div>
      </div>

      {/* Search Bar */}
      <div style={{ padding: '12px 20px', borderBottom: '1px solid #2d2d2d' }}>
        <div style={{ position: 'relative' }}>
          <Search size={16} style={{
            position: 'absolute',
            left: '12px',
            top: '50%',
            transform: 'translateY(-50%)',
            color: '#6b7280',
          }} />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Search files..."
            style={{
              width: '100%',
              padding: '10px 12px 10px 40px',
              backgroundColor: '#1a1a1a',
              border: '1px solid #2d2d2d',
              borderRadius: '4px',
              color: '#fff',
              fontSize: '13px',
              outline: 'none',
            }}
          />
        </div>
      </div>

      {/* File Stats */}
      <div style={{
        padding: '12px 20px',
        borderBottom: '1px solid #2d2d2d',
        display: 'flex',
        gap: '24px',
        fontSize: '12px',
        color: '#9ca3af',
      }}>
        <div>
          <span style={{ fontWeight: 'bold', color: '#ea580c' }}>{filteredFiles.length}</span> files
        </div>
        <div>
          <span style={{ fontWeight: 'bold', color: '#ea580c' }}>
            {formatFileSize(files.reduce((sum, f) => sum + f.size, 0))}
          </span> total
        </div>
      </div>

      {/* File List */}
      <div style={{ flex: 1, overflow: 'auto', padding: '20px' }}>
        {loading ? (
          <div style={{ textAlign: 'center', padding: '40px', color: '#6b7280' }}>
            Loading files...
          </div>
        ) : filteredFiles.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '40px', color: '#6b7280' }}>
            <Folder size={48} style={{ margin: '0 auto 16px', opacity: 0.3 }} />
            <p>No files found</p>
            <p style={{ fontSize: '12px', marginTop: '8px' }}>
              Upload files to get started
            </p>
          </div>
        ) : (
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '12px' }}>
            {filteredFiles.map((file) => (
              <div
                key={file.id}
                style={{
                  padding: '16px',
                  backgroundColor: '#1a1a1a',
                  border: '1px solid #2d2d2d',
                  borderRadius: '6px',
                  transition: 'all 0.2s',
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = '#ea580c';
                  e.currentTarget.style.backgroundColor = '#1f1f1f';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = '#2d2d2d';
                  e.currentTarget.style.backgroundColor = '#1a1a1a';
                }}
              >
                <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px' }}>
                  {getFileIcon(file.type)}
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{
                      fontSize: '13px',
                      fontWeight: '500',
                      color: '#fff',
                      marginBottom: '4px',
                      overflow: 'hidden',
                      textOverflow: 'ellipsis',
                      whiteSpace: 'nowrap',
                    }}>
                      {file.originalName}
                    </div>
                    <div style={{ fontSize: '11px', color: '#6b7280', marginBottom: '8px' }}>
                      {formatFileSize(file.size)} • {new Date(file.uploadedAt).toLocaleDateString()}
                    </div>
                    <div style={{ display: 'flex', gap: '6px' }}>
                      <button
                        onClick={() => handleDownload(file)}
                        style={{
                          padding: '4px 8px',
                          backgroundColor: '#2d2d2d',
                          border: '1px solid #3d3d3d',
                          borderRadius: '3px',
                          color: '#a3a3a3',
                          fontSize: '10px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                        }}
                      >
                        <Download size={10} />
                        Download
                      </button>
                      <button
                        onClick={() => handleDelete(file.id)}
                        style={{
                          padding: '4px 8px',
                          backgroundColor: '#2d2d2d',
                          border: '1px solid #3d3d3d',
                          borderRadius: '3px',
                          color: '#ef4444',
                          fontSize: '10px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                        }}
                      >
                        <Trash2 size={10} />
                        Delete
                      </button>
                    </div>
                  </div>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

export default FileManagerTab;
