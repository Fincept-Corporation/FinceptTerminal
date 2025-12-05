            {/* Credentials Section - Predefined API Keys */}
            {activeSection === 'credentials' && (
              <div>
                <div style={{ marginBottom: '24px' }}>
                  <h2 style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    API KEY MANAGEMENT
                  </h2>
                  <p style={{ color: colors.text, fontSize: '10px' }}>
                    Configure API keys for data providers. All keys are stored locally and encrypted.
                  </p>
                </div>

                {/* Predefined API Key Fields */}
                <div style={{ display: 'grid', gap: '16px' }}>
                  {PREDEFINED_API_KEYS.map(({ key, label, description }) => (
                    <div
                      key={key}
                      style={{
                        background: colors.panel,
                        border: '1px solid #1a1a1a',
                        padding: '16px',
                        borderRadius: '4px'
                      }}
                    >
                      <h3 style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                        {label}
                      </h3>
                      <p style={{ color: colors.text, fontSize: '9px', marginBottom: '12px', opacity: 0.7 }}>
                        {description}
                      </p>

                      <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: '12px', alignItems: 'end' }}>
                        <div>
                          <label style={{ color: colors.text, fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                            API KEY
                          </label>
                          <input
                            type="password"
                            value={apiKeys[key] || ''}
                            onChange={(e) => setApiKeys({ ...apiKeys, [key]: e.target.value })}
                            placeholder="Enter API key"
                            style={{
                              width: '100%',
                              background: colors.background,
                              border: '1px solid #2a2a2a',
                              color: colors.text,
                              padding: '8px',
                              fontSize: '10px',
                              borderRadius: '3px',
                              fontFamily: 'monospace'
                            }}
                          />
                        </div>
                        <button
                          onClick={() => handleSaveApiKeyField(key, apiKeys[key] || '')}
                          disabled={loading || !apiKeys[key]}
                          style={{
                            background: apiKeys[key] ? colors.primary : '#333',
                            color: colors.text,
                            border: 'none',
                            padding: '8px 16px',
                            fontSize: '10px',
                            fontWeight: 'bold',
                            cursor: (loading || !apiKeys[key]) ? 'not-allowed' : 'pointer',
                            borderRadius: '3px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                            opacity: (loading || !apiKeys[key]) ? 0.5 : 1,
                            whiteSpace: 'nowrap'
                          }}
                        >
                          <Save size={14} />
                          SAVE
                        </button>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            )}
