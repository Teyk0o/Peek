# Peek Frontend

Modern React/Next.js UI for the Peek Network Security Monitor.

## Features

- Real-time connection monitoring
- Network traffic visualization
- Trust status indicators
- IPC communication with daemon
- Static export for WebView2 embedding

## Development

### Prerequisites

- Node.js 18+
- npm or yarn

### Setup

```bash
cd frontend
npm install
```

### Development Server

```bash
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) to view in browser.

### Build

```bash
npm run build
```

Generates static files in `dist/` ready for WebView2 embedding.

## Structure

```
frontend/
├── src/
│   ├── pages/
│   │   ├── index.tsx       # Main dashboard
│   │   └── _app.tsx        # App root
│   ├── components/         # Reusable React components
│   ├── hooks/              # Custom hooks
│   ├── lib/                # Utility functions
│   └── styles/             # Global CSS
├── public/                 # Static assets
├── next.config.js          # Next.js configuration
├── tailwind.config.js      # Tailwind CSS config
└── tsconfig.json           # TypeScript config
```

## IPC Communication

The frontend communicates with the daemon via WebView2 message API:

```typescript
// Send command to daemon
window.chrome.webview.postMessage(JSON.stringify({
  type: 'get_connections',
  data: {}
}))

// Listen for responses
window.chrome.webview.addEventListener('message', (event) => {
  const message = JSON.parse(event.data)
  console.log(message)
})
```

## Styling

Using Tailwind CSS with custom Peek color palette:

- `peek-50` to `peek-900` for primary colors
- `trust-*` for security trust status colors

See `tailwind.config.js` for theme configuration.

## Build Output

When built, static files are generated in `dist/`:
- `index.html` - Entry point
- `_next/` - Bundles and assets
- `public/` - Static resources

These are embedded in `peek.exe` resources or served from disk.
