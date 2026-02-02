import React, { useState, useEffect } from 'react';
import { DndContext, closestCenter, KeyboardSensor, PointerSensor, useSensor, useSensors, DragEndEvent } from '@dnd-kit/core';
import { arrayMove, SortableContext, sortableKeyboardCoordinates, verticalListSortingStrategy, useSortable } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { tabConfigService } from '@/services/core/tabConfigService';
import { TabConfiguration, MenuSection, DEFAULT_TABS, DEFAULT_TAB_CONFIG } from '@/types/tabConfig';
import { GripVertical, Plus, Trash2, RotateCcw, ArrowRight, ArrowLeft } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useTranslation } from 'react-i18next';
import { showConfirm, showSuccess, showError } from '@/utils/notifications';

const SortableTab = ({ id, label, colors }: { id: string; label: string; colors: any }) => {
  const { attributes, listeners, setNodeRef, transform, transition } = useSortable({ id });
  const [isHovered, setIsHovered] = useState(false);

  const style = {
    transform: CSS.Transform.toString(transform),
    transition,
    display: 'flex',
    alignItems: 'center',
    gap: '8px',
    background: isHovered ? '#1a1a1a' : colors.panel,
    border: '1px solid #2a2a2a',
    borderRadius: '3px',
    padding: '10px 12px',
    cursor: 'move',
    fontSize: '11px',
    fontWeight: 500
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      {...attributes}
      {...listeners}
      onMouseEnter={() => setIsHovered(true)}
      onMouseLeave={() => setIsHovered(false)}
    >
      <GripVertical size={14} color="#666" />
      <span style={{ color: colors.text }}>{label}</span>
    </div>
  );
};

