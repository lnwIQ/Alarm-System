#include <SPI.h>
#include <Ethernet.h>
#include <avr/wdt.h>

/* ─── Network ─── */
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 };
IPAddress ip(192, 168, 1, 250);
IPAddress recvIP[] = {
  IPAddress(192, 168, 1, 251),
  IPAddress(192, 168, 1, 252)
};

EthernetClient client;
EthernetServer server(80);

/* ─── Pin map ─── */
const int btnPin   = 2;
const int ledMain  = 7;
const int ledRx1   = 8;
const int ledRx2   = 9;

/* ─── Vars ─── */
bool rxOnline[2] = { false, false };
unsigned long lastPoll = 0;
unsigned long lastBlink = 0;
bool blinkState[2] = { false, false };

bool webActive = false;
bool btnActive = false;


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


  pinMode(btnPin , INPUT_PULLUP);
  pinMode(ledMain, OUTPUT);
  pinMode(ledRx1 , OUTPUT);
  pinMode(ledRx2 , OUTPUT);
  digitalWrite(ledMain, LOW);
  digitalWrite(ledRx1, LOW);
  digitalWrite(ledRx2, LOW);

  Ethernet.begin(mac, ip);
  delay(1000);
  server.begin();

  Serial.print(F("Sender @ ")); Serial.println(Ethernet.localIP());
}

void sendCmd(IPAddress ip, bool on) {
  EthernetClient cmd;
  cmd.setTimeout(1000);
  if (cmd.connect(ip, 80)) {
    cmd.print(F("GET /ring?on="));
    cmd.print(on ? '1' : '0');
    cmd.println(F(" HTTP/1.1"));
    cmd.println(F("Connection: close\r\n"));
    cmd.stop();
    Serial.print(F("CMD ")); Serial.print(on ? "ON " : "OFF "); Serial.println(ip);
  } else {
    Serial.print(F("Fail connect ")); Serial.println(ip);
  }
}

bool pingReceiver(IPAddress ip) {
  EthernetClient ping;
  if (!ping.connect(ip, 80)) return false;
  ping.println(F("GET / HTTP/1.1"));
  ping.println(F("Connection: close\r\n"));
  unsigned long t0 = millis();
  while (millis() - t0 < 1000) {
    if (ping.available()) {
      String line = ping.readStringUntil('\n');
      if (line.startsWith("READY")) {
        ping.stop();
        return true;
      }
    }
  }
  ping.stop();
  return false;
}

