import { useEffect, useRef } from 'react';
import { useLiveBrainStore } from '../state/liveBrainStore';
import type { LiveBrainSnapshot } from '../types/liveBrain';

const BRAIN_WS =
  import.meta.env.VITE_BRAIN_WS_URL ||
  `ws://${typeof window !== 'undefined' ? window.location.hostname : 'localhost'}:3000/ws/brain`;

/** Authoritative Brain WebSocket — market-core snapshots only. */
export function useLiveBrainFeed() {
  const applySnapshot = useLiveBrainStore((s) => s.applySnapshot);
  const fetchLive = useLiveBrainStore((s) => s.fetchLive);
  const fetchEvents = useLiveBrainStore((s) => s.fetchEvents);
  const setWsConnected = useLiveBrainStore((s) => s.setWsConnected);
  const timer = useRef<ReturnType<typeof setInterval>>();

  useEffect(() => {
    void fetchLive();
    void fetchEvents();
    timer.current = setInterval(() => {
      void fetchLive();
      void fetchEvents();
    }, 5000);

    let ws: WebSocket | null = null;
    let reconnect: ReturnType<typeof setTimeout>;
    let closed = false;

    const connect = () => {
      if (closed) return;
      ws = new WebSocket(BRAIN_WS);
      ws.onopen = () => setWsConnected(true);
      ws.onclose = () => {
        setWsConnected(false);
        if (!closed) reconnect = setTimeout(connect, 2500);
      };
      ws.onmessage = (ev) => {
        try {
          const msg = JSON.parse(String(ev.data)) as {
            type?: string;
            snapshot?: LiveBrainSnapshot;
          };
          if (msg.type === 'brain.live' && msg.snapshot) {
            applySnapshot(msg.snapshot);
            void fetchEvents();
          }
        } catch {
          /* ignore malformed */
        }
      };
    };
    connect();

    return () => {
      closed = true;
      clearInterval(timer.current);
      clearTimeout(reconnect);
      ws?.close();
    };
  }, [applySnapshot, fetchLive, fetchEvents, setWsConnected]);
}
