/**
 * Node serverless: like / dislike / clear vote on a comment.
 */
import { commentBackendLabel, voteOnComment } from '../../lib/comment-store.mjs'
import {
  sanitizeAssetId,
  sanitizeVoterId,
  toPublic,
} from '../../lib/comments.mjs'

function sendJson(res, status, body) {
  res.statusCode = status
  res.setHeader('Content-Type', 'application/json')
  res.setHeader('Cache-Control', 'no-store')
  res.end(JSON.stringify(body))
}

export default async function handler(req, res) {
  if (req.method !== 'POST') {
    return sendJson(res, 405, { error: 'Method not allowed' })
  }

  try {
    let body = req.body
    if (typeof body === 'string') body = JSON.parse(body || '{}')
    body = body || {}
    const assetId = sanitizeAssetId(body.assetId)
    const commentId = typeof body.commentId === 'string' ? body.commentId.trim() : ''
    const voterId = sanitizeVoterId(body.voterId)
    let vote = null
    if (body.vote === 'up' || body.vote === 'down') vote = body.vote
    else if (body.vote === null || body.vote === 'clear' || body.vote === '') vote = null
    else return sendJson(res, 400, { error: 'vote must be up, down, or clear' })

    if (!assetId) return sendJson(res, 400, { error: 'Invalid assetId' })
    if (!commentId || !/^[a-zA-Z0-9_-]{8,64}$/.test(commentId)) {
      return sendJson(res, 400, { error: 'Invalid commentId' })
    }
    if (!voterId) return sendJson(res, 400, { error: 'Invalid voterId' })

    const updated = await voteOnComment(assetId, commentId, voterId, vote)
    if (!updated) return sendJson(res, 404, { error: 'Comment not found' })

    return sendJson(res, 200, {
      backend: commentBackendLabel(),
      comment: toPublic(updated, voterId),
    })
  } catch (err) {
    const message = err instanceof Error ? err.message : 'Server error'
    return sendJson(res, 500, { error: message })
  }
}
