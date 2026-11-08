"""HTTPS for the launcher: JSON requests and file downloads that resume, report progress, can be cancelled and are
checked against a sha256. Standard library only (urllib); Windows' own certificate store verifies the servers."""
import hashlib
import json
import os
import ssl
import time
import urllib.error
import urllib.request
from pathlib import Path

from . import NAME

CHUNK = 1 << 20


class Cancelled(Exception):
    pass


class DownloadError(Exception):
    pass


def user_agent(version='dev'):
    return f'{NAME}-Launcher/{version}'


_CTX = None


def _context():
    global _CTX
    if _CTX is None:
        _CTX = ssl.create_default_context()
    return _CTX


def request(url, headers=None, timeout=30, version='dev'):
    h = {'User-Agent': user_agent(version)}
    h.update(headers or {})
    req = urllib.request.Request(url, headers=h)
    ctx = _context() if url.startswith('https:') else None
    return urllib.request.urlopen(req, timeout=timeout, context=ctx)


def get_bytes(url, headers=None, timeout=30, version='dev', limit=64 << 20):
    with request(url, headers, timeout, version) as r:
        data = r.read(limit + 1)
    if len(data) > limit:
        raise DownloadError(f'{url}: more than {limit >> 20} MB')
    return data


def get_json(url, headers=None, timeout=30, version='dev'):
    return json.loads(get_bytes(url, headers, timeout, version).decode('utf-8'))


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        while b := f.read(CHUNK):
            h.update(b)
    return h.hexdigest()


def download(url, dest, sha256=None, size=None, progress=None, cancel=None, headers=None, retries=4, version='dev'):
    """Fetch url to dest through dest.part, resuming a part file a cut connection left. progress(done, total) after
    every chunk; cancel() returning True raises Cancelled. A wrong size or sha256 deletes the file and raises
    DownloadError. A dest already holding the expected sha256 is kept without a request."""
    dest = Path(dest)
    dest.parent.mkdir(parents=True, exist_ok=True)
    if sha256 and dest.is_file() and sha256_file(dest) == sha256.lower():
        if progress:
            progress(dest.stat().st_size, dest.stat().st_size)
        return dest
    part = dest.with_name(dest.name + '.part')
    last_error = None
    for attempt in range(retries):
        have = part.stat().st_size if part.is_file() else 0
        h = dict(headers or {})
        if have:
            h['Range'] = f'bytes={have}-'
        try:
            with request(url, h, 60, version) as r:
                status = getattr(r, 'status', 200)
                if have and status != 206:   # the server ignores Range: start over
                    have = 0
                total = int(r.headers.get('Content-Length') or 0) + have or size or 0
                with open(part, 'ab' if have else 'wb') as f:
                    done = have
                    while True:
                        if cancel and cancel():
                            raise Cancelled()
                        b = r.read(CHUNK)
                        if not b:
                            break
                        f.write(b)
                        done += len(b)
                        if progress:
                            progress(done, total or done)
            break
        except Cancelled:
            raise
        except urllib.error.HTTPError as e:
            if e.code == 416 and have:   # the part file is complete or longer than the file: start over
                part.unlink(missing_ok=True)
                last_error = e
                continue
            if e.code in (401, 403, 404, 410):
                raise DownloadError(f'{url}: HTTP {e.code}') from e
            last_error = e
        except (urllib.error.URLError, OSError, TimeoutError) as e:
            last_error = e
        time.sleep(min(2 ** attempt, 16))
    else:
        raise DownloadError(f'{url}: {last_error}')
    got = part.stat().st_size
    if size and got != size:
        part.unlink(missing_ok=True)
        raise DownloadError(f'{url}: {got} bytes, expected {size}')
    if sha256:
        digest = sha256_file(part)
        if digest != sha256.lower():
            part.unlink(missing_ok=True)
            raise DownloadError(f'{url}: sha256 {digest}, expected {sha256.lower()}')
    os.replace(part, dest)
    return dest
