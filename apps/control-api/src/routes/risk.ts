import { FastifyInstance } from 'fastify';
import { listRisk } from '../services/riskService.js';

export async function registerRiskRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/risk', async () => ({
    items: await listRisk(),
    timestamp: new Date().toISOString(),
  }));
}
