// MCP Tools Management - Manage both internal and external MCP tools
// Fincept UI Design System
import React, { useState, useEffect } from 'react';
import { Settings, ToggleLeft, ToggleRight, Search, CheckCircle, XCircle, AlertCircle } from 'lucide-react';
import { mcpToolService } from '@/services/mcp/mcpToolService';
import { INTERNAL_SERVER_ID } from '@/services/mcp/internal';

interface MCPToolWithStatus {
  serverId: string;
  serverName: string;
  name: string;
  description?: string;
  category?: string;
  isEnabled: boolean;
  isInternal: boolean;
}

interface MCPToolsManagementProps {
  onClose?: () => void;
}

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
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

// Tool categories with design-system-aligned colors
const TOOL_CATEGORIES = [
  { id: 'navigation', label: 'NAVIGATION', color: FINCEPT.GREEN },
  { id: 'trading', label: 'TRADING', color: FINCEPT.BLUE },
  { id: 'crypto-trading', label: 'CRYPTO TRADING', color: FINCEPT.YELLOW },
  { id: 'portfolio', label: 'PORTFOLIO', color: FINCEPT.PURPLE },
  { id: 'dashboard', label: 'DASHBOARD', color: FINCEPT.ORANGE },
  { id: 'settings', label: 'SETTINGS', color: FINCEPT.CYAN },
  { id: 'workspace', label: 'WORKSPACE', color: '#EC4899' },
  { id: 'backtesting', label: 'BACKTESTING', color: '#14B8A6' },
  { id: 'market-data', label: 'MARKET DATA', color: FINCEPT.CYAN },
  { id: 'workflow', label: 'WORKFLOW', color: '#84CC16' },
  { id: 'notes', label: 'NOTES & REPORTS', color: '#22D3EE' },
  { id: 'external', label: 'EXTERNAL TOOLS', color: FINCEPT.ORANGE },
];

