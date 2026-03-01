import React, { useState, useEffect, useCallback, useRef } from 'react';
import { MarketplaceApiService } from '@/services/marketplace/marketplaceApi';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { showWarning, showError, showSuccess } from '@/utils/notifications';
import { FINCEPT, FONT_FAMILY, API_TIMEOUT_MS } from '../types';
import { Upload, Check } from 'lucide-react';

export const UploadScreen: React.FC<{
  uploadFile: File | null;
  onFileChange: (file: File | null) => void;
  uploadMetadata: any;
  onMetadataChange: (metadata: any) => void;
  isLoading: boolean;
  session: any;
  onUploadComplete: () => void;
}> = ({ uploadFile, onFileChange, uploadMetadata, onMetadataChange, isLoading, session, onUploadComplete }) => {
  const [uploading, setUploading] = useState(false);
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  const handleUpload = useCallback(async () => {
    if (!session?.api_key || !uploadFile) {
      showWarning('Please select a file and fill in all required fields');
      return;
    }

    // Sanitize inputs
    const sanitizedTitle = sanitizeInput(uploadMetadata.title);
    const sanitizedDescription = sanitizeInput(uploadMetadata.description);

    if (!sanitizedTitle || !sanitizedDescription) {
      showWarning('Title and description are required');
      return;
    }

    setUploading(true);
    try {
      const response = await withTimeout(
        MarketplaceApiService.uploadDataset(
          session.api_key,
          uploadFile,
          {
            title: sanitizedTitle,
            description: sanitizedDescription,
            category: uploadMetadata.category,
            price_tier: uploadMetadata.price_tier,
            tags: uploadMetadata.tags ? uploadMetadata.tags.split(',').map((t: string) => sanitizeInput(t.trim())).filter((t: string) => t) : []
          }
        ),
        60000, // 60 second timeout for file uploads
        'Upload timeout'
      );

      if (!mountedRef.current) return;

      if (response.success) {
        showSuccess('Dataset uploaded successfully');
        onFileChange(null);
        onMetadataChange({
          title: '',
          description: '',
          category: 'stocks',
          price_tier: 'free',
          tags: ''
        });
        setTimeout(() => onUploadComplete(), 1000);
      } else {
        showError('Upload failed', [
          { label: 'ERROR', value: response.error || 'Unknown error' }
        ]);
      }
    } catch (err) {
      if (mountedRef.current) {
        showError('Upload failed', [
          { label: 'ERROR', value: err instanceof Error ? err.message : 'Unknown error' }
        ]);
      }
    } finally {
      if (mountedRef.current) setUploading(false);
    }
  }, [session?.api_key, uploadFile, uploadMetadata, onFileChange, onMetadataChange, onUploadComplete]);

  return (
    <div style={{ display: 'flex', height: '100%', overflow: 'hidden' }}>
      {/* Left Panel - Guidelines */}
      <div style={{
        width: '320px',
        backgroundColor: FINCEPT.PANEL_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden'
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            UPLOAD GUIDELINES
          </span>
        </div>
        <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              SUPPORTED FORMATS
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              CSV, JSON, XLSX, Parquet, HDF5
            </div>
          </div>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              FILE SIZE LIMIT
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              Maximum 500 MB per file
            </div>
          </div>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              APPROVAL PROCESS
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              All datasets undergo admin review before becoming publicly available. You'll be notified via email once approved.
            </div>
          </div>
          <div style={{ marginBottom: '16px' }}>
            <div style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, marginBottom: '8px' }}>
              PRICING TIERS
            </div>
            <div style={{ fontSize: '10px', color: FINCEPT.GRAY, lineHeight: '1.6' }}>
              • FREE: No cost<br />
              • BASIC: 10 credits<br />
              • PREMIUM: 50 credits<br />
              • ENTERPRISE: 100 credits
            </div>
          </div>
        </div>
      </div>

      {/* Center Panel - Upload Form */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`
        }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
            UPLOAD NEW DATASET
          </span>
        </div>
        <div className="marketplace-scroll" style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
          <div style={{ maxWidth: '600px', margin: '0 auto' }}>
            {/* Title Input */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                DATASET TITLE *
              </label>
              <input
                type="text"
                value={uploadMetadata.title}
                onChange={(e) => onMetadataChange({ ...uploadMetadata, title: e.target.value })}
                placeholder="Enter descriptive title..."
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
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            {/* Description Textarea */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                DESCRIPTION *
              </label>
              <textarea
                value={uploadMetadata.description}
                onChange={(e) => onMetadataChange({ ...uploadMetadata, description: e.target.value })}
                placeholder="Describe your dataset, its contents, and use cases..."
                style={{
                  width: '100%',
                  padding: '8px 10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  minHeight: '120px',
                  outline: 'none',
                  resize: 'vertical'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            {/* Category & Price Tier Row */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '16px' }}>
              <div>
                <label style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  CATEGORY *
                </label>
                <select
                  value={uploadMetadata.category}
                  onChange={(e) => onMetadataChange({ ...uploadMetadata, category: e.target.value })}
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
                  onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
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
                <label style={{
                  display: 'block',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  marginBottom: '6px',
                  letterSpacing: '0.5px'
                }}>
                  PRICE TIER *
                </label>
                <select
                  value={uploadMetadata.price_tier}
                  onChange={(e) => onMetadataChange({ ...uploadMetadata, price_tier: e.target.value })}
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
                  onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                  onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
                >
                  <option value="free">Free (0 credits)</option>
                  <option value="basic">Basic (10 credits)</option>
                  <option value="premium">Premium (50 credits)</option>
                  <option value="enterprise">Enterprise (100 credits)</option>
                </select>
              </div>
            </div>

            {/* Tags Input */}
            <div style={{ marginBottom: '16px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                TAGS (COMMA SEPARATED)
              </label>
              <input
                type="text"
                value={uploadMetadata.tags}
                onChange={(e) => onMetadataChange({ ...uploadMetadata, tags: e.target.value })}
                placeholder="stocks, equity, real-time, historical"
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
                onFocus={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = FINCEPT.BORDER}
              />
            </div>

            {/* File Upload */}
            <div style={{ marginBottom: '24px' }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '6px',
                letterSpacing: '0.5px'
              }}>
                DATASET FILE *
              </label>
              <div style={{
                border: `2px dashed ${uploadFile ? FINCEPT.GREEN : FINCEPT.BORDER}`,
                borderRadius: '2px',
                padding: '24px',
                textAlign: 'center',
                backgroundColor: FINCEPT.DARK_BG,
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
                onMouseEnter={(e) => e.currentTarget.style.borderColor = FINCEPT.ORANGE}
                onMouseLeave={(e) => e.currentTarget.style.borderColor = uploadFile ? FINCEPT.GREEN : FINCEPT.BORDER}
              >
                <input
                  type="file"
                  onChange={(e) => onFileChange(e.target.files?.[0] || null)}
                  style={{ display: 'none' }}
                  id="file-upload"
                />
                <label htmlFor="file-upload" style={{ cursor: 'pointer', display: 'block' }}>
                  {uploadFile ? (
                    <>
                      <Check size={24} color={FINCEPT.GREEN} style={{ marginBottom: '8px' }} />
                      <div style={{ fontSize: '10px', color: FINCEPT.WHITE, marginBottom: '4px' }}>
                        {uploadFile.name}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                        {(uploadFile.size / 1024 / 1024).toFixed(2)} MB
                      </div>
                    </>
                  ) : (
                    <>
                      <Upload size={24} color={FINCEPT.GRAY} style={{ marginBottom: '8px', opacity: 0.5 }} />
                      <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
                        Click to select file or drag and drop
                      </div>
                    </>
                  )}
                </label>
              </div>
            </div>

            {/* Upload Button */}
            <button
              onClick={handleUpload}
              disabled={uploading || isLoading || !uploadFile || !uploadMetadata.title || !uploadMetadata.description}
              style={{
                width: '100%',
                padding: '12px',
                backgroundColor: uploading || isLoading || !uploadFile || !uploadMetadata.title || !uploadMetadata.description ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: uploading || isLoading || !uploadFile || !uploadMetadata.title || !uploadMetadata.description ? 'not-allowed' : 'pointer',
                fontFamily: FONT_FAMILY,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px'
              }}
            >
              <Upload size={12} />
              {uploading ? 'UPLOADING...' : 'UPLOAD DATASET'}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
