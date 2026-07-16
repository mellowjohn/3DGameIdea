import { BrowserRouter, Route, Routes } from 'react-router-dom'
import { Nav } from './components/Nav'
import { AboutPage } from './pages/AboutPage'
import { HomePage } from './pages/HomePage'
import { PostPage } from './pages/PostPage'
import { PostsPage } from './pages/PostsPage'

export default function App() {
  return (
    <BrowserRouter basename={import.meta.env.BASE_URL.replace(/\/$/, '') || '/'}>
      <div className="app-shell">
        <Nav />
        <Routes>
          <Route path="/" element={<HomePage />} />
          <Route path="/posts" element={<PostsPage />} />
          <Route path="/posts/:slug" element={<PostPage />} />
          <Route path="/about" element={<AboutPage />} />
        </Routes>
        <footer className="site-footer">
          <p>Wrathful Conquest · Engine and game build notes</p>
        </footer>
      </div>
    </BrowserRouter>
  )
}
