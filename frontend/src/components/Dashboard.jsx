import React, { useState, useEffect } from 'react';
import { api } from '../api';
import { BarChart, Activity, Users, Trash2 } from 'lucide-react';

export function Dashboard() {
  const [data, setData] = useState(null);
  const [users, setUsers] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

  useEffect(() => {
    loadData();
  }, []);

  const loadData = async () => {
    try {
      const [summaryRes, usersRes] = await Promise.all([
        api.request('/api/v1/analytics/summary'),
        api.request('/api/v1/admin/users')
      ]);
      setData(summaryRes);
      setUsers(usersRes);
    } catch (err) {
      setError(err.message);
    } finally {
      setLoading(false);
    }
  };

  const handleDeleteUser = async (id) => {
    if (!window.confirm('Are you sure? This will delete the user and ALL their links.')) return;
    try {
      await api.request(`/api/v1/admin/users/${id}`, { method: 'DELETE' });
      setUsers(users.filter(u => u.id !== id));
    } catch (err) {
      alert(err.message);
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
        <div className="stat-card">
          <div className="stat-icon">
            <Users size={24} />
          </div>
          <div className="stat-content">
            <h3>Total Users</h3>
            <p className="stat-value">{users.length}</p>
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

      <div className="panel" style={{ marginTop: '2rem' }}>
        <header>
          <Users size={20} />
          <h3>User Management</h3>
        </header>
        <table className="data-table">
          <thead>
            <tr>
              <th>ID</th>
              <th>Name</th>
              <th>Email</th>
              <th>Role</th>
              <th>Joined</th>
              <th style={{ width: '50px' }}></th>
            </tr>
          </thead>
          <tbody>
            {users.map((user) => (
              <tr key={user.id}>
                <td>{user.id}</td>
                <td>{user.name}</td>
                <td>{user.email}</td>
                <td>
                  <span className={`badge ${user.role === 'ADMIN' ? 'badge-admin' : ''}`}>
                    {user.role}
                  </span>
                </td>
                <td>{new Date(user.createdAt * 1000).toLocaleDateString()}</td>
                <td>
                  {user.role !== 'ADMIN' && (
                    <button 
                      className="icon-btn danger" 
                      onClick={() => handleDeleteUser(user.id)}
                      title="Delete User"
                      style={{ padding: '0.25rem', background: 'transparent', border: 'none', color: 'var(--del-color, #e53e3e)', cursor: 'pointer' }}
                    >
                      <Trash2 size={18} />
                    </button>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
