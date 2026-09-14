import { useEffect, useState } from 'react';
import { api } from '../../services/api';
import { BrainScoreCard } from '../../components/brain/BrainScoreCard';

interface BrainResponse {
  items?: unknown[];
}

export function BrainPage() {
  const [data, setData] = useState<BrainResponse | null>(null);

  useEffect(() => {
    void api.get<BrainResponse>('/api/brain/snapshots').then(setData).catch(() => setData(null));
  }, []);

  return (
    <section className="v2-page">
      <header className="v2-page-header">
        <h1>BRAIN</h1>
        <p className="v2-muted">Structure / momentum / pressure composite scores</p>
      </header>
      <BrainScoreCard />
      <pre className="v2-panel">{JSON.stringify(data, null, 2)}</pre>
    </section>
  );
}
