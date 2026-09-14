import { FastifyInstance } from 'fastify';
import { listLearning } from '../services/learningService.js';

export async function registerLearningRoutes(app: FastifyInstance): Promise<void> {
  app.get('/api/learning', async () => ({
    items: await listLearning(),
    timestamp: new Date().toISOString(),
  }));
}
