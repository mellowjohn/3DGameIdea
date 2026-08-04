/**
 * Comment persistence: Upstash Redis REST when configured, else JSON file (local/dev).
 */
import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { MAX_COMMENTS_PER_ASSET } from './comments.mjs'

const KEY_PREFIX = 'wc-gallery:comments:'

function hasUpstash() {
  return Boolean(process.env.UPSTASH_REDIS_REST_URL && process.env.UPSTASH_REDIS_REST_TOKEN)
}

/** @param {(string | number)[]} command */
async function redisCommand(command) {
  const url = process.env.UPSTASH_REDIS_REST_URL
  const token = process.env.UPSTASH_REDIS_REST_TOKEN
  if (!url || !token) throw new Error('Upstash not configured')

  const res = await fetch(`${url.replace(/\/$/, '')}`, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${token}`,
      'Content-Type': 'application/json',
    },
    body: JSON.stringify(command),
  })
  if (!res.ok) {
    const body = await res.text()
    throw new Error(`Redis error ${res.status}: ${body}`)
  }
  const json = await res.json()
  return json.result
}

function fileStorePath() {
  const here = fileURLToPath(new URL('.', import.meta.url))
  return join(here, '..', '.data', 'comments.json')
}

function readFileStore() {
  const path = fileStorePath()
  if (!existsSync(path)) return { byAsset: {} }
  try {
    const parsed = JSON.parse(readFileSync(path, 'utf8'))
    if (!parsed.byAsset || typeof parsed.byAsset !== 'object') return { byAsset: {} }
    return parsed
  } catch {
    return { byAsset: {} }
  }
}

function writeFileStore(data) {
  const path = fileStorePath()
  mkdirSync(dirname(path), { recursive: true })
  writeFileSync(path, JSON.stringify(data, null, 2), 'utf8')
}

/** @param {string} assetId */
export async function listComments(assetId) {
  if (hasUpstash()) {
    const raw = await redisCommand(['GET', `${KEY_PREFIX}${assetId}`])
    if (!raw || typeof raw !== 'string') return []
    try {
      const list = JSON.parse(raw)
      return Array.isArray(list) ? list : []
    } catch {
      return []
    }
  }
  return readFileStore().byAsset[assetId] ?? []
}

/** @param {string} assetId @param {import('./comments.mjs').CommentRecord[]} list */
async function saveComments(assetId, list) {
  if (hasUpstash()) {
    await redisCommand(['SET', `${KEY_PREFIX}${assetId}`, JSON.stringify(list)])
    return
  }
  const data = readFileStore()
  data.byAsset[assetId] = list
  writeFileStore(data)
}

/** @param {import('./comments.mjs').CommentRecord} record */
export async function addComment(record) {
  const list = await listComments(record.assetId)
  list.unshift(record)
  if (list.length > MAX_COMMENTS_PER_ASSET) list.length = MAX_COMMENTS_PER_ASSET
  await saveComments(record.assetId, list)
  return record
}

/**
 * @param {string} assetId
 * @param {string} commentId
 * @param {string} voterId
 * @param {import('./comments.mjs').VoteValue | null} vote
 */
export async function voteOnComment(assetId, commentId, voterId, vote) {
  const list = await listComments(assetId)
  const idx = list.findIndex((c) => c.id === commentId)
  if (idx === -1) return null
  const comment = { ...list[idx], votes: { ...(list[idx].votes || {}) } }
  if (vote === null) delete comment.votes[voterId]
  else comment.votes[voterId] = vote
  list[idx] = comment
  await saveComments(assetId, list)
  return comment
}

export function commentBackendLabel() {
  return hasUpstash() ? 'upstash' : 'file'
}
