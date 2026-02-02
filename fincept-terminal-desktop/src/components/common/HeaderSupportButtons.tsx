// Header Support Buttons - Feedback, Contact, Newsletter, Notifications
// Fincept-style modal forms for user engagement

import React, { useState } from 'react';
import { MessageSquarePlus, Mail, Newspaper, X, Star, Send, Check } from 'lucide-react';
import { useAuth } from '@/contexts/AuthContext';
import {
  submitContactForm,
  submitFeedback,
  subscribeToNewsletter,
  type ContactFormData,
  type FeedbackFormData,
} from '@/services/support/supportApi';

// Fincept color palette
const COLORS = {
  ORANGE: '#ea580c',
  GREEN: '#10b981',
  RED: '#ef4444',
  YELLOW: '#fbbf24',
  BLUE: '#3b82f6',
  CYAN: '#06b6d4',
  BG: '#0f0f0f',
  PANEL: '#1a1a1a',
  BORDER: '#404040',
  TEXT: '#ffffff',
  MUTED: '#a3a3a3',
};

// Modal wrapper component
const Modal: React.FC<{
  isOpen: boolean;
  onClose: () => void;
  title: string;
  children: React.ReactNode;
}> = ({ isOpen, onClose, title, children }) => {
  if (!isOpen) return null;

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.85)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 9999,
      }}
      onClick={onClose}
    >
      <div
        style={{
          backgroundColor: COLORS.PANEL,
          border: `2px solid ${COLORS.ORANGE}`,
          borderRadius: '4px',
          padding: '16px',
          width: '450px',
          maxHeight: '80vh',
          overflow: 'auto',
          fontFamily: 'Consolas, monospace',
        }}
        onClick={(e) => e.stopPropagation()}
      >
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '12px' }}>
          <h3 style={{ color: COLORS.ORANGE, fontSize: '14px', fontWeight: 'bold', margin: 0 }}>{title}</h3>
          <button
            onClick={onClose}
            style={{
              background: 'transparent',
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.MUTED,
              cursor: 'pointer',
              padding: '2px 6px',
              fontSize: '10px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
            }}
          >
            <X size={12} /> CLOSE
          </button>
        </div>
        <div style={{ borderBottom: `1px solid ${COLORS.BORDER}`, marginBottom: '12px' }}></div>
        {children}
      </div>
    </div>
  );
};

