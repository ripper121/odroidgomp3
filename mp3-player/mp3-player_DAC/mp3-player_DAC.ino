#include <odroid_go.h>
#include <WiFi.h>
#include "AudioFileSourceSD.h"
#include "AudioFileSourceID3.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

struct Track
{
  String label;
  int timePos;
  Track *left;
  Track *right;
};

Track *trackList;
bool playing = true, mute = false;
unsigned long currentMillis;
float gain = 0.1, oldGain = 0;

extern unsigned char timer_logo[];
extern unsigned char insertsd_logo[];
extern unsigned char error_logo[];

AudioGeneratorMP3 *mp3;
AudioFileSourceSD *file;
AudioOutputI2S *out;
AudioFileSourceID3 *id3;

char* strToChar(String str) {
  int len = str.length() + 1;
  char* buf = new char[len];
  strcpy(buf, str.c_str());
  return buf;
}

int cntChar(String str, char chr) {
  int n = 0;
  for (int i = 0; i < str.length(); i++)
  {
    if (str[i] == chr) n++;
  }
  return n;
}

int posChar(String str, char chr) {
  int n = cntChar(str, chr);
  int t = 0;
  for (int p = 0; p < str.length(); p++)
  {
    if (str[p] == chr) t++;
    if (t == n) return p;
  }
  return -1;
}

String parseString(int idSeparator, char separator, String str) { // like a split JS
  String output = "";
  int separatorCout = 0;
  for (int i = 0; i < str.length(); i++)
  {
    if ((char)str[i] == separator)
    {
      separatorCout++;
    }
    else
    {
      if (separatorCout == idSeparator)
      {
        output += (char)str[i];
      }
      else if (separatorCout > idSeparator)
      {
        break;
      }
    }
  }
  return output;
}

bool createTrackList(String dir) {
  int i = 0;
  File root = SD.open(strToChar(dir));
  if (root)
  {
    while (true)
    {
      File entry =  root.openNextFile();
      if (!entry) break;
      if (!entry.isDirectory())
      {
        String ext = parseString(cntChar(entry.name(), '.'), '.', entry.name());
        if (ext == "mp3")
        {
          i++;
          Track *tmp = new Track;
          tmp->label = entry.name();
          tmp->timePos = 0;
          tmp->right = tmp;
          if (trackList == NULL)
          {
            tmp->left = tmp;
            trackList = tmp;
          }
          else
          {
            tmp->left = trackList;
            trackList->right = tmp;
            trackList = trackList->right;
          }
        }
      }
      entry.close();
    }
    if (i > 1)
    {
      do
      {
        trackList = trackList->left;
      } while (trackList != trackList->left);
    }
    root.close();
  }
  if (i > 0)
    return true;
  return false;
}

bool play(char dir) {
  Serial.println("play()");
  Serial.println(trackList->label);
  switch (dir)
  {
    case 'r':
      if (trackList == trackList->right) return false;
      trackList->timePos = 0;
      trackList = trackList->right;
      break;
    case 'l':
      if (trackList == trackList->left) return false;
      trackList->timePos = 0;
      trackList = trackList->left;
      break;
    case 'm': // mute
      delete file;
      delete out;
      delete mp3;
      mp3 = NULL;
      file = NULL;
      out = NULL;
      if (SD.exists(trackList->label)) {
        file = new AudioFileSourceSD(strToChar(trackList->label));
        id3 = new AudioFileSourceID3(file);
        id3->seek(trackList->timePos, 1);
        out = new AudioOutputI2S(0, 0, 6, 0);
        out->SetGain(gain);
        mp3 = new AudioGeneratorMP3();
        playing = false;
        mp3->begin(id3, out);
      } else {
        Serial.println("not found");
        return false;
      }
      return true;
    default:
      if (playing)
      {
        trackList->timePos = id3->getPos();
        play('m');
        return true;
      }
      break;
  }
  drawCover();
  delete file;
  delete out;
  delete mp3;
  mp3 = NULL;
  file = NULL;
  out = NULL;
  if (SD.exists(trackList->label)) {
    file = new AudioFileSourceSD(strToChar(trackList->label));
    id3 = new AudioFileSourceID3(file);
    id3->seek(trackList->timePos, 1);
    out = new AudioOutputI2S(0, 0, 6, 0);
    out->SetGain(gain);
    mp3 = new AudioGeneratorMP3();
    playing = true;
    mp3->begin(id3, out);
  } else {
    Serial.println("not found");
    return false;
  }
  return true;
}

unsigned long genSpectrum_previousMillis = 0;
void genSpectrum() {
  currentMillis = millis();
  if (currentMillis - genSpectrum_previousMillis > 100)
  {
    genSpectrum_previousMillis = currentMillis;
    drawSpectrum(random(0, 101), random(0, 101), random(0, 101), random(0, 101));

  }
}

