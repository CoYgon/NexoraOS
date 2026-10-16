#!/usr/bin/env python3
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
bridge = (root / "tools/nexora_search_bridge.py").read_text(encoding="utf-8")
contract = (root / "src/net/tls_native_contract.h").read_text(encoding="utf-8")

assert "ssl.create_default_context(purpose=ssl.Purpose.SERVER_AUTH)" in bridge
assert "tls_context.verify_mode = ssl.CERT_REQUIRED" in bridge
assert "tls_context.check_hostname = True" in bridge
assert "HTTPSHandler(context=tls_context)" in bridge
assert "unverified_context" not in bridge
assert "CERT_NONE" not in bridge
assert re.search(r"NEXORA_TLS_REQUIRE_PEER_CERTIFICATE\s+1", contract)
assert re.search(r"NEXORA_TLS_REQUIRE_HOSTNAME_MATCH\s+1", contract)
assert re.search(r"NEXORA_TLS_REQUIRE_TRUSTED_CA\s+1", contract)
assert re.search(r"NEXORA_TLS_ALLOW_INSECURE_FALLBACK\s+0", contract)
print("NexoraOS TLS policy tests passed (host bridge verification + native contract)")
