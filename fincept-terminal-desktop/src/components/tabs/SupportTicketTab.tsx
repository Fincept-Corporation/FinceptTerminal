import React, { useState, useEffect } from 'react';
import { useAuth } from '@/contexts/AuthContext';
import { SupportApiService } from '@/services/supportApi';

// Bloomberg Terminal Colors
const C = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  RED: '#FF0000',
  GREEN: '#00C800',
  YELLOW: '#FFFF00',
  GRAY: '#787878',
  BLUE: '#6496FA',
  PURPLE: '#C864FF',
  CYAN: '#00FFFF',
  DARK_BG: '#000000',
  PANEL_BG: '#0a0a0a'
};

type SupportView = 'list' | 'create' | 'details';

const SupportTicketTab: React.FC = () => {
  const { session } = useAuth();
  const [currentView, setCurrentView] = useState<SupportView>('list');
  const [loading, setLoading] = useState(false);
  const [tickets, setTickets] = useState<any[]>([]);
  const [selectedTicket, setSelectedTicket] = useState<any>(null);
  const [categories, setCategories] = useState<any[]>([]);
  const [stats, setStats] = useState<any>(null);

  // Create ticket form
  const [newTicket, setNewTicket] = useState({
    subject: '',
    description: '',
    category: 'technical',
    priority: 'medium'
  });

  // Message form
  const [newMessage, setNewMessage] = useState('');

  useEffect(() => {
    if (session?.api_key) {
      loadTickets();
      loadCategories();
      loadStats();
    }
  }, [session]);

  const loadTickets = async () => {
    if (!session?.api_key) return;
    setLoading(true);
    const result = await SupportApiService.getUserTickets(session.api_key);
    if (result.success) {
      setTickets(result.data?.tickets || result.data || []);
    }
    setLoading(false);
  };

  const loadCategories = async () => {
    const result = await SupportApiService.getSupportCategories();
    if (result.success) {
      setCategories(result.data?.categories || result.data || []);
    }
  };

  const loadStats = async () => {
    if (!session?.api_key) return;
    const result = await SupportApiService.getSupportStats(session.api_key);
    if (result.success) {
      setStats(result.data);
    }
  };

  const createTicket = async () => {
    if (!session?.api_key || !newTicket.subject || !newTicket.description) {
      alert('Please fill in all required fields');
      return;
    }

    setLoading(true);
    const result = await SupportApiService.createTicket(session.api_key, newTicket);
    setLoading(false);

    if (result.success) {
      alert('Ticket created successfully!');
      setNewTicket({ subject: '', description: '', category: 'technical', priority: 'medium' });
      setCurrentView('list');
      loadTickets();
    } else {
      alert(`Failed to create ticket: ${result.error}`);
    }
  };

  const viewTicketDetails = async (ticketId: number) => {
    if (!session?.api_key) return;
    setLoading(true);
    const result = await SupportApiService.getTicketDetails(session.api_key, ticketId);
    setLoading(false);

    if (result.success) {
      setSelectedTicket(result.data);
      setCurrentView('details');
    } else {
      alert(`Failed to load ticket details: ${result.error}`);
    }
  };

  const addMessage = async () => {
    if (!session?.api_key || !selectedTicket || !newMessage.trim()) return;

    setLoading(true);
    const result = await SupportApiService.addTicketMessage(
      session.api_key,
      selectedTicket.id || selectedTicket.ticket_id,
      newMessage
    );
    setLoading(false);

    if (result.success) {
      setNewMessage('');
      viewTicketDetails(selectedTicket.id || selectedTicket.ticket_id);
    } else {
      alert(`Failed to add message: ${result.error}`);
    }
  };

  const updateTicketStatus = async (status: string) => {
    if (!session?.api_key || !selectedTicket) return;

    setLoading(true);
    const result = await SupportApiService.updateTicket(
      session.api_key,
      selectedTicket.id || selectedTicket.ticket_id,
      status
    );
    setLoading(false);

    if (result.success) {
      viewTicketDetails(selectedTicket.id || selectedTicket.ticket_id);
      loadTickets();
    } else {
      alert(`Failed to update ticket: ${result.error}`);
    }
  };

  const formatDate = (dateStr?: string) => {
    if (!dateStr) return 'N/A';
    return new Date(dateStr).toLocaleString('en-US', {
      year: 'numeric',
      month: 'short',
      day: 'numeric',
      hour: '2-digit',
      minute: '2-digit'
    });
  };

  const getStatusColor = (status: string) => {
    switch (status?.toLowerCase()) {
      case 'open':
      case 'in_progress':
        return C.CYAN;
      case 'resolved':
      case 'closed':
        return C.GREEN;
      case 'pending':
        return C.YELLOW;
      default:
        return C.GRAY;
    }
  };

  const getPriorityColor = (priority: string) => {
    switch (priority?.toLowerCase()) {
      case 'high':
      case 'urgent':
        return C.RED;
      case 'medium':
        return C.ORANGE;
      case 'low':
        return C.GREEN;
      default:
        return C.GRAY;
    }
  };

  const renderListView = () => (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
        <div>
          <div style={{ color: C.ORANGE, fontSize: '18px', fontWeight: 'bold', fontFamily: 'Consolas, monospace' }}>
            🎫 SUPPORT TICKETS
          </div>
          {stats && (
            <div style={{ color: C.GRAY, fontSize: '11px', fontFamily: 'Consolas, monospace', marginTop: '4px' }}>
              Total: {stats.total_tickets || 0} | Open: {stats.open_tickets || 0} | Resolved: {stats.resolved_tickets || 0}
            </div>
          )}
        </div>
        <button
          onClick={() => setCurrentView('create')}
          style={{
            padding: '10px 20px',
            backgroundColor: C.GREEN,
            border: 'none',
            color: C.DARK_BG,
            fontSize: '12px',
            fontWeight: 'bold',
            cursor: 'pointer',
            fontFamily: 'Consolas, monospace'
          }}
        >
          + CREATE TICKET
        </button>
      </div>

      {/* Tickets List */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {loading ? (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px' }}>
            Loading tickets...
          </div>
        ) : tickets.length > 0 ? (
          <div style={{ display: 'grid', gap: '12px' }}>
            {tickets.map((ticket: any) => (
              <div
                key={ticket.id || ticket.ticket_id}
                onClick={() => viewTicketDetails(ticket.id || ticket.ticket_id)}
                style={{
                  backgroundColor: C.PANEL_BG,
                  border: `2px solid ${C.GRAY}`,
                  borderLeft: `6px solid ${getStatusColor(ticket.status)}`,
                  padding: '16px',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  fontFamily: 'Consolas, monospace'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = C.ORANGE;
                  e.currentTarget.style.backgroundColor = '#1a1a1a';
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = C.GRAY;
                  e.currentTarget.style.backgroundColor = C.PANEL_BG;
                }}
              >
                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '8px' }}>
                  <div>
                    <span style={{ color: C.GRAY, fontSize: '11px' }}>#{ticket.id || ticket.ticket_id}</span>
                    <div style={{ color: C.WHITE, fontSize: '14px', fontWeight: 'bold', marginTop: '4px' }}>
                      {ticket.subject}
                    </div>
                  </div>
                  <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                    <span
                      style={{
                        padding: '4px 8px',
                        backgroundColor: getStatusColor(ticket.status),
                        color: C.DARK_BG,
                        fontSize: '10px',
                        fontWeight: 'bold',
                        borderRadius: '2px'
                      }}
                    >
                      {ticket.status?.toUpperCase()}
                    </span>
                    <span
                      style={{
                        padding: '4px 8px',
                        backgroundColor: getPriorityColor(ticket.priority),
                        color: C.DARK_BG,
                        fontSize: '10px',
                        fontWeight: 'bold',
                        borderRadius: '2px'
                      }}
                    >
                      {ticket.priority?.toUpperCase()}
                    </span>
                  </div>
                </div>

                <div style={{ color: C.GRAY, fontSize: '11px', marginBottom: '8px' }}>
                  {ticket.description?.substring(0, 150)}
                  {ticket.description?.length > 150 && '...'}
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                  <span style={{ color: C.GRAY }}>
                    Category: <span style={{ color: C.CYAN }}>{ticket.category}</span>
                  </span>
                  <span style={{ color: C.GRAY }}>
                    Created: <span style={{ color: C.WHITE }}>{formatDate(ticket.created_at)}</span>
                  </span>
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div style={{ color: C.GRAY, fontFamily: 'Consolas, monospace', fontSize: '12px', textAlign: 'center', marginTop: '40px' }}>
            No support tickets found. Create your first ticket to get started!
          </div>
        )}
      </div>
    </div>
  );

  const renderCreateView = () => (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
      {/* Header */}
      <div style={{ marginBottom: '16px' }}>
        <button
          onClick={() => setCurrentView('list')}
          style={{
            padding: '6px 12px',
            backgroundColor: C.DARK_BG,
            border: `2px solid ${C.GRAY}`,
            color: C.GRAY,
            fontSize: '11px',
            fontWeight: 'bold',
            cursor: 'pointer',
            fontFamily: 'Consolas, monospace',
            marginBottom: '12px'
          }}
        >
          ← BACK TO LIST
        </button>
        <div style={{ color: C.ORANGE, fontSize: '18px', fontWeight: 'bold', fontFamily: 'Consolas, monospace' }}>
          CREATE NEW TICKET
        </div>
      </div>

      {/* Form */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.GREEN}`, padding: '20px' }}>
          <div style={{ marginBottom: '16px' }}>
            <label style={{ color: C.GRAY, fontSize: '11px', fontFamily: 'Consolas, monospace', display: 'block', marginBottom: '4px' }}>
              Subject *
            </label>
            <input
              type="text"
              value={newTicket.subject}
              onChange={(e) => setNewTicket({ ...newTicket, subject: e.target.value })}
              placeholder="Brief description of your issue"
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: C.DARK_BG,
                border: `2px solid ${C.GRAY}`,
                color: C.WHITE,
                fontSize: '12px',
                fontFamily: 'Consolas, monospace',
                outline: 'none'
              }}
            />
          </div>

          <div style={{ marginBottom: '16px' }}>
            <label style={{ color: C.GRAY, fontSize: '11px', fontFamily: 'Consolas, monospace', display: 'block', marginBottom: '4px' }}>
              Category *
            </label>
            <select
              value={newTicket.category}
              onChange={(e) => setNewTicket({ ...newTicket, category: e.target.value })}
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: C.DARK_BG,
                border: `2px solid ${C.GRAY}`,
                color: C.WHITE,
                fontSize: '12px',
                fontFamily: 'Consolas, monospace',
                outline: 'none'
              }}
            >
              <option value="technical">Technical Issue</option>
              <option value="billing">Billing</option>
              <option value="feature">Feature Request</option>
              <option value="bug">Bug Report</option>
              <option value="account">Account</option>
              <option value="other">Other</option>
            </select>
          </div>

          <div style={{ marginBottom: '16px' }}>
            <label style={{ color: C.GRAY, fontSize: '11px', fontFamily: 'Consolas, monospace', display: 'block', marginBottom: '4px' }}>
              Priority
            </label>
            <select
              value={newTicket.priority}
              onChange={(e) => setNewTicket({ ...newTicket, priority: e.target.value })}
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: C.DARK_BG,
                border: `2px solid ${C.GRAY}`,
                color: C.WHITE,
                fontSize: '12px',
                fontFamily: 'Consolas, monospace',
                outline: 'none'
              }}
            >
              <option value="low">Low</option>
              <option value="medium">Medium</option>
              <option value="high">High</option>
            </select>
          </div>

          <div style={{ marginBottom: '16px' }}>
            <label style={{ color: C.GRAY, fontSize: '11px', fontFamily: 'Consolas, monospace', display: 'block', marginBottom: '4px' }}>
              Description *
            </label>
            <textarea
              value={newTicket.description}
              onChange={(e) => setNewTicket({ ...newTicket, description: e.target.value })}
              placeholder="Provide detailed information about your issue..."
              rows={8}
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: C.DARK_BG,
                border: `2px solid ${C.GRAY}`,
                color: C.WHITE,
                fontSize: '12px',
                fontFamily: 'Consolas, monospace',
                outline: 'none',
                resize: 'vertical'
              }}
            />
          </div>

          <button
            onClick={createTicket}
            disabled={loading}
            style={{
              padding: '12px 24px',
              backgroundColor: loading ? C.GRAY : C.GREEN,
              border: 'none',
              color: C.DARK_BG,
              fontSize: '12px',
              fontWeight: 'bold',
              cursor: loading ? 'not-allowed' : 'pointer',
              fontFamily: 'Consolas, monospace'
            }}
          >
            {loading ? 'CREATING...' : 'CREATE TICKET'}
          </button>
        </div>
      </div>
    </div>
  );

  const renderDetailsView = () => {
    if (!selectedTicket) return null;

    const messages = selectedTicket.messages || [];

    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column' }}>
        {/* Header */}
        <div style={{ marginBottom: '16px' }}>
          <button
            onClick={() => setCurrentView('list')}
            style={{
              padding: '6px 12px',
              backgroundColor: C.DARK_BG,
              border: `2px solid ${C.GRAY}`,
              color: C.GRAY,
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              fontFamily: 'Consolas, monospace',
              marginBottom: '12px'
            }}
          >
            ← BACK TO LIST
          </button>

          {/* Ticket Header */}
          <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${getStatusColor(selectedTicket.status)}`, padding: '16px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start', marginBottom: '12px' }}>
              <div>
                <span style={{ color: C.GRAY, fontSize: '11px', fontFamily: 'Consolas, monospace' }}>
                  TICKET #{selectedTicket.id || selectedTicket.ticket_id}
                </span>
                <div style={{ color: C.WHITE, fontSize: '16px', fontWeight: 'bold', marginTop: '4px', fontFamily: 'Consolas, monospace' }}>
                  {selectedTicket.subject}
                </div>
              </div>
              <div style={{ display: 'flex', gap: '8px' }}>
                {selectedTicket.status !== 'closed' && (
                  <button
                    onClick={() => updateTicketStatus('closed')}
                    disabled={loading}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: C.DARK_BG,
                      border: `2px solid ${C.RED}`,
                      color: C.RED,
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    CLOSE
                  </button>
                )}
                {selectedTicket.status === 'closed' && (
                  <button
                    onClick={() => updateTicketStatus('open')}
                    disabled={loading}
                    style={{
                      padding: '6px 12px',
                      backgroundColor: C.DARK_BG,
                      border: `2px solid ${C.GREEN}`,
                      color: C.GREEN,
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      fontFamily: 'Consolas, monospace'
                    }}
                  >
                    REOPEN
                  </button>
                )}
              </div>
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '12px', fontSize: '11px', fontFamily: 'Consolas, monospace' }}>
              <div>
                <span style={{ color: C.GRAY }}>Status: </span>
                <span style={{ color: getStatusColor(selectedTicket.status), fontWeight: 'bold' }}>
                  {selectedTicket.status?.toUpperCase()}
                </span>
              </div>
              <div>
                <span style={{ color: C.GRAY }}>Priority: </span>
                <span style={{ color: getPriorityColor(selectedTicket.priority), fontWeight: 'bold' }}>
                  {selectedTicket.priority?.toUpperCase()}
                </span>
              </div>
              <div>
                <span style={{ color: C.GRAY }}>Category: </span>
                <span style={{ color: C.CYAN }}>{selectedTicket.category}</span>
              </div>
              <div>
                <span style={{ color: C.GRAY }}>Created: </span>
                <span style={{ color: C.WHITE }}>{formatDate(selectedTicket.created_at)}</span>
              </div>
            </div>

            <div style={{ marginTop: '12px', padding: '12px', backgroundColor: C.DARK_BG, borderLeft: `4px solid ${C.BLUE}` }}>
              <div style={{ color: C.WHITE, fontSize: '12px', fontFamily: 'Consolas, monospace', lineHeight: '1.6' }}>
                {selectedTicket.description}
              </div>
            </div>
          </div>
        </div>

        {/* Messages */}
        <div style={{ flex: 1, overflowY: 'auto', marginBottom: '16px' }}>
          <div style={{ color: C.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px', fontFamily: 'Consolas, monospace' }}>
            CONVERSATION ({messages.length})
          </div>

          {messages.length > 0 ? (
            <div style={{ display: 'grid', gap: '12px' }}>
              {messages.map((msg: any, idx: number) => (
                <div
                  key={idx}
                  style={{
                    backgroundColor: C.PANEL_BG,
                    border: `2px solid ${C.GRAY}`,
                    borderLeft: `6px solid ${msg.sender_type === 'user' ? C.CYAN : C.PURPLE}`,
                    padding: '12px',
                    fontFamily: 'Consolas, monospace'
                  }}
                >
                  <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                    <span style={{ color: msg.sender_type === 'user' ? C.CYAN : C.PURPLE, fontSize: '11px', fontWeight: 'bold' }}>
                      {msg.sender_type === 'user' ? 'YOU' : 'SUPPORT TEAM'}
                    </span>
                    <span style={{ color: C.GRAY, fontSize: '10px' }}>
                      {formatDate(msg.created_at)}
                    </span>
                  </div>
                  <div style={{ color: C.WHITE, fontSize: '12px', lineHeight: '1.6' }}>
                    {msg.message}
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <div style={{ color: C.GRAY, fontSize: '12px', fontFamily: 'Consolas, monospace', textAlign: 'center', padding: '20px' }}>
              No messages yet
            </div>
          )}
        </div>

        {/* Add Message */}
        {selectedTicket.status !== 'closed' && (
          <div style={{ backgroundColor: C.PANEL_BG, border: `2px solid ${C.GRAY}`, borderLeft: `6px solid ${C.GREEN}`, padding: '16px' }}>
            <div style={{ color: C.GREEN, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px', fontFamily: 'Consolas, monospace' }}>
              ADD MESSAGE
            </div>
            <textarea
              value={newMessage}
              onChange={(e) => setNewMessage(e.target.value)}
              placeholder="Type your message here..."
              rows={4}
              style={{
                width: '100%',
                padding: '10px',
                backgroundColor: C.DARK_BG,
                border: `2px solid ${C.GRAY}`,
                color: C.WHITE,
                fontSize: '12px',
                fontFamily: 'Consolas, monospace',
                outline: 'none',
                resize: 'vertical',
                marginBottom: '12px'
              }}
            />
            <button
              onClick={addMessage}
              disabled={loading || !newMessage.trim()}
              style={{
                padding: '10px 20px',
                backgroundColor: loading || !newMessage.trim() ? C.GRAY : C.GREEN,
                border: 'none',
                color: C.DARK_BG,
                fontSize: '11px',
                fontWeight: 'bold',
                cursor: loading || !newMessage.trim() ? 'not-allowed' : 'pointer',
                fontFamily: 'Consolas, monospace'
              }}
            >
              {loading ? 'SENDING...' : 'SEND MESSAGE'}
            </button>
          </div>
        )}
      </div>
    );
  };

  if (!session) {
    return (
      <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', backgroundColor: C.DARK_BG }}>
        <div style={{ textAlign: 'center', fontFamily: 'Consolas, monospace' }}>
          <div style={{ color: C.ORANGE, fontSize: '16px', marginBottom: '8px' }}>⚠️ AUTHENTICATION REQUIRED</div>
          <div style={{ color: C.GRAY, fontSize: '12px' }}>Please login to access support tickets</div>
        </div>
      </div>
    );
  }

  return (
    <div style={{ height: '100%', backgroundColor: C.DARK_BG, padding: '20px', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
      {currentView === 'list' && renderListView()}
      {currentView === 'create' && renderCreateView()}
      {currentView === 'details' && renderDetailsView()}
    </div>
  );
};

export default SupportTicketTab;
