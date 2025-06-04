/*  -------------------  BASIC ALARM RECEIVER  -------------------
    - W5100  (Ethernet Shield รุ่นเก่า)
    - ตรวจลิงก์ด้วยการ TCP connect() ไปยังเซิร์ฟเวอร์ที่แน่ใจว่าเปิดพอร์ต
    ---------------------------------------------------------------- */
#include <SPI.h>
#include <Ethernet.h>

/* ---------- Network ---------- */
byte mac[]  = { 0xDE,0xAD,0xBE,0xEF,0xFE,0x11 };
IPAddress ip(192,168,1,251);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress dns(192,168,1,1);

/* ---------- ลิงก์-เทสต์ ---------- */
#define TEST_IP   IPAddress(192,168,1,1)   // เปลี่ยนเป็น IP ที่เปิดพอร์ตแน่นอน (เช่น PC ในแลน)
#define TEST_PORT 80
const unsigned long CHECK_EVERY_MS = 3000;

EthernetServer server(80);

/* ---------- I/O ---------- */
const uint8_t relayPin = 3;
const uint8_t led_Satatus_Pin   = 8;
const uint8_t led_PowerON_Pin   = 10;

/* ---------- Blink ---------- */
bool ledState=false;
unsigned long lastBlink=0;

/* ---------- Auto-OFF ---------- */
unsigned long ringDuration = 0; // 0 = ไม่จำกัด
bool  isRinging    = false;
unsigned long lastOnTime = 0;

/* ---------- ลอง connect() ดูว่าลิงก์จริงไหม ---------- */
bool linkOK()
{
  EthernetClient c;
  bool ok = c.connect(TEST_IP, TEST_PORT); // จับมือ TCP
  c.stop();
  return ok;
}

/* ---------- HTTP ---------- */
void handleHttp(EthernetClient& c)
{
  String req;
  while (c.connected())
  {
    if (c.available())
    {
      char ch = c.read();
      req += ch;
      if (req.endsWith("\r\n\r\n")) break;
    }
  }

  int s = req.indexOf(' ');
  int e = req.indexOf(' ', s+1);
  if (s > 0 && e > 0)
  {
    String url = req.substring(s+1, e);
    
    if (url == "/")
    {
      c.println(F("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nREADY"));
    }
    else if (url.startsWith("/ring?on="))
    {
      isRinging = (url.charAt(9)=='1');
      if (isRinging) lastOnTime = millis();
      c.println(F("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK"));
    }
    else
    {
      c.println(F("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nNot Found"));
    }
  }

  c.stop();
}

/* ---------- setup ---------- */
void setup()
{
  pinMode(relayPin, OUTPUT);
  pinMode(led_Satatus_Pin,   OUTPUT);
  pinMode(led_PowerON_Pin,   OUTPUT);

  digitalWrite(led_PowerON_Pin, HIGH);

  Ethernet.begin(mac, ip, dns, gateway, subnet);
  delay(1500);
  server.begin();

  Serial.begin(9600);
  Serial.print(F("Ready @ ")); Serial.println(Ethernet.localIP());
}

/* ---------- loop ---------- */
void loop()
{
  /* รับ web */
  EthernetClient cli = server.available();
  if (cli) handleHttp(cli);

  /* Auto-OFF */
  if (isRinging && ringDuration && millis()-lastOnTime >= ringDuration)
      isRinging=false;
  digitalWrite(relayPin, isRinging);

  /* เช็กลิงก์ทุก 3 วิ */
  static unsigned long lastCheck=0;
  static bool link=true;
  unsigned long now = millis();
  if (now-lastCheck >= CHECK_EVERY_MS)
  {
    link = linkOK();
    Serial.print(F("LAN: ")); Serial.println(link?F("OK"):F("FAIL"));
    lastCheck = now;
  }

  /* กระพริบ LED */
  unsigned long interval = link ? 2000 : 200;
  if (now-lastBlink >= interval)
  {
    ledState = !ledState;
    digitalWrite(led_Satatus_Pin, ledState);
    lastBlink = now;
  }
}
