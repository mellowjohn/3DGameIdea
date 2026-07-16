import { Link } from 'react-router-dom'
import type { Post } from '../lib/posts'
import { assetUrl } from '../lib/assets'

type Props = {
  post: Post
  index?: number
}

export function PostCard({ post, index = 0 }: Props) {
  const date = new Date(post.date + 'T00:00:00')
  const formatted = date.toLocaleDateString('en-US', {
    year: 'numeric',
    month: 'short',
    day: 'numeric',
  })

  return (
    <article
      className="post-card"
      style={{ animationDelay: `${Math.min(index, 8) * 60}ms` }}
    >
      {post.cover && (
        <Link to={`/posts/${post.slug}`} className="post-card__media" tabIndex={-1} aria-hidden="true">
          <img src={assetUrl(post.cover)} alt="" loading="lazy" />
        </Link>
      )}
      <div className="post-card__body">
        <time dateTime={post.date}>{formatted}</time>
        <h3>
          <Link to={`/posts/${post.slug}`}>{post.title}</Link>
        </h3>
        <p>{post.summary}</p>
        {post.tags.length > 0 && (
          <ul className="tag-list">
            {post.tags.map((tag) => (
              <li key={tag}>{tag}</li>
            ))}
          </ul>
        )}
      </div>
    </article>
  )
}
