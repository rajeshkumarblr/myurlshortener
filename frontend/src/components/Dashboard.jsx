import React, { useState, useEffect } from 'react';
import { api } from '../api';
import { BarChart, Activity } from 'lucide-react';

export function Dashboard() {
  const [data, setData] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  useEffect(() => {
    loadData();
  }, []);

  const loadData = async () => {
    try {
      const res = await api.request('/api/v1/analytics/summary');
      setData(res);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  if (loading) return <div className="loading">Loading analytics...</div>;
  if (error) return <div className="error">Error: {error}</div>;
  if (!data) return null;

  return (
    <div className="dashboard">
      <div className="stats-grid">
        <div className="stat-card">
          <div className="stat-icon">
            <Activity size={24} />
          </div>
          <div className="stat-content">
            <h3>Total Clicks</h3>
            <p className="stat-value">{data.totalClicks}</p>
          </div>
        </div>
      </div>

      <div className="panel" style={{ marginTop: '2rem' }}>
        <header>
          <BarChart size={20} />
          <h3>Top Performing Links</h3>
        </header>
        <table className="data-table">
          <thead>
            <tr>
              <th>Short Code</th>
              <th>Clicks</th>
            </tr>
          </thead>
          <tbody>
            {data.topUrls.map((url) => (
              <tr key={url.code}>
                <td>
                  <a href={`/${url.code}`} target="_blank" rel="noopener noreferrer" className="code-link">
                    {url.code}
                  </a>
                </td>
                <td>{url.clicks}</td>
              </tr>
            ))}
            {data.topUrls.length === 0 && (
              <tr>
                <td colSpan="2" style={{ textAlign: 'center', color: '#666' }}>
                  No clicks recorded yet
                </td>
              </tr>
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
}
