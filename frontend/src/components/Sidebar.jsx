import React from 'react';
import { LayoutDashboard, Link, User, LogIn, BarChart2, Moon, Sun } from 'lucide-react';

export function Sidebar({ activeView, onViewChange, user, theme, toggleTheme }) {
  const navItems = [
    { id: 'auth', label: 'Sign in', icon: LogIn, hidden: !!user },
    { id: 'shorten', label: 'Shorten URLs', icon: LayoutDashboard, protected: true },
    { id: 'links', label: 'My Links', icon: Link, protected: true },
    { id: 'dashboard', label: 'Admin Dashboard', icon: BarChart2, protected: true, adminOnly: true },
    { id: 'account', label: 'Account', icon: User, protected: true },
  ];

  return (
    <aside className="sidebar">
      <div className="brand">
        myURL
        <small>Shortener Console</small>
      </div>
      <nav className="nav">
        {navItems.map((item) => {
          if (item.hidden) return null;
          if (item.adminOnly && user?.role !== 'ADMIN') return null;
          
          const isDisabled = item.protected && !user;
          return (
            <button
              key={item.id}
              className={`nav-item ${activeView === item.id ? 'active' : ''}`}
              onClick={() => !isDisabled && onViewChange(item.id)}
              disabled={isDisabled}
            >
              <item.icon size={18} />
              {item.label}
            </button>
          );
        })}
      </nav>
      <div className="sidebar-footer">
        <button className="nav-item" onClick={toggleTheme} style={{ marginBottom: '1rem', width: '100%', justifyContent: 'flex-start' }}>
          {theme === 'dark' ? <Sun size={18} /> : <Moon size={18} />}
          {theme === 'dark' ? 'Light Mode' : 'Dark Mode'}
        </button>
        <strong>{user ? `Signed in as ${user.name}` : 'Guest mode'}</strong>
        <p>{user ? 'Workspace active' : 'Authenticate to unlock tools.'}</p>
      </div>
    </aside>
  );
}
