import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// During development the frontend runs on Vite's dev server and proxies API
// calls to the Node service on :8787. In production the Node service serves
// the built files directly, so these proxies are dev-only.
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/app': 'http://localhost:8787',
      '/v1': 'http://localhost:8787',
      '/healthz': 'http://localhost:8787',
    },
  },
  build: {
    outDir: 'dist',
    sourcemap: false,
  },
});
