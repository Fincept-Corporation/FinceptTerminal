// MCP Tools Management - Manage both internal and external MCP tools
import React, { useState, useEffect } from 'react';
import { Settings, ToggleLeft, ToggleRight, Search, Filter, CheckCircle, XCircle, AlertCircle } from 'lucide-react';
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

// Tool categories
const TOOL_CATEGORIES = [
  { id: 'navigation', label: 'Navigation', color: '#10b981' },
  { id: 'trading', label: 'Trading', color: '#3b82f6' },
  { id: 'crypto-trading', label: 'Crypto Trading', color: '#f59e0b' },
  { id: 'portfolio', label: 'Portfolio', color: '#8b5cf6' },
  { id: 'dashboard', label: 'Dashboard', color: '#f97316' },
  { id: 'settings', label: 'Settings', color: '#6366f1' },
  { id: 'workspace', label: 'Workspace', color: '#ec4899' },
  { id: 'backtesting', label: 'Backtesting', color: '#14b8a6' },
  { id: 'market-data', label: 'Market Data', color: '#06b6d4' },
  { id: 'workflow', label: 'Workflow', color: '#84cc16' },
  { id: 'notes', label: 'Notes & Reports', color: '#22d3ee' },
  { id: 'external', label: 'External Tools', color: '#FFA500' },
];

