// Excel Marketplace Panel - Browse and download Excel files from marketplace
import React, { useState, useEffect } from 'react';
import {
  ShoppingCart, Download, FileSpreadsheet, Search, Filter,
  TrendingDown, Star, Lock, Unlock, X, Eye, FolderOpen
} from 'lucide-react';
import {
  ExcelMarketplaceService,
  ExcelFile,
  ExcelFileDetails,
  ExcelCategory,
  ExcelDownloadStats
} from '@/services/marketplace/excelMarketplaceService';
import { showSuccess, showError, showWarning } from '@/utils/notifications';
import { writeFile as tauriWriteFile } from '@tauri-apps/plugin-fs';
import { save } from '@tauri-apps/plugin-dialog';

interface ExcelMarketplacePanelProps {
  onClose: () => void;
  onOpenFile: (fileData: ArrayBuffer, fileName: string) => void;
}

const ExcelMarketplacePanel: React.FC<ExcelMarketplacePanelProps> = ({ onClose, onOpenFile }) => {
  const [loading, setLoading] = useState(false);
  const [files, setFiles] = useState<ExcelFile[]>([]);
  const tiers = [
    { name: 'Free', value: 'free' },
    { name: 'Basic', value: 'basic' },
    { name: 'Standard', value: 'standard' },
    { name: 'Pro', value: 'pro' },
    { name: 'Enterprise', value: 'enterprise' }
  ];
  const [stats, setStats] = useState<ExcelDownloadStats | null>(null);
  const [selectedCategory, setSelectedCategory] = useState<string>('');
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedFile, setSelectedFile] = useState<ExcelFileDetails | null>(null);
  const [page, setPage] = useState(1);
  const [totalPages, setTotalPages] = useState(1);
  const [userTier, setUserTier] = useState('');
  const [downloading, setDownloading] = useState<number | null>(null);

  // Load initial data
  useEffect(() => {
    loadStats();
    loadFiles();
  }, []);

  // Reload files when filters change
  useEffect(() => {
    loadFiles();
  }, [page, selectedCategory, searchQuery]);

  const loadCategories = async () => {
    try {
      const response = await ExcelMarketplaceService.getCategories();
      console.log('[Excel Marketplace] Categories response:', response);
      // Categories endpoint returns tier distribution, not categories
      // Skip for now as we'll filter by tier instead
    } catch (error) {
      console.error('[Excel Marketplace] Failed to load categories:', error);
    }
  };

  const loadStats = async () => {
    try {
      const response = await ExcelMarketplaceService.getDownloadStats();
      console.log('[Excel Marketplace] Stats response:', response);
      if (response.success && response.data) {
        setStats(response.data);
      }
    } catch (error) {
      console.error('[Excel Marketplace] Failed to load stats:', error);
    }
  };

  const loadFiles = async () => {
    setLoading(true);
    try {
      const response = await ExcelMarketplaceService.listFiles({
        tier_filter: selectedCategory,
        search: searchQuery || undefined,
        page,
        limit: 20
      });

      // API returns: { success: true, data: { files: [...], pagination: {...} } }
      if (response.success && response.data) {
        const apiData = response.data;

        setFiles(apiData.files || []);
        setTotalPages(apiData.pagination?.total_pages || 1);
        setUserTier(apiData.user_access?.tier || 'free');

        console.log(`[Excel Marketplace] Loaded ${apiData.files?.length || 0} files (page ${page}/${apiData.pagination?.total_pages || 1})`);
      } else {
        const errorMsg = response.error || 'Failed to load files';
        console.error('[Excel Marketplace] Load failed:', errorMsg);
        showError(errorMsg);
      }
    } catch (error) {
      const errorMsg = error instanceof Error ? error.message : 'Failed to load Excel files';
      console.error('[Excel Marketplace] Load files error:', error);
      showError(errorMsg);
    } finally {
      setLoading(false);
    }
  };

  const viewFileDetails = async (fileId: number) => {
    try {
      const response = await ExcelMarketplaceService.getFileDetails(fileId);
      if (response.success && response.data) {
        setSelectedFile(response.data);
      } else {
        showError(response.error || 'Failed to load file details');
      }
    } catch (error) {
      showError('Failed to load file details');
    }
  };

  const downloadAndSave = async (fileId: number, fileName: string) => {
    setDownloading(fileId);
    try {
      const result = await ExcelMarketplaceService.downloadFile(fileId);

      if (result.success && result.blob) {
        // Prompt user to choose save location
        const selectedPath = await save({
          defaultPath: result.fileName || fileName,
          filters: [{
            name: 'Excel Files',
            extensions: ['xlsx', 'xls']
          }]
        });

        if (selectedPath) {
          // Convert blob to array buffer
          const arrayBuffer = await result.blob.arrayBuffer();
          const uint8Array = new Uint8Array(arrayBuffer);

          // Save file
          await tauriWriteFile(selectedPath, uint8Array);

          showSuccess(`File downloaded: ${result.fileName || fileName}`);
          loadStats(); // Refresh stats
        }
      } else {
        showError(result.error || 'Download failed');
      }
    } catch (error) {
      showError('Failed to download file');
      console.error('Download error:', error);
    } finally {
      setDownloading(null);
    }
  };

  const downloadAndOpen = async (fileId: number, fileName: string) => {
    setDownloading(fileId);
    try {
      const result = await ExcelMarketplaceService.downloadFile(fileId);

      if (result.success && result.blob) {
        // Convert blob to array buffer
        const arrayBuffer = await result.blob.arrayBuffer();

        // Open in Excel tab
        onOpenFile(arrayBuffer, result.fileName || fileName);

        showSuccess(`File opened: ${result.fileName || fileName}`);
        loadStats(); // Refresh stats
        onClose(); // Close marketplace panel
      } else {
        showError(result.error || 'Download failed');
      }
    } catch (error) {
      showError('Failed to download and open file');
      console.error('Download and open error:', error);
    } finally {
      setDownloading(null);
    }
  };

  const getTierColor = (tier: string) => {
    switch (tier.toLowerCase()) {
      case 'free': return '#10b981';
      case 'basic': return '#3b82f6';
      case 'standard': return '#a855f7';
      case 'pro': return '#ea580c';
      case 'enterprise': return '#ef4444';
      default: return '#6b7280';
    }
  };

  const getTierBadge = (tier: string) => {
    const colors = {
      free: { bg: 'rgba(16, 185, 129, 0.2)', text: '#10b981', border: 'rgba(16, 185, 129, 0.3)' },
      basic: { bg: 'rgba(59, 130, 246, 0.2)', text: '#3b82f6', border: 'rgba(59, 130, 246, 0.3)' },
      standard: { bg: 'rgba(168, 85, 247, 0.2)', text: '#a855f7', border: 'rgba(168, 85, 247, 0.3)' },
      pro: { bg: 'rgba(234, 88, 12, 0.2)', text: '#ea580c', border: 'rgba(234, 88, 12, 0.3)' },
      enterprise: { bg: 'rgba(239, 68, 68, 0.2)', text: '#ef4444', border: 'rgba(239, 68, 68, 0.3)' }
    };
    const color = colors[tier.toLowerCase() as keyof typeof colors] || { bg: 'rgba(107, 114, 128, 0.2)', text: '#6b7280', border: 'rgba(107, 114, 128, 0.3)' };

    return (
      <span style={{
        padding: '2px 8px',
        borderRadius: '2px',
        fontSize: '10px',
        fontWeight: '600',
        border: `1px solid ${color.border}`,
        backgroundColor: color.bg,
        color: color.text
      }}>
        {tier.toUpperCase()}
      </span>
    );
  };

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.85)',
      backdropFilter: 'blur(4px)',
      zIndex: 2000,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      padding: '16px',
      fontFamily: 'Consolas, "Courier New", monospace'
    }}>
      <div style={{
        backgroundColor: '#0a0a0a',
        border: '1px solid #404040',
        borderRadius: '4px',
        width: '100%',
        maxWidth: '1600px',
        height: '90vh',
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        {/* Header */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '12px 16px',
          borderBottom: '1px solid #404040',
          backgroundColor: '#1a1a1a'
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <ShoppingCart size={18} style={{ color: '#ea580c' }} />
            <div>
              <h2 style={{
                fontSize: '14px',
                fontWeight: 'bold',
                color: '#fff',
                marginBottom: '2px',
                letterSpacing: '0.5px'
              }}>EXCEL MARKETPLACE</h2>
              <p style={{ fontSize: '10px', color: '#a3a3a3', margin: 0 }}>
                Browse and download 122 professional Excel templates • Your tier: <span style={{ color: '#ea580c', fontWeight: 'bold' }}>{userTier.toUpperCase()}</span>
              </p>
            </div>
          </div>
          <button
            onClick={onClose}
            style={{
              padding: '6px',
              backgroundColor: 'transparent',
              border: 'none',
              cursor: 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}
            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#2d2d2d'}
            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
          >
            <X size={18} style={{ color: '#a3a3a3' }} />
          </button>
        </div>

        {/* Stats Bar */}
        {stats && (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '24px',
            padding: '10px 16px',
            backgroundColor: '#0f0f0f',
            borderBottom: '1px solid #404040',
            fontSize: '11px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Download size={14} style={{ color: '#ea580c' }} />
              <span style={{ color: '#a3a3a3' }}>Total Downloads:</span>
              <span style={{ color: '#fff', fontWeight: '600' }}>{stats.total_downloads}</span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <Unlock size={14} style={{ color: '#10b981' }} />
              <span style={{ color: '#a3a3a3' }}>Unlocked:</span>
              <span style={{ color: '#fff', fontWeight: '600' }}>{stats.unlocked_files_count}/{stats.total_files_count}</span>
            </div>
          </div>
        )}

        <div style={{ display: 'flex', flex: 1, minHeight: 0, overflow: 'hidden' }}>
          {/* Sidebar - Categories */}
          <div style={{
            width: '260px',
            borderRight: '1px solid #404040',
            padding: '16px',
            overflowY: 'auto',
            backgroundColor: '#0a0a0a'
          }}>
            <div>
              <h3 style={{
                fontSize: '11px',
                fontWeight: 'bold',
                color: '#a3a3a3',
                marginBottom: '12px',
                letterSpacing: '0.5px'
              }}>FILTER BY TIER</h3>
              <button
                onClick={() => setSelectedCategory('')}
                style={{
                  width: '100%',
                  textAlign: 'left',
                  padding: '8px 12px',
                  borderRadius: '2px',
                  fontSize: '11px',
                  border: 'none',
                  cursor: 'pointer',
                  marginBottom: '4px',
                  backgroundColor: selectedCategory === '' ? 'rgba(234, 88, 12, 0.2)' : 'transparent',
                  color: selectedCategory === '' ? '#ea580c' : '#d4d4d4'
                }}
                onMouseEnter={(e) => {
                  if (selectedCategory !== '') e.currentTarget.style.backgroundColor = '#1a1a1a';
                }}
                onMouseLeave={(e) => {
                  if (selectedCategory !== '') e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                All Tiers
              </button>
              {tiers.map((tier) => (
                <button
                  key={tier.value}
                  onClick={() => setSelectedCategory(tier.value)}
                  style={{
                    width: '100%',
                    textAlign: 'left',
                    padding: '8px 12px',
                    borderRadius: '2px',
                    fontSize: '11px',
                    border: 'none',
                    cursor: 'pointer',
                    marginBottom: '4px',
                    backgroundColor: selectedCategory === tier.value ? 'rgba(234, 88, 12, 0.2)' : 'transparent',
                    color: selectedCategory === tier.value ? '#ea580c' : '#d4d4d4'
                  }}
                  onMouseEnter={(e) => {
                    if (selectedCategory !== tier.value) e.currentTarget.style.backgroundColor = '#1a1a1a';
                  }}
                  onMouseLeave={(e) => {
                    if (selectedCategory !== tier.value) e.currentTarget.style.backgroundColor = 'transparent';
                  }}
                >
                  {tier.name}
                </button>
              ))}
            </div>
          </div>

          {/* Main Content */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' }}>
            {/* Search Bar */}
            <div style={{ padding: '16px', borderBottom: '1px solid #404040' }}>
              <div style={{ position: 'relative' }}>
                <Search size={14} style={{
                  position: 'absolute',
                  left: '10px',
                  top: '50%',
                  transform: 'translateY(-50%)',
                  color: '#666'
                }} />
                <input
                  type="text"
                  placeholder="Search Excel files..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  style={{
                    width: '100%',
                    backgroundColor: '#0f0f0f',
                    border: '1px solid #404040',
                    borderRadius: '2px',
                    paddingLeft: '32px',
                    paddingRight: '12px',
                    paddingTop: '8px',
                    paddingBottom: '8px',
                    fontSize: '11px',
                    color: '#fff',
                    outline: 'none'
                  }}
                  onFocus={(e) => e.currentTarget.style.borderColor = '#ea580c'}
                  onBlur={(e) => e.currentTarget.style.borderColor = '#404040'}
                />
              </div>
            </div>

            {/* Files Grid */}
            <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
              {loading ? (
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%' }}>
                  <div style={{ color: '#a3a3a3', fontSize: '11px' }}>Loading...</div>
                </div>
              ) : files.length === 0 ? (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '100%',
                  color: '#a3a3a3'
                }}>
                  <FileSpreadsheet size={48} style={{ marginBottom: '8px', opacity: 0.5 }} />
                  <p style={{ fontSize: '11px', margin: 0 }}>No files found</p>
                </div>
              ) : (
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
                  gap: '12px'
                }}>
                  {files.map((file) => (
                      <div
                        key={file.id}
                        className="excel-file-card"
                        style={{
                          backgroundColor: '#0f0f0f',
                          border: '1px solid #404040',
                          borderRadius: '2px',
                          padding: '12px',
                          transition: 'border-color 0.2s'
                        }}
                        onMouseEnter={(e) => e.currentTarget.style.borderColor = 'rgba(234, 88, 12, 0.5)'}
                        onMouseLeave={(e) => e.currentTarget.style.borderColor = '#404040'}
                      >
                        <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', marginBottom: '8px' }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                            <FileSpreadsheet size={16} style={{ color: '#ea580c' }} />
                            {file.locked && <Lock size={14} style={{ color: '#ef4444' }} />}
                          </div>
                          {getTierBadge(file.tier)}
                        </div>

                        <h3 style={{
                          fontWeight: '600',
                          color: '#fff',
                          fontSize: '11px',
                          marginBottom: '4px',
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis'
                        }} title={file.display_name}>
                          {file.display_name}
                        </h3>

                        <p style={{
                          fontSize: '10px',
                          color: '#a3a3a3',
                          marginBottom: '10px',
                          display: '-webkit-box',
                          WebkitLineClamp: 2,
                          WebkitBoxOrient: 'vertical',
                          overflow: 'hidden',
                          minHeight: '28px'
                        }}>
                          {file.description || `${file.file_size_mb.toFixed(2)} MB professional Excel template`}
                        </p>

                        <div style={{
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'space-between',
                          fontSize: '10px',
                          color: '#666',
                          marginBottom: '10px'
                        }}>
                          <span>{file.file_size_mb.toFixed(2)} MB</span>
                          <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                            {file.can_download ? <Unlock size={12} /> : <Lock size={12} />}
                            {file.can_download ? 'Available' : 'Locked'}
                          </span>
                        </div>

                        <div style={{ display: 'flex', gap: '6px' }}>
                          <button
                            onClick={() => viewFileDetails(file.id)}
                            style={{
                              flex: 1,
                              padding: '6px 10px',
                              backgroundColor: '#1a1a1a',
                              color: '#fff',
                              fontSize: '10px',
                              border: 'none',
                              borderRadius: '2px',
                              cursor: 'pointer',
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'center',
                              gap: '4px'
                            }}
                            onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#2a2a2a'}
                            onMouseLeave={(e) => e.currentTarget.style.backgroundColor = '#1a1a1a'}
                          >
                            <Eye size={12} />
                            Details
                          </button>

                          {!file.locked && (
                            <>
                              <button
                                onClick={() => downloadAndSave(file.id, file.filename)}
                                disabled={downloading === file.id}
                                style={{
                                  flex: 1,
                                  padding: '6px 10px',
                                  backgroundColor: '#ea580c',
                                  color: '#000',
                                  fontSize: '10px',
                                  fontWeight: '600',
                                  border: 'none',
                                  borderRadius: '2px',
                                  cursor: downloading === file.id ? 'not-allowed' : 'pointer',
                                  display: 'flex',
                                  alignItems: 'center',
                                  justifyContent: 'center',
                                  gap: '4px',
                                  opacity: downloading === file.id ? 0.5 : 1
                                }}
                                onMouseEnter={(e) => {
                                  if (downloading !== file.id) e.currentTarget.style.backgroundColor = 'rgba(234, 88, 12, 0.8)';
                                }}
                                onMouseLeave={(e) => {
                                  if (downloading !== file.id) e.currentTarget.style.backgroundColor = '#ea580c';
                                }}
                              >
                                <Download size={12} />
                                {downloading === file.id ? 'Downloading...' : 'Download'}
                              </button>
                              <button
                                onClick={() => downloadAndOpen(file.id, file.filename)}
                                disabled={downloading === file.id}
                                title="Download and open in Excel tab"
                                style={{
                                  padding: '6px 10px',
                                  backgroundColor: '#10b981',
                                  color: '#fff',
                                  fontSize: '10px',
                                  fontWeight: '600',
                                  border: 'none',
                                  borderRadius: '2px',
                                  cursor: downloading === file.id ? 'not-allowed' : 'pointer',
                                  display: 'flex',
                                  alignItems: 'center',
                                  justifyContent: 'center',
                                  gap: '4px',
                                  opacity: downloading === file.id ? 0.5 : 1
                                }}
                                onMouseEnter={(e) => {
                                  if (downloading !== file.id) e.currentTarget.style.backgroundColor = '#059669';
                                }}
                                onMouseLeave={(e) => {
                                  if (downloading !== file.id) e.currentTarget.style.backgroundColor = '#10b981';
                                }}
                              >
                                <FolderOpen size={12} />
                              </button>
                            </>
                          )}

                          {file.locked && (
                            <button
                              disabled
                              style={{
                                flex: 1,
                                padding: '6px 10px',
                                backgroundColor: '#2a2a2a',
                                color: '#666',
                                fontSize: '10px',
                                border: 'none',
                                borderRadius: '2px',
                                cursor: 'not-allowed',
                                display: 'flex',
                                alignItems: 'center',
                                justifyContent: 'center',
                                gap: '4px'
                              }}
                            >
                              <Lock size={12} />
                              Locked
                            </button>
                          )}
                        </div>
                      </div>
                  ))}
                </div>
              )}
            </div>

            {/* Pagination */}
            {totalPages > 1 && (
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                padding: '12px 16px',
                borderTop: '1px solid #404040'
              }}>
                <button
                  onClick={() => setPage(p => Math.max(1, p - 1))}
                  disabled={page === 1}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: '#1a1a1a',
                    color: '#fff',
                    fontSize: '11px',
                    border: 'none',
                    borderRadius: '2px',
                    cursor: page === 1 ? 'not-allowed' : 'pointer',
                    opacity: page === 1 ? 0.5 : 1
                  }}
                  onMouseEnter={(e) => {
                    if (page !== 1) e.currentTarget.style.backgroundColor = '#2a2a2a';
                  }}
                  onMouseLeave={(e) => {
                    if (page !== 1) e.currentTarget.style.backgroundColor = '#1a1a1a';
                  }}
                >
                  Previous
                </button>
                <span style={{ fontSize: '11px', color: '#a3a3a3' }}>
                  Page {page} of {totalPages}
                </span>
                <button
                  onClick={() => setPage(p => Math.min(totalPages, p + 1))}
                  disabled={page === totalPages}
                  style={{
                    padding: '6px 12px',
                    backgroundColor: '#1a1a1a',
                    color: '#fff',
                    fontSize: '11px',
                    border: 'none',
                    borderRadius: '2px',
                    cursor: page === totalPages ? 'not-allowed' : 'pointer',
                    opacity: page === totalPages ? 0.5 : 1
                  }}
                  onMouseEnter={(e) => {
                    if (page !== totalPages) e.currentTarget.style.backgroundColor = '#2a2a2a';
                  }}
                  onMouseLeave={(e) => {
                    if (page !== totalPages) e.currentTarget.style.backgroundColor = '#1a1a1a';
                  }}
                >
                  Next
                </button>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* File Details Modal */}
      {selectedFile && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.85)',
          backdropFilter: 'blur(4px)',
          zIndex: 2100,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          padding: '16px'
        }}>
          <div style={{
            backgroundColor: '#0a0a0a',
            border: '1px solid #404040',
            borderRadius: '4px',
            width: '100%',
            maxWidth: '800px',
            maxHeight: '80vh',
            overflowY: 'auto'
          }}>
            <div style={{ padding: '20px' }}>
              <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', marginBottom: '16px' }}>
                <div style={{ flex: 1 }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '8px' }}>
                    <FileSpreadsheet size={20} style={{ color: '#ea580c' }} />
                    <h2 style={{ fontSize: '16px', fontWeight: 'bold', color: '#fff', margin: 0 }}>{selectedFile.title}</h2>
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
                    {getTierBadge(selectedFile.tier)}
                    <span style={{ fontSize: '11px', color: '#a3a3a3' }}>{selectedFile.category}</span>
                  </div>
                </div>
                <button
                  onClick={() => setSelectedFile(null)}
                  style={{
                    padding: '6px',
                    backgroundColor: 'transparent',
                    border: 'none',
                    cursor: 'pointer',
                    borderRadius: '2px'
                  }}
                  onMouseEnter={(e) => e.currentTarget.style.backgroundColor = '#1a1a1a'}
                  onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                >
                  <X size={18} style={{ color: '#a3a3a3' }} />
                </button>
              </div>

              <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                <div>
                  <h3 style={{ fontSize: '11px', fontWeight: '600', color: '#a3a3a3', marginBottom: '6px' }}>DESCRIPTION</h3>
                  <p style={{ fontSize: '11px', color: '#d4d4d4', margin: 0, lineHeight: '1.5' }}>
                    {selectedFile.detailed_description || selectedFile.description}
                  </p>
                </div>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', fontSize: '11px' }}>
                  <div>
                    <span style={{ color: '#a3a3a3' }}>File Size:</span>
                    <span style={{ marginLeft: '8px', color: '#fff', fontWeight: '600' }}>
                      {(selectedFile.file_size / 1024).toFixed(1)} KB
                    </span>
                  </div>
                  <div>
                    <span style={{ color: '#a3a3a3' }}>Downloads:</span>
                    <span style={{ marginLeft: '8px', color: '#fff', fontWeight: '600' }}>
                      {selectedFile.downloads_count}
                    </span>
                  </div>
                </div>

                {!selectedFile.locked && (
                  <div style={{ display: 'flex', gap: '10px', paddingTop: '8px' }}>
                    <button
                      onClick={() => {
                        downloadAndSave(selectedFile.id, selectedFile.filename);
                        setSelectedFile(null);
                      }}
                      disabled={downloading === selectedFile.id}
                      style={{
                        flex: 1,
                        padding: '10px 16px',
                        backgroundColor: '#ea580c',
                        color: '#000',
                        fontSize: '11px',
                        fontWeight: '600',
                        border: 'none',
                        borderRadius: '2px',
                        cursor: downloading === selectedFile.id ? 'not-allowed' : 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '6px',
                        opacity: downloading === selectedFile.id ? 0.5 : 1
                      }}
                      onMouseEnter={(e) => {
                        if (downloading !== selectedFile.id) e.currentTarget.style.backgroundColor = 'rgba(234, 88, 12, 0.8)';
                      }}
                      onMouseLeave={(e) => {
                        if (downloading !== selectedFile.id) e.currentTarget.style.backgroundColor = '#ea580c';
                      }}
                    >
                      <Download size={14} />
                      {downloading === selectedFile.id ? 'Downloading...' : 'Download (2 credits)'}
                    </button>
                    <button
                      onClick={() => {
                        downloadAndOpen(selectedFile.id, selectedFile.filename);
                        setSelectedFile(null);
                      }}
                      disabled={downloading === selectedFile.id}
                      style={{
                        padding: '10px 16px',
                        backgroundColor: '#10b981',
                        color: '#fff',
                        fontSize: '11px',
                        fontWeight: '600',
                        border: 'none',
                        borderRadius: '2px',
                        cursor: downloading === selectedFile.id ? 'not-allowed' : 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '6px',
                        opacity: downloading === selectedFile.id ? 0.5 : 1
                      }}
                      onMouseEnter={(e) => {
                        if (downloading !== selectedFile.id) e.currentTarget.style.backgroundColor = '#059669';
                      }}
                      onMouseLeave={(e) => {
                        if (downloading !== selectedFile.id) e.currentTarget.style.backgroundColor = '#10b981';
                      }}
                    >
                      <FolderOpen size={14} />
                      Open in Excel
                    </button>
                  </div>
                )}

                {selectedFile.locked && (
                  <div style={{
                    backgroundColor: 'rgba(239, 68, 68, 0.1)',
                    border: '1px solid rgba(239, 68, 68, 0.3)',
                    borderRadius: '2px',
                    padding: '12px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px'
                  }}>
                    <Lock size={14} style={{ color: '#ef4444' }} />
                    <span style={{ fontSize: '11px', color: '#ef4444' }}>
                      This file requires a higher subscription tier to download
                    </span>
                  </div>
                )}
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default ExcelMarketplacePanel;
