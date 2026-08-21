import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom'
import { GalleryPage } from './pages/GalleryPage'
import { HomePage } from './pages/HomePage'
import { LoginPage } from './pages/LoginPage'
import { StoryboardsPage } from './pages/StoryboardsPage'

export default function App() {
  return (
    <BrowserRouter>
      <Routes>
        <Route path="/login" element={<LoginPage />} />
        <Route path="/" element={<HomePage />} />
        <Route path="/browse" element={<GalleryPage />} />
        <Route path="/storyboards" element={<StoryboardsPage />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
    </BrowserRouter>
  )
}