export const TerminalConfigPanel: React.FC = () => {
  const { t } = useTranslation('settings');
  const { colors } = useTerminalTheme();
  const [config, setConfig] = useState<TabConfiguration>(DEFAULT_TAB_CONFIG);
  const [newSectionName, setNewSectionName] = useState('');
  const [showNewSection, setShowNewSection] = useState(false);

  // Load configuration on mount
  useEffect(() => {
    const loadConfig = async () => {
      const loadedConfig = await tabConfigService.getConfiguration();
      setConfig(loadedConfig);
    };
    loadConfig();
  }, []);

  const sensors = useSensors(
    useSensor(PointerSensor),
    useSensor(KeyboardSensor, { coordinateGetter: sortableKeyboardCoordinates })
  );

  const handleHeaderDragEnd = (event: DragEndEvent) => {
    const { active, over } = event;
    if (over && active.id !== over.id) {
      setConfig((prev) => {
        const oldIndex = prev.headerTabs.indexOf(active.id as string);
        const newIndex = prev.headerTabs.indexOf(over.id as string);
        const newHeaderTabs = arrayMove(prev.headerTabs, oldIndex, newIndex);
        const newConfig = { ...prev, headerTabs: newHeaderTabs };
        tabConfigService.saveConfiguration(newConfig);
        return newConfig;
      });
    }
  };

  const handleMenuDragEnd = (sectionId: string) => (event: DragEndEvent) => {
    const { active, over } = event;
    if (over && active.id !== over.id) {
      setConfig((prev) => {
        const section = prev.menuSections.find(s => s.id === sectionId);
        if (!section) return prev;

        const oldIndex = section.tabs.indexOf(active.id as string);
        const newIndex = section.tabs.indexOf(over.id as string);
        const newTabs = arrayMove(section.tabs, oldIndex, newIndex);

        const newConfig = {
          ...prev,
          menuSections: prev.menuSections.map(s =>
            s.id === sectionId ? { ...s, tabs: newTabs } : s
          )
        };
        tabConfigService.saveConfiguration(newConfig);
        return newConfig;
      });
    }
  };

  const moveToHeader = (tabId: string, fromSection: string) => {
    setConfig((prev) => {
      const newConfig = {
        ...prev,
        headerTabs: [...prev.headerTabs, tabId],
        menuSections: prev.menuSections.map(s =>
          s.id === fromSection ? { ...s, tabs: s.tabs.filter(t => t !== tabId) } : s
        )
      };
      tabConfigService.saveConfiguration(newConfig);
      return newConfig;
    });
  };

  const moveToMenu = (tabId: string, toSection: string) => {
    setConfig((prev) => {
      const newConfig = {
        ...prev,
        headerTabs: prev.headerTabs.filter(t => t !== tabId),
        menuSections: prev.menuSections.map(s =>
          s.id === toSection ? { ...s, tabs: [...s.tabs, tabId] } : s
        )
      };
      tabConfigService.saveConfiguration(newConfig);
      return newConfig;
    });
  };

  const addNewSection = () => {
    if (!newSectionName.trim()) return;

    setConfig((prev) => {
      const newConfig = {
        ...prev,
        menuSections: [
          ...prev.menuSections,
          { id: `custom-${Date.now()}`, label: newSectionName, tabs: [] }
        ]
      };
      tabConfigService.saveConfiguration(newConfig);
      return newConfig;
    });
    setNewSectionName('');
    setShowNewSection(false);
  };

  const deleteSection = (sectionId: string) => {
    setConfig((prev) => {
      const section = prev.menuSections.find(s => s.id === sectionId);
      if (!section) return prev;

      const newConfig = {
        ...prev,
        headerTabs: [...prev.headerTabs, ...section.tabs],
        menuSections: prev.menuSections.filter(s => s.id !== sectionId)
      };
      tabConfigService.saveConfiguration(newConfig);
      return newConfig;
    });
  };

  const resetToDefault = async () => {
    const confirmed = await showConfirm(
      'Reset tab configuration to default?\n\nAll custom tab visibility settings will be lost. This cannot be undone.',
      { title: 'Reset Configuration', type: 'warning', confirmText: 'Reset' }
    );

    if (!confirmed) return;

    try {
      await tabConfigService.resetToDefault();
      const loadedConfig = await tabConfigService.getConfiguration();
      setConfig(loadedConfig);
      showSuccess('Configuration reset to default');
    } catch (error) {
      showError('Failed to reset configuration');
    }
  };

  const getTabLabel = (tabId: string) => {
    return DEFAULT_TABS.find(t => t.id === tabId)?.label || tabId;
  };

  return (
    <div style={{ padding: '24px', maxWidth: '1400px' }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        marginBottom: '24px',
        paddingBottom: '12px',
        borderBottom: `2px solid ${colors.primary}`
      }}>
        <h2 style={{
          color: colors.primary,
          fontSize: '14px',
          fontWeight: 'bold',
          letterSpacing: '1px',
          margin: 0
        }}>
          {t('terminalConfig.title')}
        </h2>
        <button
          onClick={resetToDefault}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            padding: '8px 16px',
            background: '#3a0a0a',
            border: '1px solid #ff4444',
            color: '#ff4444',
            borderRadius: '3px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: 'pointer',
            letterSpacing: '0.5px'
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.background = '#4a0a0a';
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.background = '#3a0a0a';
          }}
        >
          <RotateCcw size={14} />
          {t('terminalConfig.resetToDefault')}
        </button>
      </div>

      {/* Header Tabs */}
      <div style={{
        background: colors.panel,
        border: '1px solid #1a1a1a',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '16px',
        marginBottom: '20px'
      }}>
        <h3 style={{
          color: colors.text,
          fontSize: '12px',
          fontWeight: 'bold',
          marginBottom: '8px',
          letterSpacing: '0.5px'
        }}>
          {t('terminalConfig.headerTabs')}
        </h3>
        <p style={{
          color: colors.textMuted,
          fontSize: '10px',
          marginBottom: '16px'
        }}>
          {t('terminalConfig.headerTabsDesc')}
        </p>

        <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleHeaderDragEnd}>
          <SortableContext items={config.headerTabs} strategy={verticalListSortingStrategy}>
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fill, minmax(250px, 1fr))',
              gap: '8px'
            }}>
              {config.headerTabs.map((tabId) => (
                <div key={tabId} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <div style={{ flex: 1 }}>
                    <SortableTab id={tabId} label={getTabLabel(tabId)} colors={colors} />
                  </div>
                  <div style={{ position: 'relative' }}>
                    <select
                      onChange={(e) => {
                        if (e.target.value) {
                          moveToMenu(tabId, e.target.value);
                          e.target.value = ''; // Reset selection
                        }
                      }}
                      style={{
                        padding: '8px',
                        background: '#2a2a2a',
                        border: '1px solid #3a3a3a',
                        color: colors.primary,
                        borderRadius: '3px',
                        cursor: 'pointer',
                        fontSize: '10px',
                        fontWeight: 'bold',
                        outline: 'none'
                      }}
                      title="Move to menu section"
                      onMouseEnter={(e) => {
                        e.currentTarget.style.background = '#3a3a3a';
                      }}
                      onMouseLeave={(e) => {
                        e.currentTarget.style.background = '#2a2a2a';
                      }}
                    >
                      <option value="">{t('terminalConfig.moveTo')}</option>
                      {config.menuSections.map((section) => (
                        <option key={section.id} value={section.id}>
                          {section.label}
                        </option>
                      ))}
                    </select>
                  </div>
                </div>
              ))}
            </div>
          </SortableContext>
        </DndContext>
      </div>

      {/* Menu Sections */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between'
        }}>
          <h3 style={{
            color: colors.text,
            fontSize: '12px',
            fontWeight: 'bold',
            letterSpacing: '0.5px'
          }}>
            {t('terminalConfig.toolbarSections')}
          </h3>
          <button
            onClick={() => setShowNewSection(true)}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '6px 12px',
              background: colors.primary,
              border: 'none',
              color: colors.background,
              borderRadius: '3px',
              fontSize: '10px',
              fontWeight: 'bold',
              cursor: 'pointer',
              letterSpacing: '0.5px'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.9';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}
          >
            <Plus size={14} />
            {t('terminalConfig.newSection')}
          </button>
        </div>

        {showNewSection && (
          <div style={{
            background: colors.panel,
            border: '1px solid #2a2a2a',
            borderRadius: '4px',
            padding: '16px',
            display: 'flex',
            gap: '8px',
            alignItems: 'center'
          }}>
            <input
              type="text"
              value={newSectionName}
              onChange={(e) => setNewSectionName(e.target.value)}
              placeholder="Section name..."
              style={{
                flex: 1,
                background: colors.background,
                border: '1px solid #2a2a2a',
                borderRadius: '3px',
                padding: '8px 12px',
                color: colors.text,
                fontSize: '11px',
                outline: 'none'
              }}
              onKeyPress={(e) => e.key === 'Enter' && addNewSection()}
            />
            <button
              onClick={addNewSection}
              style={{
                padding: '8px 16px',
                background: '#0a3a0a',
                border: '1px solid #00ff00',
                color: '#00ff00',
                borderRadius: '3px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer',
                whiteSpace: 'nowrap'
              }}
            >
              {t('buttons.add')}
            </button>
            <button
              onClick={() => setShowNewSection(false)}
              style={{
                padding: '8px 16px',
                background: '#2a2a2a',
                border: '1px solid #3a3a3a',
                color: colors.text,
                borderRadius: '3px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: 'pointer',
                whiteSpace: 'nowrap'
              }}
            >
              {t('buttons.cancel')}
            </button>
          </div>
        )}

        {config.menuSections.map((section) => (
          <div key={section.id} style={{
            background: colors.panel,
            border: '1px solid #1a1a1a',
            borderRadius: '4px',
            padding: '16px'
          }}>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              marginBottom: '12px'
            }}>
              <h4 style={{
                color: colors.text,
                fontSize: '11px',
                fontWeight: 'bold',
                letterSpacing: '0.5px'
              }}>
                {section.label}
              </h4>
              <button
                onClick={() => deleteSection(section.id)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  padding: '4px 8px',
                  background: 'transparent',
                  border: '1px solid #ff4444',
                  color: '#ff4444',
                  borderRadius: '3px',
                  fontSize: '9px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.background = '#3a0a0a';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.background = 'transparent';
                }}
              >
                <Trash2 size={12} />
                {t('terminalConfig.deleteSection')}
              </button>
            </div>

            <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleMenuDragEnd(section.id)}>
              <SortableContext items={section.tabs} strategy={verticalListSortingStrategy}>
                <div style={{
                  display: 'grid',
                  gridTemplateColumns: 'repeat(auto-fill, minmax(250px, 1fr))',
                  gap: '8px'
                }}>
                  {section.tabs.map((tabId) => (
                    <div key={tabId} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                      <div style={{ flex: 1 }}>
                        <SortableTab id={tabId} label={getTabLabel(tabId)} colors={colors} />
                      </div>
                      <button
                        onClick={() => moveToHeader(tabId, section.id)}
                        title="Move to header"
                        style={{
                          padding: '8px',
                          background: '#2a2a2a',
                          border: '1px solid #3a3a3a',
                          borderRadius: '3px',
                          cursor: 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.background = '#3a3a3a';
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.background = '#2a2a2a';
                        }}
                      >
                        <ArrowRight size={14} color={colors.primary} />
                      </button>
                    </div>
                  ))}
                </div>
              </SortableContext>
            </DndContext>

            {section.tabs.length === 0 && (
              <p style={{
                color: '#666',
                fontSize: '10px',
                fontStyle: 'italic',
                margin: 0
              }}>
                {t('terminalConfig.noTabsInSection')}
              </p>
            )}
          </div>
        ))}
      </div>

      {/* Info Box */}
      <div style={{
        marginTop: '20px',
        background: '#0a1a2a',
        border: '1px solid #1a4a6a',
        borderLeft: `3px solid ${colors.primary}`,
        borderRadius: '4px',
        padding: '12px'
      }}>
        <p style={{
          color: colors.primary,
          fontSize: '10px',
          margin: 0,
          lineHeight: '1.5'
        }}>
          {t('terminalConfig.tip')}
        </p>
      </div>
    </div>
  );
};
