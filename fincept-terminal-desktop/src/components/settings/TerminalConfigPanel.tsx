import React, { useState, useEffect } from 'react';
import { DndContext, closestCenter, KeyboardSensor, PointerSensor, useSensor, useSensors, DragEndEvent } from '@dnd-kit/core';
import { arrayMove, SortableContext, sortableKeyboardCoordinates, verticalListSortingStrategy, useSortable } from '@dnd-kit/sortable';
import { CSS } from '@dnd-kit/utilities';
import { tabConfigService } from '@/services/tabConfigService';
import { TabConfiguration, MenuSection, DEFAULT_TABS } from '@/types/tabConfig';
import { GripVertical, Plus, Trash2, RotateCcw, ArrowRight, ArrowLeft } from 'lucide-react';

const SortableTab = ({ id, label }: { id: string; label: string }) => {
  const { attributes, listeners, setNodeRef, transform, transition } = useSortable({ id });

  const style = {
    transform: CSS.Transform.toString(transform),
    transition
  };

  return (
    <div ref={setNodeRef} style={style} {...attributes} {...listeners}
      className="flex items-center gap-2 bg-zinc-800 border border-zinc-700 rounded px-3 py-2 cursor-move hover:bg-zinc-700">
      <GripVertical size={16} className="text-zinc-500" />
      <span className="text-sm text-white">{label}</span>
    </div>
  );
};

export const TerminalConfigPanel: React.FC = () => {
  const [config, setConfig] = useState<TabConfiguration>(tabConfigService.getConfiguration());
  const [newSectionName, setNewSectionName] = useState('');
  const [showNewSection, setShowNewSection] = useState(false);

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

  const resetToDefault = () => {
    if (confirm('Reset tab configuration to default? This cannot be undone.')) {
      tabConfigService.resetToDefault();
      setConfig(tabConfigService.getConfiguration());
    }
  };

  const getTabLabel = (tabId: string) => {
    return DEFAULT_TABS.find(t => t.id === tabId)?.label || tabId;
  };

  return (
    <div className="p-6 space-y-6 max-w-6xl">
      <div className="flex items-center justify-between">
        <h2 className="text-xl font-bold text-white">Terminal Tab Configuration</h2>
        <button
          onClick={resetToDefault}
          className="flex items-center gap-2 px-4 py-2 bg-red-600 hover:bg-red-700 text-white rounded text-sm"
        >
          <RotateCcw size={16} />
          Reset to Default
        </button>
      </div>

      {/* Header Tabs */}
      <div className="bg-zinc-900 border border-zinc-800 rounded-lg p-4">
        <h3 className="text-lg font-semibold text-white mb-3">Header Tabs (Main Tab Bar)</h3>
        <p className="text-sm text-zinc-400 mb-4">Drag to reorder. These tabs appear in the main header.</p>

        <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleHeaderDragEnd}>
          <SortableContext items={config.headerTabs} strategy={verticalListSortingStrategy}>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-2">
              {config.headerTabs.map((tabId) => (
                <SortableTab key={tabId} id={tabId} label={getTabLabel(tabId)} />
              ))}
            </div>
          </SortableContext>
        </DndContext>
      </div>

      {/* Menu Sections */}
      <div className="space-y-4">
        <div className="flex items-center justify-between">
          <h3 className="text-lg font-semibold text-white">Toolbar Menu Sections</h3>
          <button
            onClick={() => setShowNewSection(true)}
            className="flex items-center gap-2 px-3 py-1.5 bg-blue-600 hover:bg-blue-700 text-white rounded text-sm"
          >
            <Plus size={16} />
            New Section
          </button>
        </div>

        {showNewSection && (
          <div className="bg-zinc-900 border border-zinc-700 rounded-lg p-4 flex gap-2">
            <input
              type="text"
              value={newSectionName}
              onChange={(e) => setNewSectionName(e.target.value)}
              placeholder="Section name..."
              className="flex-1 bg-zinc-800 border border-zinc-700 rounded px-3 py-2 text-white text-sm"
              onKeyPress={(e) => e.key === 'Enter' && addNewSection()}
            />
            <button onClick={addNewSection} className="px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded text-sm">
              Add
            </button>
            <button onClick={() => setShowNewSection(false)} className="px-4 py-2 bg-zinc-700 hover:bg-zinc-600 text-white rounded text-sm">
              Cancel
            </button>
          </div>
        )}

        {config.menuSections.map((section) => (
          <div key={section.id} className="bg-zinc-900 border border-zinc-800 rounded-lg p-4">
            <div className="flex items-center justify-between mb-3">
              <h4 className="font-semibold text-white">{section.label}</h4>
              <button
                onClick={() => deleteSection(section.id)}
                className="flex items-center gap-1 px-2 py-1 text-red-400 hover:text-red-300 text-sm"
              >
                <Trash2 size={14} />
                Delete
              </button>
            </div>

            <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleMenuDragEnd(section.id)}>
              <SortableContext items={section.tabs} strategy={verticalListSortingStrategy}>
                <div className="grid grid-cols-1 md:grid-cols-2 gap-2">
                  {section.tabs.map((tabId) => (
                    <div key={tabId} className="flex items-center gap-2">
                      <SortableTab id={tabId} label={getTabLabel(tabId)} />
                      <button
                        onClick={() => moveToHeader(tabId, section.id)}
                        className="p-2 bg-zinc-700 hover:bg-zinc-600 rounded"
                        title="Move to header"
                      >
                        <ArrowRight size={16} className="text-white" />
                      </button>
                    </div>
                  ))}
                </div>
              </SortableContext>
            </DndContext>

            {section.tabs.length === 0 && (
              <p className="text-sm text-zinc-500 italic">No tabs in this section</p>
            )}
          </div>
        ))}
      </div>

      <div className="bg-blue-900/20 border border-blue-500/50 rounded-lg p-4">
        <p className="text-sm text-blue-300">
          <strong>Tip:</strong> Drag tabs to reorder them. Use arrow buttons to move tabs between header and menu sections. Changes are saved automatically.
        </p>
      </div>
    </div>
  );
};
