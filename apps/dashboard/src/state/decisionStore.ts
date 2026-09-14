
import { create } from 'zustand';
import { api } from '../services/api';

interface DecisionState {
  items: unknown[];
  loading: boolean;
  fetch: () => Promise<void>;
}

export const useDecisionStore = create<DecisionState>((set) => ({
  items: [],
  loading: false,
  fetch: async () => {
    set({ loading: true });
    try {
      const res = await api.get('/api/decision');
      set({ items: (res as { items?: unknown[] }).items || [], loading: false });
    } catch {
      set({ loading: false });
    }
  },
}));
