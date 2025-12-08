import React, { useState } from 'react';
import { api } from '../api';

export function Auth({ onLogin }) {
  const [isRegister, setIsRegister] = useState(false);
  const [status, setStatus] = useState({ msg: '', type: '' });
  const [formData, setFormData] = useState({ name: '', email: '', password: '' });

  const handleChange = (e) => {
    setFormData({ ...formData, [e.target.id]: e.target.value });
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setStatus({ msg: isRegister ? 'Creating account...' : 'Signing in...', type: '' });
    
    try {
      const endpoint = isRegister ? '/api/v1/register' : '/api/v1/login';
      const payload = isRegister 
        ? { name: formData.name, email: formData.email, password: formData.password }
        : { email: formData.email, password: formData.password };

      const data = await api.request(endpoint, {
        method: 'POST',
        body: JSON.stringify(payload)
      });

      const profile = { id: data.user_id, name: data.name, email: data.email, role: data.role };
      api.setSession(data.token, profile);
      onLogin(profile);
    } catch (err) {
      setStatus({ msg: err.message, type: 'error' });
    }
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', maxWidth: '520px', margin: '0 auto' }}>
      <article className="panel" style={{ width: '100%' }}>
        <header>{isRegister ? 'Create an account' : 'Sign in'}</header>
        <form onSubmit={handleSubmit}>
          {isRegister && (
            <input 
              id="name" 
              placeholder="Full name" 
              required 
              value={formData.name}
              onChange={handleChange}
            />
          )}
          <input 
            id="email" 
            type="email" 
            placeholder="Email" 
            required 
            value={formData.email}
            onChange={handleChange}
          />
          <input 
            id="password" 
            type="password" 
            placeholder="Password" 
            required 
            minLength={8}
            value={formData.password}
            onChange={handleChange}
          />
          <button type="submit" className="primary">
            {isRegister ? 'Create account' : 'Sign in'}
          </button>
          {status.msg && (
            <div className={`status ${status.type}`}>{status.msg}</div>
          )}
        </form>
      </article>
      <p className="auth-toggle">
        {isRegister ? 'Already have an account? ' : 'New user? '}
        <button onClick={() => {
          setIsRegister(!isRegister);
          setStatus({ msg: '', type: '' });
        }}>
          {isRegister ? 'Sign in' : 'Register here'}
        </button>
      </p>
    </div>
  );
}
