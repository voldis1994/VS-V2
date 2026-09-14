
import { create } from 'zustand';
import { api } from '../services/api';

interface PredictionState {
  items: unknown[];
  loading: boolean;
  fetch: () => Promise<void>;
}

export const usePredictionStore = create<PredictionState>((set) => ({
  items: [],
  loading: false,
  fetch: async () => {
    set({ loading: true });
    try {
      const res = await api.get('/api/predictions');
      set({ items: (res as { items?: unknown[] }).items || [], loading: false });
    } catch {
      set({ loading: false });
    }
  },
}));
