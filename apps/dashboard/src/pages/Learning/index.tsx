
import { useEffect, useState } from 'react';
import { api } from '../../services/api';

export default function LearningPage() {
  const [data, setData] = useState<unknown>(null);
  useEffect(() => {
    void api.get('/api/learning').then(setData).catch(() => setData(null));
  }, []);
  return (
    <section className="v2-page">
      <header className="v2-page-header">
        <h1>LEARNING</h1>
        <p className="v2-muted">VS-V2 trading desk — Learning view</p>
      </header>
      <pre className="v2-panel">{JSON.stringify(data, null, 2)}</pre>
    </section>
  );
}
export { LearningPage };
