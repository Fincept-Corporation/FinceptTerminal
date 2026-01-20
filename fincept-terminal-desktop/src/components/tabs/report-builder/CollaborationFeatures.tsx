import React, { useState, useEffect } from 'react';
import {
  MessageSquare,
  Clock,
  User,
  Plus,
  X,
  ChevronDown,
  ChevronRight,
  History,
  RotateCcw,
  Check,
  Edit3,
  Trash2,
  Eye,
  EyeOff,
} from 'lucide-react';
import { ReportTemplate, ReportComponent } from '@/services/core/reportService';

// ============ Types ============
export interface Comment {
  id: string;
  componentId: string;
  author: string;
  content: string;
  timestamp: string;
  resolved?: boolean;
  replies?: Comment[];
}

export interface VersionHistoryEntry {
  id: string;
  timestamp: string;
  author: string;
  description: string;
  template: ReportTemplate;
  changes: {
    type: 'add' | 'delete' | 'modify';
    componentId?: string;
    field?: string;
    oldValue?: any;
    newValue?: any;
  }[];
}

export interface TrackedChange {
  id: string;
  componentId: string;
  type: 'addition' | 'deletion' | 'modification';
  field?: string;
  oldValue?: any;
  newValue?: any;
  timestamp: string;
  author: string;
  accepted?: boolean;
}

// ============ Comments Panel ============
interface CommentsPanelProps {
  comments: Comment[];
  selectedComponentId: string | null;
  onAddComment: (componentId: string, content: string) => void;
  onResolveComment: (commentId: string) => void;
  onDeleteComment: (commentId: string) => void;
  onReplyComment: (commentId: string, content: string) => void;
}

