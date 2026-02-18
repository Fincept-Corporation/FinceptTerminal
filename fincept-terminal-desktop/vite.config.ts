import { defineConfig, Plugin } from 'vite'
import react from '@vitejs/plugin-react'
import tailwindcss from '@tailwindcss/vite'
import { nodePolyfills } from 'vite-plugin-node-polyfills'
import path from 'path'

// Plugin to inject globals at build time
const injectGlobalsPlugin = (): Plugin => ({
  name: 'inject-globals',
  enforce: 'pre',
  transformIndexHtml() {
    return [
      {
        tag: 'script',
        children: `
          // Critical: Set up globals before any module loads
          window.global = window;
          window.process = {
            env: { NODE_ENV: '${process.env.NODE_ENV || 'production'}' },
            version: 'v18.0.0',
            browser: true,
            nextTick: function(fn) {
              var args = Array.prototype.slice.call(arguments, 1);
              setTimeout(function() { fn.apply(null, args); }, 0);
            },
            cwd: function() { return '/'; },
            platform: 'browser',
            argv: [],
            pid: 1
          };
          window.module = { exports: {} };

          // Make them available on globalThis too
          globalThis.global = window.global;
          globalThis.process = window.process;
          globalThis.module = window.module;

        `,
        injectTo: 'head-prepend',
      }
    ];
  }
});

// Plugin to stub broken @loaders.gl/draco package (missing dist files)
const dracoStubPlugin = (): Plugin => ({
  name: 'draco-stub',
  enforce: 'pre',
  resolveId(id) {
    if (id === '@loaders.gl/draco') {
      return '\0draco-stub';
    }
    return null;
  },
  load(id) {
    if (id === '\0draco-stub') {
      return 'export const DracoLoader = null; export const DracoWorkerLoader = null; export default {};';
    }
    return null;
  },
});

// Plugin to handle CCXT's .cjs files, bn.js, and module.exports
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
    // Handle CCXT .cjs files
    if (id.includes('ccxt') && id.endsWith('.cjs')) {
      // Wrap CommonJS module.exports in ES module
      if (code.includes('module.exports')) {
        const wrappedCode = `
          const module = { exports: {} };
          const exports = module.exports;
          ${code}
          export default module.exports;
        `;
        return {
          code: wrappedCode,
          map: null
        };
      }
    }

    // Replace typeof checks that might fail
    if (id.includes('node_modules')) {
      let transformed = code;

      // Replace typeof module checks
      if (code.includes('typeof module')) {
        transformed = transformed.replace(
          /typeof module\s*(!==?|===?)\s*['"]undefined['"]/g,
          'typeof window !== "undefined"'
        );
      }

      // Replace typeof exports checks
      if (code.includes('typeof exports')) {
        transformed = transformed.replace(
          /typeof exports\s*(!==?|===?)\s*['"]undefined['"]/g,
          'typeof window !== "undefined"'
        );
      }

      if (transformed !== code) {
        return {
          code: transformed,
          map: null
        };
      }
    }

    return null;
  }
});

export default defineConfig({
  plugins: [
    dracoStubPlugin(),      // Must be before react/vite to intercept broken draco imports
    injectGlobalsPlugin(),  // Must be first to inject globals early
    react(),
    tailwindcss(),
    nodePolyfills({
      globals: {
        Buffer: true,   // Let vite-plugin-node-polyfills handle Buffer
        global: false,  // Handled by injectGlobalsPlugin
        process: true,  // Let vite-plugin-node-polyfills handle process
      },
      protocolImports: true,
      exclude: ['net', 'assert', 'vm'],  // We have custom polyfills for these
      overrides: {
        // Don't override readable-stream, let it use the polyfilled modules
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
    exclude: ['@mapbox/node-pre-gyp', 'mock-aws-s3', 'aws-sdk', 'nock', '@loaders.gl/draco'],
    // Force these to be pre-bundled and ensure they have proper globals
    include: [
      'buffer',
      'process',
      'ccxt',  // Include main CCXT package
      'plotly.js',  // Pre-bundle plotly for production
      'react-plotly.js',
      'readable-stream',
      'stream-browserify',
      'events',
      'util',
    ],
    esbuildOptions: {
      define: {
        global: 'globalThis',
      },
      // Don't inject globals via esbuild - let vite-plugin-node-polyfills handle it
      // inject: [path.resolve(__dirname, './src/polyfills/globals-inject.ts')],
    },
    // Force optimization even in production
    force: false,
  },
  define: {
    'process.env.NODE_ENV': JSON.stringify(process.env.NODE_ENV || 'development'),
    'global': 'globalThis',
    // Don't define module here - let the runtime polyfill handle it
  },
  build: {
    commonjsOptions: {
      transformMixedEsModules: true,
      include: [/node_modules/, /ccxt/],  // Explicitly include ccxt
      extensions: ['.js', '.cjs', '.mjs'],
      defaultIsModuleExports: true,  // Treat CommonJS exports as default exports
      requireReturnsDefault: 'auto',  // Auto-detect require behavior
      esmExternals: true,
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
        'node:os',
        '@loaders.gl/draco'
      ],
      output: {
        manualChunks: {
          'react-vendor': ['react', 'react-dom', 'react-router-dom'],
          'chart-vendor': ['recharts', 'lightweight-charts'],
          'plotly-vendor': ['plotly.js', 'react-plotly.js'],  // Separate chunk for Plotly (heavy 3D lib)
          'ui-vendor': ['@radix-ui/react-dialog', '@radix-ui/react-dropdown-menu', '@radix-ui/react-popover', '@radix-ui/react-tabs'],
          'flow-vendor': ['reactflow'],
          'tauri-vendor': ['@tauri-apps/api', '@tauri-apps/plugin-dialog'],
          'ccxt-vendor': ['ccxt'],  // Separate chunk for CCXT
        },
      }
    },
    chunkSizeWarningLimit: 1000,
    // Enable source maps for easier debugging in production
    sourcemap: false,
    // Use esbuild for minification (faster and already installed)
    minify: 'esbuild',
    target: 'es2020',  // Modern browsers support
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
          });
        }
      }
    }
  },
  envPrefix: ['VITE_', 'TAURI_'],
})
