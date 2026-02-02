// RSS Feed Settings Modal - Allows users to manage their RSS news sources
import React, { useState, useEffect } from 'react';
import { Plus, Trash2, Edit2, Check, X, ExternalLink, RefreshCw, AlertCircle } from 'lucide-react';
import { showConfirm } from '@/utils/notifications';
import {
  getUserRSSFeeds,
  getDefaultFeeds,
  addUserRSSFeed,
  updateUserRSSFeed,
  deleteUserRSSFeed,
  toggleUserRSSFeed,
  toggleDefaultRSSFeed,
  deleteDefaultRSSFeed,
  getDeletedDefaultFeeds,
  restoreAllDefaultFeeds,
  testRSSFeedUrl,
  RSS_CATEGORIES,
  RSS_REGIONS,
  getCategoryLabel,
  getRegionLabel,
  isValidRSSUrl,
  type UserRSSFeed,
  type RSSFeed,
} from '@/services/news/rssFeedService';

// FINCEPT Design System Colors
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

interface RSSFeedSettingsModalProps {
  isOpen: boolean;
  onClose: () => void;
  onFeedsUpdated?: () => void;
}

const RSSFeedSettingsModal: React.FC<RSSFeedSettingsModalProps> = ({
  isOpen,
  onClose,
  onFeedsUpdated,
}) => {
  const [activeTab, setActiveTab] = useState<'user' | 'default'>('user');
  const [userFeeds, setUserFeeds] = useState<UserRSSFeed[]>([]);
  const [defaultFeeds, setDefaultFeeds] = useState<RSSFeed[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const [showAddForm, setShowAddForm] = useState(false);
  const [editingFeed, setEditingFeed] = useState<UserRSSFeed | null>(null);
  const [formData, setFormData] = useState({
    name: '',
    url: '',
    category: 'GENERAL',
    region: 'GLOBAL',
    source_name: '',
  });
  const [formError, setFormError] = useState<string | null>(null);
  const [testing, setTesting] = useState(false);
  const [testResult, setTestResult] = useState<boolean | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const [deletedDefaultCount, setDeletedDefaultCount] = useState(0);

  useEffect(() => {
    if (isOpen) {
      loadFeeds();
    }
  }, [isOpen]);

  const loadFeeds = async () => {
    setLoading(true);
    setError(null);
    try {
      const [user, defaults, deletedIds] = await Promise.all([
        getUserRSSFeeds(),
        getDefaultFeeds(),
        getDeletedDefaultFeeds(),
      ]);
      setUserFeeds(user);
      setDefaultFeeds(defaults);
      setDeletedDefaultCount(deletedIds.length);
    } catch (err) {
      setError('Failed to load RSS feeds');
      console.error('[RSSFeedSettings] Failed to load feeds:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleTestUrl = async () => {
    if (!isValidRSSUrl(formData.url)) {
      setFormError('Please enter a valid URL');
      return;
    }
    setTesting(true);
    setTestResult(null);
    try {
      const result = await testRSSFeedUrl(formData.url);
      setTestResult(result);
      if (!result) {
        setFormError('URL does not appear to be a valid RSS feed');
      } else {
        setFormError(null);
      }
    } catch (err) {
      setTestResult(false);
      setFormError('Failed to test URL');
    } finally {
      setTesting(false);
    }
  };

  const handleAddFeed = async () => {
    if (!formData.name.trim()) {
      setFormError('Name is required');
      return;
    }
    if (!isValidRSSUrl(formData.url)) {
      setFormError('Please enter a valid URL');
      return;
    }
    if (!formData.source_name.trim()) {
      setFormError('Source name is required');
      return;
    }

    setSubmitting(true);
    setFormError(null);
    try {
      await addUserRSSFeed({
        name: formData.name.trim(),
        url: formData.url.trim(),
        category: formData.category,
        region: formData.region,
        source_name: formData.source_name.trim().toUpperCase(),
      });
      await loadFeeds();
      resetForm();
      onFeedsUpdated?.();
    } catch (err: any) {
      setFormError(err?.message || 'Failed to add feed');
    } finally {
      setSubmitting(false);
    }
  };

  const handleUpdateFeed = async () => {
    if (!editingFeed) return;
    if (!formData.name.trim()) {
      setFormError('Name is required');
      return;
    }
    if (!isValidRSSUrl(formData.url)) {
      setFormError('Please enter a valid URL');
      return;
    }
    if (!formData.source_name.trim()) {
      setFormError('Source name is required');
      return;
    }

    setSubmitting(true);
    setFormError(null);
    try {
      await updateUserRSSFeed(editingFeed.id, {
        name: formData.name.trim(),
        url: formData.url.trim(),
        category: formData.category,
        region: formData.region,
        source_name: formData.source_name.trim().toUpperCase(),
        enabled: editingFeed.enabled,
      });
      await loadFeeds();
      resetForm();
      onFeedsUpdated?.();
    } catch (err: any) {
      setFormError(err?.message || 'Failed to update feed');
    } finally {
      setSubmitting(false);
    }
  };

  const handleDeleteFeed = async (id: string) => {
    const confirmed = await showConfirm(
      'This action cannot be undone.',
      { title: 'Delete this RSS feed?', type: 'danger' }
    );
    if (!confirmed) return;
    try {
      await deleteUserRSSFeed(id);
      await loadFeeds();
      onFeedsUpdated?.();
    } catch (err) {
      console.error('[RSSFeedSettings] Failed to delete feed:', err);
    }
  };

  const handleToggleFeed = async (id: string, enabled: boolean) => {
    try {
      await toggleUserRSSFeed(id, enabled);
      setUserFeeds(prev =>
        prev.map(f => (f.id === id ? { ...f, enabled } : f))
      );
      onFeedsUpdated?.();
    } catch (err) {
      console.error('[RSSFeedSettings] Failed to toggle feed:', err);
    }
  };

  const handleToggleDefaultFeed = async (feedId: string, enabled: boolean) => {
    try {
      await toggleDefaultRSSFeed(feedId, enabled);
      setDefaultFeeds(prev =>
        prev.map(f => (f.id === feedId ? { ...f, enabled } : f))
      );
      onFeedsUpdated?.();
    } catch (err) {
      console.error('[RSSFeedSettings] Failed to toggle default feed:', err);
    }
  };

  const handleDeleteDefaultFeed = async (feedId: string) => {
    try {
      await deleteDefaultRSSFeed(feedId);
      setDefaultFeeds(prev => prev.filter(f => f.id !== feedId));
      setDeletedDefaultCount(prev => prev + 1);
      onFeedsUpdated?.();
    } catch (err) {
      console.error('[RSSFeedSettings] Failed to delete default feed:', err);
    }
  };

  const handleRestoreAllDefaults = async () => {
    try {
      await restoreAllDefaultFeeds();
      await loadFeeds();
      onFeedsUpdated?.();
    } catch (err) {
      console.error('[RSSFeedSettings] Failed to restore default feeds:', err);
    }
  };

  const handleEditFeed = (feed: UserRSSFeed) => {
    setEditingFeed(feed);
    setFormData({
      name: feed.name,
      url: feed.url,
      category: feed.category,
      region: feed.region,
      source_name: feed.source_name,
    });
    setShowAddForm(true);
    setFormError(null);
    setTestResult(null);
  };

  const resetForm = () => {
    setShowAddForm(false);
    setEditingFeed(null);
    setFormData({
      name: '',
      url: '',
      category: 'GENERAL',
      region: 'GLOBAL',
      source_name: '',
    });
    setFormError(null);
    setTestResult(null);
  };

  if (!isOpen) return null;

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0, 0, 0, 0.85)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 10000,
        fontFamily: FONT_FAMILY,
      }}
      onClick={onClose}
    >
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.ORANGE}`,
          borderRadius: '2px',
          width: '800px',
          maxWidth: '95vw',
          maxHeight: '85vh',
          display: 'flex',
          flexDirection: 'column',
        }}
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `2px solid ${FINCEPT.ORANGE}`,
            padding: '10px 16px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '11px', letterSpacing: '0.5px' }}>
              RSS FEED SETTINGS
            </span>
            <span style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
              MANAGE YOUR NEWS SOURCES
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              padding: '4px 10px',
              backgroundColor: FINCEPT.RED,
              color: FINCEPT.WHITE,
              border: 'none',
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <X size={12} /> CLOSE
          </button>
        </div>

        {/* Tabs */}
        <div
          style={{
            display: 'flex',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            backgroundColor: FINCEPT.HEADER_BG,
          }}
        >
          <button
            onClick={() => setActiveTab('user')}
            style={{
              padding: '8px 16px',
              backgroundColor: activeTab === 'user' ? FINCEPT.ORANGE : 'transparent',
              color: activeTab === 'user' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: 'none',
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
            }}
          >
            MY FEEDS ({userFeeds.length})
          </button>
          <button
            onClick={() => setActiveTab('default')}
            style={{
              padding: '8px 16px',
              backgroundColor: activeTab === 'default' ? FINCEPT.ORANGE : 'transparent',
              color: activeTab === 'default' ? FINCEPT.DARK_BG : FINCEPT.GRAY,
              border: 'none',
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
            }}
          >
            DEFAULT FEEDS ({defaultFeeds.length})
          </button>
        </div>

        {/* Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
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
              padding: '40px',
            }}>
              <RefreshCw size={20} style={{ marginBottom: '8px', opacity: 0.5 }} className="animate-spin" />
              <span>Loading feeds...</span>
            </div>
          ) : error ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: FINCEPT.RED,
              fontSize: '10px',
              textAlign: 'center',
              padding: '40px',
            }}>
              <AlertCircle size={20} style={{ marginBottom: '8px', opacity: 0.5 }} />
              <span>{error}</span>
            </div>
          ) : activeTab === 'user' ? (
            <>
              {/* Empty State for All Feeds Deleted */}
              {userFeeds.length === 0 && defaultFeeds.length === 0 && deletedDefaultCount > 0 && !showAddForm && (
                <div
                  style={{
                    textAlign: 'center',
                    padding: '40px 20px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.YELLOW}`,
                    borderRadius: '2px',
                    marginBottom: '16px',
                  }}
                >
                  <AlertCircle size={24} style={{ color: FINCEPT.YELLOW, marginBottom: '12px' }} />
                  <div style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
                    NO RSS FEEDS AVAILABLE
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '16px' }}>
                    All feeds have been deleted. Add a new feed or restore defaults.
                  </div>
                  <div style={{ display: 'flex', gap: '8px', justifyContent: 'center' }}>
                    <button
                      onClick={() => setShowAddForm(true)}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: FINCEPT.ORANGE,
                        color: FINCEPT.DARK_BG,
                        border: 'none',
                        borderRadius: '2px',
                        cursor: 'pointer',
                        fontSize: '9px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                      }}
                    >
                      <Plus size={12} /> ADD NEW FEED
                    </button>
                    <button
                      onClick={handleRestoreAllDefaults}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.BORDER}`,
                        color: FINCEPT.GRAY,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        fontSize: '9px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                      }}
                    >
                      <RefreshCw size={12} /> RESTORE DEFAULTS
                    </button>
                  </div>
                </div>
              )}

              {/* Add Button */}
              {!showAddForm && (userFeeds.length > 0 || defaultFeeds.length > 0 || deletedDefaultCount === 0) && (
                <button
                  onClick={() => setShowAddForm(true)}
                  style={{
                    width: '100%',
                    padding: '10px',
                    backgroundColor: 'transparent',
                    border: `1px dashed ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    color: FINCEPT.GRAY,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '6px',
                    fontSize: '9px',
                    fontWeight: 700,
                    marginBottom: '12px',
                    transition: 'all 0.2s',
                    letterSpacing: '0.5px',
                  }}
                >
                  <Plus size={12} /> ADD NEW RSS FEED
                </button>
              )}

              {/* Add/Edit Form */}
              {showAddForm && (
                <div
                  style={{
                    padding: '12px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.ORANGE}`,
                    borderRadius: '2px',
                    marginBottom: '12px',
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
                    <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '9px', letterSpacing: '0.5px' }}>
                      {editingFeed ? 'EDIT RSS FEED' : 'ADD NEW RSS FEED'}
                    </span>
                    <button
                      onClick={resetForm}
                      style={{
                        background: 'none',
                        border: 'none',
                        color: FINCEPT.GRAY,
                        cursor: 'pointer',
                      }}
                    >
                      <X size={14} />
                    </button>
                  </div>

                  {formError && (
                    <div
                      style={{
                        padding: '6px 10px',
                        backgroundColor: `${FINCEPT.RED}20`,
                        color: FINCEPT.RED,
                        fontSize: '9px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        marginBottom: '12px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                      }}
                    >
                      <AlertCircle size={12} /> {formError}
                    </div>
                  )}

                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                    {/* Name */}
                    <div>
                      <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        DISPLAY NAME *
                      </label>
                      <input
                        type="text"
                        value={formData.name}
                        onChange={e => setFormData(prev => ({ ...prev, name: e.target.value }))}
                        placeholder="e.g., Bloomberg Markets"
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: FINCEPT.DARK_BG,
                          color: FINCEPT.WHITE,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily: FONT_FAMILY,
                        }}
                      />
                    </div>

                    {/* Source Name */}
                    <div>
                      <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        SOURCE NAME (SHORT) *
                      </label>
                      <input
                        type="text"
                        value={formData.source_name}
                        onChange={e => setFormData(prev => ({ ...prev, source_name: e.target.value }))}
                        placeholder="e.g., BLOOMBERG"
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: FINCEPT.DARK_BG,
                          color: FINCEPT.WHITE,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily: FONT_FAMILY,
                          textTransform: 'uppercase',
                        }}
                      />
                    </div>

                    {/* URL */}
                    <div style={{ gridColumn: '1 / -1' }}>
                      <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        RSS FEED URL *
                      </label>
                      <div style={{ display: 'flex', gap: '8px' }}>
                        <input
                          type="url"
                          value={formData.url}
                          onChange={e => {
                            setFormData(prev => ({ ...prev, url: e.target.value }));
                            setTestResult(null);
                          }}
                          placeholder="https://example.com/rss/feed"
                          style={{
                            flex: 1,
                            padding: '8px 10px',
                            backgroundColor: FINCEPT.DARK_BG,
                            color: FINCEPT.WHITE,
                            border: `1px solid ${testResult === true ? FINCEPT.GREEN : testResult === false ? FINCEPT.RED : FINCEPT.BORDER}`,
                            borderRadius: '2px',
                            fontSize: '10px',
                            fontFamily: FONT_FAMILY,
                          }}
                        />
                        <button
                          onClick={handleTestUrl}
                          disabled={testing || !formData.url}
                          style={{
                            padding: '8px 16px',
                            backgroundColor: testing ? FINCEPT.MUTED : FINCEPT.BLUE,
                            color: FINCEPT.WHITE,
                            border: 'none',
                            borderRadius: '2px',
                            cursor: testing || !formData.url ? 'not-allowed' : 'pointer',
                            fontSize: '9px',
                            fontWeight: 700,
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                          }}
                        >
                          {testing ? <RefreshCw size={12} className="animate-spin" /> : testResult === true ? <Check size={12} /> : <ExternalLink size={12} />}
                          {testing ? 'TESTING...' : testResult === true ? 'VALID' : 'TEST URL'}
                        </button>
                      </div>
                    </div>

                    {/* Category */}
                    <div>
                      <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        CATEGORY
                      </label>
                      <select
                        value={formData.category}
                        onChange={e => setFormData(prev => ({ ...prev, category: e.target.value }))}
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: FINCEPT.DARK_BG,
                          color: FINCEPT.WHITE,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily: FONT_FAMILY,
                        }}
                      >
                        {RSS_CATEGORIES.map(cat => (
                          <option key={cat} value={cat}>
                            {getCategoryLabel(cat)}
                          </option>
                        ))}
                      </select>
                    </div>

                    {/* Region */}
                    <div>
                      <label style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        REGION
                      </label>
                      <select
                        value={formData.region}
                        onChange={e => setFormData(prev => ({ ...prev, region: e.target.value }))}
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: FINCEPT.DARK_BG,
                          color: FINCEPT.WHITE,
                          border: `1px solid ${FINCEPT.BORDER}`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily: FONT_FAMILY,
                        }}
                      >
                        {RSS_REGIONS.map(reg => (
                          <option key={reg} value={reg}>
                            {getRegionLabel(reg)}
                          </option>
                        ))}
                      </select>
                    </div>
                  </div>

                  {/* Submit Buttons */}
                  <div style={{ display: 'flex', gap: '8px', marginTop: '12px', justifyContent: 'flex-end' }}>
                    <button
                      onClick={resetForm}
                      style={{
                        padding: '6px 12px',
                        backgroundColor: 'transparent',
                        border: `1px solid ${FINCEPT.BORDER}`,
                        color: FINCEPT.GRAY,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        fontSize: '9px',
                        fontWeight: 700,
                      }}
                    >
                      CANCEL
                    </button>
                    <button
                      onClick={editingFeed ? handleUpdateFeed : handleAddFeed}
                      disabled={submitting}
                      style={{
                        padding: '6px 12px',
                        backgroundColor: submitting ? FINCEPT.MUTED : FINCEPT.ORANGE,
                        color: submitting ? FINCEPT.GRAY : FINCEPT.DARK_BG,
                        border: 'none',
                        borderRadius: '2px',
                        cursor: submitting ? 'not-allowed' : 'pointer',
                        fontSize: '9px',
                        fontWeight: 700,
                      }}
                    >
                      {submitting ? 'SAVING...' : editingFeed ? 'UPDATE FEED' : 'ADD FEED'}
                    </button>
                  </div>
                </div>
              )}

              {/* User Feeds List */}
              {userFeeds.length === 0 ? (
                <div
                  style={{
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    justifyContent: 'center',
                    height: '100%',
                    color: FINCEPT.MUTED,
                    fontSize: '10px',
                    textAlign: 'center',
                    padding: '40px',
                  }}
                >
                  <span>No custom feeds yet. Add your first RSS feed above.</span>
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  {userFeeds.map(feed => (
                    <div
                      key={feed.id}
                      style={{
                        padding: '10px 12px',
                        backgroundColor: feed.enabled ? FINCEPT.DARK_BG : `${FINCEPT.MUTED}10`,
                        border: `1px solid ${feed.enabled ? FINCEPT.BORDER : FINCEPT.MUTED}`,
                        borderRadius: '2px',
                        opacity: feed.enabled ? 1 : 0.7,
                        transition: 'all 0.2s',
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                        <div style={{ flex: 1 }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
                            <span style={{ color: FINCEPT.WHITE, fontWeight: 700, fontSize: '10px' }}>
                              {feed.name}
                            </span>
                            <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                              {feed.source_name}
                            </span>
                            <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.PURPLE}20`, color: FINCEPT.PURPLE, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                              {getCategoryLabel(feed.category)}
                            </span>
                            <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.YELLOW}20`, color: FINCEPT.YELLOW, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                              {getRegionLabel(feed.region)}
                            </span>
                          </div>
                          <div
                            style={{
                              color: FINCEPT.MUTED,
                              fontSize: '9px',
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                              whiteSpace: 'nowrap',
                              maxWidth: '500px',
                            }}
                          >
                            {feed.url}
                          </div>
                        </div>
                        <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
                          <button
                            onClick={() => handleToggleFeed(feed.id, !feed.enabled)}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: feed.enabled ? FINCEPT.GREEN : FINCEPT.MUTED,
                              color: feed.enabled ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                              border: 'none',
                              borderRadius: '2px',
                              cursor: 'pointer',
                              fontSize: '9px',
                              fontWeight: 700,
                            }}
                          >
                            {feed.enabled ? 'ON' : 'OFF'}
                          </button>
                          <button
                            onClick={() => handleEditFeed(feed)}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: 'transparent',
                              border: `1px solid ${FINCEPT.BORDER}`,
                              color: FINCEPT.GRAY,
                              borderRadius: '2px',
                              cursor: 'pointer',
                            }}
                          >
                            <Edit2 size={12} />
                          </button>
                          <button
                            onClick={() => handleDeleteFeed(feed.id)}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: FINCEPT.RED,
                              color: FINCEPT.WHITE,
                              border: 'none',
                              borderRadius: '2px',
                              cursor: 'pointer',
                            }}
                          >
                            <Trash2 size={12} />
                          </button>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </>
          ) : (
            // Default Feeds Tab
            <div>
              <div
                style={{
                  padding: '8px 12px',
                  backgroundColor: FINCEPT.DARK_BG,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  marginBottom: '12px',
                  color: FINCEPT.GRAY,
                  fontSize: '9px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                }}
              >
                <span>
                  Default RSS feeds included with Fincept Terminal. Toggle or remove as needed.
                </span>
                {deletedDefaultCount > 0 && (
                  <button
                    onClick={handleRestoreAllDefaults}
                    style={{
                      padding: '4px 10px',
                      backgroundColor: FINCEPT.ORANGE,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      borderRadius: '2px',
                      cursor: 'pointer',
                      fontSize: '9px',
                      fontWeight: 700,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px',
                      whiteSpace: 'nowrap',
                    }}
                  >
                    <RefreshCw size={10} /> RESTORE ALL ({deletedDefaultCount})
                  </button>
                )}
              </div>

              {/* Empty state for default feeds tab */}
              {defaultFeeds.length === 0 && deletedDefaultCount > 0 && (
                <div
                  style={{
                    textAlign: 'center',
                    padding: '40px 20px',
                    backgroundColor: FINCEPT.DARK_BG,
                    border: `1px solid ${FINCEPT.YELLOW}`,
                    borderRadius: '2px',
                    marginBottom: '12px',
                  }}
                >
                  <AlertCircle size={24} style={{ color: FINCEPT.YELLOW, marginBottom: '12px' }} />
                  <div style={{ color: FINCEPT.WHITE, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
                    ALL DEFAULT FEEDS DELETED
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '10px', marginBottom: '16px' }}>
                    You have deleted all {deletedDefaultCount} default feeds. Click below to restore them.
                  </div>
                  <button
                    onClick={handleRestoreAllDefaults}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: FINCEPT.ORANGE,
                      color: FINCEPT.DARK_BG,
                      border: 'none',
                      borderRadius: '2px',
                      cursor: 'pointer',
                      fontSize: '9px',
                      fontWeight: 700,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      margin: '0 auto',
                    }}
                  >
                    <RefreshCw size={12} /> RESTORE ALL DEFAULT FEEDS
                  </button>
                </div>
              )}

              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                {defaultFeeds.map(feed => (
                  <div
                    key={feed.id || feed.url}
                    style={{
                      padding: '10px 12px',
                      backgroundColor: feed.enabled ? FINCEPT.DARK_BG : `${FINCEPT.MUTED}10`,
                      border: `1px solid ${feed.enabled ? FINCEPT.BORDER : FINCEPT.MUTED}`,
                      borderRadius: '2px',
                      opacity: feed.enabled ? 1 : 0.7,
                      transition: 'all 0.2s',
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                      <div style={{ flex: 1 }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
                          <span style={{ color: FINCEPT.WHITE, fontWeight: 700, fontSize: '10px' }}>
                            {feed.name}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.GREEN}20`, color: FINCEPT.GREEN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            DEFAULT
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.CYAN}20`, color: FINCEPT.CYAN, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            {feed.source}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.PURPLE}20`, color: FINCEPT.PURPLE, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            {getCategoryLabel(feed.category)}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: `${FINCEPT.YELLOW}20`, color: FINCEPT.YELLOW, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            {getRegionLabel(feed.region)}
                          </span>
                        </div>
                        <div
                          style={{
                            color: FINCEPT.MUTED,
                            fontSize: '9px',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                            whiteSpace: 'nowrap',
                            maxWidth: '500px',
                          }}
                        >
                          {feed.url}
                        </div>
                      </div>
                      <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
                        <button
                          onClick={() => feed.id && handleToggleDefaultFeed(feed.id, !feed.enabled)}
                          style={{
                            padding: '4px 8px',
                            backgroundColor: feed.enabled ? FINCEPT.GREEN : FINCEPT.MUTED,
                            color: feed.enabled ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                            border: 'none',
                            borderRadius: '2px',
                            cursor: 'pointer',
                            fontSize: '9px',
                            fontWeight: 700,
                          }}
                        >
                          {feed.enabled ? 'ON' : 'OFF'}
                        </button>
                        <button
                          onClick={() => feed.id && handleDeleteDefaultFeed(feed.id)}
                          style={{
                            padding: '4px 8px',
                            backgroundColor: FINCEPT.RED,
                            color: FINCEPT.WHITE,
                            border: 'none',
                            borderRadius: '2px',
                            cursor: 'pointer',
                          }}
                        >
                          <Trash2 size={12} />
                        </button>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>

        {/* Footer / Status Bar */}
        <div
          style={{
            borderTop: `1px solid ${FINCEPT.BORDER}`,
            padding: '6px 16px',
            backgroundColor: FINCEPT.HEADER_BG,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            fontSize: '9px',
            color: FINCEPT.GRAY,
          }}
        >
          <span>
            TOTAL: <span style={{ color: FINCEPT.CYAN }}>{defaultFeeds.length + userFeeds.filter(f => f.enabled).length}</span> ACTIVE
            <span style={{ color: FINCEPT.BORDER }}> | </span>
            <span style={{ color: FINCEPT.CYAN }}>{defaultFeeds.length}</span> DEFAULT
            <span style={{ color: FINCEPT.BORDER }}> | </span>
            <span style={{ color: FINCEPT.CYAN }}>{userFeeds.filter(f => f.enabled).length}</span> CUSTOM
          </span>
          <button
            onClick={loadFeeds}
            style={{
              padding: '4px 10px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              borderRadius: '2px',
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <RefreshCw size={10} /> REFRESH
          </button>
        </div>
      </div>
    </div>
  );
};

export default RSSFeedSettingsModal;
