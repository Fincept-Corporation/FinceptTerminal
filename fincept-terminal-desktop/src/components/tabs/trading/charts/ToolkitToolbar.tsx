/**
 * Trading Toolkit Toolbar
 *
 * Professional toolbar UI for accessing all drawing tools, similar to TradingView
 */

import React, { useState } from 'react';
import {
  TrendingUp,
  Minus,
  Circle,
  Square,
  Triangle,
  Type,
  ArrowUpRight,
  GitBranch,
  Maximize2,
  Ruler,
  Target,
  ArrowUp,
  ArrowDown,
  StopCircle,
  DollarSign,
  Settings,
  Trash2,
  Undo2,
  Lock,
  Unlock,
  Eye,
  EyeOff,
} from 'lucide-react';
import { DrawingTool, PositionType, TradingToolkitPlugin } from './plugins';
import { showConfirm } from '@/utils/notifications';

interface ToolkitToolbarProps {
  toolkit: TradingToolkitPlugin | null;
  onToolSelect?: (tool: DrawingTool) => void;
  activeTool: DrawingTool | null;
}

interface ToolGroup {
  name: string;
  icon: React.ReactNode;
  tools: ToolItem[];
}

interface ToolItem {
  id: DrawingTool;
  name: string;
  icon: React.ReactNode;
  shortcut?: string;
}