void drawCover() {
  Serial.println("drawCover()");
  Serial.println(trackList->label + ".jpg");
  if (SD.exists(trackList->label + ".jpg")) {
    Serial.println("exists");
    GO.lcd.drawJpgFile(SD, strToChar(trackList->label + ".jpg"), 30, 15, 75, 75);
  } else {
    GO.lcd.fillRect(30, 15, 75, 75, 0xffff);
    Serial.println("not found");
  }
}

unsigned long drawTimeline_previousMillis = 0;
void drawTimeline() {
  currentMillis = millis();
  if (currentMillis - drawTimeline_previousMillis > 250)
  {
    int x = 30;
    int y = 110;
    int width = 260;
    int heightLine = 2;
    int heightMark = 20;
    int widthMark = 2;
    int yClear = y - (heightMark / 2);
    int wClear = width + (widthMark / 2);

    drawTimeline_previousMillis = currentMillis;
    GO.lcd.fillRect(x, yClear, wClear, heightMark, 0xffff);
    GO.lcd.fillRect(x, y, width, heightLine, 0x7bef);
    int size_ = id3->getSize();
    int pos_ = id3->getPos();
    int xPos = x + ((pos_ * (width - (widthMark / 2))) / size_);
    GO.lcd.fillRect(xPos, yClear, widthMark, heightMark, 0xe8e4);
  }
}

void drawSpectrum(int a, int b, int c, int d) { // %
  int x = 195;
  int y = 30;
  int padding = 10;
  int height = 30;
  int width = 15;

  int aH = ((a * height) / 100);
  int aY = y + (height - aH);
  GO.lcd.fillRect(x, y, width, height, 0xffff);
  GO.lcd.fillRect(x, aY, width, aH, 0x7bef); //0xe8e4

  int bH = ((b * height) / 100);
  int bY = y + (height - bH);
  int bX = x + width + padding;
  GO.lcd.fillRect(bX, y, width, height, 0xffff);
  GO.lcd.fillRect(bX, bY, width, bH, 0x7bef); //0xff80

  int cH = ((c * height) / 100);
  int cY = y + (height - cH);
  int cX = bX + width + padding;
  GO.lcd.fillRect(cX, y, width, height, 0xffff);
  GO.lcd.fillRect(cX, cY, width, cH, 0x7bef);//0x2589

  int dH = ((d * height) / 100);
  int dY = y + (height - dH);
  int dX = cX + width + padding;;
  GO.lcd.fillRect(dX, y, width, height, 0xffff);
  GO.lcd.fillRect(dX, dY, width, dH, 0x7bef);//0x051d
}

void drawTrackList() {
  Serial.println("drawTrackList()");
  Serial.println((trackList->left)->label);
  Serial.println(trackList->label);
  Serial.println((trackList->right)->label);

  GO.lcd.fillRect(0, 130, 320, 75, 0xffff);
  if (trackList->left != trackList)
  {
    GO.lcd.setTextSize(2);
    GO.lcd.setTextColor(0x7bef);
    GO.lcd.setCursor(10, 130);
    String label = ((trackList->left)->label).substring(1, posChar(((trackList->left)->label), '.'));
    label.replace("mp3/", "");
    if (((trackList->left)->label).length() > 24) label = label.substring(0, 24) + ".";
    GO.lcd.print(label);
  }

  GO.lcd.setTextSize(3);
  GO.lcd.setTextColor(0x0000);
  GO.lcd.setCursor(10, 155);
  String label = (trackList->label).substring(1, posChar((trackList->label), '.'));
  label.replace("mp3/", "");
  if ((trackList->label).length() > 16) label = label.substring(0, 16) + ".";
  GO.lcd.print(label);


  if (trackList->right != trackList)
  {
    GO.lcd.setTextSize(2);
    GO.lcd.setTextColor(0x7bef);
    GO.lcd.setCursor(10, 185);
    String label = ((trackList->right)->label).substring(1, posChar(((trackList->right)->label), '.'));
    label.replace("mp3/", "");
    if (((trackList->right)->label).length() > 24) label = label.substring(0, 24) + ".";
    GO.lcd.print(label);
  }

  GO.lcd.fillRect(10, 220, 200, 30, WHITE);
  GO.lcd.setCursor(10, 220);
  GO.lcd.setTextSize(2);
  GO.lcd.setTextColor(0x7bef);
  if (mute) {
    GO.lcd.print("Mute");
  } else {
    GO.lcd.print("Vol:");
    GO.lcd.print(int(gain * 100));
  }
}

