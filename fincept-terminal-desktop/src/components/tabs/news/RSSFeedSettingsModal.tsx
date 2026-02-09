// RSS Feed Settings Modal - Allows users to manage their RSS news sources
import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
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
  const { t } = useTranslation('news');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

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
      setError(t('messages.failedToLoad'));
      console.error('[RSSFeedSettings] Failed to load feeds:', err);
    } finally {
      setLoading(false);
    }
  };

  const handleTestUrl = async () => {
    if (!isValidRSSUrl(formData.url)) {
      setFormError(t('rssSettings.errors.invalidUrl'));
      return;
    }
    setTesting(true);
    setTestResult(null);
    try {
      const result = await testRSSFeedUrl(formData.url);
      setTestResult(result);
      if (!result) {
        setFormError(t('rssSettings.errors.invalidRss'));
      } else {
        setFormError(null);
      }
    } catch (err) {
      setTestResult(false);
      setFormError(t('rssSettings.errors.testFailed'));
    } finally {
      setTesting(false);
    }
  };

  const handleAddFeed = async () => {
    if (!formData.name.trim()) {
      setFormError(t('rssSettings.errors.nameRequired'));
      return;
    }
    if (!isValidRSSUrl(formData.url)) {
      setFormError(t('rssSettings.errors.invalidUrl'));
      return;
    }
    if (!formData.source_name.trim()) {
      setFormError(t('rssSettings.errors.sourceRequired'));
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
      setFormError(t('rssSettings.errors.nameRequired'));
      return;
    }
    if (!isValidRSSUrl(formData.url)) {
      setFormError(t('rssSettings.errors.invalidUrl'));
      return;
    }
    if (!formData.source_name.trim()) {
      setFormError(t('rssSettings.errors.sourceRequired'));
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
      t('rssSettings.confirmDelete.message'),
      { title: t('rssSettings.confirmDelete.title'), type: 'danger' }
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
        fontFamily,
      }}
      onClick={onClose}
    >
      <div
        style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.primary}`,
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
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: `2px solid ${colors.primary}`,
            padding: '10px 16px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <span style={{ color: colors.primary, fontWeight: 700, fontSize: fontSize.tiny, letterSpacing: '0.5px' }}>
              {t('rssSettings.title')}
            </span>
            <span style={{ color: colors.textMuted, fontSize: '9px' }}>
              {t('rssSettings.subtitle')}
            </span>
          </div>
          <button
            onClick={onClose}
            style={{
              padding: '4px 10px',
              backgroundColor: colors.alert,
              color: colors.text,
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
            <X size={12} /> {t('rssSettings.close')}
          </button>
        </div>

        {/* Tabs */}
        <div
          style={{
            display: 'flex',
            borderBottom: `1px solid var(--ft-color-border, #2A2A2A)`,
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
          }}
        >
          <button
            onClick={() => setActiveTab('user')}
            style={{
              padding: '8px 16px',
              backgroundColor: activeTab === 'user' ? colors.primary : 'transparent',
              color: activeTab === 'user' ? colors.background : colors.textMuted,
              border: 'none',
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
            }}
          >
            {t('rssSettings.myFeeds')} ({userFeeds.length})
          </button>
          <button
            onClick={() => setActiveTab('default')}
            style={{
              padding: '8px 16px',
              backgroundColor: activeTab === 'default' ? colors.primary : 'transparent',
              color: activeTab === 'default' ? colors.background : colors.textMuted,
              border: 'none',
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
            }}
          >
            {t('rssSettings.defaultFeeds')} ({defaultFeeds.length})
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
              color: colors.textMuted,
              fontSize: '10px',
              textAlign: 'center',
              padding: '40px',
            }}>
              <RefreshCw size={20} style={{ marginBottom: '8px', opacity: 0.5 }} className="animate-spin" />
              <span>{t('messages.loadingFeedsModal')}</span>
            </div>
          ) : error ? (
            <div style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: colors.alert,
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
                    backgroundColor: colors.background,
                    border: `1px solid var(--ft-color-warning, #FFD700)`,
                    borderRadius: '2px',
                    marginBottom: '16px',
                  }}
                >
                  <AlertCircle size={24} style={{ color: 'var(--ft-color-warning, #FFD700)', marginBottom: '12px' }} />
                  <div style={{ color: colors.text, fontSize: fontSize.tiny, fontWeight: 700, marginBottom: '8px' }}>
                    {t('rssSettings.noFeedsAvailable')}
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: '10px', marginBottom: '16px' }}>
                    {t('messages.allFeedsDeleted')}
                  </div>
                  <div style={{ display: 'flex', gap: '8px', justifyContent: 'center' }}>
                    <button
                      onClick={() => setShowAddForm(true)}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: colors.primary,
                        color: colors.background,
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
                      <Plus size={12} /> {t('rssSettings.addNewFeed')}
                    </button>
                    <button
                      onClick={handleRestoreAllDefaults}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: 'transparent',
                        border: `1px solid var(--ft-color-border, #2A2A2A)`,
                        color: colors.textMuted,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        fontSize: '9px',
                        fontWeight: 700,
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                      }}
                    >
                      <RefreshCw size={12} /> {t('rssSettings.restoreDefaults')}
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
                    border: `1px dashed var(--ft-color-border, #2A2A2A)`,
                    borderRadius: '2px',
                    color: colors.textMuted,
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
                  <Plus size={12} /> {t('rssSettings.addNewRssFeed')}
                </button>
              )}

              {/* Add/Edit Form */}
              {showAddForm && (
                <div
                  style={{
                    padding: '12px',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.primary}`,
                    borderRadius: '2px',
                    marginBottom: '12px',
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
                    <span style={{ color: colors.primary, fontWeight: 700, fontSize: '9px', letterSpacing: '0.5px' }}>
                      {editingFeed ? t('rssSettings.editRssFeed') : t('rssSettings.addNewRssFeed')}
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
                      <X size={14} />
                    </button>
                  </div>

                  {formError && (
                    <div
                      style={{
                        padding: '6px 10px',
                        backgroundColor: `${colors.alert}20`,
                        color: colors.alert,
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
                      <label style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        {t('rssSettings.displayName')} {t('rssSettings.required')}
                      </label>
                      <input
                        type="text"
                        value={formData.name}
                        onChange={e => setFormData(prev => ({ ...prev, name: e.target.value }))}
                        placeholder={t('rssSettings.placeholders.displayName')}
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: colors.background,
                          color: colors.text,
                          border: `1px solid var(--ft-color-border, #2A2A2A)`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily,
                        }}
                      />
                    </div>

                    {/* Source Name */}
                    <div>
                      <label style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        {t('rssSettings.sourceName')} {t('rssSettings.required')}
                      </label>
                      <input
                        type="text"
                        value={formData.source_name}
                        onChange={e => setFormData(prev => ({ ...prev, source_name: e.target.value }))}
                        placeholder={t('rssSettings.placeholders.sourceName')}
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: colors.background,
                          color: colors.text,
                          border: `1px solid var(--ft-color-border, #2A2A2A)`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily,
                          textTransform: 'uppercase',
                        }}
                      />
                    </div>

                    {/* URL */}
                    <div style={{ gridColumn: '1 / -1' }}>
                      <label style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        {t('rssSettings.feedUrl')} {t('rssSettings.required')}
                      </label>
                      <div style={{ display: 'flex', gap: '8px' }}>
                        <input
                          type="url"
                          value={formData.url}
                          onChange={e => {
                            setFormData(prev => ({ ...prev, url: e.target.value }));
                            setTestResult(null);
                          }}
                          placeholder={t('rssSettings.placeholders.feedUrl')}
                          style={{
                            flex: 1,
                            padding: '8px 10px',
                            backgroundColor: colors.background,
                            color: colors.text,
                            border: `1px solid ${testResult === true ? colors.success : testResult === false ? colors.alert : 'var(--ft-color-border, #2A2A2A)'}`,
                            borderRadius: '2px',
                            fontSize: '10px',
                            fontFamily,
                          }}
                        />
                        <button
                          onClick={handleTestUrl}
                          disabled={testing || !formData.url}
                          style={{
                            padding: '8px 16px',
                            backgroundColor: testing ? colors.textMuted : 'var(--ft-color-blue, #0088FF)',
                            color: colors.text,
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
                          {testing ? t('rssSettings.testing') : testResult === true ? t('rssSettings.valid') : t('rssSettings.testUrl')}
                        </button>
                      </div>
                    </div>

                    {/* Category */}
                    <div>
                      <label style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        {t('rssSettings.category')}
                      </label>
                      <select
                        value={formData.category}
                        onChange={e => setFormData(prev => ({ ...prev, category: e.target.value }))}
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: colors.background,
                          color: colors.text,
                          border: `1px solid var(--ft-color-border, #2A2A2A)`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily,
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
                      <label style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 700, display: 'block', marginBottom: '4px', letterSpacing: '0.5px' }}>
                        {t('rssSettings.region')}
                      </label>
                      <select
                        value={formData.region}
                        onChange={e => setFormData(prev => ({ ...prev, region: e.target.value }))}
                        style={{
                          width: '100%',
                          padding: '8px 10px',
                          backgroundColor: colors.background,
                          color: colors.text,
                          border: `1px solid var(--ft-color-border, #2A2A2A)`,
                          borderRadius: '2px',
                          fontSize: '10px',
                          fontFamily,
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
                        border: `1px solid var(--ft-color-border, #2A2A2A)`,
                        color: colors.textMuted,
                        borderRadius: '2px',
                        cursor: 'pointer',
                        fontSize: '9px',
                        fontWeight: 700,
                      }}
                    >
                      {t('rssSettings.cancel')}
                    </button>
                    <button
                      onClick={editingFeed ? handleUpdateFeed : handleAddFeed}
                      disabled={submitting}
                      style={{
                        padding: '6px 12px',
                        backgroundColor: submitting ? colors.textMuted : colors.primary,
                        color: submitting ? colors.textMuted : colors.background,
                        border: 'none',
                        borderRadius: '2px',
                        cursor: submitting ? 'not-allowed' : 'pointer',
                        fontSize: '9px',
                        fontWeight: 700,
                      }}
                    >
                      {submitting ? t('rssSettings.saving') : editingFeed ? t('rssSettings.updateFeed') : t('rssSettings.addFeed')}
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
                    color: colors.textMuted,
                    fontSize: '10px',
                    textAlign: 'center',
                    padding: '40px',
                  }}
                >
                  <span>{t('messages.noCustomFeeds')}</span>
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                  {userFeeds.map(feed => (
                    <div
                      key={feed.id}
                      style={{
                        padding: '10px 12px',
                        backgroundColor: feed.enabled ? colors.background : `${colors.textMuted}10`,
                        border: `1px solid ${feed.enabled ? 'var(--ft-color-border, #2A2A2A)' : colors.textMuted}`,
                        borderRadius: '2px',
                        opacity: feed.enabled ? 1 : 0.7,
                        transition: 'all 0.2s',
                      }}
                    >
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                        <div style={{ flex: 1 }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
                            <span style={{ color: colors.text, fontWeight: 700, fontSize: '10px' }}>
                              {feed.name}
                            </span>
                            <span style={{ padding: '2px 6px', backgroundColor: 'rgba(0, 229, 255, 0.15)', color: 'var(--ft-color-accent, #00E5FF)', fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                              {feed.source_name}
                            </span>
                            <span style={{ padding: '2px 6px', backgroundColor: 'rgba(157, 78, 221, 0.15)', color: 'var(--ft-color-purple, #9D4EDD)', fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                              {getCategoryLabel(feed.category)}
                            </span>
                            <span style={{ padding: '2px 6px', backgroundColor: 'rgba(255, 215, 0, 0.15)', color: 'var(--ft-color-warning, #FFD700)', fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                              {getRegionLabel(feed.region)}
                            </span>
                          </div>
                          <div
                            style={{
                              color: colors.textMuted,
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
                              backgroundColor: feed.enabled ? colors.success : colors.textMuted,
                              color: feed.enabled ? colors.background : colors.textMuted,
                              border: 'none',
                              borderRadius: '2px',
                              cursor: 'pointer',
                              fontSize: '9px',
                              fontWeight: 700,
                            }}
                          >
                            {feed.enabled ? t('rssSettings.on') : t('rssSettings.off')}
                          </button>
                          <button
                            onClick={() => handleEditFeed(feed)}
                            style={{
                              padding: '4px 8px',
                              backgroundColor: 'transparent',
                              border: `1px solid var(--ft-color-border, #2A2A2A)`,
                              color: colors.textMuted,
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
                              backgroundColor: colors.alert,
                              color: colors.text,
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
                  backgroundColor: colors.background,
                  border: `1px solid var(--ft-color-border, #2A2A2A)`,
                  borderRadius: '2px',
                  marginBottom: '12px',
                  color: colors.textMuted,
                  fontSize: '9px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                }}
              >
                <span>
                  {t('messages.defaultFeedsInfo')}
                </span>
                {deletedDefaultCount > 0 && (
                  <button
                    onClick={handleRestoreAllDefaults}
                    style={{
                      padding: '4px 10px',
                      backgroundColor: colors.primary,
                      color: colors.background,
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
                    <RefreshCw size={10} /> {t('rssSettings.restoreAll')} ({deletedDefaultCount})
                  </button>
                )}
              </div>

              {/* Empty state for default feeds tab */}
              {defaultFeeds.length === 0 && deletedDefaultCount > 0 && (
                <div
                  style={{
                    textAlign: 'center',
                    padding: '40px 20px',
                    backgroundColor: colors.background,
                    border: `1px solid var(--ft-color-warning, #FFD700)`,
                    borderRadius: '2px',
                    marginBottom: '12px',
                  }}
                >
                  <AlertCircle size={24} style={{ color: 'var(--ft-color-warning, #FFD700)', marginBottom: '12px' }} />
                  <div style={{ color: colors.text, fontSize: fontSize.tiny, fontWeight: 700, marginBottom: '8px' }}>
                    {t('rssSettings.allDefaultsDeleted')}
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: '10px', marginBottom: '16px' }}>
                    {t('messages.allDefaultFeedsDeleted', { count: deletedDefaultCount })}
                  </div>
                  <button
                    onClick={handleRestoreAllDefaults}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: colors.primary,
                      color: colors.background,
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
                    <RefreshCw size={12} /> {t('rssSettings.restoreAllDefaultFeeds')}
                  </button>
                </div>
              )}

              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                {defaultFeeds.map(feed => (
                  <div
                    key={feed.id || feed.url}
                    style={{
                      padding: '10px 12px',
                      backgroundColor: feed.enabled ? colors.background : `${colors.textMuted}10`,
                      border: `1px solid ${feed.enabled ? 'var(--ft-color-border, #2A2A2A)' : colors.textMuted}`,
                      borderRadius: '2px',
                      opacity: feed.enabled ? 1 : 0.7,
                      transition: 'all 0.2s',
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start' }}>
                      <div style={{ flex: 1 }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '4px' }}>
                          <span style={{ color: colors.text, fontWeight: 700, fontSize: '10px' }}>
                            {feed.name}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: `${colors.success}20`, color: colors.success, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            {t('rssSettings.default')}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: 'rgba(0, 229, 255, 0.15)', color: 'var(--ft-color-accent, #00E5FF)', fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            {feed.source}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: 'rgba(157, 78, 221, 0.15)', color: 'var(--ft-color-purple, #9D4EDD)', fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            {getCategoryLabel(feed.category)}
                          </span>
                          <span style={{ padding: '2px 6px', backgroundColor: 'rgba(255, 215, 0, 0.15)', color: 'var(--ft-color-warning, #FFD700)', fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                            {getRegionLabel(feed.region)}
                          </span>
                        </div>
                        <div
                          style={{
                            color: colors.textMuted,
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
                            backgroundColor: feed.enabled ? colors.success : colors.textMuted,
                            color: feed.enabled ? colors.background : colors.textMuted,
                            border: 'none',
                            borderRadius: '2px',
                            cursor: 'pointer',
                            fontSize: '9px',
                            fontWeight: 700,
                          }}
                        >
                          {feed.enabled ? t('rssSettings.on') : t('rssSettings.off')}
                        </button>
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
            borderTop: `1px solid var(--ft-color-border, #2A2A2A)`,
            padding: '6px 16px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            fontSize: '9px',
            color: colors.textMuted,
          }}
        >
          <span>
            {t('rssSettings.total')}: <span style={{ color: 'var(--ft-color-accent, #00E5FF)' }}>{defaultFeeds.length + userFeeds.filter(f => f.enabled).length}</span> {t('rssSettings.active')}
            <span style={{ color: 'var(--ft-color-border, #2A2A2A)' }}> | </span>
            <span style={{ color: 'var(--ft-color-accent, #00E5FF)' }}>{defaultFeeds.length}</span> {t('rssSettings.default')}
            <span style={{ color: 'var(--ft-color-border, #2A2A2A)' }}> | </span>
            <span style={{ color: 'var(--ft-color-accent, #00E5FF)' }}>{userFeeds.filter(f => f.enabled).length}</span> {t('rssSettings.custom')}
          </span>
          <button
            onClick={loadFeeds}
            style={{
              padding: '4px 10px',
              backgroundColor: 'transparent',
              border: `1px solid var(--ft-color-border, #2A2A2A)`,
              color: colors.textMuted,
              borderRadius: '2px',
              cursor: 'pointer',
              fontSize: '9px',
              fontWeight: 700,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <RefreshCw size={10} /> {t('header.refresh')}
          </button>
        </div>
      </div>
    </div>
  );
};

export default RSSFeedSettingsModal;