export const CommentsPanel: React.FC<CommentsPanelProps> = ({
  comments,
  selectedComponentId,
  onAddComment,
  onResolveComment,
  onDeleteComment,
  onReplyComment,
}) => {
  const [newComment, setNewComment] = useState('');
  const [replyingTo, setReplyingTo] = useState<string | null>(null);
  const [replyContent, setReplyContent] = useState('');
  const [showResolved, setShowResolved] = useState(false);

  const filteredComments = comments.filter(c =>
    (selectedComponentId ? c.componentId === selectedComponentId : true) &&
    (showResolved || !c.resolved)
  );

  const handleAddComment = () => {
    if (!newComment.trim() || !selectedComponentId) return;
    onAddComment(selectedComponentId, newComment.trim());
    setNewComment('');
  };

  const handleReply = (commentId: string) => {
    if (!replyContent.trim()) return;
    onReplyComment(commentId, replyContent.trim());
    setReplyContent('');
    setReplyingTo(null);
  };

  return (
    <div className="flex flex-col h-full">
      <div className="flex items-center justify-between p-3 border-b border-[#333333]">
        <div className="flex items-center gap-2">
          <MessageSquare size={14} className="text-[#FFA500]" />
          <span className="text-xs font-semibold text-[#FFA500]">COMMENTS</span>
          <span className="text-[10px] text-gray-500">({filteredComments.length})</span>
        </div>
        <button
          onClick={() => setShowResolved(!showResolved)}
          className="text-[10px] text-gray-400 hover:text-white"
        >
          {showResolved ? 'Hide' : 'Show'} resolved
        </button>
      </div>

      {/* Comments List */}
      <div className="flex-1 overflow-y-auto p-3 space-y-3">
        {filteredComments.length === 0 ? (
          <p className="text-xs text-gray-500 text-center py-4">
            {selectedComponentId ? 'No comments on this component' : 'Select a component to view comments'}
          </p>
        ) : (
          filteredComments.map(comment => (
            <CommentItem
              key={comment.id}
              comment={comment}
              onResolve={() => onResolveComment(comment.id)}
              onDelete={() => onDeleteComment(comment.id)}
              onReply={() => setReplyingTo(comment.id)}
              isReplying={replyingTo === comment.id}
              replyContent={replyContent}
              setReplyContent={setReplyContent}
              handleReply={() => handleReply(comment.id)}
              cancelReply={() => setReplyingTo(null)}
            />
          ))
        )}
      </div>

      {/* Add Comment */}
      {selectedComponentId && (
        <div className="p-3 border-t border-[#333333]">
          <div className="flex gap-2">
            <input
              type="text"
              value={newComment}
              onChange={(e) => setNewComment(e.target.value)}
              placeholder="Add a comment..."
              className="flex-1 px-3 py-2 text-xs bg-[#0a0a0a] border border-[#333333] rounded text-white focus:border-[#FFA500] outline-none"
              onKeyPress={(e) => e.key === 'Enter' && handleAddComment()}
            />
            <button
              onClick={handleAddComment}
              disabled={!newComment.trim()}
              className="px-3 py-2 bg-[#FFA500] text-black rounded text-xs disabled:opacity-50"
            >
              <Plus size={14} />
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

// Comment Item
interface CommentItemProps {
  comment: Comment;
  onResolve: () => void;
  onDelete: () => void;
  onReply: () => void;
  isReplying: boolean;
  replyContent: string;
  setReplyContent: (v: string) => void;
  handleReply: () => void;
  cancelReply: () => void;
}

const CommentItem: React.FC<CommentItemProps> = ({
  comment,
  onResolve,
  onDelete,
  onReply,
  isReplying,
  replyContent,
  setReplyContent,
  handleReply,
  cancelReply,
}) => (
  <div className={`p-3 rounded border ${comment.resolved ? 'bg-[#0a0a0a] border-[#222222] opacity-60' : 'bg-[#1a1a1a] border-[#333333]'}`}>
    <div className="flex items-start justify-between mb-2">
      <div className="flex items-center gap-2">
        <div className="w-6 h-6 rounded-full bg-[#FFA500]/20 flex items-center justify-center">
          <User size={12} className="text-[#FFA500]" />
        </div>
        <div>
          <p className="text-xs font-semibold text-white">{comment.author}</p>
          <p className="text-[10px] text-gray-500">{new Date(comment.timestamp).toLocaleString()}</p>
        </div>
      </div>
      <div className="flex gap-1">
        {!comment.resolved && (
          <button onClick={onResolve} className="p-1 hover:bg-[#2a2a2a] rounded" title="Resolve">
            <Check size={12} className="text-green-500" />
          </button>
        )}
        <button onClick={onDelete} className="p-1 hover:bg-[#2a2a2a] rounded" title="Delete">
          <Trash2 size={12} className="text-red-500" />
        </button>
      </div>
    </div>
    <p className="text-xs text-gray-300 mb-2">{comment.content}</p>

    {/* Replies */}
    {comment.replies && comment.replies.length > 0 && (
      <div className="ml-4 mt-2 space-y-2 border-l border-[#333333] pl-3">
        {comment.replies.map(reply => (
          <div key={reply.id} className="text-xs">
            <span className="font-semibold text-gray-400">{reply.author}: </span>
            <span className="text-gray-300">{reply.content}</span>
          </div>
        ))}
      </div>
    )}

    {/* Reply Input */}
    {isReplying ? (
      <div className="mt-2 flex gap-2">
        <input
          type="text"
          value={replyContent}
          onChange={(e) => setReplyContent(e.target.value)}
          placeholder="Reply..."
          className="flex-1 px-2 py-1 text-xs bg-[#0a0a0a] border border-[#333333] rounded text-white"
          autoFocus
        />
        <button onClick={handleReply} className="text-[#FFA500] text-xs">Send</button>
        <button onClick={cancelReply} className="text-gray-500 text-xs">Cancel</button>
      </div>
    ) : (
      <button onClick={onReply} className="text-xs text-gray-500 hover:text-[#FFA500] mt-1">
        Reply
      </button>
    )}
  </div>
);

// ============ Version History Panel ============
interface VersionHistoryPanelProps {
  history: VersionHistoryEntry[];
  onRestore: (entry: VersionHistoryEntry) => void;
  onPreview: (entry: VersionHistoryEntry) => void;
}

export const VersionHistoryPanel: React.FC<VersionHistoryPanelProps> = ({
  history,
  onRestore,
  onPreview,
}) => {
  const [expandedId, setExpandedId] = useState<string | null>(null);

  return (
    <div className="flex flex-col h-full">
      <div className="flex items-center gap-2 p-3 border-b border-[#333333]">
        <History size={14} className="text-[#FFA500]" />
        <span className="text-xs font-semibold text-[#FFA500]">VERSION HISTORY</span>
      </div>

      <div className="flex-1 overflow-y-auto">
        {history.length === 0 ? (
          <p className="text-xs text-gray-500 text-center py-8">No version history</p>
        ) : (
          <div className="divide-y divide-[#333333]">
            {history.map((entry, idx) => (
              <div key={entry.id} className="p-3">
                <div
                  className="flex items-center justify-between cursor-pointer"
                  onClick={() => setExpandedId(expandedId === entry.id ? null : entry.id)}
                >
                  <div className="flex items-center gap-2">
                    {expandedId === entry.id ? (
                      <ChevronDown size={12} className="text-gray-500" />
                    ) : (
                      <ChevronRight size={12} className="text-gray-500" />
                    )}
                    <div>
                      <p className="text-xs text-white">{entry.description}</p>
                      <p className="text-[10px] text-gray-500">
                        {entry.author} • {new Date(entry.timestamp).toLocaleString()}
                      </p>
                    </div>
                  </div>
                  {idx === 0 && (
                    <span className="text-[10px] px-2 py-0.5 bg-[#FFA500]/20 text-[#FFA500] rounded">
                      Current
                    </span>
                  )}
                </div>

                {expandedId === entry.id && (
                  <div className="mt-3 ml-5 space-y-2">
                    {/* Changes */}
                    <div className="text-[10px] text-gray-500">
                      {entry.changes.length} change(s)
                    </div>
                    {entry.changes.slice(0, 3).map((change, i) => (
                      <div key={i} className="text-xs text-gray-400 flex items-center gap-2">
                        <span className={`w-2 h-2 rounded-full ${
                          change.type === 'add' ? 'bg-green-500' :
                          change.type === 'delete' ? 'bg-red-500' : 'bg-yellow-500'
                        }`} />
                        <span className="capitalize">{change.type}</span>
                        {change.field && <span>- {change.field}</span>}
                      </div>
                    ))}

                    {/* Actions */}
                    {idx !== 0 && (
                      <div className="flex gap-2 mt-2">
                        <button
                          onClick={() => onPreview(entry)}
                          className="text-xs px-2 py-1 bg-[#2a2a2a] text-gray-300 rounded hover:bg-[#3a3a3a]"
                        >
                          <Eye size={12} className="inline mr-1" />
                          Preview
                        </button>
                        <button
                          onClick={() => onRestore(entry)}
                          className="text-xs px-2 py-1 bg-[#FFA500] text-black rounded hover:bg-[#ff8c00]"
                        >
                          <RotateCcw size={12} className="inline mr-1" />
                          Restore
                        </button>
                      </div>
                    )}
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};

// ============ Revision Mode (Track Changes) ============
interface RevisionModeProps {
  changes: TrackedChange[];
  enabled: boolean;
  onToggle: () => void;
  onAcceptChange: (changeId: string) => void;
  onRejectChange: (changeId: string) => void;
  onAcceptAll: () => void;
  onRejectAll: () => void;
}

export const RevisionMode: React.FC<RevisionModeProps> = ({
  changes,
  enabled,
  onToggle,
  onAcceptChange,
  onRejectChange,
  onAcceptAll,
  onRejectAll,
}) => {
  const pendingChanges = changes.filter(c => c.accepted === undefined);

  return (
    <div className="flex flex-col h-full">
      <div className="flex items-center justify-between p-3 border-b border-[#333333]">
        <div className="flex items-center gap-2">
          <Edit3 size={14} className="text-[#FFA500]" />
          <span className="text-xs font-semibold text-[#FFA500]">TRACK CHANGES</span>
        </div>
        <button
          onClick={onToggle}
          className={`px-2 py-1 text-xs rounded ${
            enabled ? 'bg-[#FFA500] text-black' : 'bg-[#333333] text-gray-400'
          }`}
        >
          {enabled ? 'ON' : 'OFF'}
        </button>
      </div>

      {enabled && (
        <>
          {/* Bulk Actions */}
          {pendingChanges.length > 0 && (
            <div className="flex gap-2 p-3 border-b border-[#333333]">
              <button
                onClick={onAcceptAll}
                className="flex-1 py-1.5 text-xs bg-green-600 text-white rounded hover:bg-green-700"
              >
                Accept All ({pendingChanges.length})
              </button>
              <button
                onClick={onRejectAll}
                className="flex-1 py-1.5 text-xs bg-red-600 text-white rounded hover:bg-red-700"
              >
                Reject All
              </button>
            </div>
          )}

          {/* Changes List */}
          <div className="flex-1 overflow-y-auto p-3 space-y-2">
            {changes.length === 0 ? (
              <p className="text-xs text-gray-500 text-center py-4">No tracked changes</p>
            ) : (
              changes.map(change => (
                <div
                  key={change.id}
                  className={`p-2 rounded border text-xs ${
                    change.accepted === true ? 'bg-green-900/20 border-green-800' :
                    change.accepted === false ? 'bg-red-900/20 border-red-800' :
                    'bg-[#1a1a1a] border-[#333333]'
                  }`}
                >
                  <div className="flex items-center justify-between mb-1">
                    <div className="flex items-center gap-2">
                      <span className={`w-2 h-2 rounded-full ${
                        change.type === 'addition' ? 'bg-green-500' :
                        change.type === 'deletion' ? 'bg-red-500' : 'bg-yellow-500'
                      }`} />
                      <span className="capitalize text-gray-300">{change.type}</span>
                    </div>
                    {change.accepted === undefined && (
                      <div className="flex gap-1">
                        <button
                          onClick={() => onAcceptChange(change.id)}
                          className="p-1 hover:bg-green-600/30 rounded"
                        >
                          <Check size={12} className="text-green-500" />
                        </button>
                        <button
                          onClick={() => onRejectChange(change.id)}
                          className="p-1 hover:bg-red-600/30 rounded"
                        >
                          <X size={12} className="text-red-500" />
                        </button>
                      </div>
                    )}
                  </div>
                  {change.field && (
                    <p className="text-gray-500 text-[10px]">Field: {change.field}</p>
                  )}
                  {change.type === 'modification' && (
                    <div className="mt-1 text-[10px]">
                      <p className="text-red-400 line-through">{String(change.oldValue).substring(0, 50)}</p>
                      <p className="text-green-400">{String(change.newValue).substring(0, 50)}</p>
                    </div>
                  )}
                  <p className="text-[10px] text-gray-600 mt-1">
                    {change.author} • {new Date(change.timestamp).toLocaleString()}
                  </p>
                </div>
              ))
            )}
          </div>
        </>
      )}
    </div>
  );
};

// ============ Hook for managing collaboration state ============
export const useCollaboration = (template: ReportTemplate) => {
  const [comments, setComments] = useState<Comment[]>([]);
  const [versionHistory, setVersionHistory] = useState<VersionHistoryEntry[]>([]);
  const [trackedChanges, setTrackedChanges] = useState<TrackedChange[]>([]);
  const [isTrackingEnabled, setIsTrackingEnabled] = useState(false);

  // Add comment
  const addComment = (componentId: string, content: string) => {
    const newComment: Comment = {
      id: `comment-${Date.now()}`,
      componentId,
      author: 'User',
      content,
      timestamp: new Date().toISOString(),
      replies: [],
    };
    setComments(prev => [...prev, newComment]);
  };

  // Resolve comment
  const resolveComment = (commentId: string) => {
    setComments(prev => prev.map(c =>
      c.id === commentId ? { ...c, resolved: true } : c
    ));
  };

  // Delete comment
  const deleteComment = (commentId: string) => {
    setComments(prev => prev.filter(c => c.id !== commentId));
  };

  // Reply to comment
  const replyToComment = (commentId: string, content: string) => {
    const reply: Comment = {
      id: `reply-${Date.now()}`,
      componentId: '',
      author: 'User',
      content,
      timestamp: new Date().toISOString(),
    };
    setComments(prev => prev.map(c =>
      c.id === commentId ? { ...c, replies: [...(c.replies || []), reply] } : c
    ));
  };

  // Save version
  const saveVersion = (description: string, changes: VersionHistoryEntry['changes']) => {
    const entry: VersionHistoryEntry = {
      id: `version-${Date.now()}`,
      timestamp: new Date().toISOString(),
      author: 'User',
      description,
      template: JSON.parse(JSON.stringify(template)),
      changes,
    };
    setVersionHistory(prev => [entry, ...prev]);
  };

  // Track change
  const trackChange = (change: Omit<TrackedChange, 'id' | 'timestamp' | 'author'>) => {
    if (!isTrackingEnabled) return;
    const tracked: TrackedChange = {
      ...change,
      id: `change-${Date.now()}`,
      timestamp: new Date().toISOString(),
      author: 'User',
    };
    setTrackedChanges(prev => [...prev, tracked]);
  };

  // Accept/Reject changes
  const acceptChange = (changeId: string) => {
    setTrackedChanges(prev => prev.map(c =>
      c.id === changeId ? { ...c, accepted: true } : c
    ));
  };

  const rejectChange = (changeId: string) => {
    setTrackedChanges(prev => prev.map(c =>
      c.id === changeId ? { ...c, accepted: false } : c
    ));
  };

  const acceptAllChanges = () => {
    setTrackedChanges(prev => prev.map(c =>
      c.accepted === undefined ? { ...c, accepted: true } : c
    ));
  };

  const rejectAllChanges = () => {
    setTrackedChanges(prev => prev.map(c =>
      c.accepted === undefined ? { ...c, accepted: false } : c
    ));
  };

  return {
    comments,
    versionHistory,
    trackedChanges,
    isTrackingEnabled,
    addComment,
    resolveComment,
    deleteComment,
    replyToComment,
    saveVersion,
    trackChange,
    acceptChange,
    rejectChange,
    acceptAllChanges,
    rejectAllChanges,
    setIsTrackingEnabled,
  };
};

export default {
  CommentsPanel,
  VersionHistoryPanel,
  RevisionMode,
  useCollaboration,
};
