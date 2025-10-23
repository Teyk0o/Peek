/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    './src/pages/**/*.{js,ts,jsx,tsx,mdx}',
    './src/components/**/*.{js,ts,jsx,tsx,mdx}',
  ],
  theme: {
    extend: {
      colors: {
        'peek': {
          50: '#f8fafc',
          100: '#f1f5f9',
          500: '#0ea5e9',
          600: '#0284c7',
          900: '#0c2d48',
        },
        'trust': {
          'microsoft': '#107c10',      // Green - Microsoft signed
          'verified': '#28a745',       // Light green - Verified
          'trusted': '#0ea5e9',        // Blue - User trusted
          'unsigned': '#ffc107',       // Amber - Unsigned
          'invalid': '#dc3545',        // Red - Invalid
          'threat': '#dc3545',         // Red - Threat
          'error': '#6c757d',          // Gray - Error
          'unknown': '#6c757d',        // Gray - Unknown
        },
      },
      fontFamily: {
        'mono': ['ui-monospace', 'SFMono-Regular', 'Menlo', 'Monaco', 'monospace'],
      },
      animation: {
        'pulse-subtle': 'pulse 2s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'flash': 'flash 0.5s ease-out',
      },
      keyframes: {
        flash: {
          '0%': { backgroundColor: 'rgba(14, 165, 233, 0.3)' },
          '100%': { backgroundColor: 'transparent' },
        },
      },
      spacing: {
        '1px': '1px',
      },
    },
  },
  plugins: [],
};
