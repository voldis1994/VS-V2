
const WS_BASE = (import.meta.env.VITE_WS_URL || 'ws://localhost:3000/ws').replace(/\/ws$/, '');

export function connectStream(channel: string, onMessage: (data: unknown) => void): () => void {
  const ws = new WebSocket(`${WS_BASE}/ws/${channel}`);
  ws.onmessage = (ev) => {
    try { onMessage(JSON.parse(ev.data)); } catch { /* ignore */ }
  };
  return () => ws.close();
}
