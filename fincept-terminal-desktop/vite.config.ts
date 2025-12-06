import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import path from 'path'

export default defineConfig({
  plugins: [react(), tailwindcss()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
      // Polyfills for Node.js modules used by CCXT
      'http': 'stream-http',
      'https': 'https-browserify',
      'stream': 'stream-browserify',
      'crypto': 'crypto-browserify',
      'buffer': 'buffer',
      'util': 'util',
      'url': 'url',
      'zlib': 'browserify-zlib',
    },
  },
  optimizeDeps: {
    exclude: ['@mapbox/node-pre-gyp', 'mock-aws-s3', 'aws-sdk', 'nock', 'ccxt', 'ws'],
    esbuildOptions: {
      define: {
        global: 'globalThis'
      }
    }
  },
  define: {
    'process.env': {},
    'global': 'globalThis',
  },
  build: {
    rollupOptions: {
      external: [
        'ccxt',
        'ws',
        'http-proxy-agent',
        'https-proxy-agent',
        'socks-proxy-agent',
        'net',
        'tls',
        'fs',
        'path',
        'os'
      ],
      output: {
        manualChunks: {
          // Split vendor chunks
          'react-vendor': ['react', 'react-dom', 'react-router-dom'],
          'chart-vendor': ['recharts', 'lightweight-charts'],
          'ui-vendor': ['@radix-ui/react-dialog', '@radix-ui/react-dropdown-menu', '@radix-ui/react-popover', '@radix-ui/react-tabs'],
          'flow-vendor': ['reactflow'],
          'tauri-vendor': ['@tauri-apps/api', '@tauri-apps/plugin-dialog', '@tauri-apps/plugin-sql'],
        }
      }
    },
    chunkSizeWarningLimit: 1000, // Increase limit to 1000kb to reduce warnings
  },
  clearScreen: false,
  server: {
    port: 1420,
    strictPort: true,
    proxy: {
      '/api': {
        target: 'https://finceptbackend.share.zrok.io',
        changeOrigin: true,
        rewrite: (path) => path.replace(/^\/api/, ''),
        configure: (proxy, options) => {
          proxy.on('proxyReq', (proxyReq, req, res) => {
            // Add the skip_zrok_interstitial header
            proxyReq.setHeader('skip_zrok_interstitial', '1');
            console.log('Proxying request:', req.method, req.url, '-> https://finceptbackend.share.zrok.io' + req.url.replace('/api', ''));
          });
        }
      }
    }
  },
  envPrefix: ['VITE_', 'TAURI_'],
})