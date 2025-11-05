import React, { useState, useEffect } from 'react';
import { Node, Edge } from 'reactflow';
import { Workflow, workflowService } from '@/services/workflowService';
import { Play, Trash2, Edit3, Eye, FileText, Clock, CheckCircle, AlertCircle } from 'lucide-react';

interface WorkflowManagerProps {
  onLoadWorkflow: (nodes: Node[], edges: Edge[], workflowId: string, workflow: Workflow) => void;
  onViewResults: (workflow: Workflow) => void;
  onEditDraft: (workflow: Workflow) => void;
}

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

  // Filter workflows
  const filteredWorkflows = workflows.filter(wf => {
    if (filter === 'all') return true;
    if (filter === 'draft') return wf.status === 'draft';
    if (filter === 'completed') return wf.status === 'completed';
    return true;
  });

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'completed': return <CheckCircle size={14} color="#10b981" />;
      case 'draft': return <FileText size={14} color="#f59e0b" />;
      case 'error': return <AlertCircle size={14} color="#ef4444" />;
      case 'running': return <Clock size={14} color="#3b82f6" />;
      default: return <FileText size={14} color="#6b7280" />;
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'completed': return '#10b981';
      case 'draft': return '#f59e0b';
      case 'error': return '#ef4444';
      case 'running': return '#3b82f6';
      default: return '#6b7280';
    }
  };

  return (
    <div style={{
      padding: '20px',
      color: '#a3a3a3',
      height: '100%',
      overflow: 'auto'
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '20px',
        paddingBottom: '12px',
        borderBottom: '2px solid #404040'
      }}>
        <h2 style={{
          margin: 0,
          fontSize: '20px',
          color: '#fff',
          fontWeight: 'bold'
        }}>
          📋 Workflow Manager
        </h2>
        <button
          onClick={loadWorkflows}
          style={{
            backgroundColor: '#3b82f6',
            color: '#fff',
            border: 'none',
            padding: '6px 12px',
            fontSize: '12px',
            fontWeight: 'bold',
            cursor: 'pointer',
            borderRadius: '4px'
          }}
        >
          🔄 REFRESH
        </button>
      </div>

      {/* Filter Tabs */}
      <div style={{
        display: 'flex',
        gap: '8px',
        marginBottom: '16px',
        borderBottom: '1px solid #404040',
        paddingBottom: '8px'
      }}>
        {['all', 'draft', 'completed'].map(f => (
          <button
            key={f}
            onClick={() => setFilter(f as any)}
            style={{
              backgroundColor: filter === f ? '#3b82f6' : 'transparent',
              color: filter === f ? '#fff' : '#a3a3a3',
              border: `1px solid ${filter === f ? '#3b82f6' : '#404040'}`,
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              borderRadius: '4px',
              textTransform: 'uppercase'
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
          color: '#6b7280'
        }}>
          Loading workflows...
        </div>
      )}

      {/* Empty State */}
      {!loading && filteredWorkflows.length === 0 && (
        <div style={{
          textAlign: 'center',
          padding: '40px',
          color: '#6b7280'
        }}>
          <FileText size={48} style={{ margin: '0 auto 16px', opacity: 0.5 }} />
          <p style={{ fontSize: '14px', marginBottom: '8px' }}>
            No {filter !== 'all' ? filter : ''} workflows found
          </p>
          <p style={{ fontSize: '12px', opacity: 0.7 }}>
            Create a new workflow in the Editor tab
          </p>
        </div>
      )}

      {/* Workflow List */}
      {!loading && filteredWorkflows.length > 0 && (
        <div style={{
          display: 'grid',
          gap: '12px'
        }}>
          {filteredWorkflows.map(workflow => (
            <div
              key={workflow.id}
              style={{
                backgroundColor: '#1a1a1a',
                border: '1px solid #404040',
                borderRadius: '6px',
                padding: '16px',
                transition: 'all 0.2s',
                cursor: 'pointer'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.borderColor = '#3b82f6';
                e.currentTarget.style.backgroundColor = '#252525';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = '#404040';
                e.currentTarget.style.backgroundColor = '#1a1a1a';
              }}
            >
              {/* Workflow Header */}
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'flex-start',
                marginBottom: '12px'
              }}>
                <div style={{ flex: 1 }}>
                  <div style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    marginBottom: '4px'
                  }}>
                    {getStatusIcon(workflow.status)}
                    <h3 style={{
                      margin: 0,
                      fontSize: '14px',
                      color: '#fff',
                      fontWeight: 'bold'
                    }}>
                      {workflow.name}
                    </h3>
                    <span style={{
                      backgroundColor: getStatusColor(workflow.status),
                      color: '#000',
                      padding: '2px 8px',
                      fontSize: '9px',
                      fontWeight: 'bold',
                      borderRadius: '3px',
                      textTransform: 'uppercase'
                    }}>
                      {workflow.status}
                    </span>
                  </div>
                  {workflow.description && (
                    <p style={{
                      margin: '4px 0 0 0',
                      fontSize: '11px',
                      color: '#6b7280',
                      lineHeight: '1.4'
                    }}>
                      {workflow.description}
                    </p>
                  )}
                </div>
              </div>

              {/* Workflow Info */}
              <div style={{
                display: 'flex',
                gap: '16px',
                marginBottom: '12px',
                fontSize: '11px',
                color: '#6b7280'
              }}>
                <span>📦 {workflow.nodes?.length || 0} nodes</span>
                <span>🔗 {workflow.edges?.length || 0} connections</span>
                <span style={{ marginLeft: 'auto', fontSize: '10px' }}>
                  ID: {workflow.id.substring(0, 12)}...
                </span>
              </div>

              {/* Action Buttons */}
              <div style={{
                display: 'flex',
                gap: '8px',
                paddingTop: '12px',
                borderTop: '1px solid #404040'
              }}>
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    handlePlay(workflow);
                  }}
                  style={{
                    flex: 1,
                    backgroundColor: '#10b981',
                    color: '#fff',
                    border: 'none',
                    padding: '6px 12px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    borderRadius: '4px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '4px'
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
                    backgroundColor: '#3b82f6',
                    color: '#fff',
                    border: 'none',
                    padding: '6px 12px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    borderRadius: '4px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '4px'
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
                      backgroundColor: '#f59e0b',
                      color: '#000',
                      border: 'none',
                      padding: '6px 12px',
                      fontSize: '11px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      borderRadius: '4px',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '4px'
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
                    backgroundColor: '#ef4444',
                    color: '#fff',
                    border: 'none',
                    padding: '6px 12px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: 'pointer',
                    borderRadius: '4px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center'
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