const MCPToolsManagement: React.FC<MCPToolsManagementProps> = ({ onClose }) => {
  const [tools, setTools] = useState<MCPToolWithStatus[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string | null>(null);
  const [statusMessage, setStatusMessage] = useState('Loading tools...');

  useEffect(() => {
    loadTools();
  }, []);

  const loadTools = async () => {
    try {
      setLoading(true);
      setStatusMessage('Loading tools...');

      const internalTools = await mcpToolService.getAllInternalTools();
      const allTools = await mcpToolService.getAllTools();
      const enabledTools = new Set(allTools.map(t => `${t.serverId}__${t.name}`));

      const categorizedInternalTools: MCPToolWithStatus[] = internalTools.map(tool => {
        const key = `${tool.serverId}__${tool.name}`;
        const category = getToolCategory(tool.name);
        return {
          serverId: tool.serverId,
          serverName: tool.serverName,
          name: tool.name,
          description: tool.description,
          category,
          isEnabled: enabledTools.has(key),
          isInternal: true,
        };
      });

      const externalTools: MCPToolWithStatus[] = allTools
        .filter(t => t.serverId !== INTERNAL_SERVER_ID)
        .map(tool => ({
          serverId: tool.serverId,
          serverName: tool.serverName,
          name: tool.name,
          description: tool.description,
          category: 'external',
          isEnabled: true,
          isInternal: false,
        }));

      setTools([...categorizedInternalTools, ...externalTools]);
      setStatusMessage(`${categorizedInternalTools.length + externalTools.length} tools loaded`);
      setLoading(false);
    } catch (error) {
      console.error('Failed to load tools:', error);
      setStatusMessage('Error loading tools');
      setLoading(false);
    }
  };

  const getToolCategory = (toolName: string): string => {
    const categoryMap: Record<string, string> = {
      navigate_tab: 'navigation', navigate_screen: 'navigation', get_active_tab: 'navigation', open_settings_section: 'navigation',
      place_order: 'trading', cancel_order: 'trading', modify_order: 'trading', get_positions: 'trading', get_balance: 'trading', get_orders: 'trading', get_holdings: 'trading', close_position: 'trading',
      crypto_place_market_order: 'crypto-trading', crypto_place_limit_order: 'crypto-trading', crypto_place_stop_loss: 'crypto-trading', crypto_cancel_order: 'crypto-trading', crypto_cancel_all_orders: 'crypto-trading', crypto_get_positions: 'crypto-trading', crypto_close_position: 'crypto-trading', crypto_get_balance: 'crypto-trading', crypto_get_holdings: 'crypto-trading', crypto_get_orders: 'crypto-trading', crypto_get_ticker: 'crypto-trading', crypto_get_orderbook: 'crypto-trading', crypto_add_to_watchlist: 'crypto-trading', crypto_remove_from_watchlist: 'crypto-trading', crypto_search_symbol: 'crypto-trading', crypto_switch_trading_mode: 'crypto-trading', crypto_switch_broker: 'crypto-trading', crypto_get_portfolio_value: 'crypto-trading', crypto_get_pnl: 'crypto-trading', crypto_set_stop_loss_for_position: 'crypto-trading', crypto_set_take_profit: 'crypto-trading',
      create_portfolio: 'portfolio', list_portfolios: 'portfolio', add_asset: 'portfolio', remove_asset: 'portfolio', add_transaction: 'portfolio', get_portfolio_summary: 'portfolio',
      add_widget: 'dashboard', remove_widget: 'dashboard', list_widgets: 'dashboard', configure_widget: 'dashboard',
      set_theme: 'settings', get_theme: 'settings', set_language: 'settings', get_settings: 'settings', set_setting: 'settings',
      save_workspace: 'workspace', load_workspace: 'workspace', list_workspaces: 'workspace', delete_workspace: 'workspace',
      run_backtest: 'backtesting', optimize_strategy: 'backtesting', get_historical_data: 'backtesting',
      get_quote: 'market-data', add_to_watchlist: 'market-data', remove_from_watchlist: 'market-data', search_symbol: 'market-data',
      create_workflow: 'workflow', run_workflow: 'workflow', stop_workflow: 'workflow', list_workflows: 'workflow',
      create_note: 'notes', list_notes: 'notes', delete_note: 'notes', generate_report: 'notes',
    };
    return categoryMap[toolName] || 'other';
  };

  const handleToggleTool = async (tool: MCPToolWithStatus) => {
    if (!tool.isInternal) {
      alert('External tools are managed through their MCP servers. Stop the server to disable all its tools.');
      return;
    }

    try {
      const newState = !tool.isEnabled;
      setStatusMessage(`${newState ? 'Enabling' : 'Disabling'} ${tool.name}...`);
      await mcpToolService.setInternalToolEnabled(tool.name, tool.category || 'other', newState);
      setTools(prev => prev.map(t =>
        t.name === tool.name && t.serverId === tool.serverId
          ? { ...t, isEnabled: newState }
          : t
      ));
      setStatusMessage(`${tool.name} ${newState ? 'enabled' : 'disabled'}`);
    } catch (error) {
      console.error('Failed to toggle tool:', error);
      setStatusMessage(`Error toggling ${tool.name}`);
    }
  };

  const filteredTools = tools.filter(tool => {
    const matchesSearch = !searchTerm ||
      tool.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      tool.description?.toLowerCase().includes(searchTerm.toLowerCase());
    const matchesCategory = !selectedCategory || tool.category === selectedCategory;
    return matchesSearch && matchesCategory;
  });

  const toolsByCategory = filteredTools.reduce((acc, tool) => {
    const category = tool.category || 'other';
    if (!acc[category]) acc[category] = [];
    acc[category].push(tool);
    return acc;
  }, {} as Record<string, MCPToolWithStatus[]>);

  const getCategoryColor = (categoryId: string) => {
    const category = TOOL_CATEGORIES.find(c => c.id === categoryId);
    return category?.color || FINCEPT.GRAY;
  };

  const getCategoryLabel = (categoryId: string) => {
    const category = TOOL_CATEGORIES.find(c => c.id === categoryId);
    return category?.label || 'OTHER';
  };

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: FONT_FAMILY,
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
    }}>
      <style>{`
        .tools-scroll::-webkit-scrollbar { width: 6px; height: 6px; }
        .tools-scroll::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        .tools-scroll::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        .tools-scroll::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
      `}</style>

      {/* Header */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        padding: '12px 16px',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <Settings size={14} color={FINCEPT.ORANGE} />
            <span style={{
              color: FINCEPT.ORANGE,
              fontSize: '11px',
              fontWeight: 700,
              letterSpacing: '0.5px',
            }}>
              TOOLS MANAGEMENT
            </span>
            <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />
            <span style={{ color: FINCEPT.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
              {tools.filter(t => t.isEnabled).length} ENABLED | {tools.length} TOTAL
            </span>
          </div>

          {onClose && (
            <button
              onClick={onClose}
              style={{
                padding: '6px 10px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                transition: 'all 0.2s',
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
              CLOSE
            </button>
          )}
        </div>

        {/* Search and Filter Row */}
        <div style={{ display: 'flex', gap: '12px', alignItems: 'center' }}>
          <div style={{ flex: 1, position: 'relative' }}>
            <Search
              size={10}
              style={{
                position: 'absolute',
                left: '10px',
                top: '50%',
                transform: 'translateY(-50%)',
                color: FINCEPT.GRAY,
              }}
            />
            <input
              type="text"
              placeholder="Search tools..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px 8px 28px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: FONT_FAMILY,
                outline: 'none',
              }}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            />
          </div>

          <select
            value={selectedCategory || ''}
            onChange={(e) => setSelectedCategory(e.target.value || null)}
            style={{
              backgroundColor: FINCEPT.DARK_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.WHITE,
              padding: '8px 10px',
              fontSize: '10px',
              borderRadius: '2px',
              outline: 'none',
              cursor: 'pointer',
              fontFamily: FONT_FAMILY,
            }}
          >
            <option value="">ALL CATEGORIES</option>
            {TOOL_CATEGORIES.map(cat => (
              <option key={cat.id} value={cat.id}>{cat.label}</option>
            ))}
          </select>
        </div>
      </div>

      {/* Tools List */}
      <div className="tools-scroll" style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {loading ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
          }}>
            <Settings size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>Loading tools...</span>
          </div>
        ) : Object.keys(toolsByCategory).length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
          }}>
            <Settings size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No tools found</span>
          </div>
        ) : (
          Object.entries(toolsByCategory).map(([category, categoryTools]) => (
            <div key={category} style={{ marginBottom: '24px' }}>
              {/* Category Header */}
              <div style={{
                padding: '12px',
                backgroundColor: FINCEPT.HEADER_BG,
                borderBottom: `1px solid ${FINCEPT.BORDER}`,
                marginBottom: '8px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
              }}>
                <div style={{
                  width: '3px',
                  height: '12px',
                  backgroundColor: getCategoryColor(category),
                  borderRadius: '2px',
                }} />
                <span style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: getCategoryColor(category),
                  letterSpacing: '0.5px',
                }}>
                  {getCategoryLabel(category)} ({categoryTools.length})
                </span>
              </div>

              {/* Tools in Category */}
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(320px, 1fr))', gap: '8px' }}>
                {categoryTools.map(tool => (
                  <div
                    key={`${tool.serverId}__${tool.name}`}
                    style={{
                      backgroundColor: FINCEPT.PANEL_BG,
                      border: `1px solid ${tool.isEnabled ? getCategoryColor(category) + '40' : FINCEPT.BORDER}`,
                      borderRadius: '2px',
                      padding: '12px',
                      transition: 'all 0.2s',
                    }}
                    onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
                    onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG; }}
                  >
                    <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between' }}>
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
                          <span style={{
                            color: FINCEPT.WHITE,
                            fontSize: '10px',
                            fontWeight: 700,
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                            whiteSpace: 'nowrap',
                          }}>
                            {tool.name}
                          </span>
                          {tool.isInternal ? (
                            tool.isEnabled ? (
                              <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.GREEN}20`, color: FINCEPT.GREEN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>ON</span>
                            ) : (
                              <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.RED}20`, color: FINCEPT.RED, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>OFF</span>
                            )
                          ) : (
                            <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>EXT</span>
                          )}
                        </div>
                        <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginBottom: '2px' }}>
                          {tool.serverName}
                        </div>
                        {tool.description && (
                          <div style={{
                            color: FINCEPT.GRAY,
                            fontSize: '9px',
                            lineHeight: '1.4',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                            whiteSpace: 'nowrap',
                          }}>
                            {tool.description}
                          </div>
                        )}
                        {!tool.isInternal && (
                          <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '4px', fontStyle: 'italic' }}>
                            Managed by external server
                          </div>
                        )}
                      </div>

                      {tool.isInternal && (
                        <button
                          onClick={() => handleToggleTool(tool)}
                          style={{
                            backgroundColor: 'transparent',
                            border: 'none',
                            color: tool.isEnabled ? FINCEPT.GREEN : FINCEPT.MUTED,
                            cursor: 'pointer',
                            padding: '4px',
                            display: 'flex',
                            alignItems: 'center',
                            flexShrink: 0,
                            transition: 'all 0.2s',
                          }}
                          title={tool.isEnabled ? 'Click to disable' : 'Click to enable'}
                        >
                          {tool.isEnabled ? <ToggleRight size={20} /> : <ToggleLeft size={20} />}
                        </button>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          ))
        )}
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        flexShrink: 0,
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
      }}>
        <span style={{ color: FINCEPT.ORANGE }}>●</span>
        <span>{statusMessage}</span>
      </div>
    </div>
  );
};

export default MCPToolsManagement;
