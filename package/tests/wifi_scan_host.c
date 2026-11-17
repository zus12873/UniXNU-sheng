// SPDX-License-Identifier: BSD-2-Clause-Patent
// Host tests of the Apple80211 scan surface of WifiNet.cpp (package/WifiScan.h): the card capability bitmap and the multiple-SSID scan
// request (type 86) as IO80211 26C5035r's __getScanData builds it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../WifiScan.h"

static unsigned checks;
#define CHECK(x) do {if (!(x)) {fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1);} ++checks;} while (0)

static int has_bit(const uint8_t caps[SHENG_WIFI_CAPS_BYTES], unsigned bit) {return (caps[bit / 8] >> (bit % 8)) & 1;}

// Apple80211Scan (26C5035r) refuses a scan that names networks (-3903, before any request reaches the driver) unless the cached
// capability byte 2 has bit 6: capability 22. A saved network airportd counts as hidden is only ever looked for that way.
static void caps_allow_named_scans(void)
{
    uint8_t caps[SHENG_WIFI_CAPS_BYTES];
    memset(caps, 0xee, sizeof caps);
    sheng_wifi_caps_fill(caps);
    CHECK(has_bit(caps, SHENG_WIFI_CAP_NAMED_SCAN));
    CHECK(caps[2] & 0x40);
    // the scan-done event the framework waits for (byte 5 bit 6), and the rest as before
    CHECK(has_bit(caps, SHENG_WIFI_CAP_SCAN_DONE_EVENT));
    CHECK(caps[5] & 0x40);
    static const unsigned before[] = {2, 3, 6, 9, 10, 14, 17, 19, 46};
    unsigned set = 0;
    for (unsigned b = 0; b < SHENG_WIFI_CAPS_BYTES * 8; ++b) set += (unsigned)has_bit(caps, b);
    for (unsigned i = 0; i < sizeof before / sizeof before[0]; ++i) CHECK(has_bit(caps, before[i]));
    CHECK(set == sizeof before / sizeof before[0] + 1);
}

// The request __getScanData fills: 0x8 the number of SSID entries, one more than the names given (the last, zeroed, is the wildcard);
// 0xc the entries, 40 bytes each {version 1, length, 32 bytes}; 0x200 the scan type.
static void put_u32(uint8_t *at, uint32_t v) {memcpy(at, &v, 4);}
static void request(uint8_t *req, const char *const *names, unsigned count, int wildcard, uint32_t scan_type)
{
    memset(req, 0, SHENG_WIFI_SCAN_MULTIPLE_BYTES);
    put_u32(req + 0, 1);
    for (unsigned i = 0; i < count; ++i) {
        uint8_t *entry = req + SHENG_WIFI_SCAN_SSIDS + i * SHENG_WIFI_SCAN_SSID_ENTRY;
        put_u32(entry, 1);
        put_u32(entry + 4, (uint32_t)strlen(names[i]));
        memcpy(entry + 8, names[i], strlen(names[i]));
    }
    put_u32(req + SHENG_WIFI_SCAN_SSID_COUNT, count + (wildcard ? 1u : 0u));
    put_u32(req + SHENG_WIFI_SCAN_TYPE, scan_type);
}

static void named_with_wildcard_is_broadcast(void)
{
    static uint8_t req[SHENG_WIFI_SCAN_MULTIPLE_BYTES];
    static const char *const names[] = {"first-network", "second"};
    request(req, names, 2, 1, 1);
    struct sheng_wifi_scan_multiple s;
    CHECK(sheng_wifi_scan_multiple_parse(req, sizeof req, &s) == 0);
    CHECK(s.named == 2 && s.wildcard == 1 && s.scan_type == 1);
    CHECK(s.length[0] == 13 && !memcmp(s.ssid[0], "first-network", 13));
    CHECK(s.length[1] == 6 && !memcmp(s.ssid[1], "second", 6));
    // the radio probes for every network: a network that broadcasts answers whatever names were asked for
    const uint8_t *ssid = (const uint8_t *)"x"; uint32_t length = 99;
    sheng_wifi_scan_multiple_target(&s, &ssid, &length);
    CHECK(ssid == NULL && length == 0);
}

static void one_name_without_wildcard_is_directed(void)
{
    static uint8_t req[SHENG_WIFI_SCAN_MULTIPLE_BYTES];
    static const char *const names[] = {"hidden-only"};
    request(req, names, 1, 0, 1);
    struct sheng_wifi_scan_multiple s;
    CHECK(sheng_wifi_scan_multiple_parse(req, sizeof req, &s) == 0);
    CHECK(s.named == 1 && s.wildcard == 0);
    const uint8_t *ssid = NULL; uint32_t length = 0;
    sheng_wifi_scan_multiple_target(&s, &ssid, &length);
    CHECK(length == 11 && ssid == s.ssid[0]);
}

static void cached_type_is_kept(void)
{
    static uint8_t req[SHENG_WIFI_SCAN_MULTIPLE_BYTES];
    static const char *const names[] = {"a"};
    request(req, names, 1, 1, SHENG_WIFI_SCAN_TYPE_CACHED);
    struct sheng_wifi_scan_multiple s;
    CHECK(sheng_wifi_scan_multiple_parse(req, sizeof req, &s) == 0);
    CHECK(s.scan_type == SHENG_WIFI_SCAN_TYPE_CACHED);
}

static void malformed_requests_are_refused(void)
{
    static uint8_t req[SHENG_WIFI_SCAN_MULTIPLE_BYTES];
    static const char *const names[] = {"a", "b"};
    struct sheng_wifi_scan_multiple s;
    request(req, names, 2, 1, 1);
    // too short to hold the scan type
    CHECK(sheng_wifi_scan_multiple_parse(req, SHENG_WIFI_SCAN_TYPE + 3, &s) != 0);
    CHECK(sheng_wifi_scan_multiple_parse(NULL, sizeof req, &s) != 0);
    // more entries than the 9 names and the wildcard the framework sends
    put_u32(req + SHENG_WIFI_SCAN_SSID_COUNT, SHENG_WIFI_SCAN_SSID_MAX + 1);
    CHECK(sheng_wifi_scan_multiple_parse(req, sizeof req, &s) != 0);
    // a name longer than 32 bytes
    request(req, names, 2, 1, 1);
    put_u32(req + SHENG_WIFI_SCAN_SSIDS + 4, 33);
    CHECK(sheng_wifi_scan_multiple_parse(req, sizeof req, &s) != 0);
    // no entries at all: a broadcast scan
    request(req, names, 0, 0, 1);
    CHECK(sheng_wifi_scan_multiple_parse(req, sizeof req, &s) == 0);
    CHECK(s.named == 0 && s.wildcard == 0);
    const uint8_t *ssid = (const uint8_t *)"x"; uint32_t length = 5;
    sheng_wifi_scan_multiple_target(&s, &ssid, &length);
    CHECK(ssid == NULL && length == 0);
}

int main(void)
{
    caps_allow_named_scans();
    named_with_wildcard_is_broadcast();
    one_name_without_wildcard_is_directed();
    cached_type_is_kept();
    malformed_requests_are_refused();
    printf("checks %u\n", checks);
    return 0;
}
