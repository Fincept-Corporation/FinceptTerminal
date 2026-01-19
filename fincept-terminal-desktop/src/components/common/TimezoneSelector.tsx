/**
 * TimezoneSelector.tsx
 * 
 * A compact timezone selector widget for terminal headers.
 * Displays current time with timezone code and allows switching
 * between major financial market timezones.
 */

import React, { useState, useRef, useEffect } from 'react';
import { Clock, ChevronDown, Globe } from 'lucide-react';
import { useTimezone, useCurrentTime, TIMEZONE_OPTIONS } from '../../contexts/TimezoneContext';

// Fincept-style colors
const COLORS = {
    DARK_BG: '#0a0a0a',
    PANEL_BG: '#141414',
    BORDER: '#333333',
    ORANGE: '#FFA500',
    CYAN: '#00D4FF',
    WHITE: '#FFFFFF',
    GRAY: '#888888',
    HOVER: '#1a1a1a',
};

interface TimezoneSelectorProps {
    /** Compact mode shows only time + short code */
    compact?: boolean;
    /** Custom styling */
    style?: React.CSSProperties;
}

export const TimezoneSelector: React.FC<TimezoneSelectorProps> = ({
    compact = false,
    style
}) => {
    const [isOpen, setIsOpen] = useState(false);
    const dropdownRef = useRef<HTMLDivElement>(null);
    const { timezone, setTimezone, options } = useTimezone();
    const { formattedTime } = useCurrentTime();

    // Close dropdown when clicking outside
    useEffect(() => {
        const handleClickOutside = (event: MouseEvent) => {
            if (dropdownRef.current && !dropdownRef.current.contains(event.target as Node)) {
                setIsOpen(false);
            }
        };

        if (isOpen) {
            document.addEventListener('mousedown', handleClickOutside);
        }

        return () => {
            document.removeEventListener('mousedown', handleClickOutside);
        };
    }, [isOpen]);

    // Handle timezone selection
    const handleSelect = (id: string) => {
        setTimezone(id);
        setIsOpen(false);
    };

    return (
        <div
            ref={dropdownRef}
            style={{
                position: 'relative',
                display: 'inline-block',
                ...style
            }}
        >
            {/* Trigger Button */}
            <button
                onClick={() => setIsOpen(!isOpen)}
                style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    padding: compact ? '2px 6px' : '4px 10px',
                    backgroundColor: 'transparent',
                    border: `1px solid ${isOpen ? COLORS.CYAN : COLORS.BORDER}`,
                    borderRadius: '3px',
                    color: COLORS.CYAN,
                    cursor: 'pointer',
                    fontSize: compact ? '10px' : '11px',
                    fontFamily: '"IBM Plex Mono", "Consolas", monospace',
                    fontWeight: 500,
                    transition: 'all 0.2s ease',
                }}
                onMouseEnter={(e) => {
                    e.currentTarget.style.borderColor = COLORS.CYAN;
                    e.currentTarget.style.backgroundColor = COLORS.HOVER;
                }}
                onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = isOpen ? COLORS.CYAN : COLORS.BORDER;
                    e.currentTarget.style.backgroundColor = 'transparent';
                }}
                title={`Current timezone: ${timezone.label}`}
            >
                <Clock size={compact ? 10 : 12} />
                <span style={{ fontVariantNumeric: 'tabular-nums' }}>
                    {formattedTime}
                </span>
                <span style={{
                    color: COLORS.ORANGE,
                    fontWeight: 700,
                    fontSize: compact ? '8px' : '9px',
                }}>
                    {timezone.shortLabel}
                </span>
                <ChevronDown
                    size={compact ? 8 : 10}
                    style={{
                        transform: isOpen ? 'rotate(180deg)' : 'rotate(0deg)',
                        transition: 'transform 0.2s ease',
                    }}
                />
            </button>

            {/* Dropdown Menu */}
            {isOpen && (
                <div
                    style={{
                        position: 'absolute',
                        top: '100%',
                        right: 0,
                        marginTop: '4px',
                        backgroundColor: COLORS.PANEL_BG,
                        border: `1px solid ${COLORS.BORDER}`,
                        borderRadius: '4px',
                        minWidth: '200px',
                        zIndex: 9999,
                        boxShadow: `0 4px 16px rgba(0,0,0,0.5)`,
                        overflow: 'hidden',
                    }}
                >
                    {/* Header */}
                    <div
                        style={{
                            padding: '8px 12px',
                            borderBottom: `1px solid ${COLORS.BORDER}`,
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                        }}
                    >
                        <Globe size={12} color={COLORS.ORANGE} />
                        <span style={{
                            color: COLORS.ORANGE,
                            fontSize: '10px',
                            fontWeight: 700,
                            letterSpacing: '0.5px',
                        }}>
                            SELECT TIMEZONE
                        </span>
                    </div>

                    {/* Options */}
                    {options.map((option) => (
                        <div
                            key={option.id}
                            onClick={() => handleSelect(option.id)}
                            style={{
                                padding: '10px 12px',
                                cursor: 'pointer',
                                fontSize: '11px',
                                backgroundColor: timezone.id === option.id ? `${COLORS.ORANGE}15` : 'transparent',
                                borderLeft: timezone.id === option.id
                                    ? `3px solid ${COLORS.ORANGE}`
                                    : '3px solid transparent',
                                display: 'flex',
                                justifyContent: 'space-between',
                                alignItems: 'center',
                                transition: 'all 0.15s ease',
                            }}
                            onMouseEnter={(e) => {
                                if (timezone.id !== option.id) {
                                    e.currentTarget.style.backgroundColor = COLORS.HOVER;
                                }
                            }}
                            onMouseLeave={(e) => {
                                if (timezone.id !== option.id) {
                                    e.currentTarget.style.backgroundColor = 'transparent';
                                }
                            }}
                        >
                            <div>
                                <div style={{
                                    color: timezone.id === option.id ? COLORS.ORANGE : COLORS.WHITE,
                                    fontWeight: 600,
                                    marginBottom: '2px',
                                }}>
                                    {option.label}
                                </div>
                                <div style={{
                                    color: COLORS.GRAY,
                                    fontSize: '9px',
                                }}>
                                    {option.zone || 'System default'}
                                </div>
                            </div>
                            <span style={{
                                color: timezone.id === option.id ? COLORS.ORANGE : COLORS.CYAN,
                                fontWeight: 700,
                                fontSize: '10px',
                            }}>
                                {option.shortLabel}
                            </span>
                        </div>
                    ))}
                </div>
            )}
        </div>
    );
};

export default TimezoneSelector;
