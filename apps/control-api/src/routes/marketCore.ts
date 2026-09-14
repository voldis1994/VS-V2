
import { FastifyInstance } from 'fastify';
import { marketCoreStatus } from '../services/marketCoreService.js';

export async function registerMarketCoreRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/market-core/status', async () => marketCoreStatus());
}
