import React, { useState } from 'react';
import { Dataset } from '@/services/marketplace/marketplaceApi';
import { useAuth } from '@/contexts/AuthContext';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { FINCEPT, FONT_FAMILY } from '../types';
import { StatBox } from '../SharedComponents';
import { showError, showSuccess } from '@/utils/notifications';
import {
  Search,
  Filter,
  RefreshCw,
  Package,
  Download,
  X
} from 'lucide-react';

// Dataset Card Component
const DatasetCard: React.FC<{ dataset: Dataset; selected: boolean; onClick: () => void }> = ({ dataset, selected, onClick }) => {
  const statusColor = dataset.status === 'approved' ? FINCEPT.GREEN : dataset.status === 'pending' ? FINCEPT.YELLOW : FINCEPT.RED;

  return (
    <div
      onClick={onClick}
      style={{
        backgroundColor: selected ? `${FINCEPT.ORANGE}10` : FINCEPT.PANEL_BG,
        border: selected ? `1px solid ${FINCEPT.ORANGE}` : `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
        cursor: 'pointer',
        transition: 'all 0.2s'
      }}
      onMouseEnter={(e) => {
        if (!selected) {
          e.currentTarget.style.borderColor = FINCEPT.ORANGE;
        }
      }}
      onMouseLeave={(e) => {
        if (!selected) {
          e.currentTarget.style.borderColor = FINCEPT.BORDER;
        }
      }}
    >
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
        <div style={{
          padding: '2px 6px',
          backgroundColor: `${FINCEPT.CYAN}20`,
          color: FINCEPT.CYAN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          letterSpacing: '0.5px'
        }}>
          {dataset.category.toUpperCase()}
        </div>
        {dataset.status && dataset.status !== 'approved' && (
          <div style={{
            padding: '2px 6px',
            backgroundColor: `${statusColor}20`,
            color: statusColor,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            letterSpacing: '0.5px'
          }}>
            {dataset.status.toUpperCase()}
          </div>
        )}
      </div>

      {/* Title */}
      <div style={{
        fontSize: '11px',
        fontWeight: 700,
        color: FINCEPT.WHITE,
        marginBottom: '4px',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        whiteSpace: 'nowrap'
      }}>
        {dataset.title}
      </div>

      {/* Author */}
      <div style={{
        fontSize: '9px',
        color: FINCEPT.GRAY,
        marginBottom: '8px'
      }}>
        by <span style={{ color: FINCEPT.CYAN }}>{dataset.uploader_username}</span>
      </div>

      {/* Description */}
      <div style={{
        fontSize: '10px',
        color: FINCEPT.GRAY,
        lineHeight: '1.4',
        marginBottom: '12px',
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        display: '-webkit-box',
        WebkitLineClamp: 2,
        WebkitBoxOrient: 'vertical'
      }}>
        {dataset.description}
      </div>

      {/* Stats */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '1fr 1fr 1fr',
        gap: '8px',
        padding: '8px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRadius: '2px'
      }}>
        <div style={{ textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '2px' }}>DOWNLOADS</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.CYAN }}>{dataset.downloads_count}</div>
        </div>
        <div style={{ textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '2px' }}>RATING</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.YELLOW }}>★ {dataset.rating.toFixed(1)}</div>
        </div>
        <div style={{ textAlign: 'center' }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '2px' }}>PRICE</div>
          <div style={{ fontSize: '11px', fontWeight: 700, color: dataset.price_credits > 0 ? FINCEPT.GREEN : FINCEPT.GRAY }}>
            {dataset.price_credits > 0 ? `${dataset.price_credits}c` : 'FREE'}
          </div>
        </div>
      </div>
    </div>
  );
};

// Dataset Details Component
const DatasetDetails: React.FC<{ dataset: Dataset; onClose: () => void }> = ({ dataset, onClose }) => {
  const { session } = useAuth();
  const [isDownloading, setIsDownloading] = useState(false);

  const handleDownload = async () => {
    if (!session?.api_key) return;
    setIsDownloading(true);
    try {
      const response = await MarketplaceApiService.downloadDataset(session.api_key, dataset.id);
      if (response.success && response.data?.download_url) {
        window.open(response.data.download_url, '_blank');
      } else {
        showError('Download failed');
      }
    } catch (error) {
      showError('Download failed');
    } finally {
      setIsDownloading(false);
    }
  };

  const handlePurchase = async () => {
    if (!session?.api_key) return;
    try {
      const response = await MarketplaceApiService.purchaseDataset(session.api_key, dataset.id);
      if (response.success) {
        showSuccess('Purchase successful');
      } else {
        showError('Purchase failed');
      }
    } catch (error) {
      showError('Purchase failed');
    }
  };

  return (
    <>
      {/* Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
          DATASET DETAILS
        </span>
        <button
          onClick={onClose}
          style={{
            padding: '4px',
            backgroundColor: 'transparent',
            border: 'none',
            cursor: 'pointer'
          }}
        >
          <X size={12} color={FINCEPT.GRAY} />
        </button>
      </div>

      {/* Content */}
      <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
        {/* Category Badge */}
        <div style={{
          padding: '2px 6px',
          backgroundColor: `${FINCEPT.CYAN}20`,
          color: FINCEPT.CYAN,
          fontSize: '8px',
          fontWeight: 700,
          borderRadius: '2px',
          display: 'inline-block',
          marginBottom: '12px',
          letterSpacing: '0.5px'
        }}>
          {dataset.category.toUpperCase()}
        </div>

        {/* Title */}
        <h3 style={{
          fontSize: '13px',
          fontWeight: 700,
          color: FINCEPT.WHITE,
          marginBottom: '4px',
          lineHeight: '1.4'
        }}>
          {dataset.title}
        </h3>

        {/* Author */}
        <div style={{
          fontSize: '9px',
          color: FINCEPT.GRAY,
          marginBottom: '16px'
        }}>
          by <span style={{ color: FINCEPT.CYAN }}>{dataset.uploader_username}</span>
        </div>

        {/* Price */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.DARK_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          marginBottom: '16px',
          textAlign: 'center'
        }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '4px' }}>PRICE</div>
          <div style={{
            fontSize: '20px',
            fontWeight: 700,
            color: dataset.price_credits > 0 ? FINCEPT.GREEN : FINCEPT.ORANGE
          }}>
            {dataset.price_credits > 0 ? `${dataset.price_credits} CREDITS` : 'FREE'}
          </div>
        </div>

        {/* Stats Grid */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr',
          gap: '8px',
          marginBottom: '16px'
        }}>
          <StatBox label="DOWNLOADS" value={dataset.downloads_count.toString()} color={FINCEPT.CYAN} />
          <StatBox label="RATING" value={`★ ${dataset.rating.toFixed(1)}`} color={FINCEPT.YELLOW} />
          <StatBox label="FILE SIZE" value={`${(dataset.file_size / 1024 / 1024).toFixed(2)} MB`} color={FINCEPT.GRAY} />
          <StatBox label="UPLOADED" value={new Date(dataset.uploaded_at).toLocaleDateString()} color={FINCEPT.GRAY} />
        </div>

        {/* Description */}
        <div style={{ marginBottom: '16px' }}>
          <div style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            marginBottom: '8px',
            letterSpacing: '0.5px'
          }}>
            DESCRIPTION
          </div>
          <div style={{
            fontSize: '10px',
            color: FINCEPT.WHITE,
            lineHeight: '1.6',
            backgroundColor: FINCEPT.DARK_BG,
            padding: '12px',
            borderRadius: '2px'
          }}>
            {dataset.description}
          </div>
        </div>

        {/* Tags */}
        {dataset.tags && dataset.tags.length > 0 && (
          <div style={{ marginBottom: '16px' }}>
            <div style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              marginBottom: '8px',
              letterSpacing: '0.5px'
            }}>
              TAGS
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
              {dataset.tags.map((tag, idx) => (
                <span key={idx} style={{
                  padding: '4px 8px',
                  backgroundColor: `${FINCEPT.PURPLE}20`,
                  color: FINCEPT.PURPLE,
                  fontSize: '9px',
                  fontWeight: 700,
                  borderRadius: '2px'
                }}>
                  #{tag}
                </span>
              ))}
            </div>
          </div>
        )}

        {/* Actions */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          {dataset.price_credits > 0 ? (
            <button
              onClick={handlePurchase}
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.GREEN,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px'
              }}
            >
              PURCHASE FOR {dataset.price_credits} CREDITS
            </button>
          ) : (
            <button
              onClick={handleDownload}
              disabled={isDownloading}
              style={{
                padding: '8px 16px',
                backgroundColor: FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: isDownloading ? 'not-allowed' : 'pointer',
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px',
                opacity: isDownloading ? 0.7 : 1,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px'
              }}
            >
              <Download size={10} />
              {isDownloading ? 'DOWNLOADING...' : 'DOWNLOAD FREE'}
            </button>
          )}
        </div>
      </div>
    </>
  );
};

// Browse Screen Component
export const BrowseScreen: React.FC<{
  datasets: Dataset[];
  categories: Array<{ name: string; count: number }>;
  selectedCategory: string;
  onCategorySelect: (cat: string) => void;
  searchQuery: string;
  onSearchChange: (q: string) => void;
  selectedDataset: Dataset | null;
  onDatasetSelect: (d: Dataset | null) => void;
  isLoading: boolean;
  onRefresh: () => void;
}> = ({ datasets, categories, selectedCategory, onCategorySelect, searchQuery, onSearchChange, selectedDataset, onDatasetSelect, isLoading, onRefresh }) => {
  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left Panel - Categories & Filters */}
      <div style={{
        width: '280px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        {/* Section Header */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Filter size={12} color={FINCEPT.ORANGE} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                CATEGORIES
              </span>
            </div>
            <button
              onClick={onRefresh}
              style={{
                padding: '4px',
                backgroundColor: 'transparent',
                border: 'none',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center'
              }}
            >
              <RefreshCw size={10} color={FINCEPT.GRAY} />
            </button>
          </div>
        </div>

        {/* Categories List */}
        <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto' }}>
          {categories.map((cat) => (
            <div
              key={cat.name}
              onClick={() => onCategorySelect(cat.name)}
              style={{
                padding: '10px 12px',
                backgroundColor: selectedCategory === cat.name ? `${FINCEPT.ORANGE}15` : 'transparent',
                borderLeft: selectedCategory === cat.name ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => {
                if (selectedCategory !== cat.name) {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (selectedCategory !== cat.name) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              <span style={{
                fontSize: '10px',
                color: selectedCategory === cat.name ? FINCEPT.WHITE : FINCEPT.GRAY,
                fontWeight: 700,
                letterSpacing: '0.5px'
              }}>
                {cat.name}
              </span>
              <span style={{
                fontSize: '9px',
                color: selectedCategory === cat.name ? FINCEPT.CYAN : FINCEPT.MUTED,
                fontWeight: 700
              }}>
                {cat.count}
              </span>
            </div>
          ))}
        </div>
      </div>

      {/* Center Panel - Dataset Grid */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {/* Search Bar */}
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <div style={{ position: 'relative' }}>
            <Search size={14} color={FINCEPT.GRAY} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} />
            <input
              type="text"
              placeholder="Search datasets..."
              value={searchQuery}
              onChange={(e) => onSearchChange(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px 8px 32px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: FONT_FAMILY,
                outline: 'none'
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
            />
          </div>
        </div>

        {/* Dataset Grid */}
        <div className="marketplace-scroll" style={{
          flex: 1,
          overflowY: 'auto',
          padding: '12px',
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(320px, 1fr))',
          gap: '12px',
          alignContent: 'start'
        }}>
          {isLoading ? (
            <div style={{
              gridColumn: '1 / -1',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              height: '200px',
              color: FINCEPT.MUTED,
              fontSize: '10px'
            }}>
              Loading datasets...
            </div>
          ) : datasets.length === 0 ? (
            <div style={{
              gridColumn: '1 / -1',
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
              <span>No datasets found</span>
            </div>
          ) : (
            datasets.map((dataset) => (
              <DatasetCard
                key={dataset.id}
                dataset={dataset}
                selected={selectedDataset?.id === dataset.id}
                onClick={() => onDatasetSelect(dataset)}
              />
            ))
          )}
        </div>
      </div>

      {/* Right Panel - Dataset Details */}
      <div style={{
        width: '320px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderLeft: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        {selectedDataset ? (
          <DatasetDetails dataset={selectedDataset} onClose={() => onDatasetSelect(null)} />
        ) : (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: FINCEPT.MUTED,
            fontSize: '10px',
            textAlign: 'center',
            padding: '16px'
          }}>
            <Package size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <span>Select a dataset to view details</span>
          </div>
        )}
      </div>
    </div>
  );
};