void drawGUI() {
  GO.lcd.clear();
  GO.lcd.fillRect(0, 0, 320, 240, 0xffff);
  GO.lcd.setCursor(0, 0);
  GO.lcd.setTextColor(0x7bef);
  GO.lcd.print("\n\n MP3 Player for ODROID-GO\n       by Ripper121\n\n       DAC-Version");
  drawTrackList();
  while (true)
  {
    if (GO.BtnA.wasPressed())
    {
      GO.lcd.fillRect(0, 0, 320, 240, 0x0000);
      GO.lcd.fillRoundRect(0, 0, 320, 240, 7, 0xffff);
      drawTrackList();
      break;
    }
    GO.update();
  }
}

void setup() {
  GO.begin();
  WiFi.mode(WIFI_OFF);
  GO.battery.setProtection(true);
  GO.lcd.fillRoundRect(0, 0, 320, 240, 7, 0xffff);
  GO.lcd.setTextColor(0x7bef);
  GO.lcd.setTextSize(2);
  GO.lcd.drawBitmap(30, 75, 59, 59, (uint16_t *)timer_logo);
  GO.lcd.setCursor(110, 90);
  GO.lcd.print("STARTING...");
  GO.lcd.setCursor(110, 110);
  GO.lcd.print("WAIT A MOMENT");
  if (!SD.begin())
  {
    GO.lcd.fillRoundRect(0, 0, 320, 240, 7, 0xffff);
    GO.lcd.drawBitmap(50, 70, 62, 115, (uint16_t *)insertsd_logo);
    GO.lcd.setCursor(130, 70);
    GO.lcd.print("INSERT");
    GO.lcd.setCursor(130, 90);
    GO.lcd.print("THE TF-CARD");
    GO.lcd.setCursor(130, 110);
    GO.lcd.print("AND TAP");
    GO.lcd.setCursor(130, 130);
    GO.lcd.setTextColor(0xe8e4);
    GO.lcd.print("POWER");
    GO.lcd.setTextColor(0x7bef);
    GO.lcd.print(" BUTTON");
    while (true);
  }
  if (!createTrackList("/mp3"))
  {
    GO.lcd.fillRoundRect(0, 0, 320, 240, 7, 0xffff);
    GO.lcd.drawBitmap(30, 75, 59, 59, (uint16_t *)error_logo);
    GO.lcd.setCursor(110, 70);
    GO.lcd.print("ADD MP3 FILES");
    GO.lcd.setCursor(110, 90);
    GO.lcd.print("TO THE TF-CARD");
    GO.lcd.setCursor(110, 110);
    GO.lcd.print("AND TAP");
    GO.lcd.setCursor(110, 130);
    GO.lcd.setTextColor(0xe8e4);
    GO.lcd.print("POWER");
    GO.lcd.setTextColor(0x7bef);
    GO.lcd.print(" BUTTON");
    while (true);
  }
  drawGUI();
  play('m');
}

unsigned long previousMillis = 0;
const long interval = 1000;
bool displayOff = false;

void loop() {
  if (!displayOff) {
    if (GO.JOY_X.wasAxisPressed() == 2)
    {
      play('l');
      if (!displayOff)
        drawTrackList();
    }

    if (GO.JOY_X.wasAxisPressed() == 1)
    {
      play('r');
      if (!displayOff)
        drawTrackList();
    }

    if (GO.BtnA.wasPressed())
    {
      play('t');
    }



    if (GO.JOY_Y.isAxisPressed() == 2)
    {
      if (gain <= 0.99)
        gain += 0.01;
      else
        gain = 1;
      out->SetGain(gain);
      if (!displayOff)
        drawTrackList();
    }

    if (GO.JOY_Y.isAxisPressed() == 1)
    {
      if (gain >= 0.01)
        gain -= 0.01;
      else
        gain = 0.01;
      out->SetGain(gain);
      if (!displayOff)
        drawTrackList();
    }

    if (GO.BtnVolume.wasPressed())
    {
      mute = !mute;

      if (mute) {
        oldGain = gain;
        gain = 0;
      }
      else {
        gain = oldGain;
      }
      if (!displayOff)
        drawTrackList();

      out->SetGain(gain);
    }
  }
  
  if (GO.BtnMenu.wasPressed())
  {
    displayOff = !displayOff;
    if (displayOff) {
      GO.lcd.setBrightness(0);
    } else {
      GO.lcd.setBrightness(255);
    }
  }


  if (playing)
  {
    if (mp3->isRunning())
    {
      if (!mp3->loop())
      {
        mp3->stop();
        playing = false;
        play('r');
        if (!displayOff)
          drawTrackList();
      }
    }
    else
    {
      delay(1000);
    }
    if (!displayOff) {
      genSpectrum();
      drawTimeline();
    }
  }

  if (!displayOff) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      // save the last time you blinked the LED
      previousMillis = currentMillis;
      GO.lcd.fillRect(270, 0, 50, 30, WHITE);
      GO.lcd.setCursor(270, 0);
      GO.lcd.setTextSize(2);
      GO.lcd.print(GO.battery.getPercentage());
      GO.lcd.print("%");
    }
  }
  GO.update();
}