export function ToolkitToolbar({ toolkit, onToolSelect, activeTool }: ToolkitToolbarProps) {
  const [activeGroup, setActiveGroup] = useState<string | null>(null);
  const [showSettings, setShowSettings] = useState(false);

  const toolGroups: ToolGroup[] = [
    {
      name: 'Lines',
      icon: <TrendingUp size={16} />,
      tools: [
        {
          id: DrawingTool.TREND_LINE,
          name: 'Trend Line',
          icon: <TrendingUp size={14} />,
          shortcut: 'T',
        },
        {
          id: DrawingTool.HORIZONTAL_LINE,
          name: 'Horizontal Line',
          icon: <Minus size={14} />,
          shortcut: 'H',
        },
        {
          id: DrawingTool.VERTICAL_LINE,
          name: 'Vertical Line',
          icon: <Minus size={14} style={{ transform: 'rotate(90deg)' }} />,
          shortcut: 'V',
        },
        {
          id: DrawingTool.RAY,
          name: 'Ray',
          icon: <ArrowUpRight size={14} />,
        },
      ],
    },
    {
      name: 'Channels',
      icon: <GitBranch size={16} style={{ transform: 'rotate(90deg)' }} />,
      tools: [
        {
          id: DrawingTool.PARALLEL_CHANNEL,
          name: 'Parallel Channel',
          icon: <GitBranch size={14} style={{ transform: 'rotate(90deg)' }} />,
        },
        {
          id: DrawingTool.REGRESSION_CHANNEL,
          name: 'Regression Channel',
          icon: <GitBranch size={14} />,
        },
        {
          id: DrawingTool.PITCHFORK,
          name: "Andrew's Pitchfork",
          icon: <GitBranch size={14} />,
        },
      ],
    },
    {
      name: 'Shapes',
      icon: <Square size={16} />,
      tools: [
        {
          id: DrawingTool.RECTANGLE,
          name: 'Rectangle',
          icon: <Square size={14} />,
          shortcut: 'R',
        },
        {
          id: DrawingTool.ELLIPSE,
          name: 'Ellipse',
          icon: <Circle size={14} />,
          shortcut: 'E',
        },
        {
          id: DrawingTool.TRIANGLE,
          name: 'Triangle',
          icon: <Triangle size={14} />,
        },
      ],
    },
    {
      name: 'Fibonacci',
      icon: <Ruler size={16} />,
      tools: [
        {
          id: DrawingTool.FIBO_RETRACEMENT,
          name: 'Fibonacci Retracement',
          icon: <Ruler size={14} />,
          shortcut: 'F',
        },
        {
          id: DrawingTool.FIBO_EXTENSION,
          name: 'Fibonacci Extension',
          icon: <Ruler size={14} />,
        },
        {
          id: DrawingTool.FIBO_FAN,
          name: 'Fibonacci Fan',
          icon: <Ruler size={14} />,
        },
        {
          id: DrawingTool.FIBO_CIRCLE,
          name: 'Fibonacci Circle',
          icon: <Circle size={14} />,
        },
      ],
    },
    {
      name: 'Positions',
      icon: <Target size={16} />,
      tools: [
        {
          id: DrawingTool.LONG_POSITION,
          name: 'Long Position',
          icon: <ArrowUp size={14} />,
          shortcut: 'L',
        },
        {
          id: DrawingTool.SHORT_POSITION,
          name: 'Short Position',
          icon: <ArrowDown size={14} />,
          shortcut: 'S',
        },
        {
          id: DrawingTool.STOP_LOSS,
          name: 'Stop Loss',
          icon: <StopCircle size={14} />,
        },
        {
          id: DrawingTool.TAKE_PROFIT,
          name: 'Take Profit',
          icon: <DollarSign size={14} />,
        },
      ],
    },
    {
      name: 'Annotations',
      icon: <Type size={16} />,
      tools: [
        {
          id: DrawingTool.TEXT,
          name: 'Text',
          icon: <Type size={14} />,
          shortcut: 'A',
        },
        {
          id: DrawingTool.ARROW,
          name: 'Arrow',
          icon: <ArrowUpRight size={14} />,
        },
        {
          id: DrawingTool.CALLOUT,
          name: 'Callout',
          icon: <Type size={14} />,
        },
      ],
    },
    {
      name: 'Measure',
      icon: <Ruler size={16} />,
      tools: [
        {
          id: DrawingTool.MEASURE,
          name: 'Price & Date Range',
          icon: <Ruler size={14} />,
        },
        {
          id: DrawingTool.ANGLE,
          name: 'Angle',
          icon: <Ruler size={14} />,
        },
      ],
    },
  ];

  const handleToolSelect = (tool: DrawingTool) => {
    console.log('[ToolkitToolbar] Tool selected:', tool, 'hasToolkit:', !!toolkit);

    if (toolkit) {
      console.log('[ToolkitToolbar] Activating tool in toolkit plugin');
      toolkit.activateTool(tool);
    }

    console.log('[ToolkitToolbar] Calling onToolSelect callback');
    onToolSelect?.(tool);
    setActiveGroup(null);
  };

  const handleClearAll = async () => {
    if (toolkit) {
      const confirmed = await showConfirm('Clear all drawings and positions?', {
        title: 'Clear All',
        type: 'warning'
      });
      if (confirmed) {
        toolkit.clearAll();
      }
    }
  };

  const handleClearDrawings = async () => {
    if (toolkit) {
      const confirmed = await showConfirm('Clear all drawings?', {
        title: 'Clear Drawings',
        type: 'warning'
      });
      if (confirmed) {
        toolkit.clearDrawings();
      }
    }
  };

  const handleDeleteLast = () => {
    if (toolkit) {
      toolkit.deleteLastDrawing();
    }
  };

  const stats = toolkit
    ? {
        drawings: toolkit.getDrawings().length,
        positions: toolkit.getPositions().length,
      }
    : { drawings: 0, positions: 0 };

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '4px',
        padding: '4px 12px',
        backgroundColor: '#000000',
        borderBottom: '1px solid #2A2A2A',
        fontSize: '10px',
        position: 'relative',
        height: '28px',
      }}
    >
      {/* Tool Groups */}
      {toolGroups.map((group) => (
        <div key={group.name} style={{ position: 'relative' }}>
          <button
            onClick={() => setActiveGroup(activeGroup === group.name ? null : group.name)}
            title={group.name}
            style={{
              padding: '6px 8px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              border: 'none',
              borderRadius: '3px',
              backgroundColor: activeGroup === group.name ? '#FF8800' : '#1A1A1A',
              color: '#ffffff',
              cursor: 'pointer',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => {
              if (activeGroup !== group.name) {
                e.currentTarget.style.backgroundColor = '#2A2A2A';
              }
            }}
            onMouseLeave={(e) => {
              if (activeGroup !== group.name) {
                e.currentTarget.style.backgroundColor = '#1A1A1A';
              }
            }}
          >
            {group.icon}
            <span style={{ fontSize: '10px' }}>{group.name}</span>
          </button>

          {/* Dropdown menu */}
          {activeGroup === group.name && (
            <div
              style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                marginTop: '4px',
                backgroundColor: '#0F0F0F',
                border: '1px solid #2A2A2A',
                borderRadius: '4px',
                padding: '4px',
                minWidth: '200px',
                zIndex: 1000,
                boxShadow: '0 4px 12px rgba(0, 0, 0, 0.5)',
              }}
            >
              {group.tools.map((tool) => (
                <button
                  key={tool.id}
                  onClick={() => handleToolSelect(tool.id)}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between',
                    gap: '8px',
                    border: 'none',
                    borderRadius: '3px',
                    backgroundColor: activeTool === tool.id ? '#FF8800' : 'transparent',
                    color: '#ffffff',
                    cursor: 'pointer',
                    textAlign: 'left',
                    fontSize: '11px',
                    transition: 'all 0.15s',
                  }}
                  onMouseEnter={(e) => {
                    if (activeTool !== tool.id) {
                      e.currentTarget.style.backgroundColor = '#1A1A1A';
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (activeTool !== tool.id) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                    }
                  }}
                >
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    {tool.icon}
                    <span>{tool.name}</span>
                  </div>
                  {tool.shortcut && (
                    <span
                      style={{
                        fontSize: '9px',
                        color: '#6b7280',
                        backgroundColor: '#0a0a0a',
                        padding: '2px 6px',
                        borderRadius: '2px',
                      }}
                    >
                      {tool.shortcut}
                    </span>
                  )}
                </button>
              ))}
            </div>
          )}
        </div>
      ))}

      {/* Divider */}
      <div
        style={{
          width: '1px',
          height: '20px',
          backgroundColor: '#2a2a2a',
          margin: '0 4px',
        }}
      />

      {/* Quick Actions */}
      <button
        onClick={handleDeleteLast}
        title="Undo Last Drawing (Delete/Backspace)"
        disabled={stats.drawings === 0}
        style={{
          padding: '6px 8px',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          border: 'none',
          borderRadius: '3px',
          backgroundColor: stats.drawings === 0 ? '#0F0F0F' : '#1A1A1A',
          color: stats.drawings === 0 ? '#4A4A4A' : '#ffffff',
          cursor: stats.drawings === 0 ? 'not-allowed' : 'pointer',
          transition: 'all 0.2s',
          opacity: stats.drawings === 0 ? 0.5 : 1,
        }}
        onMouseEnter={(e) => {
          if (stats.drawings > 0) {
            e.currentTarget.style.backgroundColor = '#2A2A2A';
          }
        }}
        onMouseLeave={(e) => {
          if (stats.drawings > 0) {
            e.currentTarget.style.backgroundColor = '#1A1A1A';
          }
        }}
      >
        <Undo2 size={14} />
      </button>

      <button
        onClick={handleClearDrawings}
        title="Clear All Drawings"
        disabled={stats.drawings === 0}
        style={{
          padding: '6px 8px',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          border: 'none',
          borderRadius: '3px',
          backgroundColor: stats.drawings === 0 ? '#0F0F0F' : '#1A1A1A',
          color: stats.drawings === 0 ? '#4A4A4A' : '#ffffff',
          cursor: stats.drawings === 0 ? 'not-allowed' : 'pointer',
          transition: 'all 0.2s',
          opacity: stats.drawings === 0 ? 0.5 : 1,
        }}
        onMouseEnter={(e) => {
          if (stats.drawings > 0) {
            e.currentTarget.style.backgroundColor = '#2A2A2A';
          }
        }}
        onMouseLeave={(e) => {
          if (stats.drawings > 0) {
            e.currentTarget.style.backgroundColor = '#1A1A1A';
          }
        }}
      >
        <Trash2 size={14} />
      </button>

      {/* Stats */}
      <div
        style={{
          marginLeft: 'auto',
          display: 'flex',
          alignItems: 'center',
          gap: '12px',
          fontSize: '10px',
          color: '#787878',
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <TrendingUp size={12} />
          <span>{stats.drawings} drawings</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <Target size={12} />
          <span>{stats.positions} positions</span>
        </div>
      </div>

      {/* Active Tool Indicator */}
      {activeTool && (
        <div
          style={{
            padding: '4px 10px',
            backgroundColor: '#FF8800',
            color: '#000000',
            borderRadius: '3px',
            fontSize: '10px',
            fontWeight: 600,
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
          }}
        >
          <span>Active:</span>
          <span>{activeTool.replace(/_/g, ' ').toUpperCase()}</span>
          <button
            onClick={() => {
              toolkit?.deactivateTool();
              onToolSelect?.(null as any);
            }}
            style={{
              background: 'none',
              border: 'none',
              color: '#000000',
              cursor: 'pointer',
              padding: 0,
              marginLeft: '4px',
              fontSize: '14px',
              fontWeight: 700,
            }}
          >
            ✕
          </button>
        </div>
      )}
    </div>
  );
}
