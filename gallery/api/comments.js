/**
 * Node serverless: list + create comments.
 */
import { addComment, commentBackendLabel, listComments } from '../lib/comment-store.mjs'
import {
  newCommentId,
  sanitizeAssetId,
  sanitizeAuthor,
  sanitizeText,
  sanitizeVoterId,
  toPublic,
} from '../lib/comments.mjs'

function sendJson(res, status, body) {
  res.statusCode = status
  res.setHeader('Content-Type', 'application/json')
  res.setHeader('Cache-Control', 'no-store')
  res.end(JSON.stringify(body))
}

export default async function handler(req, res) {
  try {
    if (req.method === 'GET') {
      const q = req.query || {}
      const assetId = sanitizeAssetId(q.assetId)
      if (!assetId) return sendJson(res, 400, { error: 'Invalid assetId' })
      const voterId = sanitizeVoterId(q.voterId) ?? null
      const list = await listComments(assetId)
      return sendJson(res, 200, {
        backend: commentBackendLabel(),
        comments: list.map((c) => toPublic(c, voterId)),
      })
    }

    if (req.method === 'POST') {
      let body = req.body
      if (typeof body === 'string') body = JSON.parse(body || '{}')
      body = body || {}
      const assetId = sanitizeAssetId(body.assetId)
      const author = sanitizeAuthor(body.author)
      const text = sanitizeText(body.text)
      if (!assetId) return sendJson(res, 400, { error: 'Invalid assetId' })
      if (!author) return sendJson(res, 400, { error: 'Name required (1–48 chars)' })
      if (!text) return sendJson(res, 400, { error: 'Comment required (1–2000 chars)' })

      const record = await addComment({
        id: newCommentId(),
        assetId,
        author,
        text,
        createdAt: new Date().toISOString(),
        votes: {},
      })
      const voterId = sanitizeVoterId(body.voterId) ?? null
      return sendJson(res, 201, {
        backend: commentBackendLabel(),
        comment: toPublic(record, voterId),
      })
    }

    return sendJson(res, 405, { error: 'Method not allowed' })
  } catch (err) {
    const message = err instanceof Error ? err.message : 'Server error'
    return sendJson(res, 500, { error: message })
  }
}
