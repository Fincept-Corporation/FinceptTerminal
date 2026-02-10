// Base Node Components
// Reusable components for building consistent nodes

import React, { ReactNode } from 'react';
import {
  FINCEPT,
  SPACING,
  getHeaderStyle,
  getHeaderTitleStyle,
  getHeaderSubtitleStyle,
  getIconButtonStyle,
  getButtonStyle,
  getInputStyle,
  getTextareaStyle,
  getSelectStyle,
  getLabelStyle,
  getPanelStyle,
  getStatusPanelStyle,
  getEmptyStateStyle,
} from './FinceptNodeDesign';

// Node Header Component
interface NodeHeaderProps {
  icon: ReactNode;
  title: string;
  subtitle?: string;
  rightActions?: ReactNode;
  color?: string;
}

export const NodeHeader: React.FC<NodeHeaderProps> = ({
  icon,
  title,
  subtitle,
  rightActions,
  color = FINCEPT.WHITE,
}) => {
  return (
    <div style={getHeaderStyle()}>
      <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MD }}>
        <div style={{ color }}>{icon}</div>
        <div>
          <div style={getHeaderTitleStyle()}>{title}</div>
          {subtitle && <div style={getHeaderSubtitleStyle()}>{subtitle}</div>}
        </div>
      </div>
      {rightActions && <div style={{ display: 'flex', gap: SPACING.SM }}>{rightActions}</div>}
    </div>
  );
};

// Icon Button Component
interface IconButtonProps {
  icon: ReactNode;
  onClick?: () => void;
  active?: boolean;
  title?: string;
  disabled?: boolean;
}

export const IconButton: React.FC<IconButtonProps> = ({
  icon,
  onClick,
  active = false,
  title,
  disabled = false,
}) => {
  return (
    <button
      onClick={onClick}
      disabled={disabled}
      title={title}
      style={getIconButtonStyle(active)}
    >
      {icon}
    </button>
  );
};

// Primary Button Component
interface ButtonProps {
  label: string;
  icon?: ReactNode;
  onClick?: () => void;
  variant?: 'primary' | 'secondary' | 'danger';
  disabled?: boolean;
  fullWidth?: boolean;
}

export const Button: React.FC<ButtonProps> = ({
  label,
  icon,
  onClick,
  variant = 'primary',
  disabled = false,
  fullWidth = false,
}) => {
  return (
    <button
      onClick={onClick}
      disabled={disabled}
      style={{
        ...getButtonStyle(variant, disabled),
        width: fullWidth ? '100%' : 'auto',
      }}
    >
      {icon}
      {label}
    </button>
  );
};

// Input Field Component
interface InputFieldProps {
  label: string;
  value: string | number;
  onChange: (value: string) => void;
  type?: 'text' | 'number' | 'date';
  placeholder?: string;
  required?: boolean;
  disabled?: boolean;
}

export const InputField: React.FC<InputFieldProps> = ({
  label,
  value,
  onChange,
  type = 'text',
  placeholder,
  required = false,
  disabled = false,
}) => {
  return (
    <div style={{ marginBottom: SPACING.MD }}>
      <label style={getLabelStyle(required)}>
        {label}
        {required && ' *'}
      </label>
      <input
        type={type}
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        disabled={disabled}
        style={getInputStyle(disabled)}
      />
    </div>
  );
};

// Textarea Field Component
interface TextareaFieldProps {
  label: string;
  value: string;
  onChange: (value: string) => void;
  placeholder?: string;
  required?: boolean;
  disabled?: boolean;
  minHeight?: string;
}

export const TextareaField: React.FC<TextareaFieldProps> = ({
  label,
  value,
  onChange,
  placeholder,
  required = false,
  disabled = false,
  minHeight,
}) => {
  return (
    <div style={{ marginBottom: SPACING.MD }}>
      <label style={getLabelStyle(required)}>
        {label}
        {required && ' *'}
      </label>
      <textarea
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        disabled={disabled}
        style={{
          ...getTextareaStyle(disabled),
          ...(minHeight && { minHeight }),
        }}
      />
    </div>
  );
};

// Select Field Component
interface SelectFieldProps {
  label: string;
  value: string;
  options: Array<{ value: string; label: string }>;
  onChange: (value: string) => void;
  required?: boolean;
}

