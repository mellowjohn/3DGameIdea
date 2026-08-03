export type VoteValue = 'up' | 'down'

export type CommentPublic = {
  id: string
  assetId: string
  author: string
  text: string
  createdAt: string
  likes: number
  dislikes: number
  myVote: VoteValue | null
}

const VOTER_KEY = 'wc-gallery-voter-id'
const AUTHOR_KEY = 'wc-gallery-author-name'

export function getVoterId(): string {
  try {
    let id = localStorage.getItem(VOTER_KEY)
    if (id && /^[a-zA-Z0-9_-]{8,64}$/.test(id)) return id
    id = crypto.randomUUID().replace(/-/g, '')
    localStorage.setItem(VOTER_KEY, id)
    return id
  } catch {
    return `anon${Date.now().toString(36)}`
  }
}

export function getSavedAuthor(): string {
  try {
    return localStorage.getItem(AUTHOR_KEY) || ''
  } catch {
    return ''
  }
}

export function saveAuthor(name: string) {
  try {
    localStorage.setItem(AUTHOR_KEY, name)
  } catch {
    // ignore
  }
}