const MCPToolsManagement: React.FC<MCPToolsManagementProps> = ({ onClose }) => {
  const [tools, setTools] = useState<MCPToolWithStatus[]>([]);
  const [loading, setLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedCategory, setSelectedCategory] = useState<string | null>(null);
  const [statusMessage, setStatusMessage] = useState('Loading tools...');

  // Colors
  const ORANGE = '#FFA500';
  const WHITE = '#FFFFFF';
  const GRAY = '#787878';
  const DARK_BG = '#0a0a0a';
  const PANEL_BG = '#1a1a1a';
  const BORDER = '#2d2d2d';
  const GREEN = '#10b981';
  const RED = '#ef4444';

  useEffect(() => {
    loadTools();
  }, []);

  const loadTools = async () => {
    try {
      setLoading(true);
      setStatusMessage('Loading tools...');

      // Get all internal tools (including disabled)
      const internalTools = await mcpToolService.getAllInternalTools();

      // Get all tools (enabled only)
      const allTools = await mcpToolService.getAllTools();

      // Create a map to track which tools are enabled
      const enabledTools = new Set(allTools.map(t => `${t.serverId}__${t.name}`));

      // Categorize internal tools
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

      // Add external tools
      const externalTools: MCPToolWithStatus[] = allTools
        .filter(t => t.serverId !== INTERNAL_SERVER_ID)
        .map(tool => ({
          serverId: tool.serverId,
          serverName: tool.serverName,
          name: tool.name,
          description: tool.description,
          category: 'external',
          isEnabled: true, // External tools are always enabled if server is running
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
      // Navigation
      navigate_tab: 'navigation',
      navigate_screen: 'navigation',
      get_active_tab: 'navigation',
      open_settings_section: 'navigation',

      // Trading
      place_order: 'trading',
      cancel_order: 'trading',
      modify_order: 'trading',
      get_positions: 'trading',
      get_balance: 'trading',
      get_orders: 'trading',
      get_holdings: 'trading',
      close_position: 'trading',

      // Crypto Trading
      crypto_place_market_order: 'crypto-trading',
      crypto_place_limit_order: 'crypto-trading',
      crypto_place_stop_loss: 'crypto-trading',
      crypto_cancel_order: 'crypto-trading',
      crypto_cancel_all_orders: 'crypto-trading',
      crypto_get_positions: 'crypto-trading',
      crypto_close_position: 'crypto-trading',
      crypto_get_balance: 'crypto-trading',
      crypto_get_holdings: 'crypto-trading',
      crypto_get_orders: 'crypto-trading',
      crypto_get_ticker: 'crypto-trading',
      crypto_get_orderbook: 'crypto-trading',
      crypto_add_to_watchlist: 'crypto-trading',
      crypto_remove_from_watchlist: 'crypto-trading',
      crypto_search_symbol: 'crypto-trading',
      crypto_switch_trading_mode: 'crypto-trading',
      crypto_switch_broker: 'crypto-trading',
      crypto_get_portfolio_value: 'crypto-trading',
      crypto_get_pnl: 'crypto-trading',
      crypto_set_stop_loss_for_position: 'crypto-trading',
      crypto_set_take_profit: 'crypto-trading',

      // Portfolio
      create_portfolio: 'portfolio',
      list_portfolios: 'portfolio',
      add_asset: 'portfolio',
      remove_asset: 'portfolio',
      add_transaction: 'portfolio',
      get_portfolio_summary: 'portfolio',

      // Dashboard
      add_widget: 'dashboard',
      remove_widget: 'dashboard',
      list_widgets: 'dashboard',
      configure_widget: 'dashboard',

      // Settings
      set_theme: 'settings',
      get_theme: 'settings',
      set_language: 'settings',
      get_settings: 'settings',
      set_setting: 'settings',

      // Workspace
      save_workspace: 'workspace',
      load_workspace: 'workspace',
      list_workspaces: 'workspace',
      delete_workspace: 'workspace',

      // Backtesting
      run_backtest: 'backtesting',
      optimize_strategy: 'backtesting',
      get_historical_data: 'backtesting',

      // Market Data
      get_quote: 'market-data',
      add_to_watchlist: 'market-data',
      remove_from_watchlist: 'market-data',
      search_symbol: 'market-data',

      // Workflow
      create_workflow: 'workflow',
      run_workflow: 'workflow',
      stop_workflow: 'workflow',
      list_workflows: 'workflow',

      // Notes
      create_note: 'notes',
      list_notes: 'notes',
      delete_note: 'notes',
      generate_report: 'notes',
    };

    return categoryMap[toolName] || 'other';
  };

  const handleToggleTool = async (tool: MCPToolWithStatus) => {
    if (!tool.isInternal) {
      // External tools can't be disabled individually - must stop the server
      alert('External tools are managed through their MCP servers. Stop the server to disable all its tools.');
      return;
    }

    try {
      const newState = !tool.isEnabled;
      setStatusMessage(`${newState ? 'Enabling' : 'Disabling'} ${tool.name}...`);

      await mcpToolService.setInternalToolEnabled(tool.name, tool.category || 'other', newState);

      // Update local state
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

  // Filter tools
  const filteredTools = tools.filter(tool => {
    const matchesSearch = !searchTerm ||
      tool.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
      tool.description?.toLowerCase().includes(searchTerm.toLowerCase());

    const matchesCategory = !selectedCategory || tool.category === selectedCategory;

    return matchesSearch && matchesCategory;
  });

  // Group by category
  const toolsByCategory = filteredTools.reduce((acc, tool) => {
    const category = tool.category || 'other';
    if (!acc[category]) acc[category] = [];
    acc[category].push(tool);
    return acc;
  }, {} as Record<string, MCPToolWithStatus[]>);

  const getCategoryColor = (categoryId: string) => {
    const category = TOOL_CATEGORIES.find(c => c.id === categoryId);
    return category?.color || GRAY;
  };

  const getCategoryLabel = (categoryId: string) => {
    const category = TOOL_CATEGORIES.find(c => c.id === categoryId);
    return category?.label || 'Other';
  };

  return (
    <div style={{
      width: '100%',
      height: '100%',
      backgroundColor: DARK_BG,
      color: WHITE,
      fontFamily: 'Consolas, monospace',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${BORDER}`,
        padding: '12px 16px',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <Settings size={16} color={ORANGE} />
            <span style={{ color: ORANGE, fontSize: '14px', fontWeight: 'bold' }}>
              MCP TOOLS MANAGEMENT
            </span>
            <div style={{ width: '1px', height: '20px', backgroundColor: '#404040' }}></div>
            <span style={{ color: GRAY, fontSize: '11px' }}>
              {tools.filter(t => t.isEnabled).length} enabled | {tools.length} total
            </span>
          </div>

          {onClose && (
            <button
              onClick={onClose}
              style={{
                backgroundColor: 'transparent',
                border: `1px solid ${BORDER}`,
                color: WHITE,
                padding: '6px 12px',
                fontSize: '10px',
                cursor: 'pointer',
                borderRadius: '3px',
                fontWeight: 'bold',
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
              size={14}
              style={{
                position: 'absolute',
                left: '10px',
                top: '50%',
                transform: 'translateY(-50%)',
                color: GRAY,
              }}
            />
            <input
              type="text"
              placeholder="Search tools..."
              value={searchTerm}
              onChange={(e) => setSearchTerm(e.target.value)}
              style={{
                width: '100%',
                backgroundColor: DARK_BG,
                border: `1px solid ${BORDER}`,
                color: WHITE,
                padding: '8px 12px 8px 36px',
                fontSize: '11px',
                borderRadius: '4px',
                outline: 'none',
              }}
            />
          </div>

          <select
            value={selectedCategory || ''}
            onChange={(e) => setSelectedCategory(e.target.value || null)}
            style={{
              backgroundColor: DARK_BG,
              border: `1px solid ${BORDER}`,
              color: WHITE,
              padding: '8px 12px',
              fontSize: '11px',
              borderRadius: '4px',
              outline: 'none',
              cursor: 'pointer',
            }}
          >
            <option value="">All Categories</option>
            {TOOL_CATEGORIES.map(cat => (
              <option key={cat.id} value={cat.id}>{cat.label}</option>
            ))}
          </select>
        </div>
      </div>

      {/* Tools List */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {loading ? (
          <div style={{ textAlign: 'center', padding: '40px', color: GRAY }}>
            Loading tools...
          </div>
        ) : Object.keys(toolsByCategory).length === 0 ? (
          <div style={{ textAlign: 'center', padding: '40px', color: GRAY }}>
            No tools found
          </div>
        ) : (
          Object.entries(toolsByCategory).map(([category, categoryTools]) => (
            <div key={category} style={{ marginBottom: '24px' }}>
              {/* Category Header */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                marginBottom: '12px',
                paddingBottom: '8px',
                borderBottom: `1px solid ${BORDER}`,
              }}>
                <div
                  style={{
                    width: '4px',
                    height: '16px',
                    backgroundColor: getCategoryColor(category),
                    borderRadius: '2px',
                  }}
                />
                <span style={{
                  color: getCategoryColor(category),
                  fontSize: '12px',
                  fontWeight: 'bold',
                  textTransform: 'uppercase',
                }}>
                  {getCategoryLabel(category)} ({categoryTools.length})
                </span>
              </div>

              {/* Tools in Category */}
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(350px, 1fr))', gap: '12px' }}>
                {categoryTools.map(tool => (
                  <div
                    key={`${tool.serverId}__${tool.name}`}
                    style={{
                      backgroundColor: PANEL_BG,
                      border: `1px solid ${tool.isEnabled ? getCategoryColor(category) + '60' : BORDER}`,
                      borderRadius: '6px',
                      padding: '12px',
                      transition: 'all 0.2s',
                    }}
                  >
                    <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', marginBottom: '8px' }}>
                      <div style={{ flex: 1 }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                          <span style={{ color: WHITE, fontSize: '12px', fontWeight: 'bold' }}>
                            {tool.name}
                          </span>
                          {tool.isInternal ? (
                            tool.isEnabled ? (
                              <CheckCircle size={12} color={GREEN} />
                            ) : (
                              <XCircle size={12} color={RED} />
                            )
                          ) : (
                            <AlertCircle size={12} color={ORANGE} />
                          )}
                        </div>
                        <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>
                          {tool.serverName}
                        </div>
                        {tool.description && (
                          <div style={{ color: GRAY, fontSize: '10px', lineHeight: '1.4' }}>
                            {tool.description.length > 100
                              ? tool.description.substring(0, 100) + '...'
                              : tool.description}
                          </div>
                        )}
                      </div>

                      {tool.isInternal && (
                        <button
                          onClick={() => handleToggleTool(tool)}
                          style={{
                            backgroundColor: 'transparent',
                            border: 'none',
                            color: tool.isEnabled ? GREEN : GRAY,
                            cursor: 'pointer',
                            padding: '4px',
                            display: 'flex',
                            alignItems: 'center',
                            flexShrink: 0,
                          }}
                          title={tool.isEnabled ? 'Click to disable' : 'Click to enable'}
                        >
                          {tool.isEnabled ? <ToggleRight size={24} /> : <ToggleLeft size={24} />}
                        </button>
                      )}
                    </div>

                    {!tool.isInternal && (
                      <div style={{
                        fontSize: '8px',
                        color: GRAY,
                        fontStyle: 'italic',
                        marginTop: '4px',
                      }}>
                        Managed by external server
                      </div>
                    )}
                  </div>
                ))}
              </div>
            </div>
          ))
        )}
      </div>

      {/* Footer */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderTop: `1px solid ${BORDER}`,
        padding: '8px 16px',
        fontSize: '9px',
        color: GRAY,
        flexShrink: 0,
      }}>
        <span style={{ color: ORANGE }}>●</span> {statusMessage}
      </div>
    </div>
  );
};

export default MCPToolsManagement;
