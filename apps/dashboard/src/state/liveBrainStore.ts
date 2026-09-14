import { create } from 'zustand';
import { api } from '../services/api';
import type {
  BrainEventRecord,
  BrainFeedStatus,
  InstrumentBrainView,
  LiveBrainSnapshot,
} from '../types/liveBrain';

interface LiveBrainState {
  snapshot: LiveBrainSnapshot | null;
  status: BrainFeedStatus | null;
  events: BrainEventRecord[];
  selectedEvent: BrainEventRecord | null;
  selectedInstrumentId: number | null;
  wsConnected: boolean;
  awaitingMarketCore: boolean;
  loading: boolean;
  error: string | null;
  fetchLive: () => Promise<void>;
  fetchEvents: (kind?: string) => Promise<void>;
  openEvent: (id: string) => Promise<void>;
  closeEvent: () => void;
  selectInstrument: (id: number | null) => void;
  applySnapshot: (snap: LiveBrainSnapshot) => void;
  setWsConnected: (v: boolean) => void;
}

function pickInstrumentId(instruments: InstrumentBrainView[], selected: number | null): number | null {
  if (selected != null && instruments.some((i) => i.instrument_id === selected)) return selected;
  return instruments[0]?.instrument_id ?? null;
}

export const useLiveBrainStore = create<LiveBrainState>((set, get) => ({
  snapshot: null,
  status: null,
  events: [],
  selectedEvent: null,
  selectedInstrumentId: null,
  wsConnected: false,
  awaitingMarketCore: true,
  loading: false,
  error: null,

  fetchLive: async () => {
    set({ loading: true, error: null });
    try {
      const res = await api.get<{
        snapshot: LiveBrainSnapshot | null;
        status: BrainFeedStatus;
        awaiting_market_core?: boolean;
      }>('/api/brain/live');
      const instruments = res.snapshot?.instruments ?? [];
      set({
        snapshot: res.snapshot,
        status: res.status,
        awaitingMarketCore: Boolean(res.awaiting_market_core ?? res.snapshot === null),
        selectedInstrumentId: pickInstrumentId(instruments, get().selectedInstrumentId),
        loading: false,
      });
    } catch (err) {
      set({ loading: false, error: err instanceof Error ? err.message : String(err) });
    }
  },

  fetchEvents: async (kind = 'all') => {
    try {
      const q =
        kind && kind !== 'all'
          ? `?kind=${encodeURIComponent(kind)}&limit=200`
          : '?limit=200';
      const res = await api.get<{ items: BrainEventRecord[] }>(`/api/brain/events${q}`);
      set({ events: res.items || [] });
    } catch {
      /* keep */
    }
  },

  openEvent: async (id: string) => {
    try {
      const res = await api.get<{ event: BrainEventRecord }>(`/api/brain/events/${id}`);
      set({ selectedEvent: res.event });
    } catch {
      set({ selectedEvent: get().events.find((e) => e.id === id) ?? null });
    }
  },

  closeEvent: () => set({ selectedEvent: null }),
  selectInstrument: (id) => set({ selectedInstrumentId: id }),

  applySnapshot: (snap) => {
    set({
      snapshot: snap,
      awaitingMarketCore: false,
      selectedInstrumentId: pickInstrumentId(snap.instruments, get().selectedInstrumentId),
      status: {
        connected: true,
        ingest_count: (get().status?.ingest_count ?? 0) + 1,
        last_ingest_ms: Date.now(),
        age_ms: 0,
        last_error: null,
        brain_version: snap.brain_version,
        model_id: snap.model_id,
        model_version: snap.model_version,
        instrument_count: snap.instruments.length,
        source: snap.source,
        authoritative: true,
        invents_decisions: false,
      },
    });
  },

  setWsConnected: (v) => set({ wsConnected: v }),
}));

export function selectActiveInstrument(state: LiveBrainState): InstrumentBrainView | null {
  const id = state.selectedInstrumentId;
  if (id == null || !state.snapshot) return null;
  return state.snapshot.instruments.find((i) => i.instrument_id === id) ?? null;
}
