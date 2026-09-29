#include "services/HttpsClient.h"

extern "C" {
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
}

void initCertificateBundle() {
  WiFiClientSecure loader;
  loader.setCACertBundle(x509_crt_bundle_start);
}

bool beginNetwork(HTTPClient &http, BundleTlsClient &tls, const String &url) {
  if (url.startsWith("https://")) {
    return http.begin(tls, url);
  }
  return http.begin(url);
}
