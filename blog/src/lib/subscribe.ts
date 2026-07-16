export type SubscribeResult =
  | { ok: true }
  | { ok: false; message: string }

/** Formspree form id from https://formspree.io (safe to ship in a static build). */
export function getFormspreeId(): string | undefined {
  const value = import.meta.env.VITE_FORMSPREE_ID as string | undefined
  return value?.trim() || undefined
}

/**
 * Collect a subscriber email via Formspree.
 * You export/manage the list and send new-post updates from your own email.
 */
export async function subscribeEmail(email: string): Promise<SubscribeResult> {
  const formId = getFormspreeId()
  if (!formId) {
    return {
      ok: false,
      message: 'Email signup is temporarily unavailable. Please try again later.',
    }
  }

  try {
    const res = await fetch(`https://formspree.io/f/${formId}`, {
      method: 'POST',
      headers: {
        Accept: 'application/json',
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        email,
        _subject: 'Wrathful Conquest blog subscribe',
        source: 'wrathful-conquest-devlog',
      }),
    })

    if (res.ok) {
      return { ok: true }
    }

    const body = (await res.json().catch(() => null)) as { errors?: { message?: string }[] } | null
    const detail = body?.errors?.[0]?.message
    return {
      ok: false,
      message: detail || 'Could not subscribe. Please try again later.',
    }
  } catch {
    return {
      ok: false,
      message: 'Network error. Check your connection and try again.',
    }
  }
}
