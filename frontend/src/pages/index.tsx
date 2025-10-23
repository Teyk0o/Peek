import React, { useEffect, useState } from 'react'
import Head from 'next/head'

interface Connection {
  connection_id: number
  process_name: string
  direction: 'INBOUND' | 'OUTBOUND'
  protocol: 'TCP' | 'UDP'
  remote_address: string
  remote_port: number
  trust_status: string
}

export default function Home() {
  const [connections, setConnections] = useState<Connection[]>([])
  const [isConnected, setIsConnected] = useState(false)
  const [loading, setLoading] = useState(true)

  useEffect(() => {
    // Check if running in WebView2 environment
    const isWebView = (window as any).chrome?.webview !== undefined

    setIsConnected(isWebView)
    setLoading(false)

    // Set up message listener for daemon events
    if (isWebView) {
      ;(window as any).chrome.webview.addEventListener('message', (event: any) => {
        const message = JSON.parse(event.data)

        if (message.type === 'connections_updated') {
          setConnections(message.data)
        }
      })

      // Request initial connections
      ;(window as any).chrome.webview.postMessage(JSON.stringify({
        type: 'get_connections',
        data: {}
      }))
    }
  }, [])

  const getTrustIcon = (status: string) => {
    switch (status) {
      case 'microsoft':
        return '✓✓'
      case 'verified':
        return '✓'
      case 'trusted':
        return '🔒👍'
      case 'unsigned':
        return '○'
      case 'invalid':
        return '✗'
      case 'threat':
        return '🔒⚠'
      default:
        return '?'
    }
  }

  const getTrustColor = (status: string) => {
    switch (status) {
      case 'microsoft':
        return 'text-green-700'
      case 'verified':
        return 'text-green-600'
      case 'trusted':
        return 'text-blue-600'
      case 'unsigned':
        return 'text-yellow-600'
      case 'invalid':
        return 'text-red-600'
      case 'threat':
        return 'text-red-600'
      default:
        return 'text-gray-600'
    }
  }

  return (
    <>
      <Head>
        <title>Peek - Network Security Monitor</title>
        <meta name="description" content="Network connection monitoring" />
        <meta name="viewport" content="width=device-width, initial-scale=1" />
      </Head>

      <div className="min-h-screen bg-gradient-to-br from-slate-50 to-slate-100">
        {/* Header */}
        <header className="bg-white shadow-sm border-b border-slate-200">
          <div className="max-w-7xl mx-auto px-6 py-4">
            <div className="flex items-center justify-between">
              <div>
                <h1 className="text-2xl font-bold text-peek-900">Peek</h1>
                <p className="text-sm text-gray-600">Network Security Monitor</p>
              </div>
              <div className="flex items-center gap-3">
                <div className={`h-3 w-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`} />
                <span className="text-sm font-medium">
                  {isConnected ? 'Connected to Daemon' : 'No Daemon Connection'}
                </span>
              </div>
            </div>
          </div>
        </header>

        {/* Main Content */}
        <main className="max-w-7xl mx-auto px-6 py-8">
          {loading ? (
            <div className="flex items-center justify-center h-64">
              <div className="text-center">
                <div className="inline-block animate-spin mb-3">
                  <div className="h-8 w-8 border-4 border-peek-500 border-r-transparent rounded-full"></div>
                </div>
                <p className="text-gray-600">Initializing...</p>
              </div>
            </div>
          ) : !isConnected ? (
            <div className="bg-yellow-50 border border-yellow-200 rounded-lg p-6 text-center">
              <p className="text-yellow-800 font-medium mb-2">Daemon Not Connected</p>
              <p className="text-yellow-700 text-sm mb-4">
                Make sure peekd.exe is running as a Windows service:
              </p>
              <code className="bg-yellow-100 px-3 py-1 rounded text-sm font-mono">
                peekd.exe --install-service
              </code>
            </div>
          ) : (
            <>
              {/* Stats */}
              <div className="grid grid-cols-1 md:grid-cols-4 gap-4 mb-6">
                <div className="bg-white rounded-lg shadow p-4 border-l-4 border-peek-500">
                  <p className="text-gray-600 text-sm font-medium">Total Connections</p>
                  <p className="text-2xl font-bold text-peek-900 mt-1">{connections.length}</p>
                </div>
                <div className="bg-white rounded-lg shadow p-4 border-l-4 border-green-500">
                  <p className="text-gray-600 text-sm font-medium">Outbound</p>
                  <p className="text-2xl font-bold text-green-600 mt-1">
                    {connections.filter(c => c.direction === 'OUTBOUND').length}
                  </p>
                </div>
                <div className="bg-white rounded-lg shadow p-4 border-l-4 border-orange-500">
                  <p className="text-gray-600 text-sm font-medium">Inbound</p>
                  <p className="text-2xl font-bold text-orange-600 mt-1">
                    {connections.filter(c => c.direction === 'INBOUND').length}
                  </p>
                </div>
                <div className="bg-white rounded-lg shadow p-4 border-l-4 border-blue-500">
                  <p className="text-gray-600 text-sm font-medium">TCP</p>
                  <p className="text-2xl font-bold text-blue-600 mt-1">
                    {connections.filter(c => c.protocol === 'TCP').length}
                  </p>
                </div>
              </div>

              {/* Connections Table */}
              <div className="bg-white rounded-lg shadow overflow-hidden">
                <div className="px-6 py-4 border-b border-gray-200">
                  <h2 className="text-lg font-semibold text-gray-900">Active Connections</h2>
                </div>

                {connections.length === 0 ? (
                  <div className="p-6 text-center text-gray-500">
                    No connections being monitored
                  </div>
                ) : (
                  <div className="overflow-x-auto">
                    <table className="w-full">
                      <thead className="bg-gray-50 border-b border-gray-200">
                        <tr>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-700 uppercase tracking-wider">
                            Process
                          </th>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-700 uppercase tracking-wider">
                            Direction
                          </th>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-700 uppercase tracking-wider">
                            Remote Address
                          </th>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-700 uppercase tracking-wider">
                            Protocol
                          </th>
                          <th className="px-6 py-3 text-left text-xs font-medium text-gray-700 uppercase tracking-wider">
                            Trust Status
                          </th>
                        </tr>
                      </thead>
                      <tbody className="divide-y divide-gray-200">
                        {connections.map((conn, idx) => (
                          <tr key={conn.connection_id} className={idx % 2 === 0 ? 'bg-white' : 'bg-gray-50'}>
                            <td className="px-6 py-4 text-sm font-medium text-gray-900">
                              {conn.process_name}
                            </td>
                            <td className="px-6 py-4 text-sm">
                              <span className={`px-2 py-1 rounded text-xs font-medium ${
                                conn.direction === 'OUTBOUND'
                                  ? 'bg-green-100 text-green-800'
                                  : 'bg-orange-100 text-orange-800'
                              }`}>
                                {conn.direction}
                              </span>
                            </td>
                            <td className="px-6 py-4 text-sm text-gray-600 font-mono">
                              {conn.remote_address}:{conn.remote_port}
                            </td>
                            <td className="px-6 py-4 text-sm">
                              <span className="text-gray-700 font-medium">{conn.protocol}</span>
                            </td>
                            <td className={`px-6 py-4 text-sm font-semibold ${getTrustColor(conn.trust_status)}`}>
                              {getTrustIcon(conn.trust_status)}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                )}
              </div>
            </>
          )}
        </main>
      </div>
    </>
  )
}
