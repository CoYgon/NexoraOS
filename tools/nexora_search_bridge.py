#!/usr/bin/env python3
"""NexoraOS Google Search bridge.

The freestanding NexoraOS kernel currently has TCP/IP but no TLS implementation.
This small host-side bridge fetches Google over HTTPS using Python's verified TLS
stack and returns a compact, plain-text result list to the guest over QEMU SLIRP.
It deliberately exposes only /search, not an arbitrary URL proxy.
"""
from __future__ import annotations

import argparse
import html
from html.parser import HTMLParser
import ipaddress
import re
import ssl
import sys
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

MAX_QUERY_CHARS = 180
MAX_RESULTS = 5
USER_AGENT = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36 NexoraOSSearchBridge/1.0"


def clean_text(value: str) -> str:
    value = html.unescape(value or "")
    return re.sub(r"\s+", " ", value).strip()


def unwrap_google_url(url: str) -> str:
    parsed = urllib.parse.urlparse(url)
    if parsed.path == "/url":
        params = urllib.parse.parse_qs(parsed.query)
        target = (params.get("q") or params.get("url") or [""])[0]
        if target.startswith(("http://", "https://")):
            return target
    return url


class GoogleResultParser(HTMLParser):
    """Best-effort extraction of external result anchors from Google's HTML."""
    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.results: list[tuple[str, str]] = []
        self._href: str | None = None
        self._text: list[str] = []
        self._depth = 0

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if tag.lower() == "a":
            attr = dict(attrs)
            href = attr.get("href")
            if href:
                self._href = urllib.parse.urljoin("https://www.google.com", href)
                self._text = []
                self._depth = 1
        elif self._depth:
            self._depth += 1

    def handle_data(self, data: str) -> None:
        if self._depth:
            self._text.append(data)

    def handle_endtag(self, tag: str) -> None:
        if not self._depth:
            return
        if tag.lower() == "a":
            href = unwrap_google_url(self._href or "")
            title = clean_text(" ".join(self._text))
            parsed = urllib.parse.urlparse(href)
            host = (parsed.hostname or "").lower()
            if (parsed.scheme in ("http", "https") and host and
                    host not in {"google.com", "www.google.com", "accounts.google.com"} and
                    not host.endswith(".google.com") and len(title) >= 3):
                candidate = (title[:140], href[:240])
                if candidate not in self.results:
                    self.results.append(candidate)
            self._href = None
            self._text = []
            self._depth = 0
        else:
            self._depth -= 1


def parse_google_results(document: str, limit: int = MAX_RESULTS) -> list[tuple[str, str]]:
    parser = GoogleResultParser()
    parser.feed(document)
    return parser.results[:limit]


def search_google(query: str) -> str:
    encoded = urllib.parse.urlencode({"q": query, "num": str(MAX_RESULTS), "gbv": "1"})
    url = "https://www.google.com/search?" + encoded
    request = urllib.request.Request(url, headers={
        "User-Agent": USER_AGENT,
        "Accept": "text/html,application/xhtml+xml",
        "Accept-Language": "en-US,en;q=0.8",
    })
    # Explicit verified TLS context: require a trusted CA chain and hostname match.
    # Never fall back to an unverified context if verification fails.
    tls_context = ssl.create_default_context(purpose=ssl.Purpose.SERVER_AUTH)
    tls_context.verify_mode = ssl.CERT_REQUIRED
    tls_context.check_hostname = True
    opener = urllib.request.build_opener(urllib.request.HTTPSHandler(context=tls_context))
    try:
        with opener.open(request, timeout=12) as response:
            page = response.read(1_500_000).decode("utf-8", errors="replace")
    except urllib.error.HTTPError as exc:
        raise RuntimeError(f"Google returned HTTP {exc.code}") from exc
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        reason = getattr(exc, "reason", exc)
        raise RuntimeError(f"Could not reach Google over verified HTTPS: {reason}") from exc

    results = parse_google_results(page)
    if not results:
        lowered = page.lower()
        if "unusual traffic" in lowered or "captcha" in lowered or "consent.google.com" in lowered:
            raise RuntimeError("Google requires a CAPTCHA or consent page; no results could be extracted")
        raise RuntimeError("Google's page format changed or no public results were found")

    lines = [f'Google results for: {query}', ""]
    for index, (title, link) in enumerate(results, 1):
        lines.extend((f"{index}. {title}", link, ""))
    text = "\n".join(lines).rstrip() + "\n"
    # The guest UI is intentionally bounded; keep responses compact.
    if len(text.encode("utf-8")) > 1150:
        text = text.encode("utf-8")[:1140].decode("utf-8", errors="ignore").rsplit("\n", 1)[0] + "\n"
    return text


class BridgeHandler(BaseHTTPRequestHandler):
    server_version = "NexoraSearchBridge/1.0"
    protocol_version = "HTTP/1.0"

    def log_message(self, fmt: str, *args: object) -> None:
        sys.stdout.write("[bridge] " + (fmt % args) + "\n")

    def send_text(self, status: int, body: str) -> None:
        data = body.encode("utf-8", errors="replace")
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self) -> None:
        # QEMU user networking's guest normally appears as 10.0.2.15.
        try:
            peer = ipaddress.ip_address(self.client_address[0])
        except ValueError:
            self.send_text(403, "Invalid client address\n")
            return
        if not (peer.is_loopback or peer in ipaddress.ip_network("10.0.2.0/24")):
            self.send_text(403, "Only local/QEMU clients are allowed\n")
            return

        parsed = urllib.parse.urlsplit(self.path)
        if parsed.path != "/search":
            self.send_text(404, "Nexora Search Bridge: use /search?q=your+query\n")
            return
        query = urllib.parse.parse_qs(parsed.query, keep_blank_values=True).get("q", [""])[0]
        query = clean_text(query)
        if not query:
            self.send_text(400, "Enter a search query\n")
            return
        if len(query) > MAX_QUERY_CHARS:
            self.send_text(400, f"Search query is limited to {MAX_QUERY_CHARS} characters\n")
            return

        print(f"[bridge] Google search: {query[:100]}", flush=True)
        try:
            result = search_google(query)
        except RuntimeError as exc:
            self.send_text(502, f"Search failed: {exc}\n")
            return
        self.send_text(200, result)


def main() -> int:
    parser = argparse.ArgumentParser(description="Verified-HTTPS Google search bridge for NexoraOS/QEMU")
    parser.add_argument("--host", default="0.0.0.0", help="listen address (default: 0.0.0.0 for QEMU guest access)")
    parser.add_argument("--port", type=int, default=8080, help="listen TCP port (default: 8080)")
    args = parser.parse_args()
    server = ThreadingHTTPServer((args.host, args.port), BridgeHandler)
    print(f"NexoraOS Google Search Bridge listening on {args.host}:{args.port}")
    print("Uses Python's normal HTTPS certificate validation for Google.")
    print("Only /search is exposed; QEMU/loopback clients are accepted.")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nBridge stopped.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
