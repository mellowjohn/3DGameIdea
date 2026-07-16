import { useEffect, useState } from 'react'
import { PostCard } from '../components/PostCard'
import { SubscribeForm } from '../components/SubscribeForm'
import { loadPosts, type Post } from '../lib/posts'
import { setPageMeta } from '../lib/seo'

export function PostsPage() {
  const [posts, setPosts] = useState<Post[]>([])

  useEffect(() => {
    setPageMeta({
      title: 'Posts',
      description: 'Archive of Wrathful Conquest engine and tooling build notes.',
      path: '/posts',
    })
    void loadPosts().then(setPosts)
  }, [])

  return (
    <main className="page">
      <header className="page-header">
        <h1>Posts</h1>
        <p>Newest first. Short build notes and occasional deep dives.</p>
      </header>
      <div className="post-list post-list--archive">
        {posts.map((post, i) => (
          <PostCard key={post.slug} post={post} index={i} />
        ))}
      </div>
      <section className="section--subscribe">
        <SubscribeForm compact />
      </section>
    </main>
  )
}
