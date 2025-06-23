/*  -------------------  BASIC ALARM RECEIVER (UPDATED)  -------------------
      - รองรับ Ethernet Shield รุ่นเก่า W5100
      - ตรวจสอบสถานะ LAN โดยการ connect() ไปยัง IP/Port ปลายทาง
      - หากหลุด LAN: ขา 3 จะหยุดทำงานทันที เพื่อความปลอดภัย
      - หากหลุด LAN เกิน 60 วินาที: จะ restart ตัวเองอัตโนมัติ
      - มี Auto reconnect และปรับ LED blink ตามสถานะ
      - ไม่ใช้ Ethernet.linkStatus() เพราะ W5100 ไม่รองรับ
   ------------------------------------------------------------------------ */

#include <SPI.h>
#include <Ethernet.h>
#include <avr/wdt.h>

/* ---------- Network ---------- */
byte mac[]  = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xDE };
IPAddress ip(192, 168, 1, 252);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);
EthernetServer server(80);

/* ---------- ตรวจ Port ---------- */
IPAddress testIP(192, 168, 1, 1); // Router
const uint16_t testPort = 80;     // เปิดจริงบน Router
const unsigned long checkInterval = 3000;
const unsigned long maxLostDuration = 60000;

/* ---------- I/O ---------- */
const uint8_t relayPin = 3;
const uint8_t led_Status_Pin = 8;
const uint8_t led_PowerON_Pin = 9;

/* ---------- Blink ---------- */
bool ledState = false;
unsigned long lastBlink = 0;

/* ---------- Auto-OFF ---------- */
unsigned long ringDuration = 0;
bool isRinging = false;
unsigned long lastOnTime = 0;

/* ---------- LAN Status ---------- */
bool lanOK = false;
unsigned long lastCheck = 0;
unsigned long lastOKtime = 0;

/* ---------- Utils ---------- */
bool hasIPAddress() {
  return Ethernet.localIP() != IPAddress(0, 0, 0, 0);
}


bool checkPortOpen(IPAddress ip, uint16_t port) {
  EthernetClient client;
  bool ok = client.connect(ip, port);
  client.stop();
  return ok;
}

/* ---------- HTTP ---------- */
void handleHttp(EthernetClient& c) {
  String req;
  while (c.connected()) {
    if (c.available()) {
      char ch = c.read();
      req += ch;
      if (req.endsWith("\r\n\r\n")) break;
    }
  }

  int s = req.indexOf(' ');
  int e = req.indexOf(' ', s + 1);
  if (s > 0 && e > 0) {
    String url = req.substring(s + 1, e);

    if (url == "/") {
      c.println(F("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nREADY"));
    } else if (url.startsWith("/ring?on=")) {
      isRinging = (url.charAt(9) == '1');
      if (isRinging) lastOnTime = millis();
      Serial.print(F("[CMD] RING = "));
      Serial.println(isRinging ? F("ON") : F("OFF"));
      c.println(F("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK"));
    } else {
      c.println(F("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nNot Found"));
    }
  }
  c.stop();
}

void setup() {

  Serial.begin(9600);

  if (MCUSR & (1 << WDRF)) {
    Serial.println(F("[INFO] Reset by Watchdog"));
  } else {
    Serial.println(F("[INFO] Normal Boot"));
  }

  // ปิด WDT ก่อนเริ่มโปรแกรม
  MCUSR &= ~(1 << WDRF);
  wdt_disable();
  delay(1000);

  // เปิดใช้งาน WDT ให้รีเซ็ตทุก 8 วินาที
  wdt_enable(WDTO_8S);



  pinMode(relayPin, OUTPUT);
  pinMode(led_Status_Pin, OUTPUT);
  pinMode(led_PowerON_Pin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // ปิดเสียงก่อนเริ่ม

  digitalWrite(led_PowerON_Pin, LOW); delay(3000);  // เห็นว่ารีบูต
  digitalWrite(led_PowerON_Pin, HIGH);

  Serial.println(F("=== STARTING RECEIVER ==="));

  Ethernet.begin(mac, ip, dns, gateway, subnet);
  delay(1500);
  server.begin();

  Serial.print(F("[IP] Local IP: "));
  Serial.println(Ethernet.localIP());
}

void loop() {

  // รีเซ็ต WDT ทุกครั้งที่ loop ทำงาน
  wdt_reset();

  EthernetClient cli = server.available();
  if (cli) handleHttp(cli);

  unsigned long now = millis();

  // ตรวจสถานะ LAN ทุก 3 วิ
  if (now - lastCheck >= checkInterval) {
    lastCheck = now;

    bool gotIP = hasIPAddress();
    bool canConnect = gotIP ? checkPortOpen(testIP, testPort) : false;
    lanOK = gotIP && canConnect;

    Serial.print(F("[LAN] IP="));
    Serial.print(Ethernet.localIP());
    Serial.print(F("  Link="));
    Serial.print(gotIP ? F("OK") : F("FAIL"));
    Serial.print(F("  Connect="));
    Serial.print(canConnect ? F("OK") : F("FAIL"));
    Serial.print(F("  LAN OK="));
    Serial.println(lanOK ? F("YES") : F("NO"));

    if (lanOK) lastOKtime = now;
  }

  // ถ้าหลุด LAN เกิน 60 วิ → รีบูต
  if (now - lastOKtime >= maxLostDuration) {
    Serial.println(F("[RESTART] No LAN > 60s. Restarting..."));
    delay(100);
    asm volatile("jmp 0");
   // while (1);
  }

  // ถ้าหลุด LAN → ปิดเสียง
  if (!lanOK) {
    if (isRinging) Serial.println(F("[SAFE] Lost LAN → Ring OFF"));
    isRinging = false;
  }

  // ถ้าเปิดเสียงและหมดเวลา → ปิด
  if (isRinging && ringDuration && now - lastOnTime >= ringDuration) {
    Serial.println(F("[TIMEOUT] Ring expired → OFF"));
    isRinging = false;
  }

  digitalWrite(relayPin, !isRinging);  // Active LOW

  // กระพริบ LED ตามสถานะ
  unsigned long interval = lanOK ? 2000 : 200;
  if (now - lastBlink >= interval) {
    ledState = !ledState;
    digitalWrite(led_Status_Pin, ledState);
    lastBlink = now;
  }
}