export const SelectField: React.FC<SelectFieldProps> = ({
  label,
  value,
  options,
  onChange,
  required = false,
}) => {
  return (
    <div style={{ marginBottom: SPACING.MD }}>
      <label style={getLabelStyle(required)}>
        {label}
        {required && ' *'}
      </label>
      <select
        value={value}
        onChange={(e) => onChange(e.target.value)}
        style={getSelectStyle()}
      >
        {options.map((option) => (
          <option key={option.value} value={option.value}>
            {option.label}
          </option>
        ))}
      </select>
    </div>
  );
};

// Info Panel Component
interface InfoPanelProps {
  title: string;
  children: ReactNode;
  color?: string;
}

export const InfoPanel: React.FC<InfoPanelProps> = ({
  title,
  children,
  color = FINCEPT.ORANGE,
}) => {
  return (
    <div style={{ ...getPanelStyle(), marginBottom: SPACING.MD }}>
      <div
        style={{
          color,
          fontSize: '9px',
          fontWeight: 700,
          marginBottom: SPACING.XS,
          textTransform: 'uppercase',
        }}
      >
        {title}
      </div>
      {children}
    </div>
  );
};

// Status Panel Component
interface StatusPanelProps {
  type: 'success' | 'error' | 'warning' | 'info';
  icon: ReactNode;
  message: string;
}

export const StatusPanel: React.FC<StatusPanelProps> = ({ type, icon, message }) => {
  const colors = {
    success: FINCEPT.GREEN,
    error: FINCEPT.RED,
    warning: FINCEPT.ORANGE,
    info: FINCEPT.CYAN,
  };

  const color = colors[type];

  return (
    <div style={getStatusPanelStyle(type)}>
      <div style={{ color }}>{icon}</div>
      <div style={{ color, fontSize: '9px', flex: 1 }}>{message}</div>
    </div>
  );
};

// Empty State Component
interface EmptyStateProps {
  icon: ReactNode;
  title: string;
  subtitle?: string;
}

export const EmptyState: React.FC<EmptyStateProps> = ({ icon, title, subtitle }) => {
  return (
    <div style={getEmptyStateStyle()}>
      <div style={{ color: FINCEPT.GRAY, opacity: 0.3, marginBottom: SPACING.LG }}>
        {icon}
      </div>
      <div
        style={{
          color: FINCEPT.GRAY,
          fontSize: '10px',
          marginBottom: SPACING.SM,
        }}
      >
        {title}
      </div>
      {subtitle && (
        <div
          style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            opacity: 0.7,
          }}
        >
          {subtitle}
        </div>
      )}
    </div>
  );
};

// Key-Value Display Component
interface KeyValueProps {
  label: string;
  value: string | number;
  valueColor?: string;
}

export const KeyValue: React.FC<KeyValueProps> = ({
  label,
  value,
  valueColor = FINCEPT.WHITE,
}) => {
  return (
    <div
      style={{
        color: FINCEPT.GRAY,
        fontSize: '8px',
        display: 'flex',
        justifyContent: 'space-between',
        marginBottom: '2px',
      }}
    >
      <span>{label}:</span>
      <span style={{ color: valueColor }}>{value}</span>
    </div>
  );
};

// Collapsible Settings Panel
interface SettingsPanelProps {
  isOpen: boolean;
  children: ReactNode;
}

export const SettingsPanel: React.FC<SettingsPanelProps> = ({ isOpen, children }) => {
  if (!isOpen) return null;

  return (
    <div
      style={{
        backgroundColor: FINCEPT.DARK_BG,
        border: `1px solid ${FINCEPT.ORANGE}`,
        borderRadius: '4px',
        padding: SPACING.MD,
        marginBottom: SPACING.MD,
        maxHeight: '300px',
        overflowY: 'auto',
      }}
    >
      <div
        style={{
          color: FINCEPT.ORANGE,
          fontSize: '10px',
          fontWeight: 700,
          marginBottom: SPACING.MD,
        }}
      >
        CONFIGURATION
      </div>
      {children}
    </div>
  );
};

// Tag Component
interface TagProps {
  label: string;
  color?: string;
}

export const Tag: React.FC<TagProps> = ({ label, color = FINCEPT.ORANGE }) => {
  return (
    <span
      style={{
        backgroundColor: `${color}30`,
        border: `1px solid ${color}`,
        color: color,
        fontSize: '7px',
        padding: '2px 6px',
        borderRadius: '3px',
        textTransform: 'uppercase',
      }}
    >
      {label}
    </span>
  );
};
