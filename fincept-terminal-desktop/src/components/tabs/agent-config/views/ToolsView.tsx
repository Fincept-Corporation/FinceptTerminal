/**
 * ToolsView - Searchable tool browser with category expand/collapse and selection
 */

import React from 'react';
import { Wrench, Search, Check, Copy, ChevronRight, ChevronDown } from 'lucide-react';
import { FINCEPT } from '../types';
import type { AgentConfigState } from '../hooks/useAgentConfig';

interface ToolsViewProps {
  state: AgentConfigState;
}

export const ToolsView: React.FC<ToolsViewProps> = ({ state }) => {
  const {
    toolsInfo, toolSearch, setToolSearch,
    expandedCategories, selectedTools, setSelectedTools, copiedTool,
    toggleToolCategory, copyTool, toggleToolSelection,
  } = state;

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left - Selected Tools */}
      <div style={{ width: '280px', minWidth: '280px', backgroundColor: FINCEPT.PANEL_BG, borderRight: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', flexShrink: 0 }}>
        <div style={{ padding: '12px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>SELECTED ({selectedTools.length})</span>
          {selectedTools.length > 0 && (
            <button
              onClick={() => setSelectedTools([])}
              style={{ padding: '2px 6px', backgroundColor: 'transparent', border: `1px solid ${FINCEPT.RED}`, color: FINCEPT.RED, fontSize: '8px', cursor: 'pointer', borderRadius: '2px' }}
            >
              CLEAR
            </button>
          )}
        </div>
        <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
          {selectedTools.length === 0 ? (
            <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: FINCEPT.MUTED, fontSize: '10px', textAlign: 'center' }}>
              <Wrench size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
              <span>No tools selected</span>
            </div>
          ) : (
            selectedTools.map(tool => (
              <div
                key={tool}
                style={{ padding: '8px 10px', marginBottom: '4px', backgroundColor: `${FINCEPT.ORANGE}15`, border: `1px solid ${FINCEPT.ORANGE}40`, borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}
              >
                <span style={{ fontSize: '10px', color: FINCEPT.ORANGE }}>{tool}</span>
                <button
                  onClick={() => toggleToolSelection(tool)}
                  style={{ background: 'none', border: 'none', color: FINCEPT.RED, cursor: 'pointer', fontSize: '12px' }}
                >
                  ×
                </button>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Center - Tool Browser */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
        <div style={{ padding: '12px 16px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Wrench size={14} style={{ color: FINCEPT.ORANGE }} />
            <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>AVAILABLE TOOLS</span>
            {toolsInfo && (
              <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN, fontSize: '8px', borderRadius: '2px' }}>
                {toolsInfo.total_count}
              </span>
            )}
          </div>
          <div style={{ flex: 1, position: 'relative' }}>
            <Search size={12} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)', color: FINCEPT.GRAY }} />
            <input
              type="text"
              value={toolSearch}
              onChange={e => setToolSearch(e.target.value)}
              placeholder="Search tools..."
              style={{ width: '100%', padding: '6px 10px 6px 30px', backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '10px' }}
            />
          </div>
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
          {toolsInfo?.categories?.map(category => {
            const tools = (toolsInfo.tools as Record<string, string[]>)[category] || [];
            const filteredTools = toolSearch
              ? tools.filter(t => t.toLowerCase().includes(toolSearch.toLowerCase()))
              : tools;

            if (filteredTools.length === 0) return null;

            return (
              <div key={category} style={{ marginBottom: '8px', backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, borderRadius: '2px' }}>
                <button
                  onClick={() => toggleToolCategory(category)}
                  style={{ width: '100%', padding: '10px 12px', backgroundColor: 'transparent', border: 'none', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}
                >
                  <span style={{ fontSize: '10px', fontWeight: 600, color: FINCEPT.WHITE }}>{category}</span>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{filteredTools.length}</span>
                    {expandedCategories.has(category) ? <ChevronDown size={12} color={FINCEPT.GRAY} /> : <ChevronRight size={12} color={FINCEPT.GRAY} />}
                  </div>
                </button>
                {expandedCategories.has(category) && (
                  <div style={{ padding: '8px 12px 12px', borderTop: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                    {filteredTools.map(tool => (
                      <span
                        key={tool}
                        onClick={() => toggleToolSelection(tool)}
                        style={{ padding: '4px 8px', backgroundColor: selectedTools.includes(tool) ? FINCEPT.ORANGE : FINCEPT.DARK_BG, color: selectedTools.includes(tool) ? FINCEPT.DARK_BG : FINCEPT.WHITE, border: `1px solid ${selectedTools.includes(tool) ? FINCEPT.ORANGE : FINCEPT.BORDER}`, borderRadius: '2px', fontSize: '9px', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '6px' }}
                      >
                        {tool}
                        <button
                          onClick={e => { e.stopPropagation(); copyTool(tool); }}
                          style={{ background: 'none', border: 'none', cursor: 'pointer', color: selectedTools.includes(tool) ? FINCEPT.DARK_BG : FINCEPT.GRAY, padding: 0 }}
                        >
                          {copiedTool === tool ? <Check size={10} /> : <Copy size={10} />}
                        </button>
                      </span>
                    ))}
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
};
