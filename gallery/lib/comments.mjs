/**
 * Shared comment model + sanitization (Edge-safe, plain ESM).
 */

/** @typedef {'up' | 'down'} VoteValue */

/**
 * @typedef {object} CommentRecord
 * @property {string} id
 * @property {string} assetId
 * @property {string} author
 * @property {string} text
 * @property {string} createdAt
 * @property {Record<string, VoteValue>} votes
 */

/**
 * @typedef {object} CommentPublic
 * @property {string} id
 * @property {string} assetId
 * @property {string} author
 * @property {string} text
 * @property {string} createdAt
 * @property {number} likes
 * @property {number} dislikes
 * @property {VoteValue | null} myVote
 */

export const MAX_AUTHOR = 48
export const MAX_TEXT = 2000
export const MAX_COMMENTS_PER_ASSET = 200

/** @param {unknown} raw */
export function sanitizeAuthor(raw) {
  if (typeof raw !== 'string') return null
  const t = raw.trim().replace(/\s+/g, ' ')
  if (t.length < 1 || t.length > MAX_AUTHOR) return null
  return t
}

/** @param {unknown} raw */
export function sanitizeText(raw) {
  if (typeof raw !== 'string') return null
  const t = raw.trim()
  if (t.length < 1 || t.length > MAX_TEXT) return null
  return t
}

/** @param {unknown} raw */
export function sanitizeVoterId(raw) {
  if (typeof raw !== 'string') return null
  const t = raw.trim()
  if (!/^[a-zA-Z0-9_-]{8,64}$/.test(t)) return null
  return t
}

/** @param {unknown} raw */
export function sanitizeAssetId(raw) {
  if (typeof raw !== 'string') return null
  const t = raw.trim()
  if (!/^[a-f0-9]{8,32}$/i.test(t)) return null
  return t
}

/**
 * @param {CommentRecord} c
 * @param {string | null} voterId
 * @returns {CommentPublic}
 */
export function toPublic(c, voterId) {
  let likes = 0
  let dislikes = 0
  for (const v of Object.values(c.votes || {})) {
    if (v === 'up') likes++
    else if (v === 'down') dislikes++
  }
  return {
    id: c.id,
    assetId: c.assetId,
    author: c.author,
    text: c.text,
    createdAt: c.createdAt,
    likes,
    dislikes,
    myVote: voterId && c.votes?.[voterId] ? c.votes[voterId] : null,
  }
}

export function newCommentId() {
  return crypto.randomUUID().replace(/-/g, '')
}
