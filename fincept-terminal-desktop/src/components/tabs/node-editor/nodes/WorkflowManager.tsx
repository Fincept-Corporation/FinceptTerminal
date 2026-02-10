import React, { useState, useEffect } from 'react';
import { Node, Edge } from 'reactflow';
import { Workflow, workflowService } from '@/services/core/workflowService';
import { Play, Trash2, Edit3, Eye, FileText, Clock, CheckCircle, AlertCircle } from 'lucide-react';
import { showConfirm, showError } from '@/utils/notifications';
import {
  FINCEPT,
  SPACING,
  BORDER_RADIUS,
  FONT_FAMILY,
  FONT_SIZE,
} from './shared';

interface WorkflowManagerProps {
  onLoadWorkflow: (nodes: Node[], edges: Edge[], workflowId: string, workflow: Workflow, shouldExecute?: boolean) => void;
  onViewResults: (workflow: Workflow) => void;
  onEditDraft: (workflow: Workflow) => void;
}

const getStatusIcon = (status: string) => {
  switch (status) {
    case 'completed': return <CheckCircle size={14} color={FINCEPT.GREEN} />;
    case 'draft': return <FileText size={14} color={FINCEPT.ORANGE} />;
    case 'error': return <AlertCircle size={14} color={FINCEPT.RED} />;
    case 'running': return <Clock size={14} color={FINCEPT.BLUE} />;
    default: return <FileText size={14} color={FINCEPT.GRAY} />;
  }
};

const getStatusColor = (status: string) => {
  switch (status) {
    case 'completed': return FINCEPT.GREEN;
    case 'draft': return FINCEPT.ORANGE;
    case 'error': return FINCEPT.RED;
    case 'running': return FINCEPT.BLUE;
    default: return FINCEPT.GRAY;
  }
};

