const lsTokenKey = 'urlshort_token';
const lsProfileKey = 'urlshort_profile';

export const api = {
  getToken() {
    return localStorage.getItem(lsTokenKey);
  },

  getProfile() {
    try {
      return JSON.parse(localStorage.getItem(lsProfileKey) || 'null');
    } catch {
      return null;
    }
  },

  setSession(token, profile) {
    if (token) {
      localStorage.setItem(lsTokenKey, token);
    } else {
      localStorage.removeItem(lsTokenKey);
    }
    if (profile) {
      localStorage.setItem(lsProfileKey, JSON.stringify(profile));
    } else {
      localStorage.removeItem(lsProfileKey);
    }
  },

  logout() {
    this.setSession(null, null);
    window.location.reload();
  },

  async request(path, options = {}) {
    const headers = options.headers ? { ...options.headers } : {};
    headers['Content-Type'] = 'application/json';
    
    const token = this.getToken();
    if (token) {
      headers['Authorization'] = `Bearer ${token}`;
    }

    const res = await fetch(path, { ...options, headers });
    const text = await res.text();
    let data;
    try { 
      data = text ? JSON.parse(text) : {}; 
    } catch { 
      data = { error: text || 'Invalid JSON' }; 
    }

    if (!res.ok) {
      if (res.status === 401) {
        // Optional: Auto logout on 401
        // this.logout();
      }
      const message = data && data.error ? data.error : 'Request failed';
      throw new Error(message);
    }
    return data;
  }
};
