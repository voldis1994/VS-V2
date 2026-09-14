
const BASE = import.meta.env.VITE_API_URL || 'http://localhost:3000';

async function request<T>(path: string): Promise<T> {
  const res = await fetch(`${BASE}${path}`, { credentials: 'include' });
  if (!res.ok) throw new Error(`${res.status} ${path}`);
  return res.json() as Promise<T>;
}

export const api = {
  get: request,
};
