import React, { useState } from 'react';
import { api } from '../api';

export function Shortener() {
  const [url, setUrl] = useState('');
  const [ttl, setTtl] = useState('');
  const [status, setStatus] = useState({ msg: '', type: '' });
  const [lastShort, setLastShort] = useState(null);

  const handleSubmit = async (e) => {
    e.preventDefault();
    setStatus({ msg: 'Shortening...', type: '' });
    setLastShort(null);

    try {
      const payload = { url };
      if (ttl) payload.ttl = Number(ttl);

      const data = await api.request('/api/v1/shorten', {
        method: 'POST',
        body: JSON.stringify(payload)
      });

      setStatus({ msg: 'Short link created!', type: 'success' });
      setLastShort(data.short_url);
      setUrl('');
    } catch (err) {
      setStatus({ msg: err.message, type: 'error' });
    }
  };

  return (
    <article className="panel">
      <header>Generate short links</header>
      <p style={{ color: 'var(--muted)', marginTop: 0 }}>
        Authenticated users can convert long URLs into branded redirects.
      </p>
      <form onSubmit={handleSubmit}>
        <input 
          type="url" 
          placeholder="https://example.com/article" 
          required 
          value={url}
          onChange={(e) => setUrl(e.target.value)}
        />
        <div style={{ display: 'flex', gap: '0.75rem', flexWrap: 'wrap' }}>
          <input 
            type="number" 
            min="0" 
            placeholder="TTL seconds (optional)" 
            style={{ flex: '1 1 160px' }}
            value={ttl}
            onChange={(e) => setTtl(e.target.value)}
          />
          <button type="submit" className="primary">Shorten link</button>
        </div>
        {status.msg && <div className={`status ${status.type}`}>{status.msg}</div>}
      </form>
      
      {lastShort && (
        <div className="panel" style={{ marginTop: '1rem', background: '#f8fafc', borderStyle: 'dashed' }}>
          <header>Latest short URL</header>
          <a href={lastShort} target="_blank" rel="noopener noreferrer">{lastShort}</a>
        </div>
      )}
    </article>
  );
}
