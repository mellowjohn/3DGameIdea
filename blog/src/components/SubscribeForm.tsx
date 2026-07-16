import { useId, useState, type FormEvent } from 'react'
import { getFormspreeId, subscribeEmail } from '../lib/subscribe'

type Props = {
  compact?: boolean
}

export function SubscribeForm({ compact = false }: Props) {
  const id = useId()
  const configured = Boolean(getFormspreeId())
  const [email, setEmail] = useState('')
  const [status, setStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle')
  const [message, setMessage] = useState('')

  async function onSubmit(e: FormEvent) {
    e.preventDefault()
    if (!configured) {
      return
    }
    setStatus('loading')
    setMessage('')
    const result = await subscribeEmail(email.trim())
    if (result.ok) {
      setStatus('success')
      setMessage('You are in. I will email you when new posts land.')
      setEmail('')
    } else {
      setStatus('error')
      setMessage(result.message)
    }
  }

  return (
    <div
      className={`subscribe ${compact ? 'subscribe--compact' : ''} ${status === 'success' ? 'subscribe--success' : ''}`}
    >
      {!compact && (
        <div className="subscribe__copy">
          <h2 className="subscribe__title">Follow the build</h2>
          <p className="subscribe__lede">
            Leave your email and I will write when a new post goes up. Occasional updates only.
          </p>
        </div>
      )}

      {!configured ? (
        <p className="subscribe__hint" role="status">
          Email updates will be available soon. Check back shortly, or browse the archive in the
          meantime.
        </p>
      ) : (
        <form className="subscribe__form" onSubmit={onSubmit} noValidate>
          <label className="visually-hidden" htmlFor={id}>
            Email
          </label>
          <input
            id={id}
            type="email"
            name="email"
            required
            autoComplete="email"
            placeholder="you@example.com"
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            disabled={status === 'loading' || status === 'success'}
          />
          <button type="submit" disabled={status === 'loading' || status === 'success'}>
            {status === 'loading' ? 'Joining…' : status === 'success' ? 'Subscribed' : 'Subscribe'}
          </button>
        </form>
      )}

      {message && (
        <p className={`subscribe__message subscribe__message--${status}`} role="status">
          {message}
        </p>
      )}
    </div>
  )
}
