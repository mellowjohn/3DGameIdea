import { useEffect, useState } from 'react'
import { Link } from 'react-router-dom'
import { PostCard } from '../components/PostCard'
import { SubscribeForm } from '../components/SubscribeForm'
import { assetUrl } from '../lib/assets'
import { loadPosts, type Post } from '../lib/posts'
import { setPageMeta } from '../lib/seo'

export function HomePage() {
  const [posts, setPosts] = useState<Post[]>([])

  useEffect(() => {
    setPageMeta({ path: '/', image: '/images/og-default.jpg' })
    void loadPosts().then(setPosts)
  }, [])

  const latest = posts.slice(0, 3)

  return (
    <main>
      <section className="hero">
        <div
          className="hero__image"
          style={{ backgroundImage: `url(${assetUrl('/images/hero-bg.webp')})` }}
          aria-hidden="true"
        />
        <div className="hero__veil" aria-hidden="true" />
        <div className="hero__content">
          <p className="hero__brand">Wrathful Conquest</p>
          <h1 className="hero__title">Building an open-world RPG engine in the open</h1>
          <p className="hero__lede">
            Dev notes from a from-scratch C++20 / Direct3D 12 stack, built so people and AI
            collaborators can ship real engine work together.
          </p>
          <div className="hero__actions">
            <a className="btn btn--primary" href="#subscribe">
              Subscribe
            </a>
            <Link className="btn btn--ghost" to="/posts">
              Read the archive
            </Link>
          </div>
        </div>
      </section>

      <section className="section" id="latest">
        <div className="section__head">
          <h2>Latest</h2>
          <Link to="/posts">All posts</Link>
        </div>
        <div className="post-list">
          {latest.map((post, i) => (
            <PostCard key={post.slug} post={post} index={i} />
          ))}
        </div>
      </section>

      <section className="section section--subscribe" id="subscribe">
        <SubscribeForm />
      </section>
    </main>
  )
}
