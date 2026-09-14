import { useEffect, useState } from 'react';
import { api } from '../../services/api';
import { MarketTape } from '../../components/market/MarketTape';

export function MarketsPage() {
  const [instruments, setInstruments] = useState<unknown[]>([]);

  useEffect(() => {
    void api.get<unknown[]>('/api/market/instruments').then(setInstruments).catch(() => setInstruments([]));
  }, []);

  return (
    <section className="v2-page">
      <header className="v2-page-header">
        <h1>MARKETS</h1>
        <p className="v2-muted">Live instruments, regimes, and feed quality</p>
      </header>
      <MarketTape label="tape" />
      <pre className="v2-panel">{JSON.stringify(instruments, null, 2)}</pre>
    </section>
  );
}
