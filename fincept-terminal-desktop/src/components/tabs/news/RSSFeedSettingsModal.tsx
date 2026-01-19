// RSS Feed Settings Modal - Allows users to manage their RSS news sources
import React, { useState, useEffect } from 'react';
import { Plus, Trash2, Edit2, Check, X, ExternalLink, RefreshCw, AlertCircle } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
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
  const { colors } = useTerminalTheme();

  // State
  const [activeTab, setActiveTab] = useState<'user' | 'default'>('user');
  const [userFeeds, setUserFeeds] = useState<UserRSSFeed[]>([]);
  const [defaultFeeds, setDefaultFeeds] = useState<RSSFeed[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  // Add/Edit form state
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

  // Load feeds
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

  // Test URL
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

  // Add feed
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

  // Update feed
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

  // Delete feed
  const handleDeleteFeed = async (id: string) => {
    if (!confirm('Are you sure you want to delete this RSS feed?')) return;
    try {
      await deleteUserRSSFeed(id);
      await loadFeeds();
      onFeedsUpdated?.();
    } catch (err) {
      console.error('[RSSFeedSettings] Failed to delete feed:', err);
    }
  };

  // Toggle user feed
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

  // Toggle default feed
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

  // Delete default feed
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

  // Restore all default feeds
  const handleRestoreAllDefaults = async () => {
    try {
      await restoreAllDefaultFeeds();
      await loadFeeds();
      onFeedsUpdated?.();
    } catch (err) {
      console.error('[RSSFeedSettings] Failed to restore default feeds:', err);
    }
  };

  // Edit feed
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

  // Reset form
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
      }}
      onClick={onClose}
    >
      <div
        style={{
          backgroundColor: colors.panel,
          border: `2px solid ${colors.primary}`,
          borderRadius: '4px',
          width: '800px',
          maxWidth: '95vw',
          maxHeight: '85vh',
          display: 'flex',
          flexDirection: 'column',
          boxShadow: 'none',
        }}
        onClick={e => e.stopPropagation()}
      >
        {/* Header */}
        <div
          style={{
            backgroundColor: colors.background,
            borderBottom: `2px solid ${colors.primary}`,
            padding: '12px 16px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: colors.primary, fontWeight: 'bold', fontSize: '14px' }}>
              RSS FEED SETTINGS
            </span>
            <span style={{ color: colors.textMuted, fontSize: '11px' }}>
              Manage your news sources
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              backgroundColor: colors.alert,
              color: colors.text,
              border: 'none',
              padding: '6px 12px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <X size={14} /> CLOSE
          </button>
        </div>

        {/* Tabs */}
        <div
          style={{
            display: 'flex',
            borderBottom: `1px solid ${colors.textMuted}`,
            backgroundColor: colors.background,
          }}
        >
          <button
            onClick={() => setActiveTab('user')}
            style={{
              padding: '10px 20px',
              backgroundColor: activeTab === 'user' ? colors.panel : 'transparent',
              color: activeTab === 'user' ? colors.primary : colors.textMuted,
              border: 'none',
              borderBottom: activeTab === 'user' ? `2px solid ${colors.primary}` : 'none',
              cursor: 'pointer',
              fontSize: '12px',
              fontWeight: 'bold',
            }}
          >
            MY FEEDS ({userFeeds.length})
          </button>
          <button
            onClick={() => setActiveTab('default')}
            style={{
              padding: '10px 20px',
              backgroundColor: activeTab === 'default' ? colors.panel : 'transparent',
              color: activeTab === 'default' ? colors.primary : colors.textMuted,
              border: 'none',
              borderBottom: activeTab === 'default' ? `2px solid ${colors.primary}` : 'none',
              cursor: 'pointer',
              fontSize: '12px',
              fontWeight: 'bold',
            }}
          >
            DEFAULT FEEDS ({defaultFeeds.length})
          </button>
        </div>

        {/* Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {loading ? (
            <div style={{ textAlign: 'center', padding: '40px', color: colors.textMuted }}>
              Loading feeds...
            </div>
          ) : error ? (
            <div style={{ textAlign: 'center', padding: '40px', color: colors.alert }}>
              {error}
            </div>
          ) : activeTab === 'user' ? (
            <>
              {/* Empty State for All Feeds Deleted */}
              {userFeeds.length === 0 && defaultFeeds.length === 0 && deletedDefaultCount > 0 && !showAddForm && (
                <div
                  style={{
                    textAlign: 'center',
                    padding: '40px 20px',
                    backgroundColor: 'rgba(255, 200, 0, 0.05)',
                    border: `1px dashed ${colors.warning}`,
                    borderRadius: '4px',
                    marginBottom: '16px',
                  }}
                >
                  <AlertCircle size={32} style={{ color: colors.warning, marginBottom: '12px' }} />
                  <div style={{ color: colors.text, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    No RSS Feeds Available
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: '12px', marginBottom: '16px' }}>
                    All feeds have been deleted. Add a new feed or restore defaults to see news.
                  </div>
                  <div style={{ display: 'flex', gap: '12px', justifyContent: 'center' }}>
                    <button
                      onClick={() => setShowAddForm(true)}
                      style={{
                        padding: '10px 20px',
                        backgroundColor: colors.secondary,
                        color: colors.background,
                        border: 'none',
                        borderRadius: '4px',
                        cursor: 'pointer',
                        fontSize: '12px',
                        fontWeight: 'bold',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                      }}
                    >
                      <Plus size={14} /> ADD NEW FEED
                    </button>
                    <button
                      onClick={handleRestoreAllDefaults}
                      style={{
                        padding: '10px 20px',
                        backgroundColor: colors.info,
                        color: colors.text,
                        border: 'none',
                        borderRadius: '4px',
                        cursor: 'pointer',
                        fontSize: '12px',
                        fontWeight: 'bold',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                      }}
                    >
                      <RefreshCw size={14} /> RESTORE DEFAULTS
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
                    padding: '12px',
                    backgroundColor: 'rgba(0, 255, 0, 0.1)',
                    border: `1px dashed ${colors.secondary}`,
                    borderRadius: '4px',
                    color: colors.secondary,
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '8px',
                    fontSize: '12px',
                    fontWeight: 'bold',
                    marginBottom: '16px',
                  }}
                >
                  <Plus size={16} /> ADD NEW RSS FEED
                </button>
              )}

              {/* Add/Edit Form */}
              {showAddForm && (
                <div
                  style={{
                    padding: '16px',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    borderRadius: '4px',
                    marginBottom: '16px',
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
                    <span style={{ color: colors.primary, fontWeight: 'bold', fontSize: '12px' }}>
                      {editingFeed ? 'EDIT RSS FEED' : 'ADD NEW RSS FEED'}
                    </span>
                    <button
                      onClick={resetForm}
                      style={{
                        background: 'none',
                        border: 'none',
                        color: colors.textMuted,
                        cursor: 'pointer',
                      }}
                    >
                      <X size={16} />
                    </button>
                  </div>

                  {formError && (
                    <div
                      style={{
                        padding: '8px 12px',
                        backgroundColor: 'rgba(255, 0, 0, 0.1)',
                        border: `1px solid ${colors.alert}`,
                        borderRadius: '2px',
                        color: colors.alert,
                        fontSize: '11px',
                        marginBottom: '12px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '8px',
                      }}
                    >
                      <AlertCircle size={14} /> {formError}
                    </div>
                  )}

                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                    {/* Name */}
                    <div>
                      <label style={{ color: colors.textMuted, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                        DISPLAY NAME *
                      </label>
                      <input
                        type="text"
                        value={formData.name}
                        onChange={e => setFormData(prev => ({ ...prev, name: e.target.value }))}
                        placeholder="e.g., Bloomberg Markets"
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: colors.panel,
                          border: `1px solid ${colors.textMuted}`,
                          borderRadius: '2px',
                          color: colors.text,
                          fontSize: '12px',
                        }}
                      />
                    </div>

                    {/* Source Name */}
                    <div>
                      <label style={{ color: colors.textMuted, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                        SOURCE NAME (SHORT) *
                      </label>
                      <input
                        type="text"
                        value={formData.source_name}
                        onChange={e => setFormData(prev => ({ ...prev, source_name: e.target.value }))}
                        placeholder="e.g., BLOOMBERG"
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: colors.panel,
                          border: `1px solid ${colors.textMuted}`,
                          borderRadius: '2px',
                          color: colors.text,
                          fontSize: '12px',
                          textTransform: 'uppercase',
                        }}
                      />
                    </div>

                    {/* URL */}
                    <div style={{ gridColumn: '1 / -1' }}>
                      <label style={{ color: colors.textMuted, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
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
                            padding: '8px',
                            backgroundColor: colors.panel,
                            border: `1px solid ${testResult === true ? colors.secondary : testResult === false ? colors.alert : colors.textMuted}`,
                            borderRadius: '2px',
                            color: colors.text,
                            fontSize: '12px',
                          }}
                        />
                        <button
                          onClick={handleTestUrl}
                          disabled={testing || !formData.url}
                          style={{
                            padding: '8px 12px',
                            backgroundColor: testing ? colors.textMuted : colors.info,
                            color: colors.text,
                            border: 'none',
                            borderRadius: '2px',
                            cursor: testing || !formData.url ? 'not-allowed' : 'pointer',
                            fontSize: '11px',
                            fontWeight: 'bold',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '4px',
                          }}
                        >
                          {testing ? <RefreshCw size={14} className="animate-spin" /> : testResult === true ? <Check size={14} /> : <ExternalLink size={14} />}
                          {testing ? 'TESTING...' : testResult === true ? 'VALID' : 'TEST URL'}
                        </button>
                      </div>
                    </div>

                    {/* Category */}
                    <div>
                      <label style={{ color: colors.textMuted, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                        CATEGORY
                      </label>
                      <select
                        value={formData.category}
                        onChange={e => setFormData(prev => ({ ...prev, category: e.target.value }))}
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: colors.panel,
                          border: `1px solid ${colors.textMuted}`,
                          borderRadius: '2px',
                          color: colors.text,
                          fontSize: '12px',
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
                      <label style={{ color: colors.textMuted, fontSize: '10px', display: 'block', marginBottom: '4px' }}>
                        REGION
                      </label>
                      <select
                        value={formData.region}
                        onChange={e => setFormData(prev => ({ ...prev, region: e.target.value }))}
                        style={{
                          width: '100%',
                          padding: '8px',
                          backgroundColor: colors.panel,
                          border: `1px solid ${colors.textMuted}`,
                          borderRadius: '2px',
                          color: colors.text,
                          fontSize: '12px',
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
                  <div style={{ display: 'flex', gap: '8px', marginTop: '16px', justifyContent: 'flex-end' }}>
                    <button
                      onClick={resetForm}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: colors.textMuted,
                        color: colors.text,
                        border: 'none',
                        borderRadius: '2px',
                        cursor: 'pointer',
                        fontSize: '11px',
                        fontWeight: 'bold',
                      }}
                    >
                      CANCEL
                    </button>
                    <button
                      onClick={editingFeed ? handleUpdateFeed : handleAddFeed}
                      disabled={submitting}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: submitting ? colors.textMuted : colors.secondary,
                        color: colors.background,
                        border: 'none',
                        borderRadius: '2px',
                        cursor: submitting ? 'not-allowed' : 'pointer',
                        fontSize: '11px',
                        fontWeight: 'bold',
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
                    textAlign: 'center',
                    padding: '40px',
                    color: colors.textMuted,
                    fontSize: '12px',
                  }}
                >
                  No custom feeds yet. Add your first RSS feed above!
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                  {userFeeds.map(feed => (
                    <div
                      key={feed.id}
                      style={{
                        padding: '12px',
                        backgroundColor: feed.enabled ? colors.background : 'rgba(0,0,0,0.3)',
                        border: `1px solid ${feed.enabled ? colors.textMuted : 'rgba(100,100,100,0.3)'}`,
                        borderRadius: '4px',
                        opacity: feed.enabled ? 1 : 0.6,
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                        <div style={{ flex: 1 }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                            <span style={{ color: colors.text, fontWeight: 'bold', fontSize: '12px' }}>
                              {feed.name}
                            </span>
                            <span
                              style={{
                                padding: '2px 6px',
                                backgroundColor: colors.info,
                                color: colors.text,
                                fontSize: '9px',
                                fontWeight: 'bold',
                                borderRadius: '2px',
                              }}
                            >
                              {feed.source_name}
                            </span>
                            <span
                              style={{
                                padding: '2px 6px',
                                backgroundColor: colors.purple,
                                color: colors.text,
                                fontSize: '9px',
                                borderRadius: '2px',
                              }}
                            >
                              {getCategoryLabel(feed.category)}
                            </span>
                            <span
                              style={{
                                padding: '2px 6px',
                                backgroundColor: colors.warning,
                                color: colors.background,
                                fontSize: '9px',
                                borderRadius: '2px',
                              }}
                            >
                              {getRegionLabel(feed.region)}
                            </span>
                          </div>
                          <div
                            style={{
                              color: colors.textMuted,
                              fontSize: '10px',
                              overflow: 'hidden',
                              textOverflow: 'ellipsis',
                              whiteSpace: 'nowrap',
                              maxWidth: '500px',
                            }}
                          >
                            {feed.url}
                          </div>
                        </div>
                        <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                          {/* Toggle */}
                          <button
                            onClick={() => handleToggleFeed(feed.id, !feed.enabled)}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: feed.enabled ? colors.secondary : colors.textMuted,
                              color: feed.enabled ? colors.background : colors.text,
                              border: 'none',
                              borderRadius: '2px',
                              cursor: 'pointer',
                              fontSize: '10px',
                              fontWeight: 'bold',
                            }}
                          >
                            {feed.enabled ? 'ON' : 'OFF'}
                          </button>
                          {/* Edit */}
                          <button
                            onClick={() => handleEditFeed(feed)}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: colors.info,
                              color: colors.text,
                              border: 'none',
                              borderRadius: '2px',
                              cursor: 'pointer',
                            }}
                          >
                            <Edit2 size={14} />
                          </button>
                          {/* Delete */}
                          <button
                            onClick={() => handleDeleteFeed(feed.id)}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: colors.alert,
                              color: colors.text,
                              border: 'none',
                              borderRadius: '2px',
                              cursor: 'pointer',
                            }}
                          >
                            <Trash2 size={14} />
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
                  padding: '12px',
                  backgroundColor: 'rgba(0, 255, 0, 0.05)',
                  border: `1px solid ${colors.secondary}`,
                  borderRadius: '4px',
                  marginBottom: '16px',
                  color: colors.textMuted,
                  fontSize: '11px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                }}
              >
                <span>
                  These are the default RSS feeds that come with Fincept Terminal. You can enable, disable, or delete them.
                </span>
                {deletedDefaultCount > 0 && (
                  <button
                    onClick={handleRestoreAllDefaults}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: colors.info,
                      color: colors.text,
                      border: 'none',
                      borderRadius: '2px',
                      cursor: 'pointer',
                      fontSize: '10px',
                      fontWeight: 'bold',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px',
                      whiteSpace: 'nowrap',
                    }}
                  >
                    <RefreshCw size={12} /> RESTORE ALL ({deletedDefaultCount})
                  </button>
                )}
              </div>

              {/* Empty state for default feeds tab */}
              {defaultFeeds.length === 0 && deletedDefaultCount > 0 && (
                <div
                  style={{
                    textAlign: 'center',
                    padding: '40px 20px',
                    backgroundColor: 'rgba(255, 200, 0, 0.05)',
                    border: `1px dashed ${colors.warning}`,
                    borderRadius: '4px',
                    marginBottom: '16px',
                  }}
                >
                  <AlertCircle size={32} style={{ color: colors.warning, marginBottom: '12px' }} />
                  <div style={{ color: colors.text, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    All Default Feeds Deleted
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: '12px', marginBottom: '16px' }}>
                    You have deleted all {deletedDefaultCount} default feeds. Click below to restore them.
                  </div>
                  <button
                    onClick={handleRestoreAllDefaults}
                    style={{
                      padding: '10px 20px',
                      backgroundColor: colors.info,
                      color: colors.text,
                      border: 'none',
                      borderRadius: '4px',
                      cursor: 'pointer',
                      fontSize: '12px',
                      fontWeight: 'bold',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      margin: '0 auto',
                    }}
                  >
                    <RefreshCw size={14} /> RESTORE ALL DEFAULT FEEDS
                  </button>
                </div>
              )}

              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {defaultFeeds.map(feed => (
                  <div
                    key={feed.id || feed.url}
                    style={{
                      padding: '12px',
                      backgroundColor: feed.enabled ? colors.background : 'rgba(0,0,0,0.3)',
                      border: `1px solid ${feed.enabled ? colors.textMuted : 'rgba(100,100,100,0.3)'}`,
                      borderRadius: '4px',
                      opacity: feed.enabled ? 1 : 0.6,
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                      <div style={{ flex: 1 }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '4px' }}>
                          <span style={{ color: colors.text, fontWeight: 'bold', fontSize: '12px' }}>
                            {feed.name}
                          </span>
                          <span
                            style={{
                              padding: '2px 6px',
                              backgroundColor: colors.secondary,
                              color: colors.background,
                              fontSize: '9px',
                              fontWeight: 'bold',
                              borderRadius: '2px',
                            }}
                          >
                            DEFAULT
                          </span>
                          <span
                            style={{
                              padding: '2px 6px',
                              backgroundColor: colors.info,
                              color: colors.text,
                              fontSize: '9px',
                              fontWeight: 'bold',
                              borderRadius: '2px',
                            }}
                          >
                            {feed.source}
                          </span>
                          <span
                            style={{
                              padding: '2px 6px',
                              backgroundColor: colors.purple,
                              color: colors.text,
                              fontSize: '9px',
                              borderRadius: '2px',
                            }}
                          >
                            {getCategoryLabel(feed.category)}
                          </span>
                          <span
                            style={{
                              padding: '2px 6px',
                              backgroundColor: colors.warning,
                              color: colors.background,
                              fontSize: '9px',
                              borderRadius: '2px',
                            }}
                          >
                            {getRegionLabel(feed.region)}
                          </span>
                        </div>
                        <div
                          style={{
                            color: colors.textMuted,
                            fontSize: '10px',
                            overflow: 'hidden',
                            textOverflow: 'ellipsis',
                            whiteSpace: 'nowrap',
                            maxWidth: '500px',
                          }}
                        >
                          {feed.url}
                        </div>
                      </div>
                      <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                        {/* Toggle */}
                        <button
                          onClick={() => feed.id && handleToggleDefaultFeed(feed.id, !feed.enabled)}
                          style={{
                            padding: '4px 8px',
                            backgroundColor: feed.enabled ? colors.secondary : colors.textMuted,
                            color: feed.enabled ? colors.background : colors.text,
                            border: 'none',
                            borderRadius: '2px',
                            cursor: 'pointer',
                            fontSize: '10px',
                            fontWeight: 'bold',
                          }}
                        >
                          {feed.enabled ? 'ON' : 'OFF'}
                        </button>
                        {/* Delete */}
                        <button
                          onClick={() => feed.id && handleDeleteDefaultFeed(feed.id)}
                          style={{
                            padding: '4px 8px',
                            backgroundColor: colors.alert,
                            color: colors.text,
                            border: 'none',
                            borderRadius: '2px',
                            cursor: 'pointer',
                          }}
                        >
                          <Trash2 size={14} />
                        </button>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div
          style={{
            borderTop: `1px solid ${colors.textMuted}`,
            padding: '10px 16px',
            backgroundColor: colors.background,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            fontSize: '10px',
            color: colors.textMuted,
          }}
        >
          <span>
            Total Sources: {defaultFeeds.length + userFeeds.filter(f => f.enabled).length} active ({defaultFeeds.length} default + {userFeeds.filter(f => f.enabled).length} custom)
          </span>
          <button
            onClick={loadFeeds}
            style={{
              padding: '4px 8px',
              backgroundColor: colors.info,
              color: colors.text,
              border: 'none',
              borderRadius: '2px',
              cursor: 'pointer',
              fontSize: '10px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <RefreshCw size={12} /> REFRESH
          </button>
        </div>
      </div>
    </div>
  );
};

export default RSSFeedSettingsModal;
