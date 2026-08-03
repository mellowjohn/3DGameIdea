/**
 * Shared session cookie helpers for gallery auth (Edge-safe ESM).
 */

export const COOKIE_NAME = 'wc_gallery_session'
export const COOKIE_MAX_AGE = 60 * 60 * 24 * 30 // 30 days

function encoder() {
  return new TextEncoder()
}

function bytesToHex(bytes) {
  return [...new Uint8Array(bytes)].map((b) => b.toString(16).padStart(2, '0')).join('')
}

export async function makeSessionToken(password, sessionSecret) {
  const keyMaterial = (sessionSecret && sessionSecret.trim()) || password
  const key = await crypto.subtle.importKey(
    'raw',
    encoder().encode(keyMaterial),
    { name: 'HMAC', hash: 'SHA-256' },
    false,
    ['sign'],
  )
  const sig = await crypto.subtle.sign('HMAC', key, encoder().encode(`wc-gallery-v1:${password}`))
  return bytesToHex(sig)
}

export async function isValidSession(cookieHeader, password, sessionSecret) {
  if (!password) return false
  const token = parseCookie(cookieHeader)[COOKIE_NAME]
  if (!token) return false
  const expected = await makeSessionToken(password, sessionSecret)
  return timingSafeEqual(token, expected)
}

export function parseCookie(header) {
  const out = {}
  if (!header) return out
  for (const part of header.split(';')) {
    const idx = part.indexOf('=')
    if (idx === -1) continue
    const k = part.slice(0, idx).trim()
    const v = part.slice(idx + 1).trim()
    if (k) out[k] = decodeURIComponent(v)
  }
  return out
}

export function buildSessionCookie(token, secure) {
  const parts = [
    `${COOKIE_NAME}=${encodeURIComponent(token)}`,
    'Path=/',
    `Max-Age=${COOKIE_MAX_AGE}`,
    'HttpOnly',
    'SameSite=Lax',
  ]
  if (secure) parts.push('Secure')
  return parts.join('; ')
}

export function buildClearCookie(secure) {
  const parts = [`${COOKIE_NAME}=`, 'Path=/', 'Max-Age=0', 'HttpOnly', 'SameSite=Lax']
  if (secure) parts.push('Secure')
  return parts.join('; ')
}

function timingSafeEqual(a, b) {
  if (a.length !== b.length) return false
  let diff = 0
  for (let i = 0; i < a.length; i++) {
    diff |= a.charCodeAt(i) ^ b.charCodeAt(i)
  }
  return diff === 0
}

export function isPublicPath(pathname) {
  if (pathname === '/login') return true
  if (pathname.startsWith('/api/login') || pathname.startsWith('/api/logout')) return true
  if (pathname.startsWith('/assets/')) return true
  if (pathname === '/favicon.svg' || pathname === '/favicon.ico' || pathname === '/favicon.png') return true
  return false
}
