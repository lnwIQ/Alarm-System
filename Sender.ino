/* -------- Sender : UNO + W5100 -------- */

#include <SPI.h>
#include <Ethernet.h>

/* ─── Network ─── */
byte mac[] = { 0xDE,0xAD,0xBE,0xEF,0xFE,0x01 };
IPAddress ip(192,168,1,250);          // Sender
IPAddress recvIP[] = {                // Receivers
  IPAddress(192,168,1,251),
  IPAddress(192,168,1,252)
};

EthernetClient client;

/* ─── Pin map ─── */
const int btnPin   = 2;    // ปุ่ม
const int ledMain  = 10;   // LED แสดงการกด
const int ledRx1   = 8;    // LED สถานะ Receiver 1
const int ledRx2   = 9;    // LED สถานะ Receiver 2

/* ─── Vars ─── */
bool lastBtn = HIGH;
bool rxOnline[2] = {false,false};
unsigned long lastPoll = 0;

void setup() {
  pinMode(btnPin , INPUT_PULLUP);
  pinMode(ledMain, OUTPUT);
  pinMode(ledRx1 , OUTPUT);
  pinMode(ledRx2 , OUTPUT);
  digitalWrite(ledMain, LOW);
  digitalWrite(ledRx1,  LOW);
  digitalWrite(ledRx2,  LOW);

  Ethernet.begin(mac, ip);
  delay(1000);
  Serial.begin(9600);
  Serial.print(F("Sender @ ")); Serial.println(Ethernet.localIP());
}

void sendCmd(IPAddress ip, bool on) {
  if (client.connect(ip, 80)) {
    client.print(F("GET /ring?on="));
    client.print(on ? '1':'0');
    client.println(F(" HTTP/1.1"));
    client.println(F("Connection: close\r\n"));
    client.stop();
    Serial.print(F("CMD ")); Serial.print(on?"ON ":"OFF "); Serial.println(ip);
  } else {
    Serial.print(F("Fail connect ")); Serial.println(ip);
  }
}

bool pingReceiver(IPAddress ip) {
  if (!client.connect(ip, 80)) return false;
  client.println(F("GET / HTTP/1.1"));
  client.println(F("Connection: close\r\n"));

  unsigned long t0 = millis();
  bool ok=false;
  while (client.connected() && millis()-t0 < 1500) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      if (line.startsWith("READY")) { ok=true; break; }
      if (line.startsWith("HTTP/1.1 404")) break;
    }
  }
  client.stop();
  return ok;
}

void loop() {
  /* ── Handle button ── */
  bool cur = digitalRead(btnPin);
  if (cur != lastBtn) {
    delay(10);
    cur = digitalRead(btnPin);
    if (cur != lastBtn) {
      digitalWrite(ledMain, !cur);          // ติดเฉพาะตอนกด
      for (auto ip : recvIP) sendCmd(ip, cur==LOW);
      lastBtn = cur;
    }
  }

  /* ── Poll receivers ทุก 10 s ── */
  if (millis()-lastPoll >= 10000) {
    lastPoll = millis();
    for (int i=0;i<2;i++) {
      bool ok = pingReceiver(recvIP[i]);
      rxOnline[i] = ok;
      digitalWrite(i==0?ledRx1:ledRx2, ok ? HIGH:LOW);
      Serial.print(F("RX ")); Serial.print(recvIP[i]);
      Serial.println(ok?F(" READY"):F(" OFFLINE"));
    }
  }
}