const WorkflowManager: React.FC<WorkflowManagerProps> = ({ onLoadWorkflow, onViewResults, onEditDraft }) => {
  const [workflows, setWorkflows] = useState<Workflow[]>([]);
  const [loading, setLoading] = useState(true);
  const [filter, setFilter] = useState<'all' | 'draft' | 'completed'>('all');

  // Load workflows on mount
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
    const confirmed = await showConfirm(
      'This action cannot be undone.',
      {
        title: 'Delete this workflow?',
        type: 'danger'
      }
    );
    if (!confirmed) return;

    try {
      await workflowService.deleteWorkflow(workflowId);
      setWorkflows(prev => prev.filter(wf => wf.id !== workflowId));
    } catch (error) {
      console.error('[WorkflowManager] Failed to delete workflow:', error);
      showError('Failed to delete workflow');
    }
  };

  const handlePlay = (workflow: Workflow) => {
    onLoadWorkflow(workflow.nodes, workflow.edges, workflow.id, workflow, true);
  };

  const handleEdit = (workflow: Workflow) => {
    onEditDraft(workflow);
  };

  const handleViewResults = (workflow: Workflow) => {
    onViewResults(workflow);
  };

  // Filter workflows
  const filteredWorkflows = workflows.filter(wf => {
    if (filter === 'all') return true;
    if (filter === 'draft') return wf.status === 'draft';
    if (filter === 'completed') return wf.status === 'completed';
    return true;
  });

  return (
    <div style={{
      padding: SPACING.XXL,
      color: FINCEPT.GRAY,
      height: '100%',
      overflow: 'auto',
      fontFamily: FONT_FAMILY,
      backgroundColor: FINCEPT.DARK_BG,
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: SPACING.XXL,
        paddingBottom: SPACING.LG,
        borderBottom: `2px solid ${FINCEPT.BORDER}`,
      }}>
        <h2 style={{
          margin: 0,
          fontSize: FONT_SIZE.XXL,
          color: FINCEPT.WHITE,
          fontWeight: 700,
          fontFamily: FONT_FAMILY,
        }}>
          WORKFLOW MANAGER
        </h2>
        <button
          onClick={loadWorkflows}
          style={{
            backgroundColor: FINCEPT.BLUE,
            color: FINCEPT.WHITE,
            border: 'none',
            padding: `${SPACING.SM} ${SPACING.LG}`,
            fontSize: FONT_SIZE.XL,
            fontWeight: 700,
            cursor: 'pointer',
            borderRadius: BORDER_RADIUS.SM,
            fontFamily: FONT_FAMILY,
          }}
        >
          REFRESH
        </button>
      </div>

      {/* Filter Tabs */}
      <div style={{
        display: 'flex',
        gap: SPACING.MD,
        marginBottom: SPACING.XL,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        paddingBottom: SPACING.MD,
      }}>
        {['all', 'draft', 'completed'].map(f => (
          <button
            key={f}
            onClick={() => setFilter(f as any)}
            style={{
              backgroundColor: filter === f ? FINCEPT.BLUE : 'transparent',
              color: filter === f ? FINCEPT.WHITE : FINCEPT.GRAY,
              border: `1px solid ${filter === f ? FINCEPT.BLUE : FINCEPT.BORDER}`,
              padding: `${SPACING.SM} ${SPACING.XL}`,
              fontSize: FONT_SIZE.LG,
              fontWeight: 700,
              cursor: 'pointer',
              borderRadius: BORDER_RADIUS.SM,
              textTransform: 'uppercase',
              fontFamily: FONT_FAMILY,
            }}
          >
            {f} ({workflows.filter(wf => f === 'all' || wf.status === f).length})
          </button>
        ))}
      </div>

      {/* Loading State */}
      {loading && (
        <div style={{
          textAlign: 'center',
          padding: '40px',
          color: FINCEPT.GRAY,
        }}>
          Loading workflows...
        </div>
      )}

      {/* Empty State */}
      {!loading && filteredWorkflows.length === 0 && (
        <div style={{
          textAlign: 'center',
          padding: '40px',
          color: FINCEPT.GRAY,
        }}>
          <FileText size={48} style={{ margin: '0 auto 16px', opacity: 0.3 }} />
          <p style={{ fontSize: FONT_SIZE.XXL, marginBottom: SPACING.MD }}>
            No {filter !== 'all' ? filter : ''} workflows found
          </p>
          <p style={{ fontSize: FONT_SIZE.XL, opacity: 0.7 }}>
            Create a new workflow in the Editor tab
          </p>
        </div>
      )}

      {/* Workflow List */}
      {!loading && filteredWorkflows.length > 0 && (
        <div style={{
          display: 'grid',
          gap: SPACING.LG,
        }}>
          {filteredWorkflows.map(workflow => (
            <div
              key={workflow.id}
              style={{
                backgroundColor: FINCEPT.PANEL_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: BORDER_RADIUS.SM,
                padding: SPACING.XL,
                transition: 'all 0.2s',
                cursor: 'pointer',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.BLUE;
                e.currentTarget.style.backgroundColor = FINCEPT.HEADER_BG;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
                e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
              }}
            >
              {/* Workflow Header */}
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'flex-start',
                marginBottom: SPACING.LG,
              }}>
                <div style={{ flex: 1 }}>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: SPACING.MD,
                    marginBottom: SPACING.XS,
                  }}>
                    {getStatusIcon(workflow.status)}
                    <h3 style={{
                      margin: 0,
                      fontSize: FONT_SIZE.XXL,
                      color: FINCEPT.WHITE,
                      fontWeight: 700,
                      fontFamily: FONT_FAMILY,
                    }}>
                      {workflow.name}
                    </h3>
                    <span style={{
                      backgroundColor: getStatusColor(workflow.status),
                      color: FINCEPT.DARK_BG,
                      padding: `2px ${SPACING.MD}`,
                      fontSize: FONT_SIZE.SM,
                      fontWeight: 700,
                      borderRadius: BORDER_RADIUS.SM,
                      textTransform: 'uppercase',
                    }}>
                      {workflow.status}
                    </span>
                  </div>
                  {workflow.description && (
                    <p style={{
                      margin: `${SPACING.XS} 0 0 0`,
                      fontSize: FONT_SIZE.LG,
                      color: FINCEPT.GRAY,
                      lineHeight: '1.4',
                    }}>
                      {workflow.description}
                    </p>
                  )}
                </div>
              </div>

              {/* Workflow Info */}
              <div style={{
                display: 'flex',
                gap: SPACING.XL,
                marginBottom: SPACING.LG,
                fontSize: FONT_SIZE.LG,
                color: FINCEPT.GRAY,
              }}>
                <span>{workflow.nodes?.length || 0} nodes</span>
                <span>{workflow.edges?.length || 0} connections</span>
                <span style={{ marginLeft: 'auto', fontSize: FONT_SIZE.MD }}>
                  ID: {workflow.id.substring(0, 12)}...
                </span>
              </div>

              {/* Action Buttons */}
              <div style={{
                display: 'flex',
                gap: SPACING.MD,
                paddingTop: SPACING.LG,
                borderTop: `1px solid ${FINCEPT.BORDER}`,
              }}>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    handlePlay(workflow);
                  }}
                  style={{
                    flex: 1,
                    backgroundColor: FINCEPT.GREEN,
                    color: FINCEPT.DARK_BG,
                    border: 'none',
                    padding: `${SPACING.SM} ${SPACING.LG}`,
                    fontSize: FONT_SIZE.LG,
                    fontWeight: 700,
                    cursor: 'pointer',
                    borderRadius: BORDER_RADIUS.SM,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: SPACING.XS,
                    fontFamily: FONT_FAMILY,
                  }}
                >
                  <Play size={12} />
                  LOAD & RUN
                </button>

                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    handleEdit(workflow);
                  }}
                  style={{
                    flex: 1,
                    backgroundColor: FINCEPT.BLUE,
                    color: FINCEPT.WHITE,
                    border: 'none',
                    padding: `${SPACING.SM} ${SPACING.LG}`,
                    fontSize: FONT_SIZE.LG,
                    fontWeight: 700,
                    cursor: 'pointer',
                    borderRadius: BORDER_RADIUS.SM,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: SPACING.XS,
                    fontFamily: FONT_FAMILY,
                  }}
                >
                  <Edit3 size={12} />
                  EDIT
                </button>

                {workflow.results && (
                  <button
                    onClick={(e) => {
                      e.stopPropagation();
                      handleViewResults(workflow);
                    }}
                    style={{
                      backgroundColor: FINCEPT.ORANGE,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      padding: `${SPACING.SM} ${SPACING.LG}`,
                      fontSize: FONT_SIZE.LG,
                      fontWeight: 700,
                      cursor: 'pointer',
                      borderRadius: BORDER_RADIUS.SM,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: SPACING.XS,
                      fontFamily: FONT_FAMILY,
                    }}
                  >
                    <Eye size={12} />
                    RESULTS
                  </button>
                )}

                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    handleDelete(workflow.id);
                  }}
                  style={{
                    backgroundColor: FINCEPT.RED,
                    color: FINCEPT.WHITE,
                    border: 'none',
                    padding: `${SPACING.SM} ${SPACING.LG}`,
                    fontSize: FONT_SIZE.LG,
                    fontWeight: 700,
                    cursor: 'pointer',
                    borderRadius: BORDER_RADIUS.SM,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    fontFamily: FONT_FAMILY,
                  }}
                >
                  <Trash2 size={12} />
                </button>
              </div>
            </div>
          ))}
        </div>
      )}
    </div>
  );
};

export default WorkflowManager;
