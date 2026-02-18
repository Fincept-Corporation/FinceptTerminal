import React, { useState, useEffect } from 'react';
import { RefreshCw, Trash2, HardDrive, Database, Lock, Eye, EyeOff, Plus, Edit2, Save, X, Search, Table2, ChevronRight, Download, Loader2, LogOut } from 'lucide-react';
import { cacheService, type CacheStats } from '@/services/cache/cacheService';
import { LLMModelsService } from '@/services/llmModelsService';
import type { SettingsColors } from '../types';
import { showConfirm } from '@/utils/notifications';
import { invoke } from '@tauri-apps/api/core';

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
};

interface StorageCacheSectionProps {
  colors: SettingsColors;
  showMessage: (type: 'success' | 'error', text: string) => void;
}

interface DatabaseInfo {
  name: string;
  path: string;
  size_bytes: number;
  table_count: number;
}

interface TableInfo {
  name: string;
  row_count: number;
  columns: ColumnInfo[];
}

interface ColumnInfo {
  name: string;
  data_type: string;
  not_null: boolean;
  primary_key: boolean;
}

interface TableRow {
  data: Record<string, any>;
}

export function StorageCacheSection({
  colors,
  showMessage,
}: StorageCacheSectionProps) {
  const [activeTab, setActiveTab] = useState<'cache' | 'database'>('cache');

  // Cache state
  const [cacheStats, setCacheStats] = useState<CacheStats | null>(null);
  const [clearingCache, setClearingCache] = useState(false);

  // Database state
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [hasPassword, setHasPassword] = useState(false);
  const [showPasswordModal, setShowPasswordModal] = useState(false);
  const [password, setPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [isSettingPassword, setIsSettingPassword] = useState(false);
  const [databases, setDatabases] = useState<DatabaseInfo[]>([]);
  const [selectedDb, setSelectedDb] = useState<DatabaseInfo | null>(null);
  const [tables, setTables] = useState<TableInfo[]>([]);
  const [selectedTable, setSelectedTable] = useState<TableInfo | null>(null);
  const [tableData, setTableData] = useState<TableRow[]>([]);
  const [page, setPage] = useState(0);
  const [editingRow, setEditingRow] = useState<number | null>(null);
  const [editedData, setEditedData] = useState<Record<string, any>>({});
  const [isAddingRow, setIsAddingRow] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [loading, setLoading] = useState(false);

  const PAGE_SIZE = 50;

  useEffect(() => {
    checkPasswordAndSession();
  }, []);

  const checkPasswordAndSession = async () => {
    try {
      const exists = await invoke<boolean>('db_admin_check_password');
      setHasPassword(exists);

      // Check if there's a valid session
      if (exists) {
        const hasValidSession = await invoke<boolean>('db_admin_check_session');
        if (hasValidSession) {
          setIsAuthenticated(true);
          loadDatabases();
        }
      }
    } catch (error) {
      console.error('Failed to check password:', error);
    }
  };

  const handlePasswordSubmit = async () => {
    if (isSettingPassword) {
      if (password !== confirmPassword) {
        showMessage('error', 'Passwords do not match');
        return;
      }
      if (password.length < 6) {
        showMessage('error', 'Password must be at least 6 characters');
        return;
      }
      try {
        await invoke('db_admin_set_password', { password });
        showMessage('success', 'Password set successfully');
        setHasPassword(true);
        setIsSettingPassword(false);
        setPassword('');
        setConfirmPassword('');
        setIsAuthenticated(true);
        setShowPasswordModal(false);
        loadDatabases();
      } catch (error) {
        showMessage('error', 'Failed to set password');
      }
    } else {
      try {
        const valid = await invoke<boolean>('db_admin_verify_password', { password });
        if (valid) {
          // Create session (24 hours)
          await invoke('db_admin_create_session', { durationHours: 24 });

          setIsAuthenticated(true);
          setShowPasswordModal(false);
          setPassword('');
          showMessage('success', 'Access granted (session valid for 24 hours)');
          loadDatabases();
        } else {
          showMessage('error', 'Invalid password');
        }
      } catch (error) {
        showMessage('error', 'Authentication failed');
      }
    }
  };

  const loadDatabases = async () => {
    try {
      const dbs = await invoke<DatabaseInfo[]>('db_admin_get_databases');
      setDatabases(dbs);
    } catch (error) {
      showMessage('error', 'Failed to load databases');
    }
  };

  const loadTables = async (db: DatabaseInfo) => {
    try {
      setLoading(true);
      const tbls = await invoke<TableInfo[]>('db_admin_get_tables', { dbPath: db.path });
      setTables(tbls);
      setSelectedDb(db);
      setSelectedTable(null);
      setTableData([]);
    } catch (error) {
      showMessage('error', 'Failed to load tables');
    } finally {
      setLoading(false);
    }
  };

  const loadTableData = async (table: TableInfo, resetPage = false) => {
    if (!selectedDb) return;
    try {
      setLoading(true);
      const currentPage = resetPage ? 0 : page;
      if (resetPage) setPage(0);

      const data = await invoke<TableRow[]>('db_admin_get_table_data', {
        dbPath: selectedDb.path,
        tableName: table.name,
        limit: PAGE_SIZE,
        offset: currentPage * PAGE_SIZE,
      });
      setTableData(data);
      setSelectedTable(table);
    } catch (error) {
      showMessage('error', 'Failed to load table data');
    } finally {
      setLoading(false);
    }
  };

  const handleUpdateRow = async (rowIndex: number) => {
    if (!selectedDb || !selectedTable) return;
    const pkCol = selectedTable.columns.find(c => c.primary_key);
    if (!pkCol) {
      showMessage('error', 'No primary key found');
      return;
    }

    try {
      await invoke('db_admin_update_row', {
        dbPath: selectedDb.path,
        tableName: selectedTable.name,
        rowData: editedData,
        primaryKeyColumn: pkCol.name,
        primaryKeyValue: editedData[pkCol.name],
      });
      showMessage('success', 'Row updated successfully');
      setEditingRow(null);
      loadTableData(selectedTable);
    } catch (error: any) {
      showMessage('error', error.toString());
    }
  };

  const handleInsertRow = async () => {
    if (!selectedDb || !selectedTable) return;

    try {
      await invoke('db_admin_insert_row', {
        dbPath: selectedDb.path,
        tableName: selectedTable.name,
        rowData: editedData,
      });
      showMessage('success', 'Row added successfully');
      setIsAddingRow(false);
      setEditedData({});
      loadTableData(selectedTable);
    } catch (error: any) {
      showMessage('error', error.toString());
    }
  };

  const handleDeleteRow = async (row: TableRow) => {
    if (!selectedDb || !selectedTable) return;
    const pkCol = selectedTable.columns.find(c => c.primary_key);
    if (!pkCol) {
      showMessage('error', 'No primary key found');
      return;
    }

    const confirmed = await showConfirm('This action cannot be undone.', {
      title: 'Delete row?',
      type: 'danger',
    });
    if (!confirmed) return;

    try {
      await invoke('db_admin_delete_row', {
        dbPath: selectedDb.path,
        tableName: selectedTable.name,
        primaryKeyColumn: pkCol.name,
        primaryKeyValue: row.data[pkCol.name],
      });
      showMessage('success', 'Row deleted successfully');
      loadTableData(selectedTable);
    } catch (error: any) {
      showMessage('error', error.toString());
    }
  };

  const refreshStats = async () => {
    const stats = await cacheService.getStats();
    setCacheStats(stats);
  };

  const handleCleanup = async () => {
    setClearingCache(true);
    try {
      const removed = await cacheService.cleanup();
      showMessage('success', `Cleaned up ${removed} expired cache entries`);
      await refreshStats();
    } catch (error) {
      showMessage('error', 'Failed to cleanup cache');
    } finally {
      setClearingCache(false);
    }
  };

  const handleClearAll = async () => {
    const confirmed = await showConfirm(
      'This action cannot be undone.\n\nNote: Settings and saved data will not be affected.',
      {
        title: 'Clear all cached data?',
        type: 'danger'
      }
    );
    if (!confirmed) return;

    setClearingCache(true);
    try {
      const removed = await cacheService.clearAll();
      await LLMModelsService.clearCache();
      showMessage('success', `Cleared ${removed} cache entries successfully`);
      setCacheStats(null);
      await refreshStats();
    } catch (error) {
      showMessage('error', 'Failed to clear cache');
    } finally {
      setClearingCache(false);
    }
  };

  const formatBytes = (bytes: number) => {
    if (bytes < 1024) return bytes + ' B';
    if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(2) + ' KB';
    return (bytes / 1024 / 1024).toFixed(2) + ' MB';
  };

  return (
    <div style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace', height: '100%', display: 'flex', flexDirection: 'column' }}>
      <style>{`
        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }
        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }
        *::-webkit-scrollbar-track {
          background: ${FINCEPT.DARK_BG};
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb {
          background: ${FINCEPT.BORDER};
          border-radius: 4px;
        }
        *::-webkit-scrollbar-thumb:hover {
          background: ${FINCEPT.ORANGE};
        }
        *::-webkit-scrollbar-corner {
          background: ${FINCEPT.DARK_BG};
        }
      `}</style>
      {/* Tabs */}
      <div style={{ display: 'flex', gap: '8px', marginBottom: '16px', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
        <button
          onClick={() => setActiveTab('cache')}
          style={{
            padding: '8px 16px',
            backgroundColor: activeTab === 'cache' ? `${FINCEPT.ORANGE}20` : 'transparent',
            borderBottom: activeTab === 'cache' ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
            color: activeTab === 'cache' ? FINCEPT.ORANGE : FINCEPT.GRAY,
            fontSize: '10px',
            fontWeight: 700,
            cursor: 'pointer',
            border: 'none',
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
          }}
        >
          <HardDrive size={12} style={{ display: 'inline', marginRight: '6px' }} />
          Cache Management
        </button>
        <button
          onClick={() => {
            setActiveTab('database');
            if (!isAuthenticated && !showPasswordModal) {
              setShowPasswordModal(true);
            }
          }}
          style={{
            padding: '8px 16px',
            backgroundColor: activeTab === 'database' ? `${FINCEPT.ORANGE}20` : 'transparent',
            borderBottom: activeTab === 'database' ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
            color: activeTab === 'database' ? FINCEPT.ORANGE : FINCEPT.GRAY,
            fontSize: '10px',
            fontWeight: 700,
            cursor: 'pointer',
            border: 'none',
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
          }}
        >
          <Database size={12} style={{ display: 'inline', marginRight: '6px' }} />
          Database Manager
          <Lock size={10} style={{ display: 'inline', marginLeft: '6px' }} />
        </button>
      </div>

      {/* Cache Tab */}
      {activeTab === 'cache' && (
        <>
          {/* Section Header */}
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            marginBottom: '16px'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
              <HardDrive size={14} color={FINCEPT.ORANGE} />
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px',
                textTransform: 'uppercase'
              }}>
                Storage & Cache Management
              </span>
            </div>
            <p style={{
              fontSize: '10px',
              color: FINCEPT.GRAY,
              margin: 0,
              lineHeight: 1.5
            }}>
              Manage application cache and storage. Clearing cache can help resolve issues and free up disk space.
            </p>
          </div>

          {/* Cache Statistics */}
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            padding: '12px',
            marginBottom: '12px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <Database size={12} color={FINCEPT.ORANGE} />
                <span style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.WHITE,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>
                  Cache Statistics
                </span>
              </div>
              <button
                onClick={refreshStats}
                style={{
                  padding: '6px 10px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.GRAY,
                  fontSize: '9px',
                  fontWeight: 700,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  e.currentTarget.style.color = FINCEPT.WHITE;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }}
              >
                <RefreshCw size={10} />
                Refresh
              </button>
            </div>

            {cacheStats ? (
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
                <div style={{
                  backgroundColor: FINCEPT.HEADER_BG,
                  padding: '10px',
                  borderRadius: '2px',
                  border: `1px solid ${FINCEPT.BORDER}`
                }}>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '4px'
                  }}>
                    Total Entries
                  </div>
                  <div style={{ color: FINCEPT.CYAN, fontSize: '14px', fontWeight: 700 }}>
                    {cacheStats.total_entries.toLocaleString()}
                  </div>
                </div>
                <div style={{
                  backgroundColor: FINCEPT.HEADER_BG,
                  padding: '10px',
                  borderRadius: '2px',
                  border: `1px solid ${FINCEPT.BORDER}`
                }}>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '4px'
                  }}>
                    Cache Size
                  </div>
                  <div style={{ color: FINCEPT.CYAN, fontSize: '14px', fontWeight: 700 }}>
                    {(cacheStats.total_size_bytes / 1024 / 1024).toFixed(2)} MB
                  </div>
                </div>
                <div style={{
                  backgroundColor: FINCEPT.HEADER_BG,
                  padding: '10px',
                  borderRadius: '2px',
                  border: `1px solid ${FINCEPT.BORDER}`
                }}>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '4px'
                  }}>
                    Expired Entries
                  </div>
                  <div style={{
                    color: cacheStats.expired_entries > 0 ? FINCEPT.YELLOW : FINCEPT.CYAN,
                    fontSize: '14px',
                    fontWeight: 700
                  }}>
                    {cacheStats.expired_entries.toLocaleString()}
                  </div>
                </div>
              </div>
            ) : (
              <div style={{
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                justifyContent: 'center',
                padding: '20px',
                color: FINCEPT.MUTED,
                fontSize: '10px',
                textAlign: 'center'
              }}>
                <Database size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>Click "Refresh" to load cache statistics</span>
              </div>
            )}

            {cacheStats && cacheStats.categories && cacheStats.categories.length > 0 && (
              <div style={{ marginTop: '12px' }}>
                <div style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                  marginBottom: '6px'
                }}>
                  Categories
                </div>
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                  {cacheStats.categories.map(cat => (
                    <div
                      key={cat.category}
                      style={{
                        padding: '2px 6px',
                        backgroundColor: `${FINCEPT.ORANGE}20`,
                        color: FINCEPT.ORANGE,
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px'
                      }}
                    >
                      <span style={{ textTransform: 'uppercase', letterSpacing: '0.5px' }}>{cat.category}</span>
                      <span style={{ color: FINCEPT.GRAY }}>
                        {cat.entry_count} · {(cat.total_size / 1024).toFixed(1)}KB
                      </span>
                    </div>
                  ))}
                </div>
              </div>
            )}
          </div>

          {/* Cache Actions */}
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            padding: '12px',
            marginBottom: '12px'
          }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              marginBottom: '12px'
            }}>
              Cache Actions
            </div>

            <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
              {/* Clear Expired */}
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '10px',
                backgroundColor: FINCEPT.HEADER_BG,
                borderRadius: '2px',
                border: `1px solid ${FINCEPT.BORDER}`
              }}>
                <div>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.WHITE,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '2px'
                  }}>
                    Clear Expired Entries
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
                    Remove all expired cache entries to free up space
                  </div>
                </div>
                <button
                  onClick={handleCleanup}
                  disabled={clearingCache}
                  style={{
                    padding: '6px 10px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    cursor: clearingCache ? 'not-allowed' : 'pointer',
                    opacity: clearingCache ? 0.5 : 1,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    transition: 'all 0.2s',
                    whiteSpace: 'nowrap'
                  }}
                  onMouseEnter={(e) => {
                    if (!clearingCache) {
                      e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                      e.currentTarget.style.color = FINCEPT.WHITE;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!clearingCache) {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                      e.currentTarget.style.color = FINCEPT.GRAY;
                    }
                  }}
                >
                  {clearingCache ? 'Cleaning...' : 'Clean Up'}
                </button>
              </div>

              {/* Clear All Cache */}
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                padding: '10px',
                backgroundColor: FINCEPT.HEADER_BG,
                borderRadius: '2px',
                border: `1px solid ${FINCEPT.RED}40`
              }}>
                <div>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.WHITE,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    marginBottom: '2px'
                  }}>
                    Clear All Cache
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
                    Remove all cached data. This will not affect your settings or saved data.
                  </div>
                </div>
                <button
                  onClick={handleClearAll}
                  disabled={clearingCache}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: clearingCache ? FINCEPT.MUTED : FINCEPT.RED,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: clearingCache ? 'not-allowed' : 'pointer',
                    opacity: clearingCache ? 0.5 : 1,
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    whiteSpace: 'nowrap'
                  }}
                >
                  <Trash2 size={10} />
                  {clearingCache ? 'Clearing...' : 'Clear All'}
                </button>
              </div>
            </div>
          </div>

          {/* Info */}
          <div style={{
            padding: '10px',
            backgroundColor: `${FINCEPT.CYAN}10`,
            borderRadius: '2px',
            border: `1px solid ${FINCEPT.CYAN}40`
          }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.CYAN,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              marginBottom: '4px'
            }}>
              About Cache
            </div>
            <div style={{
              color: FINCEPT.GRAY,
              fontSize: '9px',
              lineHeight: 1.5
            }}>
              The cache stores temporary data like market quotes, news articles, and API responses to improve performance.
              Clearing the cache will not delete your settings, credentials, portfolios, or other saved data.
              The application will automatically rebuild the cache as you use it.
            </div>
          </div>
        </>
      )}

      {/* Database Tab */}
      {activeTab === 'database' && (
        <>
          {!isAuthenticated ? (
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              height: '400px',
              color: FINCEPT.GRAY,
              fontSize: '10px'
            }}>
              <Lock size={48} style={{ opacity: 0.3 }} />
            </div>
          ) : (
            <>
              {/* Session Info & Logout Bar */}
              <div style={{
                padding: '8px 12px',
                backgroundColor: `${FINCEPT.GREEN}10`,
                border: `1px solid ${FINCEPT.GREEN}40`,
                borderRadius: '2px',
                marginBottom: '12px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                  🔓 <strong style={{ color: FINCEPT.GREEN }}>Authenticated</strong> - Session valid for 24 hours from login
                </div>
                <button
                  onClick={async () => {
                    await invoke('db_admin_clear_session');
                    setIsAuthenticated(false);
                    setSelectedDb(null);
                    setSelectedTable(null);
                    setDatabases([]);
                    setTables([]);
                    setTableData([]);
                    showMessage('success', 'Logged out successfully');
                  }}
                  style={{
                    padding: '4px 10px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${FINCEPT.RED}`,
                    color: FINCEPT.RED,
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.RED;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.color = FINCEPT.RED;
                  }}
                >
                  <LogOut size={10} />
                  Logout
                </button>
              </div>

              <div style={{ display: 'flex', gap: '12px', flex: 1, minHeight: 0, overflow: 'hidden' }}>
              {/* Databases List */}
              <div style={{
                width: '280px',
                minWidth: '280px',
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                display: 'flex',
                flexDirection: 'column',
                overflow: 'hidden'
              }}>
                <div style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                  padding: '12px 12px 8px 12px',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  flexShrink: 0
                }}>
                  Databases ({databases.length})
                </div>
                <div style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
                {databases.map(db => (
                  <div
                    key={db.path}
                    onClick={() => loadTables(db)}
                    style={{
                      padding: '8px',
                      backgroundColor: selectedDb?.path === db.path ? `${FINCEPT.ORANGE}20` : 'transparent',
                      borderLeft: selectedDb?.path === db.path ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                      cursor: 'pointer',
                      marginBottom: '4px',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      if (selectedDb?.path !== db.path) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    }}
                    onMouseLeave={(e) => {
                      if (selectedDb?.path !== db.path) e.currentTarget.style.backgroundColor = 'transparent';
                    }}
                  >
                    <div style={{ fontSize: '10px', color: FINCEPT.WHITE, fontWeight: 600, marginBottom: '4px' }}>
                      {db.name}
                    </div>
                    <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>
                      {db.table_count} tables · {formatBytes(db.size_bytes)}
                    </div>
                  </div>
                ))}
                </div>
              </div>

              {/* Tables List */}
              {selectedDb && (
                <div style={{
                  width: '220px',
                  minWidth: '220px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  display: 'flex',
                  flexDirection: 'column',
                  overflow: 'hidden'
                }}>
                  <div style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    padding: '12px 12px 8px 12px',
                    borderBottom: `1px solid ${FINCEPT.BORDER}`,
                    flexShrink: 0
                  }}>
                    Tables ({tables.length})
                  </div>
                  <div style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
                  {tables.map(tbl => (
                    <div
                      key={tbl.name}
                      onClick={() => loadTableData(tbl)}
                      style={{
                        padding: '8px',
                        backgroundColor: selectedTable?.name === tbl.name ? `${FINCEPT.ORANGE}20` : 'transparent',
                        borderLeft: selectedTable?.name === tbl.name ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                        cursor: 'pointer',
                        marginBottom: '4px',
                        transition: 'all 0.2s'
                      }}
                      onMouseEnter={(e) => {
                        if (selectedTable?.name !== tbl.name) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                      }}
                      onMouseLeave={(e) => {
                        if (selectedTable?.name !== tbl.name) e.currentTarget.style.backgroundColor = 'transparent';
                      }}
                    >
                      <div style={{ fontSize: '9px', color: FINCEPT.WHITE, fontWeight: 600, marginBottom: '2px' }}>
                        {tbl.name}
                      </div>
                      <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>
                        {tbl.row_count} rows · {tbl.columns.length} cols
                      </div>
                    </div>
                  ))}
                  </div>
                </div>
              )}

              {/* Table Data */}
              {selectedTable && (
                <div style={{
                  flex: 1,
                  minWidth: 0,
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  display: 'flex',
                  flexDirection: 'column',
                  overflow: 'hidden'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', padding: '12px', borderBottom: `1px solid ${FINCEPT.BORDER}`, flexShrink: 0 }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <Table2 size={12} color={FINCEPT.ORANGE} />
                      <div style={{
                        fontSize: '9px',
                        fontWeight: 700,
                        color: FINCEPT.WHITE,
                        letterSpacing: '0.5px',
                        textTransform: 'uppercase'
                      }}>
                        {selectedTable.name} ({selectedTable.row_count.toLocaleString()} rows)
                      </div>
                      {loading && (
                        <Loader2 size={12} color={FINCEPT.ORANGE} style={{ animation: 'spin 1s linear infinite' }} />
                      )}
                    </div>
                    <div style={{ display: 'flex', gap: '8px' }}>
                      <button
                        onClick={() => loadTableData(selectedTable, true)}
                        disabled={loading}
                        style={{
                          padding: '6px 10px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${FINCEPT.BORDER}`,
                          color: FINCEPT.GRAY,
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: loading ? 'not-allowed' : 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                          letterSpacing: '0.5px',
                          textTransform: 'uppercase',
                          opacity: loading ? 0.5 : 1,
                          transition: 'all 0.2s'
                        }}
                        onMouseEnter={(e) => {
                          if (!loading) {
                            e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                            e.currentTarget.style.color = FINCEPT.WHITE;
                          }
                        }}
                        onMouseLeave={(e) => {
                          if (!loading) {
                            e.currentTarget.style.borderColor = FINCEPT.BORDER;
                            e.currentTarget.style.color = FINCEPT.GRAY;
                          }
                        }}
                      >
                        <RefreshCw size={10} />
                        Refresh
                      </button>
                      <button
                        onClick={() => {
                          setIsAddingRow(true);
                          setEditedData({});
                        }}
                        disabled={loading}
                        style={{
                          padding: '6px 10px',
                          backgroundColor: FINCEPT.GREEN,
                          color: FINCEPT.DARK_BG,
                          border: 'none',
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: loading ? 'not-allowed' : 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                          letterSpacing: '0.5px',
                          textTransform: 'uppercase',
                          opacity: loading ? 0.5 : 1,
                          transition: 'all 0.2s',
                          boxShadow: `0 2px 4px ${FINCEPT.GREEN}40`
                        }}
                        onMouseEnter={(e) => {
                          if (!loading) {
                            e.currentTarget.style.backgroundColor = FINCEPT.WHITE;
                            e.currentTarget.style.color = FINCEPT.GREEN;
                          }
                        }}
                        onMouseLeave={(e) => {
                          if (!loading) {
                            e.currentTarget.style.backgroundColor = FINCEPT.GREEN;
                            e.currentTarget.style.color = FINCEPT.DARK_BG;
                          }
                        }}
                      >
                        <Plus size={10} />
                        Add Row
                      </button>
                    </div>
                  </div>

                  {/* Table Container with Horizontal & Vertical Scroll */}
                  <div style={{
                    flex: 1,
                    overflow: 'auto',
                    position: 'relative',
                    minHeight: 0
                  }}>
                    <table style={{
                      minWidth: '100%',
                      fontSize: '9px',
                      borderCollapse: 'collapse',
                      tableLayout: 'auto'
                    }}>
                      <thead>
                        <tr>
                          <th style={{
                            padding: '8px',
                            backgroundColor: FINCEPT.HEADER_BG,
                            color: FINCEPT.GRAY,
                            textAlign: 'left',
                            position: 'sticky',
                            top: 0,
                            fontWeight: 700,
                            fontSize: '8px',
                            textTransform: 'uppercase',
                            zIndex: 10,
                            minWidth: '100px',
                            whiteSpace: 'nowrap'
                          }}>
                            Actions
                          </th>
                          {selectedTable.columns.map(col => (
                            <th key={col.name} style={{
                              padding: '8px',
                              backgroundColor: FINCEPT.HEADER_BG,
                              color: FINCEPT.GRAY,
                              textAlign: 'left',
                              position: 'sticky',
                              top: 0,
                              fontWeight: 700,
                              fontSize: '8px',
                              textTransform: 'uppercase',
                              zIndex: 10,
                              minWidth: '120px',
                              whiteSpace: 'nowrap'
                            }}>
                              {col.name}
                              {col.primary_key && <span style={{ color: FINCEPT.ORANGE, marginLeft: '4px' }}>PK</span>}
                            </th>
                          ))}
                        </tr>
                      </thead>
                      <tbody>
                        {isAddingRow && (
                          <tr style={{ backgroundColor: `${FINCEPT.GREEN}10` }}>
                            <td style={{ padding: '4px', whiteSpace: 'nowrap' }}>
                              <div style={{ display: 'flex', gap: '4px', minWidth: '100px' }}>
                                <button
                                  onClick={handleInsertRow}
                                  style={{
                                    padding: '4px',
                                    backgroundColor: FINCEPT.GREEN,
                                    color: FINCEPT.DARK_BG,
                                    border: 'none',
                                    borderRadius: '2px',
                                    cursor: 'pointer'
                                  }}
                                >
                                  <Save size={10} />
                                </button>
                                <button
                                  onClick={() => {
                                    setIsAddingRow(false);
                                    setEditedData({});
                                  }}
                                  style={{
                                    padding: '4px',
                                    backgroundColor: FINCEPT.RED,
                                    color: FINCEPT.WHITE,
                                    border: 'none',
                                    borderRadius: '2px',
                                    cursor: 'pointer'
                                  }}
                                >
                                  <X size={10} />
                                </button>
                              </div>
                            </td>
                            {selectedTable.columns.map(col => (
                              <td key={col.name} style={{ padding: '4px' }}>
                                <input
                                  type="text"
                                  value={editedData[col.name] ?? ''}
                                  onChange={(e) => setEditedData({ ...editedData, [col.name]: e.target.value })}
                                  placeholder={col.data_type}
                                  style={{
                                    minWidth: '120px',
                                    width: '100%',
                                    padding: '4px',
                                    backgroundColor: FINCEPT.DARK_BG,
                                    border: `1px solid ${FINCEPT.BORDER}`,
                                    color: FINCEPT.WHITE,
                                    fontSize: '9px',
                                    borderRadius: '2px',
                                    boxSizing: 'border-box'
                                  }}
                                />
                              </td>
                            ))}
                          </tr>
                        )}
                        {tableData.map((row, idx) => (
                          <tr key={idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                            <td style={{ padding: '4px', whiteSpace: 'nowrap' }}>
                              {editingRow === idx ? (
                                <div style={{ display: 'flex', gap: '4px', minWidth: '100px' }}>
                                  <button
                                    onClick={() => handleUpdateRow(idx)}
                                    style={{
                                      padding: '4px',
                                      backgroundColor: FINCEPT.GREEN,
                                      color: FINCEPT.DARK_BG,
                                      border: 'none',
                                      borderRadius: '2px',
                                      cursor: 'pointer'
                                    }}
                                  >
                                    <Save size={10} />
                                  </button>
                                  <button
                                    onClick={() => setEditingRow(null)}
                                    style={{
                                      padding: '4px',
                                      backgroundColor: FINCEPT.RED,
                                      color: FINCEPT.WHITE,
                                      border: 'none',
                                      borderRadius: '2px',
                                      cursor: 'pointer'
                                    }}
                                  >
                                    <X size={10} />
                                  </button>
                                </div>
                              ) : (
                                <div style={{ display: 'flex', gap: '4px', minWidth: '100px' }}>
                                  <button
                                    onClick={() => {
                                      setEditingRow(idx);
                                      setEditedData(row.data);
                                    }}
                                    style={{
                                      padding: '4px',
                                      backgroundColor: 'transparent',
                                      color: FINCEPT.ORANGE,
                                      border: `1px solid ${FINCEPT.ORANGE}`,
                                      borderRadius: '2px',
                                      cursor: 'pointer',
                                      flexShrink: 0
                                    }}
                                  >
                                    <Edit2 size={10} />
                                  </button>
                                  <button
                                    onClick={() => handleDeleteRow(row)}
                                    style={{
                                      padding: '4px',
                                      backgroundColor: 'transparent',
                                      color: FINCEPT.RED,
                                      border: `1px solid ${FINCEPT.RED}`,
                                      borderRadius: '2px',
                                      cursor: 'pointer',
                                      flexShrink: 0
                                    }}
                                  >
                                    <Trash2 size={10} />
                                  </button>
                                </div>
                              )}
                            </td>
                            {selectedTable.columns.map(col => (
                              <td key={col.name} style={{
                                padding: '8px',
                                color: FINCEPT.WHITE,
                                fontSize: '9px',
                                maxWidth: '300px',
                                overflow: 'hidden',
                                textOverflow: 'ellipsis',
                                whiteSpace: editingRow === idx ? 'normal' : 'nowrap'
                              }}>
                                {editingRow === idx ? (
                                  <input
                                    type="text"
                                    value={editedData[col.name] ?? ''}
                                    onChange={(e) => setEditedData({ ...editedData, [col.name]: e.target.value })}
                                    style={{
                                      minWidth: '120px',
                                      width: '100%',
                                      padding: '4px',
                                      backgroundColor: FINCEPT.DARK_BG,
                                      border: `1px solid ${FINCEPT.BORDER}`,
                                      color: FINCEPT.WHITE,
                                      fontSize: '9px',
                                      borderRadius: '2px',
                                      boxSizing: 'border-box'
                                    }}
                                  />
                                ) : (
                                  <span title={String(row.data[col.name])}>
                                    {String(row.data[col.name])}
                                  </span>
                                )}
                              </td>
                            ))}
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>

                  {/* Pagination & Info Bar */}
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    padding: '12px',
                    borderTop: `1px solid ${FINCEPT.BORDER}`,
                    backgroundColor: FINCEPT.HEADER_BG,
                    flexShrink: 0
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        Page {page + 1} of {Math.ceil(selectedTable.row_count / PAGE_SIZE)}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>|</div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        Showing {Math.min(page * PAGE_SIZE + 1, selectedTable.row_count)}-{Math.min((page + 1) * PAGE_SIZE, selectedTable.row_count)} of {selectedTable.row_count.toLocaleString()} rows
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>|</div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {selectedTable.columns.length} columns
                      </div>
                    </div>
                    <div style={{ display: 'flex', gap: '4px' }}>
                      <button
                        onClick={() => {
                          setPage(Math.max(0, page - 1));
                          loadTableData(selectedTable);
                        }}
                        disabled={page === 0}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: page === 0 ? 'transparent' : FINCEPT.ORANGE,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          color: page === 0 ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                          fontSize: '9px',
                          fontWeight: 700,
                          borderRadius: '2px',
                          cursor: page === 0 ? 'not-allowed' : 'pointer',
                          opacity: page === 0 ? 0.5 : 1,
                          letterSpacing: '0.5px',
                          textTransform: 'uppercase'
                        }}
                      >
                        Previous
                      </button>
                      <button
                        onClick={() => {
                          setPage(page + 1);
                          loadTableData(selectedTable);
                        }}
                        disabled={(page + 1) * PAGE_SIZE >= selectedTable.row_count}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: (page + 1) * PAGE_SIZE >= selectedTable.row_count ? 'transparent' : FINCEPT.ORANGE,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          color: (page + 1) * PAGE_SIZE >= selectedTable.row_count ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                          fontSize: '9px',
                          fontWeight: 700,
                          borderRadius: '2px',
                          cursor: (page + 1) * PAGE_SIZE >= selectedTable.row_count ? 'not-allowed' : 'pointer',
                          opacity: (page + 1) * PAGE_SIZE >= selectedTable.row_count ? 0.5 : 1,
                          letterSpacing: '0.5px',
                          textTransform: 'uppercase'
                        }}
                      >
                        Next
                      </button>
                    </div>
                  </div>
                </div>
              )}
              </div>
            </>
          )}
        </>
      )}

      {/* Password Modal */}
      {showPasswordModal && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999
        }}>
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `2px solid ${FINCEPT.ORANGE}`,
            borderRadius: '4px',
            padding: '24px',
            width: '400px',
            boxShadow: `0 4px 16px ${FINCEPT.ORANGE}40`
          }}>
            <div style={{
              fontSize: '11px',
              fontWeight: 700,
              color: FINCEPT.WHITE,
              letterSpacing: '0.5px',
              textTransform: 'uppercase',
              marginBottom: '16px',
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              <Lock size={16} color={FINCEPT.ORANGE} />
              {hasPassword ? 'Enter Database Password' : 'Set Database Password'}
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{
                fontSize: '9px',
                color: FINCEPT.GRAY,
                display: 'block',
                marginBottom: '6px',
                textTransform: 'uppercase',
                letterSpacing: '0.5px'
              }}>
                Password
              </label>
              <input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handlePasswordSubmit()}
                placeholder="Enter password"
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  color: FINCEPT.WHITE,
                  fontSize: '10px'
                }}
              />
            </div>

            {!hasPassword && (
              <div style={{ marginBottom: '16px' }}>
                <label style={{
                  fontSize: '9px',
                  color: FINCEPT.GRAY,
                  display: 'block',
                  marginBottom: '6px',
                  textTransform: 'uppercase',
                  letterSpacing: '0.5px'
                }}>
                  Confirm Password
                </label>
                <input
                  type="password"
                  value={confirmPassword}
                  onChange={(e) => setConfirmPassword(e.target.value)}
                  onKeyPress={(e) => e.key === 'Enter' && handlePasswordSubmit()}
                  placeholder="Confirm password"
                  style={{
                    width: '100%',
                    padding: '8px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    color: FINCEPT.WHITE,
                    fontSize: '10px'
                  }}
                />
              </div>
            )}

            {!hasPassword && (
              <div style={{
                padding: '10px',
                backgroundColor: `${FINCEPT.CYAN}10`,
                border: `1px solid ${FINCEPT.CYAN}40`,
                borderRadius: '2px',
                marginBottom: '16px'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, lineHeight: 1.5 }}>
                  <strong style={{ color: FINCEPT.CYAN }}>ℹ Security Notice:</strong><br />
                  This password will be stored securely using SHA-256 hashing and required to access the database manager. Minimum 6 characters required.
                </div>
              </div>
            )}

            {hasPassword && (
              <div style={{
                padding: '10px',
                backgroundColor: `${FINCEPT.YELLOW}10`,
                border: `1px solid ${FINCEPT.YELLOW}40`,
                borderRadius: '2px',
                marginBottom: '16px'
              }}>
                <div style={{ fontSize: '9px', color: FINCEPT.GRAY, lineHeight: 1.5 }}>
                  <strong style={{ color: FINCEPT.YELLOW }}>⚠ Warning:</strong><br />
                  You have full access to modify all database records. Changes are permanent and cannot be undone. Use with caution.
                </div>
              </div>
            )}

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '8px' }}>
              <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
                Press <kbd style={{ padding: '2px 4px', backgroundColor: FINCEPT.HEADER_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}>Enter</kbd> to submit
              </div>
              <div style={{ display: 'flex', gap: '8px' }}>
                <button
                  onClick={() => {
                    setShowPasswordModal(false);
                    setPassword('');
                    setConfirmPassword('');
                    if (!isAuthenticated) setActiveTab('cache');
                  }}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${FINCEPT.BORDER}`,
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    transition: 'all 0.2s'
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.borderColor = FINCEPT.RED;
                    e.currentTarget.style.color = FINCEPT.RED;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.color = FINCEPT.GRAY;
                  }}
                >
                  Cancel
                </button>
                <button
                  onClick={handlePasswordSubmit}
                  style={{
                    padding: '8px 16px',
                    backgroundColor: FINCEPT.ORANGE,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    transition: 'all 0.2s',
                    boxShadow: `0 2px 8px ${FINCEPT.ORANGE}40`
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.WHITE;
                    e.currentTarget.style.boxShadow = `0 4px 12px ${FINCEPT.ORANGE}60`;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = FINCEPT.ORANGE;
                    e.currentTarget.style.boxShadow = `0 2px 8px ${FINCEPT.ORANGE}40`;
                  }}
                >
                  {hasPassword ? '🔓 Unlock' : '🔒 Set Password'}
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
