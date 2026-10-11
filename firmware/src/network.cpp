#include "network.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebSocketsClient.h>
#include <WiFi.h>
#include <relay_host.h>
#include <stdarg.h>
#include <string.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Copy firmware/include/secrets.example.h to secrets.h and fill it in."
#endif

const char* MY_NAME = "esp32";               // "from" in every event I send
const uint32_t RECONNECT_MS = 3000;          // wait between relay connect tries
const uint32_t LOOKUP_AGAIN_MS = 10000;      // no relay this long: look up the Mac's IP again
const int NETWORK_CORE = 0;                  // loop() runs on core 1
const int LOG_LINE_SIZE = 64;

struct SendItem {
  bool down;
  uint32_t t;
};

// static: these names are private to this file, so they cannot clash with
// a name inside the WiFi or WebSocket libraries.
static QueueHandle_t sendQueue;     // loop() to network task: my key events
static QueueHandle_t receiveQueue;  // network task to loop(): events from others
static QueueHandle_t logQueue;      // network task to loop(): lines for serial

// Written only by the network task, read by loop(). One byte, so it is
// always read whole.
static volatile NetStatus status = NET_NO_WIFI;

static WebSocketsClient relay;
static bool relayStarted = false;
static uint32_t relayStartedMs = 0;
static uint32_t lastLookupFailMs = 0;
static bool lookupFailed = false;

// Queue one line for loop() to print. Works like printf.
static void logLine(const char* format, ...) {
  char line[LOG_LINE_SIZE];
  va_list args;
  va_start(args, format);
  vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  xQueueSend(logQueue, line, 0);  // 0: never wait. If loop() is behind, drop it.
}

// Runs inside relay.loop() when something happens on the connection.
static void onRelayEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      status = NET_RELAY;
      logLine("relay connected");
      break;
    case WStype_DISCONNECTED:
      if (status == NET_RELAY) {
        status = NET_WIFI;
        relayStartedMs = millis();  // count from now before looking up the IP again
        logLine("relay disconnected");
      }
      break;
    case WStype_TEXT: {
      KeyEvent event;
      if (parseKeyEvent((const char*)payload, length, &event)) {
        xQueueSend(receiveQueue, &event, 0);
      }
      break;
    }
    default:
      break;
  }
}

// The relay's IP, found the way RELAY_HOST asks for:
//   an IP like "192.168.0.188" is used directly, with no lookup
//   a Bonjour name like "Akritis-MacBook-Pro.local" is looked up with mDNS
//   any other name is looked up with normal DNS
static bool findRelay(IPAddress* ip) {
  switch (relayHostKind(RELAY_HOST)) {
    case HOST_IP:
      return ip->fromString(RELAY_HOST);
    case HOST_MDNS: {
      char name[64];
      if (!mdnsName(RELAY_HOST, name, sizeof(name))) return false;
      *ip = MDNS.queryHost(name, 2000);
      return *ip != IPAddress(0, 0, 0, 0);
    }
    case HOST_NAME:
      return WiFi.hostByName(RELAY_HOST, *ip) == 1;
    default:
      return false;  // HOST_EMPTY
  }
}

// Look up the Mac and point the WebSocket client at it. The client then keeps
// trying to connect by itself every RECONNECT_MS.
static void startRelay() {
  IPAddress ip;
  if (!findRelay(&ip)) {
    if (!lookupFailed && relayHostKind(RELAY_HOST) == HOST_EMPTY) logLine("no RELAY_HOST in secrets.h");
    else if (!lookupFailed) logLine("relay host %s not found, retrying", RELAY_HOST);
    lookupFailed = true;
    lastLookupFailMs = millis();
    return;
  }
  lookupFailed = false;
  logLine("relay at %s:%d", ip.toString().c_str(), RELAY_PORT);
  relay.begin(ip.toString(), RELAY_PORT, "/");
  relay.setReconnectInterval(RECONNECT_MS);
  // A ping every 15 s. Two missed answers mean the relay is gone, even if
  // the Mac just went to sleep without closing the connection.
  relay.enableHeartbeat(15000, 3000, 2);
  relayStarted = true;
  relayStartedMs = millis();
}

static void stopRelay() {
  relay.disconnect();
  relayStarted = false;
}

static void networkTask(void*) {
  bool wasOnWifi = false;
  bool mdnsStarted = false;
  for (;;) {
    bool onWifi = WiFi.status() == WL_CONNECTED;
    if (onWifi != wasOnWifi) {
      if (onWifi) {
        logLine("WiFi connected, ESP32 IP %s", WiFi.localIP().toString().c_str());
        if (!mdnsStarted) mdnsStarted = MDNS.begin("morse-esp32");
      } else {
        logLine("WiFi lost, reconnecting");
        if (relayStarted) stopRelay();
      }
      wasOnWifi = onWifi;
    }
    if (status != NET_RELAY || !onWifi) status = onWifi ? NET_WIFI : NET_NO_WIFI;

    if (onWifi) {
      bool lookupDue = !lookupFailed || millis() - lastLookupFailMs >= RECONNECT_MS;
      if (!relayStarted && lookupDue) startRelay();

      // Still no relay after a while: the Mac may have a new IP. Look again.
      if (relayStarted && status != NET_RELAY && millis() - relayStartedMs >= LOOKUP_AGAIN_MS) {
        stopRelay();
      }
      if (relayStarted) relay.loop();
    }

    // Send my key events, or drop them if the relay is not connected.
    SendItem item;
    while (xQueueReceive(sendQueue, &item, 0) == pdTRUE) {
      if (status != NET_RELAY) continue;
      KeyEvent event = {item.down, item.t, ""};
      strncpy(event.from, MY_NAME, MAX_SENDER_NAME);
      char text[80];
      size_t length = formatKeyEvent(event, text, sizeof(text));
      if (length > 0) relay.sendTXT(text, length);
    }

    vTaskDelay(1);  // rest 1 ms, so WiFi and the other tasks on core 0 can run
  }
}

void networkBegin() {
  sendQueue = xQueueCreate(32, sizeof(SendItem));
  receiveQueue = xQueueCreate(32, sizeof(KeyEvent));
  logQueue = xQueueCreate(8, LOG_LINE_SIZE);

  if (strlen(WIFI_SSID) == 0) {
    // Keying and the local decoder still work, just not over the network.
    logLine("no WiFi name in secrets.h, network off");
    return;
  }

  WiFi.mode(WIFI_STA);
  // WiFi power saving can hold received messages back by 100 ms or more,
  // which would upset the playout timing. Off costs about 100 mA more.
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);  // the WiFi driver rejoins by itself
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);  // returns at once, joining goes on in the background

  relay.onEvent(onRelayEvent);
  xTaskCreatePinnedToCore(networkTask, "network", 8192, nullptr, 1, nullptr, NETWORK_CORE);
}

bool networkSend(bool down, uint32_t t) {
  if (sendQueue == nullptr || status != NET_RELAY) return false;
  SendItem item = {down, t};
  return xQueueSend(sendQueue, &item, 0) == pdTRUE;  // 0: never wait
}

bool networkReceive(KeyEvent* event) {
  return receiveQueue != nullptr && xQueueReceive(receiveQueue, event, 0) == pdTRUE;
}

bool networkLogLine(char* line, size_t size) {
  char buffer[LOG_LINE_SIZE];
  if (logQueue == nullptr || xQueueReceive(logQueue, buffer, 0) != pdTRUE) return false;
  snprintf(line, size, "%s", buffer);
  return true;
}

NetStatus networkStatus() {
  return status;
}
