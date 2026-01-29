/**
 * Surface Analytics - Batch Download Panel Component
 * Submit and manage batch download jobs for large data requests
 */

import React, { useState, useCallback, useEffect } from 'react';
import {
  Download,
  RefreshCw,
  AlertCircle,
  FileDown,
  Clock,
  CheckCircle,
  Loader2,
  Archive,
  Trash2,
  ChevronRight,
  Plus,
  Info,
  Calendar,
  Database,
  FileText,
} from 'lucide-react';
import { invoke } from '@tauri-apps/api/core';
import { FINCEPT_COLORS } from '../constants';

interface BatchJob {
  job_id: string;
  state: 'queued' | 'processing' | 'done' | 'expired';
  dataset: string;
  schema: string;
  symbols: string[];
  start: string;
  end: string;
  encoding: string;
  compression: string;
  cost: number;
  record_count: number;
  ts_received: string;
  ts_process_start: string;
  ts_process_done: string;
  size_bytes: number;
}

interface BatchFile {
  filename: string;
  size_bytes: number;
  hash: string;
  urls: Record<string, string>;
}

interface BatchDownloadPanelProps {
  apiKey: string | null;
  accentColor: string;
  textColor: string;
}

const JOB_STATE_COLORS: Record<string, string> = {
  queued: FINCEPT_COLORS.YELLOW,
  processing: FINCEPT_COLORS.CYAN,
  done: FINCEPT_COLORS.GREEN,
  expired: FINCEPT_COLORS.RED,
};

const JOB_STATE_ICONS: Record<string, React.ReactNode> = {
  queued: <Clock size={12} />,
  processing: <Loader2 size={12} className="animate-spin" />,
  done: <CheckCircle size={12} />,
  expired: <AlertCircle size={12} />,
};

const POPULAR_DATASETS = [
  { id: 'XNAS.ITCH', name: 'NASDAQ' },
  { id: 'GLBX.MDP3', name: 'CME Globex' },
  { id: 'OPRA.PILLAR', name: 'Options' },
  { id: 'DBEQ.BASIC', name: 'Sample' },
];

const SCHEMA_OPTIONS = [
  { value: 'trades', label: 'Trades' },
  { value: 'ohlcv-1d', label: 'OHLCV (Daily)' },
  { value: 'ohlcv-1h', label: 'OHLCV (Hourly)' },
  { value: 'ohlcv-1m', label: 'OHLCV (1min)' },
  { value: 'mbp-1', label: 'BBO' },
  { value: 'mbp-10', label: 'MBP-10' },
  { value: 'mbo', label: 'MBO' },
];

const ENCODING_OPTIONS = [
  { value: 'dbn', label: 'DBN (Binary)' },
  { value: 'csv', label: 'CSV' },
  { value: 'json', label: 'JSON' },
];

const COMPRESSION_OPTIONS = [
  { value: 'zstd', label: 'Zstandard' },
  { value: 'none', label: 'None' },
];

