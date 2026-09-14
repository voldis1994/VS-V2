
import { create } from 'zustand';
import { api } from '../services/api';

interface PositionState {
  items: unknown[];
  loading: boolean;
  fetch: () => Promise<void>;
}

export const usePositionStore = create<PositionState>((set) => ({
  items: [],
  loading: false,
  fetch: async () => {
    set({ loading: true });
    try {
      const res = await api.get('/api/position');
      set({ items: (res as { items?: unknown[] }).items || [], loading: false });
    } catch {
      set({ loading: false });
    }
  },
}));
