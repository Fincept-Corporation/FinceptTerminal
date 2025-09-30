// Tauri CORS Fetch Configuration
// This configures which requests should bypass CORS using the tauri-plugin-cors-fetch

// Declare the global CORSFetch API
declare global {
  interface Window {
    CORSFetch?: {
      config: (options: {
        include?: RegExp[];
        exclude?: string[];
        request?: {
          maxRedirections?: number;
          connectTimeout?: number;
          proxy?: {
            all?: string;
          };
        };
      }) => void;
    };
  }
}

export function initTauriCORS() {
  // Only configure in Tauri environment (production build)
  if (window.CORSFetch) {
    console.log('🔧 Configuring Tauri CORS plugin...');

    window.CORSFetch.config({
      // Include all HTTPS requests to bypass CORS
      include: [/^https?:\/\//i],

      // Optionally exclude specific URLs that don't need CORS bypass
      exclude: [],

      // Request configuration
      request: {
        maxRedirections: 5,
        connectTimeout: 30 * 1000, // 30 seconds
      }
    });

    console.log('✅ Tauri CORS plugin configured successfully');
  } else {
    console.log('ℹ️  CORSFetch not available (running in dev mode with Vite proxy)');
  }
}

// Auto-initialize on import
if (typeof window !== 'undefined') {
  // Wait for Tauri to be ready
  if (window.CORSFetch) {
    initTauriCORS();
  } else {
    // Try again after a short delay (Tauri may still be initializing)
    setTimeout(() => {
      if (window.CORSFetch) {
        initTauriCORS();
      }
    }, 100);
  }
}