export const BatchDownloadPanel: React.FC<BatchDownloadPanelProps> = ({
  apiKey,
  accentColor,
  textColor,
}) => {
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [jobs, setJobs] = useState<BatchJob[]>([]);
  const [selectedJob, setSelectedJob] = useState<BatchJob | null>(null);
  const [jobFiles, setJobFiles] = useState<BatchFile[]>([]);
  const [showNewJobForm, setShowNewJobForm] = useState(false);
  const [downloadingJob, setDownloadingJob] = useState<string | null>(null);

  // New job form state
  const [newJobDataset, setNewJobDataset] = useState('XNAS.ITCH');
  const [newJobSymbols, setNewJobSymbols] = useState('SPY');
  const [newJobSchema, setNewJobSchema] = useState('trades');
  const [newJobStartDate, setNewJobStartDate] = useState('');
  const [newJobEndDate, setNewJobEndDate] = useState('');
  const [newJobEncoding, setNewJobEncoding] = useState('dbn');
  const [newJobCompression, setNewJobCompression] = useState('zstd');

  // Initialize dates
  useEffect(() => {
    const end = new Date();
    const start = new Date();
    start.setDate(start.getDate() - 7);
    setNewJobStartDate(start.toISOString().split('T')[0]);
    setNewJobEndDate(end.toISOString().split('T')[0]);
  }, []);

  const fetchJobs = useCallback(async () => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    try {
      const result = await invoke<string>('databento_list_batch_jobs', {
        apiKey,
        states: 'queued,processing,done',
        sinceDays: 14,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setJobs(parsed.jobs || []);
      }
    } catch (err) {
      setError(`Failed to fetch jobs: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey]);

  const fetchJobFiles = useCallback(async (jobId: string) => {
    if (!apiKey) return;

    try {
      const result = await invoke<string>('databento_list_batch_files', {
        apiKey,
        jobId,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setJobFiles(parsed.files || []);
      }
    } catch (err) {
      setError(`Failed to fetch files: ${err}`);
    }
  }, [apiKey]);

  const submitJob = useCallback(async () => {
    if (!apiKey) return;

    setLoading(true);
    setError(null);

    const symbols = newJobSymbols.split(',').map((s) => s.trim().toUpperCase()).filter(Boolean);

    if (symbols.length === 0) {
      setError('Please enter at least one symbol');
      setLoading(false);
      return;
    }

    try {
      const result = await invoke<string>('databento_submit_batch_job', {
        apiKey,
        dataset: newJobDataset,
        symbols,
        schema: newJobSchema,
        startDate: newJobStartDate,
        endDate: newJobEndDate,
        encoding: newJobEncoding,
        compression: newJobCompression,
        stypeIn: 'raw_symbol',
        splitDuration: 'day',
        delivery: 'download',
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        setShowNewJobForm(false);
        fetchJobs();
      }
    } catch (err) {
      setError(`Failed to submit job: ${err}`);
    } finally {
      setLoading(false);
    }
  }, [apiKey, newJobDataset, newJobSymbols, newJobSchema, newJobStartDate, newJobEndDate, newJobEncoding, newJobCompression, fetchJobs]);

  const downloadJob = useCallback(async (jobId: string) => {
    if (!apiKey) return;

    setDownloadingJob(jobId);

    try {
      const result = await invoke<string>('databento_download_batch_job', {
        apiKey,
        jobId,
        outputDir: null,
        filename: null,
      });
      const parsed = JSON.parse(result);

      if (parsed.error) {
        setError(parsed.message);
      } else {
        // Show success message with download location
        setError(null);
        alert(`Downloaded ${parsed.count} file(s) to:\n${parsed.output_dir}`);
      }
    } catch (err) {
      setError(`Failed to download: ${err}`);
    } finally {
      setDownloadingJob(null);
    }
  }, [apiKey]);

  useEffect(() => {
    if (apiKey) {
      fetchJobs();
    }
  }, [apiKey, fetchJobs]);

  const formatBytes = (bytes: number): string => {
    if (!bytes || bytes === 0) return '--';
    if (bytes >= 1073741824) return `${(bytes / 1073741824).toFixed(1)} GB`;
    if (bytes >= 1048576) return `${(bytes / 1048576).toFixed(1)} MB`;
    if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${bytes} B`;
  };

  const formatRecordCount = (count: number): string => {
    if (!count || count === 0) return '--';
    if (count >= 1000000000) return `${(count / 1000000000).toFixed(1)}B`;
    if (count >= 1000000) return `${(count / 1000000).toFixed(1)}M`;
    if (count >= 1000) return `${(count / 1000).toFixed(1)}K`;
    return count.toString();
  };

  const formatDate = (dateStr: string): string => {
    if (!dateStr) return '--';
    try {
      return new Date(dateStr).toLocaleDateString('en-US', {
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
      });
    } catch {
      return dateStr;
    }
  };

  if (!apiKey) {
    return (
      <div
        className="p-4 text-center"
        style={{
          backgroundColor: FINCEPT_COLORS.DARK_BG,
          border: `1px solid ${FINCEPT_COLORS.BORDER}`,
          borderRadius: '2px',
        }}
      >
        <Download size={24} style={{ color: FINCEPT_COLORS.MUTED, margin: '0 auto 8px' }} />
        <div className="text-xs" style={{ color: FINCEPT_COLORS.MUTED }}>
          Configure Databento API key for batch downloads
        </div>
      </div>
    );
  }

  return (
    <div
      style={{
        backgroundColor: FINCEPT_COLORS.DARK_BG,
        border: `1px solid ${FINCEPT_COLORS.BORDER}`,
        borderRadius: '2px',
        overflow: 'hidden',
      }}
    >
      {/* Header */}
      <div
        className="flex items-center justify-between p-2"
        style={{
          backgroundColor: FINCEPT_COLORS.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`,
        }}
      >
        <div className="flex items-center gap-2">
          <Archive size={14} style={{ color: accentColor }} />
          <span className="text-xs font-bold" style={{ color: accentColor }}>
            BATCH DOWNLOADS
          </span>
          <span className="text-[9px] px-1" style={{ backgroundColor: FINCEPT_COLORS.BORDER, color: FINCEPT_COLORS.MUTED, borderRadius: '2px' }}>
            {jobs.length}
          </span>
        </div>
        <div className="flex items-center gap-1">
          <button
            onClick={() => setShowNewJobForm(!showNewJobForm)}
            className="p-1"
            style={{ color: showNewJobForm ? FINCEPT_COLORS.GREEN : accentColor }}
            title="New Job"
          >
            <Plus size={12} />
          </button>
          <button
            onClick={fetchJobs}
            disabled={loading}
            style={{ color: loading ? FINCEPT_COLORS.MUTED : accentColor }}
          >
            <RefreshCw size={12} className={loading ? 'animate-spin' : ''} />
          </button>
        </div>
      </div>

      {/* Error */}
      {error && (
        <div
          className="flex items-center gap-2 m-2 p-2 text-xs"
          style={{
            backgroundColor: `${FINCEPT_COLORS.RED}15`,
            border: `1px solid ${FINCEPT_COLORS.RED}50`,
            borderRadius: '2px',
            color: FINCEPT_COLORS.RED,
          }}
        >
          <AlertCircle size={12} />
          <span>{error}</span>
        </div>
      )}

      {/* New Job Form */}
      {showNewJobForm && (
        <div className="p-2" style={{ borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`, backgroundColor: FINCEPT_COLORS.BLACK }}>
          <div className="text-[10px] font-bold mb-2" style={{ color: accentColor }}>
            NEW BATCH JOB
          </div>

          <div className="grid grid-cols-2 gap-2 mb-2">
            {/* Dataset */}
            <div>
              <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.MUTED }}>Dataset</label>
              <select
                value={newJobDataset}
                onChange={(e) => setNewJobDataset(e.target.value)}
                className="w-full text-[10px] p-1"
                style={{
                  backgroundColor: FINCEPT_COLORS.DARK_BG,
                  border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                  color: textColor,
                  borderRadius: '2px',
                }}
              >
                {POPULAR_DATASETS.map((ds) => (
                  <option key={ds.id} value={ds.id}>{ds.name} ({ds.id})</option>
                ))}
              </select>
            </div>

            {/* Schema */}
            <div>
              <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.MUTED }}>Schema</label>
              <select
                value={newJobSchema}
                onChange={(e) => setNewJobSchema(e.target.value)}
                className="w-full text-[10px] p-1"
                style={{
                  backgroundColor: FINCEPT_COLORS.DARK_BG,
                  border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                  color: textColor,
                  borderRadius: '2px',
                }}
              >
                {SCHEMA_OPTIONS.map((s) => (
                  <option key={s.value} value={s.value}>{s.label}</option>
                ))}
              </select>
            </div>
          </div>

          {/* Symbols */}
          <div className="mb-2">
            <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.MUTED }}>Symbols (comma-separated)</label>
            <input
              type="text"
              value={newJobSymbols}
              onChange={(e) => setNewJobSymbols(e.target.value)}
              placeholder="SPY, QQQ, AAPL"
              className="w-full text-[10px] p-1"
              style={{
                backgroundColor: FINCEPT_COLORS.DARK_BG,
                border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                color: textColor,
                borderRadius: '2px',
              }}
            />
          </div>

          <div className="grid grid-cols-2 gap-2 mb-2">
            {/* Start Date */}
            <div>
              <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.MUTED }}>Start Date</label>
              <input
                type="date"
                value={newJobStartDate}
                onChange={(e) => setNewJobStartDate(e.target.value)}
                className="w-full text-[10px] p-1"
                style={{
                  backgroundColor: FINCEPT_COLORS.DARK_BG,
                  border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                  color: textColor,
                  borderRadius: '2px',
                }}
              />
            </div>

            {/* End Date */}
            <div>
              <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.MUTED }}>End Date</label>
              <input
                type="date"
                value={newJobEndDate}
                onChange={(e) => setNewJobEndDate(e.target.value)}
                className="w-full text-[10px] p-1"
                style={{
                  backgroundColor: FINCEPT_COLORS.DARK_BG,
                  border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                  color: textColor,
                  borderRadius: '2px',
                }}
              />
            </div>
          </div>

          <div className="grid grid-cols-2 gap-2 mb-2">
            {/* Encoding */}
            <div>
              <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.MUTED }}>Encoding</label>
              <select
                value={newJobEncoding}
                onChange={(e) => setNewJobEncoding(e.target.value)}
                className="w-full text-[10px] p-1"
                style={{
                  backgroundColor: FINCEPT_COLORS.DARK_BG,
                  border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                  color: textColor,
                  borderRadius: '2px',
                }}
              >
                {ENCODING_OPTIONS.map((e) => (
                  <option key={e.value} value={e.value}>{e.label}</option>
                ))}
              </select>
            </div>

            {/* Compression */}
            <div>
              <label className="text-[9px] mb-1 block" style={{ color: FINCEPT_COLORS.MUTED }}>Compression</label>
              <select
                value={newJobCompression}
                onChange={(e) => setNewJobCompression(e.target.value)}
                className="w-full text-[10px] p-1"
                style={{
                  backgroundColor: FINCEPT_COLORS.DARK_BG,
                  border: `1px solid ${FINCEPT_COLORS.BORDER}`,
                  color: textColor,
                  borderRadius: '2px',
                }}
              >
                {COMPRESSION_OPTIONS.map((c) => (
                  <option key={c.value} value={c.value}>{c.label}</option>
                ))}
              </select>
            </div>
          </div>

          <button
            onClick={submitJob}
            disabled={loading}
            className="w-full py-1.5 text-[10px] font-bold flex items-center justify-center gap-1"
            style={{
              backgroundColor: loading ? FINCEPT_COLORS.BORDER : accentColor,
              color: FINCEPT_COLORS.BLACK,
              borderRadius: '2px',
            }}
          >
            {loading ? <Loader2 size={12} className="animate-spin" /> : <Plus size={12} />}
            SUBMIT JOB
          </button>
        </div>
      )}

      {/* Jobs List */}
      <div className="p-2">
        {loading && jobs.length === 0 ? (
          <div className="flex items-center justify-center p-4">
            <RefreshCw size={16} className="animate-spin" style={{ color: accentColor }} />
          </div>
        ) : jobs.length === 0 ? (
          <div className="text-xs text-center p-4" style={{ color: FINCEPT_COLORS.MUTED }}>
            No batch jobs found. Click + to create one.
          </div>
        ) : (
          <div className="space-y-2">
            {jobs.map((job) => (
              <div
                key={job.job_id}
                className="p-2"
                style={{
                  backgroundColor: selectedJob?.job_id === job.job_id ? `${accentColor}15` : FINCEPT_COLORS.BLACK,
                  borderRadius: '2px',
                  border: `1px solid ${selectedJob?.job_id === job.job_id ? accentColor : FINCEPT_COLORS.BORDER}`,
                }}
              >
                {/* Job Header */}
                <div className="flex items-center justify-between mb-1">
                  <div className="flex items-center gap-2">
                    <span style={{ color: JOB_STATE_COLORS[job.state] }}>
                      {JOB_STATE_ICONS[job.state]}
                    </span>
                    <span className="text-[10px] font-bold" style={{ color: textColor }}>
                      {job.dataset}
                    </span>
                    <span className="text-[9px] px-1" style={{ backgroundColor: FINCEPT_COLORS.BORDER, borderRadius: '2px', color: FINCEPT_COLORS.MUTED }}>
                      {job.schema}
                    </span>
                  </div>
                  <span className="text-[9px] font-bold" style={{ color: JOB_STATE_COLORS[job.state] }}>
                    {job.state.toUpperCase()}
                  </span>
                </div>

                {/* Job Details */}
                <div className="grid grid-cols-3 gap-1 text-[9px] mb-1" style={{ color: FINCEPT_COLORS.MUTED }}>
                  <div className="flex items-center gap-1">
                    <FileText size={9} />
                    <span>{job.symbols?.slice(0, 3).join(', ')}{job.symbols?.length > 3 ? '...' : ''}</span>
                  </div>
                  <div className="flex items-center gap-1">
                    <Calendar size={9} />
                    <span>{job.start?.substring(0, 10)}</span>
                  </div>
                  <div className="flex items-center gap-1">
                    <Database size={9} />
                    <span>{formatRecordCount(job.record_count)} records</span>
                  </div>
                </div>

                {/* Job Size & Cost */}
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2 text-[9px]" style={{ color: FINCEPT_COLORS.MUTED }}>
                    <span>{formatBytes(job.size_bytes)}</span>
                    {job.cost > 0 && (
                      <span style={{ color: FINCEPT_COLORS.YELLOW }}>${job.cost.toFixed(2)}</span>
                    )}
                  </div>

                  {/* Actions */}
                  <div className="flex items-center gap-1">
                    {job.state === 'done' && (
                      <button
                        onClick={() => downloadJob(job.job_id)}
                        disabled={downloadingJob === job.job_id}
                        className="p-1 text-[9px] flex items-center gap-1"
                        style={{
                          backgroundColor: accentColor,
                          color: FINCEPT_COLORS.BLACK,
                          borderRadius: '2px',
                        }}
                      >
                        {downloadingJob === job.job_id ? (
                          <Loader2 size={10} className="animate-spin" />
                        ) : (
                          <FileDown size={10} />
                        )}
                        Download
                      </button>
                    )}
                    <button
                      onClick={() => {
                        if (selectedJob?.job_id === job.job_id) {
                          setSelectedJob(null);
                          setJobFiles([]);
                        } else {
                          setSelectedJob(job);
                          if (job.state === 'done') {
                            fetchJobFiles(job.job_id);
                          }
                        }
                      }}
                      className="p-1"
                      style={{ color: FINCEPT_COLORS.MUTED }}
                    >
                      <ChevronRight
                        size={12}
                        style={{
                          transform: selectedJob?.job_id === job.job_id ? 'rotate(90deg)' : 'none',
                          transition: 'transform 0.2s',
                        }}
                      />
                    </button>
                  </div>
                </div>

                {/* Expanded Job Files */}
                {selectedJob?.job_id === job.job_id && job.state === 'done' && jobFiles.length > 0 && (
                  <div className="mt-2 pt-2" style={{ borderTop: `1px solid ${FINCEPT_COLORS.BORDER}` }}>
                    <div className="text-[9px] mb-1" style={{ color: FINCEPT_COLORS.MUTED }}>
                      FILES ({jobFiles.length})
                    </div>
                    <div className="space-y-1 max-h-32 overflow-y-auto">
                      {jobFiles.map((file, idx) => (
                        <div
                          key={idx}
                          className="flex items-center justify-between p-1 text-[9px]"
                          style={{ backgroundColor: FINCEPT_COLORS.DARK_BG, borderRadius: '2px' }}
                        >
                          <span style={{ color: textColor }}>{file.filename}</span>
                          <span style={{ color: FINCEPT_COLORS.MUTED }}>{formatBytes(file.size_bytes)}</span>
                        </div>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Info Footer */}
      <div
        className="flex items-center gap-1 p-2 text-[9px]"
        style={{
          borderTop: `1px solid ${FINCEPT_COLORS.BORDER}`,
          color: FINCEPT_COLORS.MUTED,
        }}
      >
        <Info size={10} />
        <span>Jobs expire after 7 days. Download completed jobs promptly.</span>
      </div>
    </div>
  );
};
