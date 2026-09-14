
import { create } from 'zustand';
import { api } from '../services/api';

interface BrainState {
  items: unknown[];
  loading: boolean;
  fetch: () => Promise<void>;
}

export const useBrainStore = create<BrainState>((set) => ({
  items: [],
  loading: false,
  fetch: async () => {
    set({ loading: true });
    try {
      const res = await api.get('/api/brain');
      set({ items: (res as { items?: unknown[] }).items || [], loading: false });
    } catch {
      set({ loading: false });
    }
  },
}));
