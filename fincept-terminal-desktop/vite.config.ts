import { defineConfig, Plugin } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import { nodePolyfills } from 'vite-plugin-node-polyfills'
import path from 'path'

// Plugin to handle CCXT's .cjs files
const ccxtFixPlugin = (): Plugin => ({
  name: 'ccxt-cjs-fix',
  enforce: 'pre',
  resolveId(id) {
    if (id.includes('dydx-v4-client/long/index.cjs')) {
      return id.replace('.cjs', '.js');
    }
    return null;
  },
  transform(code, id) {
    if (id.includes('ccxt') && id.endsWith('.cjs')) {
      if (code.includes('typeof exports === "object"')) {
        return {
          code: code + '\nexport default module.exports;',
          map: null
        };
      }
    }
    return null;
  }
});

export default defineConfig({
  plugins: [
    react(),
    tailwindcss(),
    nodePolyfills({
      globals: {
        Buffer: 'build',  // Changed from true to 'build' to inject in build output
        global: true,
        process: true,
      },
      protocolImports: true,
      exclude: ['net', 'assert', 'vm'],
      overrides: {
        // Force readable-stream to use proper Buffer
        'readable-stream': 'readable-stream',
      },
    }),
    ccxtFixPlugin()
  ],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
      // Custom polyfills
      'net': path.resolve(__dirname, './src/polyfills/net-shim.ts'),
      'node:net': path.resolve(__dirname, './src/polyfills/net-shim.ts'),
      'assert': path.resolve(__dirname, './src/polyfills/assert-shim.ts'),
      'node:assert': path.resolve(__dirname, './src/polyfills/assert-shim.ts'),
      'vm': path.resolve(__dirname, './src/polyfills/vm-shim.ts'),
      'node:vm': path.resolve(__dirname, './src/polyfills/vm-shim.ts'),
      'ws': path.resolve(__dirname, './src/polyfills/ws-shim.ts'),
    },
    extensions: ['.mjs', '.js', '.mts', '.ts', '.jsx', '.tsx', '.json', '.cjs']
  },
  optimizeDeps: {
    exclude: ['@mapbox/node-pre-gyp', 'mock-aws-s3', 'aws-sdk', 'nock'],
    include: ['buffer', 'process', 'ccxt', 'readable-stream', 'crypto-browserify', 'stream-browserify'],
    esbuildOptions: {
      define: {
        global: 'globalThis',
      }
    }
  },
  define: {
    'process.env.NODE_ENV': JSON.stringify(process.env.NODE_ENV || 'development'),
    'global': 'globalThis',
  },
  build: {
    commonjsOptions: {
      transformMixedEsModules: true,
      include: [/node_modules/],
      extensions: ['.js', '.cjs'],
    },
    rollupOptions: {
      external: [
        'http-proxy-agent',
        'https-proxy-agent',
        'socks-proxy-agent',
        'tls',
        'node:tls',
        'fs',
        'node:fs',
        'path',
        'node:path',
        'os',
        'node:os'
      ],
      output: {
        manualChunks: {
          'react-vendor': ['react', 'react-dom', 'react-router-dom'],
          'chart-vendor': ['recharts', 'lightweight-charts'],
          'ui-vendor': ['@radix-ui/react-dialog', '@radix-ui/react-dropdown-menu', '@radix-ui/react-popover', '@radix-ui/react-tabs'],
          'flow-vendor': ['reactflow'],
          'tauri-vendor': ['@tauri-apps/api', '@tauri-apps/plugin-dialog', '@tauri-apps/plugin-sql'],
        }
      }
    },
    chunkSizeWarningLimit: 1000,
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
            proxyReq.setHeader('skip_zrok_interstitial', '1');
            console.log('Proxying request:', req.method, req.url, '-> https://finceptbackend.share.zrok.io' + req.url.replace('/api', ''));
          });
        }
      }
    }
  },
  envPrefix: ['VITE_', 'TAURI_'],
})
