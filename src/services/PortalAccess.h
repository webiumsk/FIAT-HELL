#pragma once

#include "portal/PortalPolicy.h"
#include <WebServer.h>
#include <WiFi.h>

// A settings request counts as local only while the setup hotspot is up and
// the connection arrived on the hotspot interface. The socket's local address
// is the address the client connected to, so this is exact even when the home
// LAN happens to use the same 192.168.4.0/24 range as the hotspot.
inline bool clientOnSoftAp(WiFiClient client) {
  if ((WiFi.getMode() & WIFI_AP) == 0) {
    return false;
  }
  const IPAddress ap = WiFi.softAPIP();
  return ap != IPAddress(0, 0, 0, 0) && client.localIP() == ap;
}

inline bool portalBasicAuth(WebServer &srv, const char *password) {
  if (srv.authenticate("admin", password)) {
    return true;
  }
  srv.requestAuthentication(BASIC_AUTH, "LN ATM", "Authentication required");
  return false;
}

// Per-boot hotspot passphrase used while the stored password fails the
// policy: 16 hex digits (64 bits). esp_random() is only a true RNG while the
// radio runs, so the radio is started first. A radio that is already up (the
// setup hotspot) is left alone: switching modes would drop its clients.
inline String makeFallbackApPsk() {
  if (WiFi.getMode() == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
  }
  char psk[17];
  snprintf(psk, sizeof(psk), "%08lx%08lx", (unsigned long)esp_random(),
           (unsigned long)esp_random());
  return String(psk);
}

// Registered first so it runs before AutoConnect. It claims a request only
// when the path is a settings path and the client is not on the hotspot.
class ApOnlyConfigHandler : public RequestHandler {
public:
  explicit ApOnlyConfigHandler(WebServer &webServer) : webServer(webServer) {}

  bool canHandle(HTTPMethod method, String uri) override {
    (void)method;
    if (!portalPathIsSensitive(uri.c_str())) {
      return false;
    }
    return !clientOnSoftAp(webServer.client());
  }

  bool handle(WebServer &srv, HTTPMethod method, String uri) override {
    (void)method;
    (void)uri;
    srv.send(403, "text/plain",
             "Settings are only available on the setup hotspot");
    return true;
  }

private:
  WebServer &webServer;
};
