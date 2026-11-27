/*
  ESP32-S3 SuperMini + DFPlayer Mini (MP3-TF-16P V3.0)
  Library: DFRobotDFPlayerMini

  Wiring:
    ESP32 TX (GPIO43) -> 1k resistor -> DFPlayer RX
    ESP32 RX (GPIO44) <- DFPlayer TX
    ESP32 5V          -> DFPlayer VCC
    ESP32 GND         -> DFPlayer GND
    Speaker (8 ohm)   -> DFPlayer SPK1 and SPK2

  SD card (FAT32): /MP3/0001.mp3 ... /MP3/0005.mp3

  Serial commands (115200 baud):
    n = next, p = previous, s = pause, r = resume,
    + = volume up, - = volume down, 1..9 = play /MP3/000N.mp3
*/

#include <Arduino.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define DF_TX_PIN   43   // ESP32 TX -> DFPlayer RX (through 1k resistor)
#define DF_RX_PIN   44   // ESP32 RX <- DFPlayer TX
#define TRACK_COUNT 5    // number of files in /MP3

HardwareSerial dfSerial(1);
DFRobotDFPlayerMini player;

int volume = 15;                 // 0..30
int currentTrack = 1;
unsigned long lastPlayMs = 0;

void printDetail(uint8_t type, int value);

void playTrack(int n) {
  if (n > TRACK_COUNT) n = 1;
  if (n < 1) n = TRACK_COUNT;
  currentTrack = n;
  lastPlayMs = millis();
  player.playMp3Folder(n);       // plays /MP3/NNNN.mp3
  Serial.printf("Playing /MP3/%04d.mp3\n", n);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dfSerial.begin(9600, SERIAL_8N1, DF_RX_PIN, DF_TX_PIN);

  Serial.println();
  Serial.println(F("Initializing DFPlayer..."));

  if (!player.begin(dfSerial, true, true)) {
    Serial.println(F("DFPlayer init failed. Check wiring, 5V power and SD card (FAT32)."));
    while (true) delay(1000);
  }
  Serial.println(F("DFPlayer online."));

  player.setTimeOut(500);
  player.outputDevice(DFPLAYER_DEVICE_SD);
  delay(1000);                   // let the module settle after selecting the SD card
  player.volume(volume);
  player.EQ(DFPLAYER_EQ_NORMAL);

  // Informational only: many MP3-TF-16P clones don't answer this query (-1).
  int count = -1;
  for (int i = 0; i < 3 && count < 0; i++) {
    count = player.readFileCounts();
    if (count < 0) delay(500);
  }
  if (count < 0) Serial.println(F("File count not supported by this module (ignored)."));
  else           Serial.printf("Files on SD: %d\n", count);

  playTrack(1);
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'n': playTrack(currentTrack + 1); break;
      case 'p': playTrack(currentTrack - 1); break;
      case 's': player.pause();              break;
      case 'r': player.start();              break;
      case '+': volume = min(volume + 2, 30); player.volume(volume); Serial.printf("Volume: %d\n", volume); break;
      case '-': volume = max(volume - 2, 0);  player.volume(volume); Serial.printf("Volume: %d\n", volume); break;
      case '1': case '2': case '3': case '4': case '5':
      case '6': case '7': case '8': case '9':
        playTrack(c - '0');
        break;
      default: break;
    }
  }

  if (player.available()) {
    printDetail(player.readType(), player.read());
  }
}

void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time out!")); break;
    case WrongStack:
      Serial.println(F("Stack wrong!")); break;
    case DFPlayerCardInserted:
      Serial.println(F("Card inserted.")); break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card removed.")); break;
    case DFPlayerCardOnline:
      Serial.println(F("Card online.")); break;
    case DFPlayerPlayFinished:
      Serial.printf("Track %d finished.\n", value);
      // Some modules send this event twice; ignore repeats within 1 s
      if (millis() - lastPlayMs > 1000) playTrack(currentTrack + 1);
      break;
    case DFPlayerError:
      Serial.print(F("DFPlayerError: "));
      switch (value) {
        case Busy:             Serial.println(F("Card not found")); break;
        case Sleeping:         Serial.println(F("Sleeping")); break;
        case SerialWrongStack: Serial.println(F("Wrong stack")); break;
        case CheckSumNotMatch: Serial.println(F("Checksum mismatch")); break;
        case FileIndexOut:     Serial.println(F("File index out of range")); break;
        case FileMismatch:     Serial.println(F("File not found")); break;
        case Advertise:        Serial.println(F("In advertise mode")); break;
        default:               Serial.println(value); break;
      }
      break;
    default: break;
  }
}