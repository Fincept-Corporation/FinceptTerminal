// Reusable Tailwind class strings for Algo Trading tab UI
// Dynamic colors still use inline style={{ }} with F constants from theme.ts

export const S = {
  // Section containers
  section: 'rounded border border-[#2A2A2A] overflow-hidden',
  sectionHeader: 'flex items-center gap-2 px-4 py-3 border-b border-[#2A2A2A]',
  sectionBody: 'p-4',

  // Typography
  pageTitle: 'text-[15px] font-bold tracking-wide text-white',
  heading: 'text-[13px] font-bold tracking-wide text-white',
  body: 'text-[11px] text-white',
  label: 'text-[10px] font-bold text-[#787878] tracking-wide uppercase mb-1.5 block',
  muted: 'text-[10px] text-[#4A4A4A]',
  secondary: 'text-[10px] text-[#787878]',

  // Inputs
  input: 'w-full px-3 py-2 bg-black text-white border border-[#2A2A2A] rounded text-[11px] font-mono outline-none transition-colors duration-200 focus:border-[#FF8800]',
  select: 'bg-black text-white border border-[#2A2A2A] rounded px-3 py-2 text-[11px] font-mono outline-none transition-colors duration-200 focus:border-[#FF8800]',

  // Buttons
  btnPrimary: 'flex items-center justify-center gap-1.5 px-4 py-2 rounded text-[11px] font-bold tracking-wide cursor-pointer transition-all duration-200 border-none',
  btnOutline: 'flex items-center gap-1.5 px-3 py-1.5 bg-transparent border border-[#2A2A2A] text-[#787878] rounded text-[10px] font-bold tracking-wide cursor-pointer transition-all duration-200 hover:border-[#FF8800] hover:text-white',
  btnGhost: 'flex items-center gap-1.5 p-1.5 bg-transparent border-none text-[#787878] rounded cursor-pointer transition-all duration-200 hover:text-white hover:bg-[#1F1F1F]',
  btnDanger: 'flex items-center gap-1.5 px-3 py-1.5 bg-transparent border border-[#2A2A2A] text-[#787878] rounded text-[10px] font-bold tracking-wide cursor-pointer transition-all duration-200 hover:border-[#FF3B3B] hover:text-[#FF3B3B]',

  // Cards
  card: 'rounded border border-[#2A2A2A] overflow-hidden transition-colors duration-200 hover:border-[#FF8800]/40',
  cardHeader: 'px-4 py-3 border-b border-[#2A2A2A]',
  cardBody: 'p-4',

  // Badges
  badge: 'text-[9px] font-bold px-2 py-0.5 rounded tracking-wide inline-flex items-center gap-1',

  // Layout
  modalOverlay: 'fixed inset-0 flex items-center justify-center z-[1000]',
  flexCenter: 'flex items-center justify-center',
  flexBetween: 'flex items-center justify-between',

  // Table
  th: 'px-3 py-2 text-[10px] font-bold text-[#787878] tracking-wide uppercase',
  td: 'px-3 py-2 text-[11px] font-mono',

  // Metric card
  metricLabel: 'text-[10px] font-bold text-[#787878] tracking-wide flex items-center gap-1.5 mb-1.5',
  metricValue: 'text-lg font-bold',
} as const;
