/** @type {import('next').NextConfig} */
const nextConfig = {
  // Export as static HTML (no Node.js server needed)
  output: 'export',

  // Output directory
  distDir: 'dist',

  // Disable image optimization for static export
  images: {
    unoptimized: true,
  },

  // Disable static generation tracing
  productionBrowserSourceMaps: false,

  // Base path for WebView2 embedded assets
  basePath: '',
  assetPrefix: './',

  // React strict mode
  reactStrictMode: true,

  // Security headers
  headers: async () => {
    return [
      {
        source: '/:path*',
        headers: [
          {
            key: 'X-Content-Type-Options',
            value: 'nosniff',
          },
          {
            key: 'X-Frame-Options',
            value: 'DENY',
          },
          {
            key: 'X-XSS-Protection',
            value: '1; mode=block',
          },
          {
            key: 'Referrer-Policy',
            value: 'strict-origin-when-cross-origin',
          },
        ],
      },
    ];
  },

  // Environment variables
  env: {
    NEXT_PUBLIC_APP_VERSION: process.env.APP_VERSION || '2.0.0-alpha.1',
    NEXT_PUBLIC_APP_NAME: 'Peek',
  },
};

module.exports = nextConfig;
