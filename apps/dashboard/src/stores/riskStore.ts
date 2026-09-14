
import { create } from 'zustand';
import { api } from '../services/api';

interface RiskState {
  items: unknown[];
  loading: boolean;
  fetch: () => Promise<void>;
}

export const useRiskStore = create<RiskState>((set) => ({
  items: [],
  loading: false,
  fetch: async () => {
    set({ loading: true });
    try {
      const res = await api.get('/api/risk');
      set({ items: (res as { items?: unknown[] }).items || [], loading: false });
    } catch {
      set({ loading: false });
    }
  },
}));
