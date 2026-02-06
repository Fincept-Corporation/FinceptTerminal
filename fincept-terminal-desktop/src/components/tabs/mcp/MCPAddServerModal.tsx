// MCP Add Server Modal Component
// Add custom MCP servers manually - Fincept UI Design System
// Production-ready: Input validation, sanitization, error handling

import React, { useState, useCallback } from 'react';
import { X, Plus, AlertCircle } from 'lucide-react';
import { showWarning } from '@/utils/notifications';
import { validateString, sanitizeInput } from '@/services/core/validators';

interface MCPAddServerModalProps {
  onClose: () => void;
  onAdd: (serverConfig: any) => void;
}

interface FormErrors {
  name?: string;
  command?: string;
  args?: string;
  envVars?: string;
}

// Allowed commands for security
const ALLOWED_COMMANDS = ['bunx', 'npx', 'node', 'python', 'python3', 'uvx', 'uv', 'pip', 'pipx'];

// Pattern for valid server names
const SERVER_NAME_PATTERN = /^[a-zA-Z0-9\s\-_]+$/;

// Pattern for valid command
const COMMAND_PATTERN = /^[a-zA-Z0-9\-_]+$/;

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
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

const MCPAddServerModal: React.FC<MCPAddServerModalProps> = ({ onClose, onAdd }) => {
  const [formData, setFormData] = useState({
    name: '',
    description: '',
    command: 'bunx',
    args: '',
    envVars: '',
    category: 'custom',
    icon: '🔧',
  });

  const [errors, setErrors] = useState<FormErrors>({});
  const [isSubmitting, setIsSubmitting] = useState(false);

  // Validate server name
  const validateName = useCallback((name: string): string | undefined => {
    const trimmed = name.trim();

    if (!trimmed) {
      return 'Server name is required';
    }

    if (trimmed.length < 2) {
      return 'Server name must be at least 2 characters';
    }

    if (trimmed.length > 50) {
      return 'Server name must be at most 50 characters';
    }

    if (!SERVER_NAME_PATTERN.test(trimmed)) {
      return 'Server name can only contain letters, numbers, spaces, hyphens, and underscores';
    }

    return undefined;
  }, []);

  // Validate command
  const validateCommand = useCallback((command: string): string | undefined => {
    const trimmed = command.trim().toLowerCase();

    if (!trimmed) {
      return 'Command is required';
    }

    if (!COMMAND_PATTERN.test(trimmed)) {
      return 'Command contains invalid characters';
    }

    if (!ALLOWED_COMMANDS.includes(trimmed)) {
      return `Command must be one of: ${ALLOWED_COMMANDS.join(', ')}`;
    }

    return undefined;
  }, []);

  // Validate arguments
  const validateArgs = useCallback((args: string): string | undefined => {
    const trimmed = args.trim();

    if (!trimmed) {
      return 'Arguments are required';
    }

    if (trimmed.length > 500) {
      return 'Arguments too long (max 500 characters)';
    }

    // Check for potentially dangerous characters (basic shell injection prevention)
    const dangerousPatterns = [/;/, /&&/, /\|\|/, /\|/, /`/, /\$\(/, /\$\{/];
    for (const pattern of dangerousPatterns) {
      if (pattern.test(trimmed)) {
        return 'Arguments contain potentially unsafe characters';
      }
    }

    return undefined;
  }, []);

  // Validate environment variables
  const validateEnvVars = useCallback((envVars: string): string | undefined => {
    if (!envVars.trim()) {
      return undefined; // Optional field
    }

    const lines = envVars.trim().split('\n');

    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line) continue;

      // Check format KEY=VALUE
      if (!line.includes('=')) {
        return `Line ${i + 1}: Invalid format. Use KEY=VALUE`;
      }

      const [key] = line.split('=');
      const trimmedKey = key.trim();

      // Validate key name
      if (!/^[A-Z_][A-Z0-9_]*$/i.test(trimmedKey)) {
        return `Line ${i + 1}: Invalid key name "${trimmedKey}"`;
      }
    }

    return undefined;
  }, []);

  // Validate entire form
  const validateForm = useCallback((): boolean => {
    const newErrors: FormErrors = {
      name: validateName(formData.name),
      command: validateCommand(formData.command),
      args: validateArgs(formData.args),
      envVars: validateEnvVars(formData.envVars),
    };

    setErrors(newErrors);

    return !Object.values(newErrors).some(error => error !== undefined);
  }, [formData, validateName, validateCommand, validateArgs, validateEnvVars]);

  // Handle field change with real-time validation
  const handleFieldChange = useCallback((field: keyof typeof formData, value: string) => {
    setFormData(prev => ({ ...prev, [field]: value }));

    // Clear error on change
    if (errors[field as keyof FormErrors]) {
      setErrors(prev => ({ ...prev, [field]: undefined }));
    }
  }, [errors]);

  // Handle blur with validation
  const handleFieldBlur = useCallback((field: keyof typeof formData) => {
    let error: string | undefined;

    switch (field) {
      case 'name':
        error = validateName(formData.name);
        break;
      case 'command':
        error = validateCommand(formData.command);
        break;
      case 'args':
        error = validateArgs(formData.args);
        break;
      case 'envVars':
        error = validateEnvVars(formData.envVars);
        break;
    }

    if (error) {
      setErrors(prev => ({ ...prev, [field]: error }));
    }
  }, [formData, validateName, validateCommand, validateArgs, validateEnvVars]);

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();

    if (!validateForm()) {
      showWarning('Please fix the validation errors');
      return;
    }

    setIsSubmitting(true);

    try {
      // Sanitize inputs
      const sanitizedName = sanitizeInput(formData.name.trim());
      const sanitizedDescription = sanitizeInput(formData.description.trim());
      const sanitizedCommand = formData.command.trim().toLowerCase();

      // Parse and sanitize arguments
      const args = formData.args
        .trim()
        .split(/[\s,]+/)
        .filter(arg => arg.length > 0)
        .map(arg => sanitizeInput(arg));

      // Parse environment variables
      const env: Record<string, string> = {};
      if (formData.envVars.trim()) {
        formData.envVars.split('\n').forEach(line => {
          const trimmedLine = line.trim();
          if (!trimmedLine) return;

          const [key, ...valueParts] = trimmedLine.split('=');
          if (key && valueParts.length > 0) {
            env[key.trim()] = valueParts.join('=').trim();
          }
        });
      }

      // Generate safe ID
      const id = sanitizedName.toLowerCase().replace(/[^a-z0-9]/g, '-').replace(/-+/g, '-');

      const serverConfig = {
        id,
        name: sanitizedName,
        description: sanitizedDescription || `Custom MCP server: ${sanitizedName}`,
        command: sanitizedCommand,
        args,
        env,
        category: formData.category,
        icon: formData.icon.slice(0, 2) || '🔧', // Limit emoji length
      };

      onAdd(serverConfig);
    } catch (error) {
      console.error('Error creating server config:', error);
      showWarning('Failed to create server configuration');
    } finally {
      setIsSubmitting(false);
    }
  };

  const inputStyle: React.CSSProperties = {
    width: '100%',
    padding: '8px 10px',
    backgroundColor: FINCEPT.DARK_BG,
    color: FINCEPT.WHITE,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '10px',
    fontFamily: FONT_FAMILY,
    outline: 'none',
  };

  const inputErrorStyle: React.CSSProperties = {
    ...inputStyle,
    borderColor: FINCEPT.RED,
  };

  const labelStyle: React.CSSProperties = {
    display: 'block',
    fontSize: '9px',
    fontWeight: 700,
    color: FINCEPT.GRAY,
    letterSpacing: '0.5px',
    marginBottom: '6px',
  };

  const errorTextStyle: React.CSSProperties = {
    color: FINCEPT.RED,
    fontSize: '9px',
    marginTop: '4px',
    display: 'flex',
    alignItems: 'center',
    gap: '4px',
  };

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000,
      fontFamily: FONT_FAMILY,
    }}>
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.ORANGE}`,
        borderRadius: '2px',
        width: '480px',
        maxHeight: '80vh',
        overflow: 'auto',
        display: 'flex',
        flexDirection: 'column',
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `2px solid ${FINCEPT.ORANGE}`,
          padding: '12px 16px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexShrink: 0,
        }}>
          <span style={{
            color: FINCEPT.ORANGE,
            fontSize: '11px',
            fontWeight: 700,
            letterSpacing: '0.5px',
          }}>
            ADD CUSTOM MCP SERVER
          </span>
          <button
            onClick={onClose}
            style={{
              backgroundColor: 'transparent',
              border: 'none',
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              padding: '4px',
              transition: 'all 0.2s',
            }}
            onMouseEnter={(e) => { e.currentTarget.style.color = FINCEPT.WHITE; }}
            onMouseLeave={(e) => { e.currentTarget.style.color = FINCEPT.GRAY; }}
          >
            <X size={14} />
          </button>
        </div>

        {/* Form */}
        <form onSubmit={handleSubmit} style={{ padding: '16px', overflow: 'auto', flex: 1 }}>
          {/* Server Name */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>SERVER NAME *</label>
            <input
              type="text"
              value={formData.name}
              onChange={(e) => handleFieldChange('name', e.target.value)}
              onBlur={() => handleFieldBlur('name')}
              placeholder="e.g., My Custom Server"
              required
              maxLength={50}
              style={errors.name ? inputErrorStyle : inputStyle}
              onFocus={(e) => {
                if (!errors.name) e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              }}
            />
            {errors.name && (
              <div style={errorTextStyle}>
                <AlertCircle size={10} />
                {errors.name}
              </div>
            )}
          </div>

          {/* Description */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>DESCRIPTION</label>
            <input
              type="text"
              value={formData.description}
              onChange={(e) => handleFieldChange('description', e.target.value)}
              placeholder="What does this server do?"
              maxLength={200}
              style={inputStyle}
              onFocus={(e) => { e.currentTarget.style.borderColor = FINCEPT.ORANGE; }}
              onBlur={(e) => { e.currentTarget.style.borderColor = FINCEPT.BORDER; }}
            />
          </div>

          {/* Command */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>COMMAND *</label>
            <select
              value={formData.command}
              onChange={(e) => handleFieldChange('command', e.target.value)}
              style={{
                ...inputStyle,
                cursor: 'pointer',
              }}
            >
              {ALLOWED_COMMANDS.map(cmd => (
                <option key={cmd} value={cmd}>{cmd}</option>
              ))}
            </select>
            {errors.command && (
              <div style={errorTextStyle}>
                <AlertCircle size={10} />
                {errors.command}
              </div>
            )}
            <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '4px' }}>
              Allowed: {ALLOWED_COMMANDS.join(', ')}
            </div>
          </div>

          {/* Arguments */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>ARGUMENTS * (SPACE OR COMMA SEPARATED)</label>
            <input
              type="text"
              value={formData.args}
              onChange={(e) => handleFieldChange('args', e.target.value)}
              onBlur={() => handleFieldBlur('args')}
              placeholder="e.g., -y @modelcontextprotocol/server-example"
              required
              maxLength={500}
              style={errors.args ? inputErrorStyle : inputStyle}
              onFocus={(e) => {
                if (!errors.args) e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              }}
            />
            {errors.args && (
              <div style={errorTextStyle}>
                <AlertCircle size={10} />
                {errors.args}
              </div>
            )}
            <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '4px' }}>
              Example: -y @modelcontextprotocol/server-postgres
            </div>
          </div>

          {/* Environment Variables */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>ENVIRONMENT VARIABLES (ONE PER LINE)</label>
            <textarea
              value={formData.envVars}
              onChange={(e) => handleFieldChange('envVars', e.target.value)}
              onBlur={() => handleFieldBlur('envVars')}
              placeholder={'DATABASE_URL=postgresql://...\nAPI_KEY=your-key-here'}
              rows={3}
              style={{
                ...(errors.envVars ? inputErrorStyle : inputStyle),
                resize: 'vertical',
              }}
              onFocus={(e) => {
                if (!errors.envVars) e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              }}
            />
            {errors.envVars && (
              <div style={errorTextStyle}>
                <AlertCircle size={10} />
                {errors.envVars}
              </div>
            )}
            <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '4px' }}>
              Format: KEY=VALUE (one per line)
            </div>
          </div>

          {/* Icon */}
          <div style={{ marginBottom: '16px' }}>
            <label style={labelStyle}>ICON (EMOJI)</label>
            <input
              type="text"
              value={formData.icon}
              onChange={(e) => handleFieldChange('icon', e.target.value)}
              placeholder="🔧"
              maxLength={2}
              style={{
                ...inputStyle,
                width: '60px',
                fontSize: '16px',
                textAlign: 'center',
              }}
            />
          </div>

          {/* Actions */}
          <div style={{
            display: 'flex',
            gap: '8px',
            justifyContent: 'flex-end',
            paddingTop: '12px',
            borderTop: `1px solid ${FINCEPT.BORDER}`,
          }}>
            <button
              type="button"
              onClick={onClose}
              disabled={isSubmitting}
              style={{
                padding: '6px 10px',
                backgroundColor: 'transparent',
                border: `1px solid ${FINCEPT.BORDER}`,
                color: FINCEPT.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                borderRadius: '2px',
                cursor: isSubmitting ? 'not-allowed' : 'pointer',
                fontFamily: FONT_FAMILY,
                transition: 'all 0.2s',
                opacity: isSubmitting ? 0.5 : 1,
              }}
              onMouseEnter={(e) => {
                if (!isSubmitting) {
                  e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                  e.currentTarget.style.color = FINCEPT.WHITE;
                }
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.borderColor = FINCEPT.BORDER;
                e.currentTarget.style.color = FINCEPT.GRAY;
              }}
            >
              CANCEL
            </button>
            <button
              type="submit"
              disabled={isSubmitting}
              style={{
                padding: '8px 16px',
                backgroundColor: isSubmitting ? FINCEPT.MUTED : FINCEPT.ORANGE,
                color: FINCEPT.DARK_BG,
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 700,
                cursor: isSubmitting ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                fontFamily: FONT_FAMILY,
              }}
            >
              <Plus size={10} />
              {isSubmitting ? 'ADDING...' : 'ADD SERVER'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default MCPAddServerModal;
