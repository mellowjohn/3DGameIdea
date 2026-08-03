import { buildSessionCookie, makeSessionToken } from '../lib/auth.js'

export const config = { runtime: 'edge' }

export default async function handler(request) {
  if (request.method !== 'POST') {
    return new Response(JSON.stringify({ error: 'Method not allowed' }), {
      status: 405,
      headers: { 'Content-Type': 'application/json' },
    })
  }

  const sitePassword = process.env.SITE_PASSWORD
  if (!sitePassword) {
    return new Response(JSON.stringify({ error: 'SITE_PASSWORD not configured' }), {
      status: 503,
      headers: { 'Content-Type': 'application/json' },
    })
  }

  let body = {}
  try {
    body = await request.json()
  } catch {
    return new Response(JSON.stringify({ error: 'Invalid JSON' }), {
      status: 400,
      headers: { 'Content-Type': 'application/json' },
    })
  }

  const password = typeof body.password === 'string' ? body.password : ''
  if (password !== sitePassword) {
    return new Response(JSON.stringify({ error: 'Invalid password' }), {
      status: 401,
      headers: { 'Content-Type': 'application/json' },
    })
  }

  const token = await makeSessionToken(sitePassword, process.env.SESSION_SECRET)
  const secure = new URL(request.url).protocol === 'https:'
  return new Response(JSON.stringify({ ok: true }), {
    status: 200,
    headers: {
      'Content-Type': 'application/json',
      'Set-Cookie': buildSessionCookie(token, secure),
    },
  })
}
