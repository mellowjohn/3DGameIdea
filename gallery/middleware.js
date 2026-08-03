import { next } from '@vercel/edge'
import { isPublicPath, isValidSession } from './lib/auth.js'

/**
 * Vercel Edge Middleware: shared password cookie gate.
 */
export const config = {
  matcher: ['/((?!_next/static|_next/image).*)'],
}

export default async function middleware(request) {
  const url = new URL(request.url)
  const { pathname } = url

  if (isPublicPath(pathname)) {
    return next()
  }

  const password = process.env.SITE_PASSWORD
  if (!password) {
    if (pathname.startsWith('/media/') || pathname === '/catalog.json') {
      return new Response('SITE_PASSWORD not configured', { status: 503 })
    }
    return Response.redirect(new URL('/login', request.url))
  }

  const ok = await isValidSession(request.headers.get('cookie'), password, process.env.SESSION_SECRET)
  if (ok) {
    return next()
  }

  if (
    pathname.startsWith('/media/') ||
    pathname === '/catalog.json' ||
    pathname.startsWith('/api/')
  ) {
    return new Response(JSON.stringify({ error: 'Unauthorized' }), {
      status: 401,
      headers: { 'Content-Type': 'application/json' },
    })
  }

  const login = new URL('/login', request.url)
  if (pathname !== '/' && pathname !== '/login') {
    login.searchParams.set('next', pathname)
  }
  return Response.redirect(login)
}
