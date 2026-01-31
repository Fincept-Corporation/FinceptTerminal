import React, { useState, useEffect } from 'react';
import { Node, Edge } from 'reactflow';
import { Workflow, workflowService } from '@/services/core/workflowService';
import { Play, Trash2, Edit3, Eye, FileText, Clock, CheckCircle, AlertCircle, RefreshCw, Search, X } from 'lucide-react';

interface WorkflowManagerProps {
  onLoadWorkflow: (nodes: Node[], edges: Edge[], workflowId: string, workflow: Workflow) => void;
  onViewResults: (workflow: Workflow) => void;
  onEditDraft: (workflow: Workflow) => void;
}

// FINCEPT Design System Colors
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

const WorkflowManager: React.FC<WorkflowManagerProps> = ({ onLoadWorkflow, onViewResults, onEditDraft }) => {
  const [workflows, setWorkflows] = useState<Workflow[]>([]);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState<'all' | 'draft' | 'completed'>('all');
  const [searchQuery, setSearchQuery] = useState('');

  useEffect(() => {
    loadWorkflows();
  }, []);

  const loadWorkflows = async () => {
    setLoading(true);
    try {
      const allWorkflows = await workflowService.listWorkflows();
      setWorkflows(allWorkflows);
    } catch (error) {
      console.error('[WorkflowManager] Failed to load workflows:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (workflowId: string) => {
    if (!confirm('Are you sure you want to delete this workflow?')) {
      return;
    }

    try {
      await workflowService.deleteWorkflow(workflowId);
      setWorkflows(prev => prev.filter(wf => wf.id !== workflowId));
    } catch (error) {
      console.error('[WorkflowManager] Failed to delete workflow:', error);
      alert('Failed to delete workflow');
    }
  };

  const handlePlay = (workflow: Workflow) => {
    onLoadWorkflow(workflow.nodes, workflow.edges, workflow.id, workflow);
  };

  const handleEdit = (workflow: Workflow) => {
    if (workflow.status === 'draft') {
      onEditDraft(workflow);
    } else {
      onLoadWorkflow(workflow.nodes, workflow.edges, workflow.id, workflow);
    }
  };

  const handleViewResults = (workflow: Workflow) => {
    onViewResults(workflow);
  };

  // Filter and search workflows
  const filteredWorkflows = workflows.filter(wf => {
    const matchesFilter = filter === 'all' || wf.status === filter;
    const matchesSearch = searchQuery.trim() === '' ||
      wf.name.toLowerCase().includes(searchQuery.toLowerCase()) ||
      (wf.description && wf.description.toLowerCase().includes(searchQuery.toLowerCase()));
    return matchesFilter && matchesSearch;
  });

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'completed': return <CheckCircle size={12} color={F.GREEN} />;
      case 'draft': return <FileText size={12} color={F.YELLOW} />;
      case 'error': return <AlertCircle size={12} color={F.RED} />;
      case 'running': return <Clock size={12} color={F.BLUE} />;
      default: return <FileText size={12} color={F.MUTED} />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'completed': return F.GREEN;
      case 'draft': return F.YELLOW;
      case 'error': return F.RED;
      case 'running': return F.BLUE;
      default: return F.MUTED;
    }
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: F.DARK_BG,
      color: F.WHITE,
      fontFamily: FONT,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Top Header Bar */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${F.ORANGE}20`,
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <FileText size={14} style={{ color: F.ORANGE }} />
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: F.ORANGE,
            letterSpacing: '0.5px',
          }}>
            WORKFLOW MANAGER
          </span>
        </div>
        <button
          onClick={loadWorkflows}
          disabled={loading}
          style={{
            padding: '6px 12px',
            backgroundColor: loading ? F.MUTED : F.ORANGE,
            color: F.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: loading ? 'not-allowed' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
            letterSpacing: '0.5px',
            opacity: loading ? 0.5 : 1,
          }}
        >
          <RefreshCw size={10} className={loading ? 'animate-spin' : ''} />
          REFRESH
        </button>
      </div>

      {/* Filter Bar */}
      <div style={{
        backgroundColor: F.PANEL_BG,
        borderBottom: `1px solid ${F.BORDER}`,
        padding: '8px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        flexShrink: 0,
      }}>
        {/* Filter Tabs */}
        {['all', 'draft', 'completed'].map(f => (
          <button
            key={f}
            onClick={() => setFilter(f as any)}
            style={{
              padding: '6px 12px',
              backgroundColor: filter === f ? F.ORANGE : 'transparent',
              color: filter === f ? F.DARK_BG : F.GRAY,
              border: 'none',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              borderRadius: '2px',
              letterSpacing: '0.5px',
              transition: 'all 0.2s',
            }}
          >
            {f.toUpperCase()} ({workflows.filter(wf => f === 'all' || wf.status === f).length})
          </button>
        ))}

        {/* Search */}
        <div style={{
          marginLeft: 'auto',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          backgroundColor: F.DARK_BG,
          border: `1px solid ${F.BORDER}`,
          borderRadius: '2px',
          padding: '4px 8px',
          flex: '0 1 220px',
        }}>
          <Search size={10} style={{ color: F.MUTED, flexShrink: 0 }} />
          <input
            type="text"
            value={searchQuery}
            onChange={e => setSearchQuery(e.target.value)}
            placeholder="Search workflows..."
            style={{
              flex: 1,
              backgroundColor: 'transparent',
              border: 'none',
              outline: 'none',
              color: F.WHITE,
              fontSize: '9px',
              fontFamily: FONT,
              padding: '2px 0',
            }}
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              style={{
                background: 'none',
                border: 'none',
                color: F.MUTED,
                cursor: 'pointer',
                padding: '0',
                display: 'flex',
                alignItems: 'center',
              }}
            >
              <X size={10} />
            </button>
          )}
        </div>
      </div>

      {/* Main Content Area */}
      <div style={{
        flex: 1,
        overflow: 'auto',
        padding: '12px',
      }}>
        {/* Loading State */}
        {loading && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: F.MUTED,
            fontSize: '10px',
          }}>
            <RefreshCw size={24} className="animate-spin" style={{ marginBottom: '12px', opacity: 0.5 }} />
            <span>LOADING WORKFLOWS...</span>
          </div>
        )}

        {/* Empty State */}
        {!loading && filteredWorkflows.length === 0 && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: F.MUTED,
            fontSize: '10px',
            textAlign: 'center',
          }}>
            <FileText size={32} style={{ marginBottom: '12px', opacity: 0.3 }} />
            <div style={{ fontSize: '11px', fontWeight: 700, marginBottom: '4px', letterSpacing: '0.5px' }}>
              NO {filter !== 'all' ? filter.toUpperCase() : ''} WORKFLOWS FOUND
            </div>
            <div style={{ fontSize: '9px', color: F.GRAY }}>
              {searchQuery ? 'Try adjusting your search query' : 'Create a new workflow in the Editor tab'}
            </div>
          </div>
        )}

        {/* Workflow List */}
        {!loading && filteredWorkflows.length > 0 && (
          <div style={{
            display: 'grid',
            gap: '8px',
          }}>
            {filteredWorkflows.map(workflow => (
              <WorkflowCard
                key={workflow.id}
                workflow={workflow}
                onPlay={handlePlay}
                onEdit={handleEdit}
                onViewResults={handleViewResults}
                onDelete={handleDelete}
                getStatusIcon={getStatusIcon}
                getStatusColor={getStatusColor}
              />
            ))}
          </div>
        )}
      </div>

      {/* Status Bar */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderTop: `1px solid ${F.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: F.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
      }}>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>TOTAL: <span style={{ color: F.CYAN, fontWeight: 700 }}>{workflows.length}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>SHOWING: <span style={{ color: F.CYAN, fontWeight: 700 }}>{filteredWorkflows.length}</span></span>
        </div>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>FILTER: <span style={{ color: F.ORANGE, fontWeight: 700 }}>{filter.toUpperCase()}</span></span>
        </div>
      </div>

      {/* Scrollbar Styles */}
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${F.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${F.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${F.MUTED}; }
      `}</style>
    </div>
  );
};

// Workflow Card Component
interface WorkflowCardProps {
  workflow: Workflow;
  onPlay: (workflow: Workflow) => void;
  onEdit: (workflow: Workflow) => void;
  onViewResults: (workflow: Workflow) => void;
  onDelete: (workflowId: string) => void;
  getStatusIcon: (status: string) => JSX.Element;
  getStatusColor: (status: string) => string;
}

const WorkflowCard: React.FC<WorkflowCardProps> = ({
  workflow,
  onPlay,
  onEdit,
  onViewResults,
  onDelete,
  getStatusIcon,
  getStatusColor,
}) => {
  const [hovered, setHovered] = useState(false);

  return (
    <div
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        backgroundColor: hovered ? F.HOVER : F.PANEL_BG,
        border: `1px solid ${hovered ? F.ORANGE : F.BORDER}`,
        borderLeft: `3px solid ${getStatusColor(workflow.status)}`,
        borderRadius: '2px',
        padding: '12px',
        transition: 'all 0.2s',
      }}
    >
      {/* Card Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'flex-start',
        marginBottom: '8px',
      }}>
        <div style={{ flex: 1 }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            marginBottom: '4px',
          }}>
            {getStatusIcon(workflow.status)}
            <span style={{
              fontSize: '11px',
              color: F.WHITE,
              fontWeight: 700,
            }}>
              {workflow.name}
            </span>
            <span style={{
              backgroundColor: `${getStatusColor(workflow.status)}20`,
              color: getStatusColor(workflow.status),
              padding: '2px 6px',
              fontSize: '8px',
              fontWeight: 700,
              borderRadius: '2px',
              letterSpacing: '0.3px',
              border: `1px solid ${getStatusColor(workflow.status)}40`,
            }}>
              {workflow.status.toUpperCase()}
            </span>
          </div>
          {workflow.description && (
            <div style={{
              fontSize: '9px',
              color: F.GRAY,
              lineHeight: '1.4',
              marginTop: '4px',
            }}>
              {workflow.description}
            </div>
          )}
        </div>
      </div>

      {/* Workflow Info */}
      <div style={{
        display: 'flex',
        gap: '12px',
        marginBottom: '8px',
        fontSize: '9px',
        color: F.GRAY,
        paddingTop: '8px',
        borderTop: `1px solid ${F.BORDER}`,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span style={{ color: F.MUTED }}>NODES:</span>
          <span style={{ color: F.CYAN, fontWeight: 700 }}>{workflow.nodes?.length || 0}</span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <span style={{ color: F.MUTED }}>EDGES:</span>
          <span style={{ color: F.CYAN, fontWeight: 700 }}>{workflow.edges?.length || 0}</span>
        </div>
        <div style={{ marginLeft: 'auto', fontSize: '8px', color: F.MUTED }}>
          ID: {workflow.id.substring(0, 8)}
        </div>
      </div>

      {/* Action Buttons */}
      <div style={{
        display: 'flex',
        gap: '4px',
        paddingTop: '8px',
        borderTop: `1px solid ${F.BORDER}`,
      }}>
        <button
          onClick={(e) => {
            e.stopPropagation();
            onPlay(workflow);
          }}
          style={{
            flex: 1,
            backgroundColor: F.GREEN,
            color: F.DARK_BG,
            border: 'none',
            padding: '6px 8px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            letterSpacing: '0.3px',
          }}
        >
          <Play size={10} />
          RUN
        </button>

        <button
          onClick={(e) => {
            e.stopPropagation();
            onEdit(workflow);
          }}
          style={{
            flex: 1,
            backgroundColor: F.BLUE,
            color: F.WHITE,
            border: 'none',
            padding: '6px 8px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            letterSpacing: '0.3px',
          }}
        >
          <Edit3 size={10} />
          EDIT
        </button>

        {workflow.results && (
          <button
            onClick={(e) => {
              e.stopPropagation();
              onViewResults(workflow);
            }}
            style={{
              flex: 1,
              backgroundColor: F.YELLOW,
              color: F.DARK_BG,
              border: 'none',
              padding: '6px 8px',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
              letterSpacing: '0.3px',
            }}
          >
            <Eye size={10} />
            VIEW
          </button>
        )}

        <button
          onClick={(e) => {
            e.stopPropagation();
            onDelete(workflow.id);
          }}
          style={{
            backgroundColor: 'transparent',
            color: F.RED,
            border: `1px solid ${F.BORDER}`,
            padding: '6px 8px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: 'pointer',
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            transition: 'all 0.2s',
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = `${F.RED}20`;
            e.currentTarget.style.borderColor = F.RED;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = 'transparent';
            e.currentTarget.style.borderColor = F.BORDER;
          }}
        >
          <Trash2 size={10} />
        </button>
      </div>
    </div>
  );
};

export default WorkflowManager;
