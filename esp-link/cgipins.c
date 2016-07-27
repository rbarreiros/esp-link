
#include <esp8266.h>
#include "cgi.h"
#include "espfs.h"
#include "config.h"
#include "serled.h"
#include "status.h"
#include "serbridge.h"

#if 0
static char *map_names[] = {
  "esp-bridge", "jn-esp-v2", "esp-01(AVR)", "esp-01(ARM)", "esp-br-rev", "wifi-link-12",
};
static char* map_func[] = { "reset", "isp", "conn_led", "ser_led", "swap_uart" };
static int8_t map_asn[][5] = {
  { 12, 13,  0, 14, 0 },  // esp-bridge
  { 12, 13,  0,  2, 0 },  // jn-esp-v2
  {  0, -1,  2, -1, 0 },  // esp-01(AVR)
  {  0,  2, -1, -1, 0 },  // esp-01(ARM)
  { 13, 12, 14,  0, 0 },  // esp-br-rev -- for test purposes
  {  1,  3,  0,  2, 1 },  // esp-link-12
};
static const int num_map_names = sizeof(map_names)/sizeof(char*);
static const int num_map_func = sizeof(map_func)/sizeof(char*);
#endif

// Cgi to return choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsGet(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted

  char buff[1024];
  int len;

  len = os_sprintf(buff,
      "{ \"conn\":%d, \"ser\":%d, \"rxpup\":%d }",
      flashConfig.conn_led_pin, flashConfig.ser_led_pin, !!flashConfig.rx_pullup);

  jsonHeader(connData, 200);
  httpdSend(connData, buff, len);
  return HTTPD_CGI_DONE;
}

// Cgi to change choice of pin assignments
int ICACHE_FLASH_ATTR cgiPinsSet(HttpdConnData *connData) {
  if (connData->conn==NULL) {
    return HTTPD_CGI_DONE; // Connection aborted
  }

  int8_t ok = 0;
  int8_t conn, ser;
  uint8_t rxpup;
  ok |= getInt8Arg(connData, "conn", &conn);
  ok |= getInt8Arg(connData, "ser", &ser);
  ok |= getBoolArg(connData, "rxpup", &rxpup);
  if (ok < 0) return HTTPD_CGI_DONE;

  char *coll;
  if (ok > 0) {
    // check whether two pins collide
    uint16_t pins = 0;
    if (conn >= 0) {
      if (pins & (1<<conn)) { coll = "Conn LED"; goto collision; }
      pins |= 1 << conn;
    }
    if (ser >= 0) {
      if (pins & (1<<ser)) { coll = "Serial LED"; goto collision; }
      pins |= 1 << ser;
    }

    // we're good, set flashconfig
    flashConfig.conn_led_pin = conn;
    flashConfig.ser_led_pin = ser;
    flashConfig.rx_pullup = rxpup;
    os_printf("Pins changed: conn=%d ser=%d rx-pup=%d\n",
	conn, ser, rxpup);

    // apply the changes
    serledInit();
    statusInit();

    // save to flash
    if (configSave()) {
      httpdStartResponse(connData, 204);
      httpdEndHeaders(connData);
    } else {
      httpdStartResponse(connData, 500);
      httpdEndHeaders(connData);
      httpdSend(connData, "Failed to save config", -1);
    }
  }
  return HTTPD_CGI_DONE;

collision: {
    char buff[128];
    os_sprintf(buff, "Pin assignment for %s collides with another assignment", coll);
    errorResponse(connData, 400, buff);
    return HTTPD_CGI_DONE;
  }
}

int ICACHE_FLASH_ATTR cgiPins(HttpdConnData *connData) {
  if (connData->conn==NULL) return HTTPD_CGI_DONE; // Connection aborted. Clean up.
  if (connData->requestType == HTTPD_METHOD_GET) {
    return cgiPinsGet(connData);
  } else if (connData->requestType == HTTPD_METHOD_POST) {
    return cgiPinsSet(connData);
  } else {
    jsonHeader(connData, 404);
    return HTTPD_CGI_DONE;
  }
}
