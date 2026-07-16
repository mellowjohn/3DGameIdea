import { useEffect, useState } from 'react'
import { Link, useParams } from 'react-router-dom'
import { ShareLinks } from '../components/ShareLinks'
import { SubscribeForm } from '../components/SubscribeForm'
import { assetUrl } from '../lib/assets'
import { getPost, type Post } from '../lib/posts'
import { setPageMeta } from '../lib/seo'

export function PostPage() {
  const { slug = '' } = useParams()
  const [post, setPost] = useState<Post | null | undefined>(undefined)

  useEffect(() => {
    let cancelled = false
    void getPost(slug).then((found) => {
      if (cancelled) {
        return
      }
      setPost(found ?? null)
      if (found) {
        setPageMeta({
          title: found.title,
          description: found.summary,
          path: `/posts/${found.slug}`,
          type: 'article',
          image: found.cover,
        })
      } else {
        setPageMeta({
          title: 'Post not found',
          description: 'That article is not in the archive.',
          path: `/posts/${slug}`,
        })
      }
    })
    return () => {
      cancelled = true
    }
  }, [slug])

  if (post === undefined) {
    return (
      <main className="page">
        <p className="muted">Loading…</p>
      </main>
    )
  }

  if (post === null) {
    return (
      <main className="page">
        <header className="page-header">
          <h1>Post not found</h1>
          <p>
            <Link to="/posts">Back to the archive</Link>
          </p>
        </header>
      </main>
    )
  }

  const date = new Date(post.date + 'T00:00:00')
  const formatted = date.toLocaleDateString('en-US', {
    year: 'numeric',
    month: 'long',
    day: 'numeric',
  })

  return (
    <main className="page page--read">
      <article>
        {post.cover && (
          <div className="article-cover">
            <img src={assetUrl(post.cover)} alt="" />
          </div>
        )}
        <header className="article-header">
          <p className="article-header__meta">
            <time dateTime={post.date}>{formatted}</time>
            {post.tags.length > 0 && (
              <>
                <span aria-hidden="true"> · </span>
                <span>{post.tags.join(', ')}</span>
              </>
            )}
          </p>
          <h1>{post.title}</h1>
          <p className="article-header__summary">{post.summary}</p>
          <ShareLinks path={`/posts/${post.slug}`} title={post.title} />
        </header>
        <div className="prose" dangerouslySetInnerHTML={{ __html: post.html }} />
      </article>
      <section className="section--subscribe">
        <SubscribeForm />
      </section>
    </main>
  )
}
