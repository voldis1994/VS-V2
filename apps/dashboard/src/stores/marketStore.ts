
import { create } from 'zustand';
import { api } from '../services/api';

interface MarketState {
  items: unknown[];
  loading: boolean;
  fetch: () => Promise<void>;
}

export const useMarketStore = create<MarketState>((set) => ({
  items: [],
  loading: false,
  fetch: async () => {
    set({ loading: true });
    try {
      const res = await api.get('/api/market');
      set({ items: (res as { items?: unknown[] }).items || [], loading: false });
    } catch {
      set({ loading: false });
    }
  },
}));
