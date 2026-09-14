
export const runtimeConfig = {
  marketCoreUrl: process.env.MARKET_CORE_URL || 'http://127.0.0.1:9100',
  pipelineToken: process.env.PIPELINE_TOKEN || process.env.PIPELINE_SERVICE_TOKEN || '',
  redisUrl: process.env.REDIS_URL || 'redis://localhost:6379',
};
