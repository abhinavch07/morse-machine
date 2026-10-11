// Unit tests for lib/relay_host. They run on the Mac with: pio test -e native

#include <unity.h>

#include "relay_host.h"

void setUp() {}
void tearDown() {}

void test_plain_ip_is_used_directly() {
  TEST_ASSERT_TRUE(looksLikeIp("192.168.0.188"));
  TEST_ASSERT_EQUAL(HOST_IP, relayHostKind("192.168.0.188"));
  TEST_ASSERT_EQUAL(HOST_IP, relayHostKind("10.0.0.1"));
  TEST_ASSERT_EQUAL(HOST_IP, relayHostKind("0.0.0.0"));
  TEST_ASSERT_EQUAL(HOST_IP, relayHostKind("255.255.255.255"));
}

void test_not_an_ip() {
  const char* bad[] = {
      "192.168.0.256",     // a number over 255
      "192.168.0",         // only three numbers
      "192.168.0.188.1",   // five numbers
      "192.168..188",      // an empty number
      ".192.168.0.188",    // starts with a dot
      "192.168.0.188.",    // ends with a dot
      "192.168.0.1888",    // four digits
      " 192.168.0.188",    // a space
      "192.168.0.188 ",    // a space at the end
      "192.168.0.-1",      // a minus sign
      "",                  // empty
  };
  for (const char* host : bad) {
    TEST_ASSERT_FALSE_MESSAGE(looksLikeIp(host), host);
  }
}

void test_local_name_uses_mdns() {
  TEST_ASSERT_EQUAL(HOST_MDNS, relayHostKind("Akritis-MacBook-Pro.local"));
  char name[64];
  TEST_ASSERT_TRUE(mdnsName("Akritis-MacBook-Pro.local", name, sizeof(name)));
  TEST_ASSERT_EQUAL_STRING("Akritis-MacBook-Pro", name);
}

void test_mdns_name_needs_room() {
  char name[5];
  TEST_ASSERT_FALSE(mdnsName("Akritis-MacBook-Pro.local", name, sizeof(name)));
  TEST_ASSERT_FALSE(mdnsName("192.168.0.188", name, sizeof(name)));
}

void test_other_names_and_empty() {
  TEST_ASSERT_EQUAL(HOST_EMPTY, relayHostKind(""));
  TEST_ASSERT_EQUAL(HOST_NAME, relayHostKind("mac.lan"));
  TEST_ASSERT_EQUAL(HOST_NAME, relayHostKind(".local"));  // no name before .local
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_plain_ip_is_used_directly);
  RUN_TEST(test_not_an_ip);
  RUN_TEST(test_local_name_uses_mdns);
  RUN_TEST(test_mdns_name_needs_room);
  RUN_TEST(test_other_names_and_empty);
  return UNITY_END();
}
