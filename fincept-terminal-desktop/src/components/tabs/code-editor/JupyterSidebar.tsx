import React from 'react';
import {
  X, Trash2, Search, History, RotateCcw,
  Loader2, Download, Package,
} from 'lucide-react';

// ─── FINCEPT Design System ────────────────────────────────────────────────────
const F = {
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
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ─── Types ────────────────────────────────────────────────────────────────────
interface HistoryItem {
  id?: number;
  name: string;
  path: string | null;
  updated_at: string;
  cell_count: number;
}

interface JupyterSidebarProps {
  sidebarMode: 'history' | 'packages';
  onClose: () => void;
  // History
  historyItems: HistoryItem[];
  historySearch: string;
  onHistorySearchChange: (v: string) => void;
  onLoadFromHistory: (item: HistoryItem) => void;
  onDeleteHistoryItem: (id: number) => void;
  onClearHistory: () => void;
  // Packages
  packages: string[];
  packagesLoading: boolean;
  installInput: string;
  onInstallInputChange: (v: string) => void;
  onInstallPackage: () => void;
  onLoadPackages: () => void;
  installing: boolean;
}

// ─── Component ────────────────────────────────────────────────────────────────
export const JupyterSidebar: React.FC<JupyterSidebarProps> = ({
  sidebarMode,
  onClose,
  historyItems,
  historySearch,
  onHistorySearchChange,
  onLoadFromHistory,
  onDeleteHistoryItem,
  onClearHistory,
  packages,
  packagesLoading,
  installInput,
  onInstallInputChange,
  onInstallPackage,
  onLoadPackages,
  installing,
}) => {
  return (
    <div style={{
      width: '300px',
      backgroundColor: F.PANEL_BG,
      borderLeft: `1px solid ${F.BORDER}`,
      display: 'flex', flexDirection: 'column',
    }}>
      {/* Sidebar header */}
      <div style={{
        padding: '12px',
        backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', fontFamily: FONT }}>
          {sidebarMode === 'history' ? 'NOTEBOOK HISTORY' : 'PYTHON PACKAGES'}
        </span>
        <button onClick={onClose} style={{
          padding: '3px 6px', background: 'transparent', border: `1px solid ${F.BORDER}`,
          borderRadius: '2px', color: F.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center',
          transition: 'all 0.2s',
        }}>
          <X size={11} />
        </button>
      </div>

      {/* ── History Content ── */}
      {sidebarMode === 'history' && (
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Search input */}
          <div style={{ padding: '8px 12px' }}>
            <div style={{
              display: 'flex', alignItems: 'center', gap: '6px',
              padding: '8px 10px',
              backgroundColor: F.DARK_BG,
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
            }}>
              <Search size={11} style={{ color: F.GRAY }} />
              <input
                value={historySearch}
                onChange={(e) => onHistorySearchChange(e.target.value)}
                placeholder="Search notebooks..."
                style={{
                  flex: 1, background: 'none', border: 'none', outline: 'none',
                  color: F.WHITE, fontSize: '10px', fontFamily: FONT,
                }}
              />
            </div>
          </div>

          {/* History list */}
          <div className="nb-scroll" style={{ flex: 1, overflowY: 'auto', padding: '0 8px' }}>
            {historyItems.length === 0 ? (
              <div style={{
                display: 'flex', flexDirection: 'column',
                alignItems: 'center', justifyContent: 'center',
                height: '120px', color: F.MUTED, fontSize: '10px',
                textAlign: 'center', fontFamily: FONT,
              }}>
                <History size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>No notebooks saved yet</span>
              </div>
            ) : (
              historyItems.map((item) => (
                <div
                  key={item.id}
                  onClick={() => onLoadFromHistory(item)}
                  style={{
                    padding: '10px 12px', marginBottom: '4px',
                    backgroundColor: 'transparent',
                    borderLeft: '2px solid transparent',
                    borderRadius: '2px', cursor: 'pointer',
                    transition: 'all 0.2s',
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.backgroundColor = F.HOVER;
                    e.currentTarget.style.borderLeftColor = F.ORANGE;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.borderLeftColor = 'transparent';
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
                    <span style={{
                      fontSize: '10px', color: F.WHITE, fontFamily: FONT, fontWeight: 700,
                      flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
                    }}>
                      {item.name}
                    </span>
                    {item.id != null && (
                      <button
                        onClick={(e) => { e.stopPropagation(); onDeleteHistoryItem(item.id!); }}
                        style={{
                          background: 'none', border: 'none', color: F.MUTED, cursor: 'pointer',
                          padding: '2px', display: 'flex', transition: 'color 0.2s',
                        }}
                        onMouseEnter={(e) => { e.currentTarget.style.color = F.RED; }}
                        onMouseLeave={(e) => { e.currentTarget.style.color = F.MUTED; }}
                      >
                        <Trash2 size={10} />
                      </button>
                    )}
                  </div>
                  <div style={{
                    fontSize: '9px', color: F.GRAY, marginTop: '4px',
                    fontFamily: FONT, display: 'flex', gap: '8px',
                  }}>
                    <span style={{ color: F.CYAN }}>{item.cell_count} cells</span>
                    <span>{new Date(item.updated_at).toLocaleDateString()}</span>
                  </div>
                </div>
              ))
            )}
          </div>

          {/* Clear history */}
          <div style={{ padding: '8px 12px', borderTop: `1px solid ${F.BORDER}` }}>
            <button
              onClick={onClearHistory}
              style={{
                width: '100%', padding: '6px 10px', fontSize: '9px', fontFamily: FONT, fontWeight: 700,
                backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                borderRadius: '2px', color: F.GRAY, cursor: 'pointer', letterSpacing: '0.5px',
                display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.RED; }}
              onMouseLeave={(e) => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
            >
              <RotateCcw size={10} /> CLEAR HISTORY
            </button>
          </div>
        </div>
      )}

      {/* ── Packages Content ── */}
      {sidebarMode === 'packages' && (
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Install input */}
          <div style={{ padding: '8px 12px', display: 'flex', gap: '6px' }}>
            <input
              value={installInput}
              onChange={(e) => onInstallInputChange(e.target.value)}
              onKeyDown={(e) => { if (e.key === 'Enter') onInstallPackage(); }}
              placeholder="Package name..."
              style={{
                flex: 1, padding: '8px 10px',
                backgroundColor: F.DARK_BG, color: F.WHITE,
                border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                fontSize: '10px', fontFamily: FONT, outline: 'none',
              }}
              onFocus={(e) => { e.currentTarget.style.borderColor = F.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = F.BORDER; }}
            />
            <button
              onClick={onInstallPackage}
              disabled={installing || !installInput.trim()}
              style={{
                padding: '8px 16px',
                backgroundColor: (installing || !installInput.trim()) ? F.MUTED : F.ORANGE,
                color: F.DARK_BG, border: 'none', borderRadius: '2px',
                fontSize: '9px', fontWeight: 700, fontFamily: FONT,
                cursor: (installing || !installInput.trim()) ? 'not-allowed' : 'pointer',
                display: 'flex', alignItems: 'center', gap: '4px',
                letterSpacing: '0.5px',
              }}
            >
              {installing ? <Loader2 size={10} className="animate-spin" /> : <Download size={10} />}
              INSTALL
            </button>
          </div>

          {/* Package count */}
          {packages.length > 0 && (
            <div style={{
              padding: '4px 12px', borderBottom: `1px solid ${F.BORDER}`,
              fontSize: '9px', color: F.GRAY, fontFamily: FONT, letterSpacing: '0.5px',
            }}>
              <span style={{ color: F.CYAN }}>{packages.length}</span> PACKAGES INSTALLED
            </div>
          )}

          {/* Package list */}
          <div className="nb-scroll" style={{ flex: 1, overflowY: 'auto' }}>
            {packagesLoading ? (
              <div style={{
                display: 'flex', flexDirection: 'column',
                alignItems: 'center', justifyContent: 'center',
                height: '120px', color: F.MUTED, fontSize: '10px',
                fontFamily: FONT, gap: '8px',
              }}>
                <Loader2 size={20} className="animate-spin" style={{ color: F.ORANGE, opacity: 0.5 }} />
                <span style={{ letterSpacing: '0.5px' }}>LOADING PACKAGES...</span>
              </div>
            ) : packages.length === 0 ? (
              <div style={{
                display: 'flex', flexDirection: 'column',
                alignItems: 'center', justifyContent: 'center',
                height: '120px', color: F.MUTED, fontSize: '10px',
                textAlign: 'center', fontFamily: FONT,
              }}>
                <Package size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
                <span>Click refresh to load</span>
              </div>
            ) : (
              packages.map((pkg, idx) => {
                const [name, version] = pkg.split('==');
                return (
                  <div key={idx} style={{
                    padding: '6px 12px',
                    borderBottom: `1px solid ${F.BORDER}`,
                    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                    transition: 'background 0.2s',
                  }}
                    onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                    onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                  >
                    <span style={{ fontSize: '10px', color: F.WHITE, fontFamily: FONT }}>
                      {name}
                    </span>
                    {version && (
                      <span style={{ fontSize: '9px', color: F.CYAN, fontFamily: FONT }}>
                        {version}
                      </span>
                    )}
                  </div>
                );
              })
            )}
          </div>

          {/* Refresh */}
          <div style={{ padding: '8px 12px', borderTop: `1px solid ${F.BORDER}` }}>
            <button
              onClick={onLoadPackages}
              disabled={packagesLoading}
              style={{
                width: '100%', padding: '6px 10px', fontSize: '9px', fontFamily: FONT, fontWeight: 700,
                backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
                borderRadius: '2px', color: F.GRAY, cursor: packagesLoading ? 'not-allowed' : 'pointer',
                display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
                letterSpacing: '0.5px', transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.ORANGE; }}
              onMouseLeave={(e) => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
            >
              <RotateCcw size={10} /> REFRESH LIST
            </button>
          </div>
        </div>
      )}
    </div>
  );
};