void handleWebRequest(EthernetClient& cli) {
  String req = "";
  while (cli.connected()) {
    if (cli.available()) {
      char c = cli.read();
      req += c;
      if (req.endsWith("\r\n\r\n")) break;
    }
  }

  if (req.indexOf("GET /on") >= 0) {
    if (!btnActive && !webActive) {
      for (int i = 0; i < 2; i++) sendCmd(recvIP[i], true);
      digitalWrite(ledMain, HIGH);
      webActive = true;
    }
  } else if (req.indexOf("GET /off") >= 0) {
    if (!btnActive && webActive) {
      for (int i = 0; i < 2; i++) sendCmd(recvIP[i], false);
      digitalWrite(ledMain, LOW);
      webActive = false;
    }
  }

  // Web page response
  cli.println(F("HTTP/1.1 200 OK"));
  cli.println(F("Content-Type: text/html"));
  cli.println(F("Connection: close"));
  cli.println();
  cli.println(F("<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"));

  cli.println(F("<style>html,body{margin:0;padding:0;overflow:hidden;height:100vh}</style>"));

  cli.println(F("<style>"));
  cli.println(F("body {margin: 0; padding: 0; font-family: sans-serif; text-align: center; display: flex; flex-direction: column; height: 100vh}"));
  cli.println(F(".circle {width: 15px; height: 15px; border-radius: 50%; display: inline-block; margin-right: 10px}"));
  cli.println(F(".sw {width: 70px; height: 36px; position: relative; display: inline-block}"));
  cli.println(F(".slider {position: absolute; cursor: grab; top: 0; left: 0; right: 0; bottom: 0; background: #ccc; border-radius: 30px; transition: .4s}"));
  cli.println(F(".slider:before {position: absolute; content: ''; height: 30px; width: 30px; left: 3px; bottom: 3px; background: white; border-radius: 50%; transition: .4s}"));
  cli.println(F("input:checked + .slider {background: #2196F3}"));
  cli.println(F("input:checked + .slider:before {transform: translateX(34px)}"));
  cli.println(F("</style>"));

  // JavaScript รองรับการลากบนมือถือ
  cli.println(F("<script>"));
  cli.println(F("document.addEventListener('DOMContentLoaded', function () {"));
  cli.println(F("let slider = document.querySelector('.slider');"));
  cli.println(F("let checkbox = document.querySelector('.sw input');"));
  cli.println(F("let isDragging = false;"));

  cli.println(F("function handleMove(event) {"));
  cli.println(F("let rect = slider.getBoundingClientRect();"));
  cli.println(F("let posX = (event.touches ? event.touches[0].clientX : event.clientX) - rect.left;"));
  cli.println(F("let percent = Math.max(0, Math.min(1, posX / rect.width));"));
  cli.println(F("checkbox.checked = percent > 0.5;"));
  cli.println(F("}"));

  cli.println(F("slider.addEventListener('mousedown', () => isDragging = true);"));
  cli.println(F("slider.addEventListener('touchstart', () => isDragging = true);"));

  cli.println(F("document.addEventListener('mousemove', (event) => { if (isDragging) handleMove(event); });"));
  cli.println(F("document.addEventListener('touchmove', (event) => { if (isDragging) handleMove(event); });"));

  cli.println(F("document.addEventListener('mouseup', () => { isDragging = false; location.href = checkbox.checked ? 'on' : 'off'; });"));
  cli.println(F("document.addEventListener('touchend', () => { isDragging = false; location.href = checkbox.checked ? 'on' : 'off'; });"));
  cli.println(F("});"));
  cli.println(F("</script>"));

  cli.println(F("<body>"));
  cli.println(F("<h2>InwIQ IoT Engineering<br>Alarm Monitor Service</h2>"));

  cli.println(F("<div style='display: flex; justify-content: center; gap: 10px;'>"));

  String tempStr1 = "<div><span class='circle' style='background:" + String(rxOnline[0] ? "green" : "red") + "'></span>กริ่งตัวที่ 1</div>";
  String tempStr2 = "<div><span class='circle' style='background:" + String(rxOnline[1] ? "green" : "red") + "'></span>กริ่งตัวที่ 2</div>";

  cli.println(tempStr1);
  cli.println(tempStr2);

  cli.println(F("</div>"));

  // Switch control
  cli.println(F("<div style='margin:20px 0'>"));
  cli.println(F("<h3>Sender Online</h3>"));
  cli.println(F("<div>เลื่อนสวิตช์เพื่อเปิด/ปิด กระดิ่ง</div>"));
  cli.println(F("</br>"));

  String tempSwitch = "<label class='sw'><input type='checkbox' " + String(webActive ? "checked" : "") + "><span class='slider'></span></label>";

  cli.println(tempSwitch);
  cli.println(F("</div>"));

  // Status Message
  cli.println(F("<div style='font-size:16px;color:#444;margin-bottom:10px'>"));

  if (webActive) {
    cli.print(F("สถานะ: "));
    if (rxOnline[0] && rxOnline[1]) {
      cli.println(F("เปิดแล้ว (ตัวรับทั้ง 2 ทำงานปกติ)"));
    } else if (rxOnline[0] && !rxOnline[1]) {
      cli.println(F("เปิดแล้ว (ตัวรับที่ 2 ไม่ตอบสนอง)"));
    } else if (!rxOnline[0] && rxOnline[1]) {
      cli.println(F("เปิดแล้ว (ตัวรับที่ 1 ไม่ตอบสนอง)"));
    } else {
      cli.println(F("เปิดแล้ว (ตัวรับทั้ง 2 ไม่ตอบสนอง)"));
    }
  } else {
    cli.println(F("สถานะ: ปิดแล้ว"));
  }

  cli.println(F("</div>"));

  // Footer ติดอยู่ด้านล่าง
  cli.println(F("<footer style='position:absolute;bottom:20px;width:100%;text-align:center;color:gray'>Copyright© 2025 <b>InwIQ</b></footer>"));
  cli.println(F("</body></html>"));
  cli.stop();
}

void loop() {

  // รีเซ็ต WDT ทุกครั้งที่ loop ทำงาน
  wdt_reset();

  if (millis() - lastPoll >= 5000) {
    lastPoll = millis();
    for (int i = 0; i < 2; i++) {
      rxOnline[i] = pingReceiver(recvIP[i]);
    }
  }
  /*
    // ถ้าหลุด LAN เกิน 60 วิ → รีบูต
    static unsigned long offlineSince = 0;
    if (!rxOnline[0] || !rxOnline[1]) {
      if (offlineSince == 0) offlineSince = millis();
      if (millis() - offlineSince >= 60000) {
        Serial.println(F("[ERROR] Receiver offline > 60s. Rebooting..."));
        delay(100);   // รอ Serial print
       asm volatile("jmp 0");
      }
    } else {
      offlineSince = 0;  // รีเซ็ตถ้ากลับมาออนไลน์
    }
  */
  EthernetClient newClient = server.available();
  if (newClient) handleWebRequest(newClient);

  // Physical button
  if (digitalRead(btnPin) == LOW) {
    if (!btnActive) {
      btnActive = true;
      for (int i = 0; i < 2; i++) sendCmd(recvIP[i], true);
      digitalWrite(ledMain, HIGH);
    }
  } else {
    if (btnActive) {
      btnActive = false;
      for (int i = 0; i < 2; i++) sendCmd(recvIP[i], false);
      digitalWrite(ledMain, LOW);
    }
  }

  // Poll every 5s
  if (millis() - lastPoll >= 5000) {
    lastPoll = millis();
    for (int i = 0; i < 2; i++) {
      rxOnline[i] = pingReceiver(recvIP[i]);
    }
  }

  // Blink LEDs if offline
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    for (int i = 0; i < 2; i++) {
      if (rxOnline[i]) {
        digitalWrite(i == 0 ? ledRx1 : ledRx2, HIGH);
      } else {
        blinkState[i] = !blinkState[i];
        digitalWrite(i == 0 ? ledRx1 : ledRx2, blinkState[i]);
      }
    }
  }
}
//ตรวจสอบได้เลยครับ แต่ ห้าม! ตัดข้อความ หรือ เงื่อนไข สำคัญๆ ก็แล้วกัน
