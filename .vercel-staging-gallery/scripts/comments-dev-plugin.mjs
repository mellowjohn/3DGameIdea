/**
 * Vite dev middleware that mirrors /api/comments using the same file store.
 */
import {
  addComment,
  commentBackendLabel,
  listComments,
  voteOnComment,
} from '../lib/comment-store.mjs'
import {
  newCommentId,
  sanitizeAssetId,
  sanitizeAuthor,
  sanitizeText,
  sanitizeVoterId,
  toPublic,
} from '../lib/comments.mjs'

async function readBody(req) {
  const chunks = []
  for await (const chunk of req) chunks.push(chunk)
  const raw = Buffer.concat(chunks).toString('utf8')
  if (!raw) return {}
  return JSON.parse(raw)
}

function json(res, status, body) {
  res.statusCode = status
  res.setHeader('Content-Type', 'application/json')
  res.setHeader('Cache-Control', 'no-store')
  res.end(JSON.stringify(body))
}

/** @returns {import('vite').Plugin} */
export function commentsDevApiPlugin() {
  return {
    name: 'gallery-comments-dev-api',
    configureServer(server) {
      server.middlewares.use(async (req, res, next) => {
        const url = req.url || ''
        if (!url.startsWith('/api/comments')) return next()

        try {
          if (url.startsWith('/api/comments/vote') && req.method === 'POST') {
            const body = await readBody(req)
            const assetId = sanitizeAssetId(body.assetId)
            const commentId = typeof body.commentId === 'string' ? body.commentId.trim() : ''
            const voterId = sanitizeVoterId(body.voterId)
            let vote = null
            if (body.vote === 'up' || body.vote === 'down') vote = body.vote
            else if (body.vote === null || body.vote === 'clear' || body.vote === '') vote = null
            else return json(res, 400, { error: 'vote must be up, down, or clear' })

            if (!assetId || !commentId || !voterId) {
              return json(res, 400, { error: 'Invalid vote payload' })
            }
            const updated = await voteOnComment(assetId, commentId, voterId, vote)
            if (!updated) return json(res, 404, { error: 'Comment not found' })
            return json(res, 200, {
              backend: commentBackendLabel(),
              comment: toPublic(updated, voterId),
            })
          }

          if (url.startsWith('/api/comments') && !url.startsWith('/api/comments/vote')) {
            if (req.method === 'GET') {
              const u = new URL(url, 'http://localhost')
              const assetId = sanitizeAssetId(u.searchParams.get('assetId'))
              const voterId = sanitizeVoterId(u.searchParams.get('voterId'))
              if (!assetId) return json(res, 400, { error: 'Invalid assetId' })
              const list = await listComments(assetId)
              return json(res, 200, {
                backend: commentBackendLabel(),
                comments: list.map((c) => toPublic(c, voterId)),
              })
            }
            if (req.method === 'POST') {
              const body = await readBody(req)
              const assetId = sanitizeAssetId(body.assetId)
              const author = sanitizeAuthor(body.author)
              const text = sanitizeText(body.text)
              if (!assetId || !author || !text) {
                return json(res, 400, { error: 'Invalid comment payload' })
              }
              const record = await addComment({
                id: newCommentId(),
                assetId,
                author,
                text,
                createdAt: new Date().toISOString(),
                votes: {},
              })
              const voterId = sanitizeVoterId(body.voterId)
              return json(res, 201, {
                backend: commentBackendLabel(),
                comment: toPublic(record, voterId),
              })
            }
          }

          return json(res, 405, { error: 'Method not allowed' })
        } catch (err) {
          const message = err instanceof Error ? err.message : 'Server error'
          return json(res, 500, { error: message })
        }
      })
    },
  }
}
