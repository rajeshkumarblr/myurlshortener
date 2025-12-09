import React, { useEffect, useState } from 'react';
import { api } from '../api';
import { Trash2, RefreshCw } from 'lucide-react';

export function LinkList() {
  const [links, setLinks] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  const fetchLinks = async () => {
    setLoading(true);
    setError('');
    try {
      const data = await api.request('/api/v1/urls');
      setLinks(Array.isArray(data) ? data : []);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  const handleDelete = async (code) => {
    if (!window.confirm('Are you sure you want to delete this link?')) return;
    try {
      await api.request(`/api/v1/urls/${code}`, { method: 'DELETE' });
      fetchLinks();
    } catch (err) {
      alert(err.message);
    }
  };

  useEffect(() => {
    fetchLinks();
  }, []);

  return (
    <article className="panel">
      <header>Your URLs</header>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '1rem', marginBottom: '1rem' }}>
        <p style={{ margin: 0, color: 'var(--muted)' }}>Monitor recently created links.</p>
        <button className="secondary outline" onClick={fetchLinks} disabled={loading}>
          <RefreshCw size={16} style={{ marginRight: '0.5rem' }} /> Refresh
        </button>
      </div>
      
      {error && <div className="status error">{error}</div>}
      
      <table>
        <thead>
          <tr>
            <th>Code</th>
            <th>Destination</th>
            <th>Clicks</th>
            <th>Created</th>
            <th>Expires</th>
            <th style={{ width: '50px' }}></th>
          </tr>
        </thead>
        <tbody>
          {loading ? (
            <tr><td colSpan="6" style={{ textAlign: 'center' }}>Loading...</td></tr>
          ) : links.length === 0 ? (
            <tr><td colSpan="6" style={{ textAlign: 'center' }}>No URLs yet</td></tr>
          ) : (
            links.map((item) => (
              <tr key={item.code}>
                <td>
                  <a href={item.short} target="_blank" rel="noopener noreferrer" className="code-link">{item.code}</a>
                </td>
                <td style={{ maxWidth: '300px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }} title={item.url}>
                  {item.url}
                </td>
                <td>{item.clicks || 0}</td>
                <td>{new Date(item.created_at * 1000).toLocaleString()}</td>
                <td>{item.expires_at ? new Date(item.expires_at * 1000).toLocaleString() : '—'}</td>
                <td>
                  <button 
                    className="icon-btn danger" 
                    onClick={() => handleDelete(item.code)}
                    title="Delete Link"
                    style={{ padding: '0.25rem', background: 'transparent', border: 'none', color: 'var(--del-color, #e53e3e)', cursor: 'pointer' }}
                  >
                    <Trash2 size={18} />
                  </button>
                </td>
              </tr>
            ))
          )}
        </tbody>
      </table>
    </article>
  );
}
