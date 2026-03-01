import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Dataset } from '@/services/marketplace/marketplaceApi';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { showConfirm, showWarning, showError, showSuccess } from '@/utils/notifications';
import { FINCEPT, FONT_FAMILY, API_TIMEOUT_MS } from '../types';
import { Package, Edit, Trash2 } from 'lucide-react';

export const MyDatasetsScreen: React.FC<{ session: any; onRefresh: () => void }> = ({ session, onRefresh }) => {
  const [myDatasets, setMyDatasets] = useState<Dataset[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [editingDataset, setEditingDataset] = useState<Dataset | null>(null);
  const [editForm, setEditForm] = useState({
    title: '',
    description: '',
    category: '',
    price_tier: '',
    tags: ''
  });
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  const loadMyDatasets = useCallback(async () => {
    if (!session?.api_key) return;
    setIsLoading(true);
    try {
      const response = await withTimeout(
        MarketplaceApiService.getMyDatasets(session.api_key, 1, 100),
        API_TIMEOUT_MS,
        'Fetch my datasets timeout'
      );
      if (!mountedRef.current) return;
      if (response.success && response.data) {
        setMyDatasets(response.data.datasets || []);
      }
    } catch (err) {
      if (!mountedRef.current) return;
      console.error('Failed to fetch my datasets:', err);
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  }, [session?.api_key]);

  useEffect(() => {
    loadMyDatasets();
  }, [loadMyDatasets]);

  const handleEdit = useCallback((dataset: Dataset) => {
    setEditingDataset(dataset);
    setEditForm({
      title: dataset.title,
      description: dataset.description,
      category: dataset.category,
      price_tier: dataset.price_tier,
      tags: dataset.tags.join(', ')
    });
  }, []);

  const handleUpdate = useCallback(async () => {
    if (!session?.api_key || !editingDataset) return;

    // Sanitize inputs
    const sanitizedTitle = sanitizeInput(editForm.title);
    const sanitizedDescription = sanitizeInput(editForm.description);

    if (!sanitizedTitle || !sanitizedDescription) {
      showWarning('Title and description are required');
      return;
    }

    setIsLoading(true);
    try {
      const response = await withTimeout(
        MarketplaceApiService.updateDataset(session.api_key, editingDataset.id, {
          title: sanitizedTitle,
          description: sanitizedDescription,
          category: editForm.category,
          price_tier: editForm.price_tier,
          tags: editForm.tags.split(',').map(t => sanitizeInput(t.trim())).filter(t => t)
        }),
        API_TIMEOUT_MS,
        'Update dataset timeout'
      );
      if (!mountedRef.current) return;
      if (response.success) {
        showSuccess('Dataset updated successfully');
        setEditingDataset(null);
        loadMyDatasets();
        onRefresh();
      } else {
        showError('Update failed', [
          { label: 'ERROR', value: response.error || 'Unknown error' }
        ]);
      }
    } catch (err) {
      if (mountedRef.current) {
        showError('Update failed', [
          { label: 'ERROR', value: err instanceof Error ? err.message : 'Unknown error' }
        ]);
      }
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  }, [session?.api_key, editingDataset, editForm, loadMyDatasets, onRefresh]);

  const handleDelete = useCallback(async (datasetId: number, datasetTitle: string) => {
    if (!session?.api_key) return;
    const confirmed = await showConfirm(`This action cannot be undone.`, {
      title: `Delete "${datasetTitle}"?`,
      type: 'danger'
    });
    if (!confirmed) return;

    setIsLoading(true);
    try {
      const response = await withTimeout(
        MarketplaceApiService.deleteDataset(session.api_key, datasetId),
        API_TIMEOUT_MS,
        'Delete dataset timeout'
      );
      if (!mountedRef.current) return;
      if (response.success) {
        showSuccess('Dataset deleted successfully');
        loadMyDatasets();
        onRefresh();
      } else {
        showError('Delete failed', [
          { label: 'ERROR', value: response.error || 'Unknown error' }
        ]);
      }
    } catch (err) {
      if (mountedRef.current) {
        showError('Delete failed', [
          { label: 'ERROR', value: err instanceof Error ? err.message : 'Unknown error' }
        ]);
      }
    } finally {
      if (mountedRef.current) setIsLoading(false);
    }
  }, [session?.api_key, loadMyDatasets, onRefresh]);

  const getStatusColor = (datasetStatus: string) => {
    switch (status) {
      case 'approved': return FINCEPT.GREEN;
      case 'pending': return FINCEPT.YELLOW;
      case 'rejected': return FINCEPT.RED;
      default: return FINCEPT.GRAY;
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Package size={12} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            MY DATASETS ({myDatasets.length})
          </span>
        </div>
      </div>

      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {editingDataset && (
          <div style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `2px solid ${FINCEPT.ORANGE}`,
            borderRadius: '2px',
            padding: '16px',
            marginBottom: '16px',
            maxWidth: '800px',
            margin: '0 auto 16px'
          }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '16px', letterSpacing: '0.5px' }}>
              EDIT DATASET
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                TITLE
              </label>
              <input
                type="text"
                value={editForm.title}
                onChange={(e) => setEditForm({ ...editForm, title: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  outline: 'none'
                }}
              />
            </div>

            <div style={{ marginBottom: '12px' }}>
              <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                DESCRIPTION
              </label>
              <textarea
                value={editForm.description}
                onChange={(e) => setEditForm({ ...editForm, description: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  minHeight: '80px',
                  outline: 'none'
                }}
              />
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '16px' }}>
              <div>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  CATEGORY
                </label>
                <select
                  value={editForm.category}
                  onChange={(e) => setEditForm({ ...editForm, category: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: FONT_FAMILY
                  }}
                >
                  <option value="stocks">Stocks</option>
                  <option value="forex">Forex</option>
                  <option value="crypto">Cryptocurrency</option>
                  <option value="commodities">Commodities</option>
                  <option value="bonds">Bonds</option>
                  <option value="indices">Indices</option>
                  <option value="economic_data">Economic Data</option>
                </select>
              </div>
              <div>
                <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                  PRICE TIER
                </label>
                <select
                  value={editForm.price_tier}
                  onChange={(e) => setEditForm({ ...editForm, price_tier: e.target.value })}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.WHITE,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: FONT_FAMILY
                  }}
                >
                  <option value="free">Free</option>
                  <option value="basic">Basic</option>
                  <option value="premium">Premium</option>
                  <option value="enterprise">Enterprise</option>
                </select>
              </div>
            </div>

            <div style={{ marginBottom: '16px' }}>
              <label style={{ display: 'block', fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, marginBottom: '6px', letterSpacing: '0.5px' }}>
                TAGS
              </label>
              <input
                type="text"
                value={editForm.tags}
                onChange={(e) => setEditForm({ ...editForm, tags: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  outline: 'none'
                }}
              />
            </div>

            <div style={{ display: 'flex', gap: '8px' }}>
              <button
                onClick={handleUpdate}
                disabled={isLoading}
                style={{
                  padding: '8px 16px',
                  backgroundColor: isLoading ? FINCEPT.MUTED : FINCEPT.GREEN,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  fontFamily: FONT_FAMILY,
                  letterSpacing: '0.5px'
                }}
              >
                {isLoading ? 'SAVING...' : 'SAVE CHANGES'}
              </button>
              <button
                onClick={() => setEditingDataset(null)}
                style={{
                  padding: '8px 16px',
                  backgroundColor: 'transparent',
                  color: FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: FONT_FAMILY,
                  letterSpacing: '0.5px'
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        )}

        {isLoading && !editingDataset ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px'
          }}>
            Loading datasets...
          </div>
        ) : myDatasets.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '200px',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center'
          }}>
            <Package size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>No datasets uploaded yet</span>
          </div>
        ) : (
          <div style={{ maxWidth: '1000px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {myDatasets.map((dataset) => {
              const statusColor = getStatusColor(dataset.status || 'pending');
              return (
                <div key={dataset.id} style={{
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderLeft: `4px solid ${statusColor}`,
                  borderRadius: '2px',
                  padding: '16px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
                    <div style={{ flex: 1 }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '6px' }}>
                        <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
                          {dataset.title}
                        </div>
                        <div style={{
                          padding: '2px 6px',
                          backgroundColor: `${statusColor}20`,
                          color: statusColor,
                          fontSize: '8px',
                          fontWeight: 700,
                          borderRadius: '2px',
                          letterSpacing: '0.5px'
                        }}>
                          {(dataset.status || 'pending').toUpperCase()}
                        </div>
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
                        {dataset.category} • {dataset.price_tier} • {dataset.downloads_count} downloads • {new Date(dataset.uploaded_at).toLocaleDateString()}
                      </div>
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.5' }}>
                        {dataset.description}
                      </div>
                      {dataset.rejection_reason && (
                        <div style={{
                          marginTop: '12px',
                          padding: '8px',
                          backgroundColor: `${FINCEPT.RED}10`,
                          border: `1px solid ${FINCEPT.RED}`,
                          borderRadius: '2px'
                        }}>
                          <div style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.RED, marginBottom: '4px', letterSpacing: '0.5px' }}>
                            REJECTION REASON
                          </div>
                          <div style={{ fontSize: '9px', color: FINCEPT.RED }}>
                            {dataset.rejection_reason}
                          </div>
                        </div>
                      )}
                    </div>
                    <div style={{ display: 'flex', gap: '8px', marginLeft: '16px' }}>
                      {(dataset.status === 'pending' || dataset.status === 'rejected') && (
                        <button
                          onClick={() => handleEdit(dataset)}
                          style={{
                            padding: '6px 12px',
                            backgroundColor: 'transparent',
                            border: `1px solid ${FINCEPT.BLUE}`,
                            color: FINCEPT.BLUE,
                            borderRadius: '2px',
                            fontSize: '9px',
                            fontWeight: 700,
                            cursor: 'pointer',
                            fontFamily: FONT_FAMILY,
                            letterSpacing: '0.5px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px'
                          }}
                        >
                          <Edit size={10} />
                          EDIT
                        </button>
                      )}
                      <button
                        onClick={() => handleDelete(dataset.id, dataset.title)}
                        style={{
                          padding: '6px 12px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${FINCEPT.RED}`,
                          color: FINCEPT.RED,
                          borderRadius: '2px',
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: 'pointer',
                          fontFamily: FONT_FAMILY,
                          letterSpacing: '0.5px',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px'
                        }}
                      >
                        <Trash2 size={10} />
                        DELETE
                      </button>
                    </div>
                  </div>
                  {dataset.tags && dataset.tags.length > 0 && (
                    <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                      {dataset.tags.map((tag, idx) => (
                        <span key={idx} style={{
                          padding: '2px 6px',
                          backgroundColor: `${FINCEPT.PURPLE}20`,
                          color: FINCEPT.PURPLE,
                          fontSize: '8px',
                          fontWeight: 700,
                          borderRadius: '2px'
                        }}>
                          #{tag}
                        </span>
                      ))}
                    </div>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>
    </div>
  );
};
