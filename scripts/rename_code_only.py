"""Substitute identifiers in C code only, never inside strings/chars/comments."""
import re, sys

def split_code(s):
    """Yield (is_code, text) segments."""
    i, n, start = 0, len(s), 0
    while i < n:
        c = s[i]
        if c == '/' and i + 1 < n and s[i+1] == '*':
            yield True, s[start:i]
            j = s.find('*/', i + 2)
            j = n if j < 0 else j + 2
            yield False, s[i:j]
            i = start = j
        elif c == '/' and i + 1 < n and s[i+1] == '/':
            yield True, s[start:i]
            j = s.find('\n', i)
            j = n if j < 0 else j
            yield False, s[i:j]
            i = start = j
        elif c in '"\'':
            yield True, s[start:i]
            j = i + 1
            while j < n:
                if s[j] == '\\':
                    j += 2
                    continue
                if s[j] == c:
                    j += 1
                    break
                j += 1
            yield False, s[i:j]
            i = start = j
        else:
            i += 1
    yield True, s[start:]

def rename(s, pairs):
    out, hits = [], 0
    for is_code, text in split_code(s):
        if is_code:
            for a, b in pairs:
                text, k = re.subn(r'\b%s\b' % a, b, text)
                hits += k
        out.append(text)
    return ''.join(out), hits
