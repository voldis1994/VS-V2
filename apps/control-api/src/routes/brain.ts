import { FastifyInstance } from 'fastify';
import { listBrainSnapshots } from '../services/brainService.js';

export async function registerBrainRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/brain/snapshots', async () => ({
    items: await listBrainSnapshots(),
    timestamp: new Date().toISOString(),
  }));
}
