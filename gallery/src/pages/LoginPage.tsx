import { useState, type FormEvent } from 'react'
import { useSearchParams } from 'react-router-dom'
import { BrandMark, BrandTitleLogo } from '../components/BrandMark'

export function LoginPage() {
  const [password, setPassword] = useState('')
  const [error, setError] = useState<string | null>(null)
  const [busy, setBusy] = useState(false)
  const [params] = useSearchParams()
  const next = params.get('next') || '/'

  async function onSubmit(e: FormEvent) {
    e.preventDefault()
    setBusy(true)
    setError(null)
    try {
      const res = await fetch('/api/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ password }),
      })
      if (!res.ok) {
        const data = (await res.json().catch(() => ({}))) as { error?: string }
        setError(data.error || 'Login failed')
        setBusy(false)
        return
      }
      window.location.assign(next.startsWith('/') ? next : '/')
    } catch {
      setError('Could not reach login API. Is the site deployed with Vercel functions?')
      setBusy(false)
    }
  }

  return (
    <div className="login-shell">
      <div className="login-brand-bg" aria-hidden />
      <form className="login-card" onSubmit={onSubmit}>
        <div className="login-brand">
          <BrandMark size="lg" to={null} />
          <BrandTitleLogo className="login-title-logo" />
          <p className="login-tool-badge">Art Atlas</p>
        </div>
        <p className="login-lead">
          Shared password for collaborators. Concepts, kits, UI, type, map art, and feedback.
        </p>
        <label htmlFor="password">Password</label>
        <input
          id="password"
          type="password"
          autoComplete="current-password"
          value={password}
          onChange={(e) => setPassword(e.target.value)}
          required
          autoFocus
        />
        {error && <p className="login-error">{error}</p>}
        <button type="submit" className="btn btn-primary" disabled={busy} style={{ width: '100%' }}>
          {busy ? 'Checking…' : 'Enter'}
        </button>
      </form>
    </div>
  )
}
