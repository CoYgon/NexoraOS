#ifndef NEXORA_TLS_NATIVE_CONTRACT_H
#define NEXORA_TLS_NATIVE_CONTRACT_H

/*
 * Security contract for the future in-kernel TLS provider.
 * This header intentionally does not claim that a TLS implementation is linked.
 * A provider must satisfy every requirement below before the browser may use it.
 */
#define NEXORA_TLS_MIN_VERSION_MAJOR 1
#define NEXORA_TLS_MIN_VERSION_MINOR 2
#define NEXORA_TLS_REQUIRE_PEER_CERTIFICATE 1
#define NEXORA_TLS_REQUIRE_HOSTNAME_MATCH   1
#define NEXORA_TLS_REQUIRE_TRUSTED_CA       1
#define NEXORA_TLS_ALLOW_INSECURE_FALLBACK  0
#define NEXORA_TLS_DEFAULT_CA_PATH          "/etc/ssl/certs/ca-bundle.pem"
#define NEXORA_TLS_DEFAULT_SERVER_NAME       "www.google.com"

/* Native TLS remains unavailable until a vetted TLS engine and X.509 verifier
 * are linked into the freestanding kernel. Do not treat these constants as a
 * TLS implementation or as proof of a verified connection. */
#endif
