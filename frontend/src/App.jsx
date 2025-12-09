import React, { useState, useEffect } from 'react';
import { Sidebar } from './components/Sidebar';
import { Auth } from './components/Auth';
import { Shortener } from './components/Shortener';
import { LinkList } from './components/LinkList';
import { Account } from './components/Account';
import { Dashboard } from './components/Dashboard';
import { api } from './api';

const VIEW_META = {
  auth: { title: 'Sign in', subtitle: 'Authenticate to unlock project tools.' },
  shorten: { title: 'Shorten URLs', subtitle: 'Authenticated workspace' },
  links: { title: 'Your links', subtitle: 'Analytics-ready list of active codes' },
  dashboard: { title: 'Admin Dashboard', subtitle: 'Platform analytics and metrics' },
  account: { title: 'Account settings', subtitle: 'Manage access tokens and profile' }
};

function App() {
  const [user, setUser] = useState(api.getProfile());
  const [view, setView] = useState(user ? 'shorten' : 'auth');
  const [theme, setTheme] = useState(localStorage.getItem('theme') || 'light');

  useEffect(() => {
    if (!user && view !== 'auth') {
      setView('auth');
    }
  }, [user, view]);

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('theme', theme);
  }, [theme]);

  const toggleTheme = () => {
    setTheme(prev => prev === 'light' ? 'dark' : 'light');
  };

  const handleLogin = (profile) => {
    setUser(profile);
    setView('shorten');
  };

  const handleLogout = () => {
    api.logout();
  };

  const renderView = () => {
    switch (view) {
      case 'auth': return <Auth onLogin={handleLogin} />;
      case 'shorten': return <Shortener />;
      case 'links': return <LinkList />;
      case 'dashboard': return <Dashboard />;
      case 'account': return <Account user={user} />;
      default: return <Auth onLogin={handleLogin} />;
    }
  };

  return (
    <div className="app-shell">
      <Sidebar 
        activeView={view} 
        onViewChange={setView} 
        user={user} 
        theme={theme}
        toggleTheme={toggleTheme}
      />
      
      <main className="workspace">
        <header className="topbar">
          <div>
            <p className="eyebrow">{VIEW_META[view]?.subtitle}</p>
            <h1>{VIEW_META[view]?.title}</h1>
          </div>
          {user && (
            <button className="ghost" onClick={handleLogout}>Log out</button>
          )}
        </header>

        <div className="view-container">
          {renderView()}
        </div>
      </main>
    </div>
  );
}

export default App;
