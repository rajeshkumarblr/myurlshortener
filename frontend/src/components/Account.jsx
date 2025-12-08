import React, { useState } from 'react';
import { api } from '../api';

export function Account({ user }) {
  const [status, setStatus] = useState('');

  const copyToken = async () => {
    const token = api.getToken();
    try {
      await navigator.clipboard.writeText(token);
      setStatus('Token copied!');
      setTimeout(() => setStatus(''), 2500);
    } catch {
      setStatus('Failed to copy token');
    }
  };

  return (
    <div className="card-grid">
      <article className="panel">
        <header>Profile</header>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(160px, 1fr))', gap: '1rem' }}>
          <div>
            <p className="eyebrow">Name</p>
            <strong>{user?.name || '—'}</strong>
          </div>
          <div>
            <p className="eyebrow">Email</p>
            <strong>{user?.email || '—'}</strong>
          </div>
          <div>
            <p className="eyebrow">User ID</p>
            <strong>{user?.id || '—'}</strong>
          </div>
        </div>
      </article>
      
      <article className="panel">
        <header>API Token</header>
        <code className="token">
          {api.getToken() || 'Sign in to view token'}
        </code>
        <div style={{ display: 'flex', gap: '0.75rem', flexWrap: 'wrap', marginTop: '0.75rem' }}>
          <button className="secondary" onClick={copyToken}>Copy token</button>
          <button className="secondary" onClick={() => api.logout()}>Sign out</button>
        </div>
        {status && <div className="status success">{status}</div>}
      </article>
    </div>
  );
}
