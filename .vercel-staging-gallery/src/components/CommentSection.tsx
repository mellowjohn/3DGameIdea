import { useCallback, useEffect, useState, type FormEvent } from 'react'
import {
  getSavedAuthor,
  getVoterId,
  saveAuthor,
  type CommentPublic,
  type VoteValue,
} from '../lib/commentsClient'

type Props = {
  assetId: string
}

export function CommentSection({ assetId }: Props) {
  const [comments, setComments] = useState<CommentPublic[]>([])
  const [author, setAuthor] = useState(getSavedAuthor)
  const [text, setText] = useState('')
  const [loading, setLoading] = useState(true)
  const [posting, setPosting] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [backend, setBackend] = useState<string | null>(null)

  const load = useCallback(async () => {
    setLoading(true)
    setError(null)
    try {
      const voterId = getVoterId()
      const res = await fetch(
        `/api/comments?assetId=${encodeURIComponent(assetId)}&voterId=${encodeURIComponent(voterId)}`,
      )
      if (!res.ok) {
        const data = (await res.json().catch(() => ({}))) as { error?: string }
        throw new Error(data.error || `Failed to load comments (${res.status})`)
      }
      const data = (await res.json()) as { comments: CommentPublic[]; backend?: string }
      setComments(data.comments || [])
      setBackend(data.backend ?? null)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load comments')
    } finally {
      setLoading(false)
    }
  }, [assetId])

  useEffect(() => {
    void load()
  }, [load])

  async function onSubmit(e: FormEvent) {
    e.preventDefault()
    setPosting(true)
    setError(null)
    try {
      const voterId = getVoterId()
      const res = await fetch('/api/comments', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ assetId, author, text, voterId }),
      })
      const data = (await res.json().catch(() => ({}))) as {
        error?: string
        comment?: CommentPublic
        backend?: string
      }
      if (!res.ok) throw new Error(data.error || 'Could not post comment')
      if (data.comment) {
        setComments((prev) => [data.comment!, ...prev])
      }
      if (data.backend) setBackend(data.backend)
      saveAuthor(author.trim())
      setText('')
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Could not post')
    } finally {
      setPosting(false)
    }
  }

  async function onVote(comment: CommentPublic, next: VoteValue) {
    const voterId = getVoterId()
    const vote = comment.myVote === next ? null : next
    try {
      const res = await fetch('/api/comments/vote', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          assetId,
          commentId: comment.id,
          voterId,
          vote: vote === null ? 'clear' : vote,
        }),
      })
      const data = (await res.json().catch(() => ({}))) as {
        error?: string
        comment?: CommentPublic
      }
      if (!res.ok) throw new Error(data.error || 'Vote failed')
      if (data.comment) {
        setComments((prev) => prev.map((c) => (c.id === data.comment!.id ? data.comment! : c)))
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Vote failed')
    }
  }

  return (
    <section className="comments" aria-label="Comments">
      <div className="comments-head">
        <h3>Comments</h3>
        <span className="comments-count">{comments.length}</span>
        {backend && <span className="chip">{backend === 'file' ? 'local store' : 'shared'}</span>}
      </div>

      <form className="comment-form" onSubmit={onSubmit}>
        <div className="comment-form-row">
          <label htmlFor={`comment-author-${assetId}`}>Name</label>
          <input
            id={`comment-author-${assetId}`}
            type="text"
            maxLength={48}
            value={author}
            onChange={(e) => setAuthor(e.target.value)}
            placeholder="Your name"
            required
          />
        </div>
        <div className="comment-form-row">
          <label htmlFor={`comment-text-${assetId}`}>Feedback</label>
          <textarea
            id={`comment-text-${assetId}`}
            maxLength={2000}
            rows={3}
            value={text}
            onChange={(e) => setText(e.target.value)}
            placeholder="What works, what to fix, references…"
            required
          />
        </div>
        <button type="submit" className="btn btn-primary" disabled={posting}>
          {posting ? 'Posting…' : 'Post comment'}
        </button>
      </form>

      {error && <p className="login-error">{error}</p>}

      {loading ? (
        <p className="comments-empty">Loading comments…</p>
      ) : comments.length === 0 ? (
        <p className="comments-empty">No comments yet. Be the first to leave feedback.</p>
      ) : (
        <ul className="comment-list">
          {comments.map((c) => (
            <li key={c.id} className="comment-item">
              <div className="comment-meta">
                <strong>{c.author}</strong>
                <time dateTime={c.createdAt}>{formatWhen(c.createdAt)}</time>
              </div>
              <p className="comment-text">{c.text}</p>
              <div className="comment-votes">
                <button
                  type="button"
                  className={`vote-btn${c.myVote === 'up' ? ' is-active' : ''}`}
                  onClick={() => void onVote(c, 'up')}
                  aria-pressed={c.myVote === 'up'}
                  title="Like"
                >
                  <span aria-hidden>▲</span> {c.likes}
                </button>
                <button
                  type="button"
                  className={`vote-btn vote-down${c.myVote === 'down' ? ' is-active' : ''}`}
                  onClick={() => void onVote(c, 'down')}
                  aria-pressed={c.myVote === 'down'}
                  title="Dislike"
                >
                  <span aria-hidden>▼</span> {c.dislikes}
                </button>
              </div>
            </li>
          ))}
        </ul>
      )}
    </section>
  )
}

function formatWhen(iso: string): string {
  try {
    return new Date(iso).toLocaleString(undefined, {
      dateStyle: 'medium',
      timeStyle: 'short',
    })
  } catch {
    return iso
  }
}