// Input field component
const InputField: React.FC<{
  label: string;
  value: string;
  onChange: (value: string) => void;
  placeholder?: string;
  type?: string;
  required?: boolean;
  multiline?: boolean;
  rows?: number;
}> = ({ label, value, onChange, placeholder, type = 'text', required, multiline, rows = 4 }) => (
  <div style={{ marginBottom: '12px' }}>
    <label style={{ color: COLORS.TEXT, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
      {label} {required && <span style={{ color: COLORS.RED }}>*</span>}
    </label>
    {multiline ? (
      <textarea
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        rows={rows}
        style={{
          width: '100%',
          backgroundColor: COLORS.BG,
          border: `1px solid ${COLORS.BORDER}`,
          color: COLORS.TEXT,
          padding: '8px',
          fontSize: '11px',
          fontFamily: 'Consolas, monospace',
          resize: 'vertical',
          borderRadius: '2px',
        }}
      />
    ) : (
      <input
        type={type}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        style={{
          width: '100%',
          backgroundColor: COLORS.BG,
          border: `1px solid ${COLORS.BORDER}`,
          color: COLORS.TEXT,
          padding: '8px',
          fontSize: '11px',
          fontFamily: 'Consolas, monospace',
          borderRadius: '2px',
        }}
      />
    )}
  </div>
);

// Star rating component
const StarRating: React.FC<{
  rating: number;
  onChange: (rating: number) => void;
}> = ({ rating, onChange }) => (
  <div style={{ marginBottom: '12px' }}>
    <label style={{ color: COLORS.TEXT, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
      Rating <span style={{ color: COLORS.RED }}>*</span>
    </label>
    <div style={{ display: 'flex', gap: '4px' }}>
      {[1, 2, 3, 4, 5].map((star) => (
        <button
          key={star}
          onClick={() => onChange(star)}
          style={{
            background: 'transparent',
            border: 'none',
            cursor: 'pointer',
            padding: '2px',
          }}
        >
          <Star
            size={24}
            fill={star <= rating ? COLORS.YELLOW : 'transparent'}
            color={star <= rating ? COLORS.YELLOW : COLORS.MUTED}
          />
        </button>
      ))}
    </div>
  </div>
);

// Submit button component
const SubmitButton: React.FC<{
  onClick: () => void;
  loading: boolean;
  success: boolean;
  text: string;
}> = ({ onClick, loading, success, text }) => (
  <button
    onClick={onClick}
    disabled={loading}
    style={{
      width: '100%',
      backgroundColor: success ? COLORS.GREEN : COLORS.ORANGE,
      color: COLORS.TEXT,
      border: 'none',
      padding: '10px 16px',
      fontSize: '12px',
      fontWeight: 'bold',
      cursor: loading ? 'not-allowed' : 'pointer',
      borderRadius: '2px',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      gap: '8px',
      opacity: loading ? 0.7 : 1,
    }}
  >
    {loading ? (
      'SUBMITTING...'
    ) : success ? (
      <>
        <Check size={14} /> SUBMITTED!
      </>
    ) : (
      <>
        <Send size={14} /> {text}
      </>
    )}
  </button>
);

// Main component
export const HeaderSupportButtons: React.FC = () => {
  const { session } = useAuth();
  const apiKey = session?.api_key || '';

  // Modal states
  const [showFeedback, setShowFeedback] = useState(false);
  const [showContact, setShowContact] = useState(false);
  const [showNewsletter, setShowNewsletter] = useState(false);

  // Feedback form state
  const [feedbackRating, setFeedbackRating] = useState(0);
  const [feedbackText, setFeedbackText] = useState('');
  const [feedbackCategory, setFeedbackCategory] = useState('general');
  const [feedbackLoading, setFeedbackLoading] = useState(false);
  const [feedbackSuccess, setFeedbackSuccess] = useState(false);
  const [feedbackError, setFeedbackError] = useState('');

  // Contact form state
  const [contactName, setContactName] = useState('');
  const [contactEmail, setContactEmail] = useState('');
  const [contactSubject, setContactSubject] = useState('');
  const [contactMessage, setContactMessage] = useState('');
  const [contactLoading, setContactLoading] = useState(false);
  const [contactSuccess, setContactSuccess] = useState(false);
  const [contactError, setContactError] = useState('');

  // Newsletter state
  const [newsletterEmail, setNewsletterEmail] = useState('');
  const [newsletterLoading, setNewsletterLoading] = useState(false);
  const [newsletterSuccess, setNewsletterSuccess] = useState(false);
  const [newsletterError, setNewsletterError] = useState('');

  // Reset functions
  const resetFeedback = () => {
    setFeedbackRating(0);
    setFeedbackText('');
    setFeedbackCategory('general');
    setFeedbackSuccess(false);
    setFeedbackError('');
  };

  const resetContact = () => {
    setContactName('');
    setContactEmail('');
    setContactSubject('');
    setContactMessage('');
    setContactSuccess(false);
    setContactError('');
  };

  const resetNewsletter = () => {
    setNewsletterEmail('');
    setNewsletterSuccess(false);
    setNewsletterError('');
  };

  // Submit handlers
  const handleFeedbackSubmit = async () => {
    if (!feedbackRating || !feedbackText.trim()) {
      setFeedbackError('Please provide a rating and feedback');
      return;
    }

    if (!apiKey) {
      setFeedbackError('Please login to submit feedback');
      return;
    }

    setFeedbackLoading(true);
    setFeedbackError('');

    const data: FeedbackFormData = {
      rating: feedbackRating,
      feedback_text: feedbackText,
      category: feedbackCategory,
    };

    const result = await submitFeedback(data, apiKey);
    setFeedbackLoading(false);

    if (result.success) {
      setFeedbackSuccess(true);
      setTimeout(() => {
        setShowFeedback(false);
        resetFeedback();
      }, 2000);
    } else {
      setFeedbackError(result.error || 'Failed to submit feedback');
    }
  };

  const handleContactSubmit = async () => {
    if (!contactName.trim() || !contactEmail.trim() || !contactSubject.trim() || !contactMessage.trim()) {
      setContactError('Please fill in all required fields');
      return;
    }

    setContactLoading(true);
    setContactError('');

    const data: ContactFormData = {
      name: contactName,
      email: contactEmail,
      subject: contactSubject,
      message: contactMessage,
    };

    const result = await submitContactForm(data);
    setContactLoading(false);

    if (result.success) {
      setContactSuccess(true);
      setTimeout(() => {
        setShowContact(false);
        resetContact();
      }, 2000);
    } else {
      setContactError(result.error || 'Failed to submit contact form');
    }
  };

  const handleNewsletterSubmit = async () => {
    if (!newsletterEmail.trim()) {
      setNewsletterError('Please enter your email');
      return;
    }

    setNewsletterLoading(true);
    setNewsletterError('');

    const result = await subscribeToNewsletter({
      email: newsletterEmail,
      source: 'terminal_header',
    });
    setNewsletterLoading(false);

    if (result.success) {
      setNewsletterSuccess(true);
      setTimeout(() => {
        setShowNewsletter(false);
        resetNewsletter();
      }, 2000);
    } else {
      setNewsletterError(result.error || 'Failed to subscribe');
    }
  };

  // Button style
  const buttonStyle: React.CSSProperties = {
    backgroundColor: 'transparent',
    color: COLORS.MUTED,
    border: `1px solid ${COLORS.BORDER}`,
    padding: '3px 8px',
    fontSize: '10px',
    cursor: 'pointer',
    borderRadius: '3px',
    display: 'flex',
    alignItems: 'center',
    gap: '4px',
    fontWeight: 'bold',
    transition: 'all 0.2s',
  };

  return (
    <>
      {/* Header Buttons */}
      <button
        onClick={() => setShowFeedback(true)}
        style={buttonStyle}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = COLORS.ORANGE;
          e.currentTarget.style.borderColor = COLORS.ORANGE;
          e.currentTarget.style.color = COLORS.TEXT;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = 'transparent';
          e.currentTarget.style.borderColor = COLORS.BORDER;
          e.currentTarget.style.color = COLORS.MUTED;
        }}
        title="Send Feedback"
      >
        <MessageSquarePlus size={12} />
        FEEDBACK
      </button>

      <button
        onClick={() => setShowContact(true)}
        style={buttonStyle}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = COLORS.BLUE;
          e.currentTarget.style.borderColor = COLORS.BLUE;
          e.currentTarget.style.color = COLORS.TEXT;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = 'transparent';
          e.currentTarget.style.borderColor = COLORS.BORDER;
          e.currentTarget.style.color = COLORS.MUTED;
        }}
        title="Contact Us"
      >
        <Mail size={12} />
        CONTACT
      </button>

      <button
        onClick={() => setShowNewsletter(true)}
        style={buttonStyle}
        onMouseEnter={(e) => {
          e.currentTarget.style.backgroundColor = COLORS.GREEN;
          e.currentTarget.style.borderColor = COLORS.GREEN;
          e.currentTarget.style.color = COLORS.TEXT;
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.backgroundColor = 'transparent';
          e.currentTarget.style.borderColor = COLORS.BORDER;
          e.currentTarget.style.color = COLORS.MUTED;
        }}
        title="Subscribe to Newsletter"
      >
        <Newspaper size={12} />
        NEWSLETTER
      </button>

      {/* Feedback Modal */}
      <Modal isOpen={showFeedback} onClose={() => { setShowFeedback(false); resetFeedback(); }} title="SEND FEEDBACK">
        <StarRating rating={feedbackRating} onChange={setFeedbackRating} />
        <div style={{ marginBottom: '12px' }}>
          <label style={{ color: COLORS.TEXT, fontSize: '11px', display: 'block', marginBottom: '4px' }}>
            Category
          </label>
          <select
            value={feedbackCategory}
            onChange={(e) => setFeedbackCategory(e.target.value)}
            style={{
              width: '100%',
              backgroundColor: COLORS.BG,
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.TEXT,
              padding: '8px',
              fontSize: '11px',
              fontFamily: 'Consolas, monospace',
              borderRadius: '2px',
            }}
          >
            <option value="general">General</option>
            <option value="feature_request">Feature Request</option>
            <option value="bug_report">Bug Report</option>
            <option value="ui_ux">UI/UX</option>
            <option value="performance">Performance</option>
            <option value="data">Data Quality</option>
          </select>
        </div>
        <InputField
          label="Feedback"
          value={feedbackText}
          onChange={setFeedbackText}
          placeholder="Tell us what you think..."
          multiline
          rows={4}
          required
        />
        {feedbackError && (
          <div style={{ color: COLORS.RED, fontSize: '10px', marginBottom: '12px' }}>{feedbackError}</div>
        )}
        <SubmitButton
          onClick={handleFeedbackSubmit}
          loading={feedbackLoading}
          success={feedbackSuccess}
          text="SUBMIT FEEDBACK"
        />
      </Modal>

      {/* Contact Modal */}
      <Modal isOpen={showContact} onClose={() => { setShowContact(false); resetContact(); }} title="CONTACT US">
        <InputField
          label="Name"
          value={contactName}
          onChange={setContactName}
          placeholder="Your name"
          required
        />
        <InputField
          label="Email"
          value={contactEmail}
          onChange={setContactEmail}
          placeholder="your@email.com"
          type="email"
          required
        />
        <InputField
          label="Subject"
          value={contactSubject}
          onChange={setContactSubject}
          placeholder="Subject of your message"
          required
        />
        <InputField
          label="Message"
          value={contactMessage}
          onChange={setContactMessage}
          placeholder="Your message..."
          multiline
          rows={5}
          required
        />
        {contactError && (
          <div style={{ color: COLORS.RED, fontSize: '10px', marginBottom: '12px' }}>{contactError}</div>
        )}
        <SubmitButton
          onClick={handleContactSubmit}
          loading={contactLoading}
          success={contactSuccess}
          text="SEND MESSAGE"
        />
      </Modal>

      {/* Newsletter Modal */}
      <Modal isOpen={showNewsletter} onClose={() => { setShowNewsletter(false); resetNewsletter(); }} title="SUBSCRIBE TO NEWSLETTER">
        <p style={{ color: COLORS.MUTED, fontSize: '11px', marginBottom: '16px', lineHeight: '1.5' }}>
          Stay updated with the latest features, market insights, and Fincept news delivered to your inbox.
        </p>
        <InputField
          label="Email Address"
          value={newsletterEmail}
          onChange={setNewsletterEmail}
          placeholder="your@email.com"
          type="email"
          required
        />
        {newsletterError && (
          <div style={{ color: COLORS.RED, fontSize: '10px', marginBottom: '12px' }}>{newsletterError}</div>
        )}
        <SubmitButton
          onClick={handleNewsletterSubmit}
          loading={newsletterLoading}
          success={newsletterSuccess}
          text="SUBSCRIBE"
        />
        <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '12px', textAlign: 'center' }}>
          You can unsubscribe at any time. We respect your privacy.
        </p>
      </Modal>
    </>
  );
};

export default HeaderSupportButtons;
