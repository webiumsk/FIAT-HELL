#pragma once

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Loads the ESP-IDF root bundle linked into libmbedtls.a
// (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL) into the core's single
// global certificate index. Call once in setup(), before any HTTPS request and
// before the price task starts: every handshake reads that index, and
// reloading it frees the array underneath a handshake running on another task.
void initCertificateBundle();

// Verifies servers against the bundle loaded by initCertificateBundle().
// WiFiClientSecure::setCACertBundle() is not used per client because it
// rebuilds the global index on every call.
class BundleTlsClient : public WiFiClientSecure {
public:
  BundleTlsClient() { _use_ca_bundle = true; }
};

// https:// goes through tls; http:// (an operator-entered LNbits URL) uses
// HTTPClient's own plain client. A failed handshake fails the request;
// nothing falls back to setInsecure().
bool beginNetwork(HTTPClient &http, BundleTlsClient &tls, const String &url);

// A self-contained request. HTTPClient keeps a pointer to its client after
// end(), so tls is declared first and outlives httpClient.
struct HttpsSession {
  BundleTlsClient tls;
  HTTPClient httpClient;
  bool begin(const String &url) { return beginNetwork(httpClient, tls, url); }
};
