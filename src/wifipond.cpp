#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h>

#define ETH_MAC_LEN 6

const uint8_t broadcast1[3] = { 0x01, 0x00, 0x5e };
const uint8_t broadcast2[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const uint8_t broadcast3[3] = { 0x33, 0x33, 0x00 };

struct clientinfo
{
  uint8_t bssid[ETH_MAC_LEN];
  uint8_t station[ETH_MAC_LEN];
  uint8_t ap[ETH_MAC_LEN];
  int channel;
  int err;
  signed rssi;
  uint16_t seq_n;
};

const uint8_t conv_blur_size = 3;
const uint8_t conv_blur[3][3] = {{1,2,1},{2,4,2},{1,2,1}};

struct RxControl {
    signed   rssi:8;
    unsigned rate:4;
    unsigned is_group:1;
    unsigned :1;
    unsigned sig_mode:2;
    unsigned legacy_length:12;
    unsigned damatch0:1;
    unsigned damatch1:1;
    unsigned bssidmatch0:1;
    unsigned bssidmatch1:1;
    unsigned MCS:7;
    unsigned CWB:1;
    unsigned HT_length:16;
    unsigned Smoothing:1;
    unsigned Not_Sounding:1;
    unsigned :1;
    unsigned Aggregation:1;
    unsigned STBC:2;
    unsigned FEC_CODING:1;
    unsigned SGI:1;
    unsigned rxend_state:8;
    unsigned ampdu_cnt:8;
    unsigned channel:4;
    unsigned :12;
};

//Aggregate MAC Protocol Data Unit
struct Ampdu_Info
{
  uint16 length;
  uint16 seq;
  uint8  address3[6];
};

struct sniffer_buf {
    struct RxControl rx_ctrl;
    uint8_t  buf[36];
    uint16_t cnt;
    struct Ampdu_Info ampdu_info[1];
};

struct clientinfo parse_data(uint8_t *frame, uint16_t framelen, signed rssi, unsigned channel)
{
  struct clientinfo ci;
  ci.channel = channel;
  ci.err = 0;
  ci.rssi = rssi;
  int pos = 36;
  uint8_t *bssid;
  uint8_t *station;
  uint8_t *ap;
  uint8_t ds;

  ds = frame[1] & 3;    //Set first 6 bits to 0
  switch (ds) {
    // p[1] - xxxx xx00 => NoDS   p[4]-DST p[10]-SRC p[16]-BSS
    case 0:
      bssid = frame + 16;
      station = frame + 10;
      ap = frame + 4;
      break;
    // p[1] - xxxx xx01 => ToDS   p[4]-BSS p[10]-SRC p[16]-DST
    case 1:
      bssid = frame + 4;
      station = frame + 10;
      ap = frame + 16;
      break;
    // p[1] - xxxx xx10 => FromDS p[4]-DST p[10]-BSS p[16]-SRC
    case 2:
      bssid = frame + 10;
      // hack - don't know why it works like this...
      if (memcmp(frame + 4, broadcast1, 3) || memcmp(frame + 4, broadcast2, 3) || memcmp(frame + 4, broadcast3, 3)) {
        station = frame + 16;
        ap = frame + 4;
      } else {
        station = frame + 4;
        ap = frame + 16;
      }
      break;
    // p[1] - xxxx xx11 => WDS    p[4]-RCV p[10]-TRM p[16]-DST p[26]-SRC
    case 3:
      bssid = frame + 10;
      station = frame + 4;
      ap = frame + 4;
      break;
  }

  memcpy(ci.station, station, ETH_MAC_LEN);
  memcpy(ci.bssid, bssid, ETH_MAC_LEN);
  memcpy(ci.ap, ap, ETH_MAC_LEN);

  ci.seq_n = frame[23] * 0xFF + (frame[22] & 0xF0);
  return ci;
}

const uint8_t gamma_eye_correction[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

////////PIXELS

#define PIN D2
#define BUTTON_PIN D8
#define NPIXELS 8*8
#define NPOS NPIXELS*3
#define DECAY 5
#define BRIGHTNESS 200

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NPIXELS, PIN, NEO_GRB + NEO_KHZ800);

uint8_t target[NPIXELS * 3];
uint8_t uncorrected_pixels[NPIXELS * 3];
uint8_t new_uncorrected_pixels[NPIXELS * 3];

// Frame refresh, move the pixels towards their target values.
void move_to_target() {
  for (uint8_t p = 0; p < NPIXELS * 3; p++) {
    float diff = ((float) target[p]) - uncorrected_pixels[p];
    float new_target = uncorrected_pixels[p]; //+ diff * .03;

    //uncorrected_pixels[p] = min(255, max(0, (int) new_target));
  }
}

void blur() {
  for (uint16_t i = 0; i < 3 * 8 * 8; i++) {
    new_uncorrected_pixels[i] = 0;
  }
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      uint16_t sum_r = 0;
      uint16_t sum_g = 0;
      uint16_t sum_b = 0;
      for (int yk = -1; yk < conv_blur_size - 1; yk++) {
        for (int xk = -1; xk < conv_blur_size - 1; xk++) {
          int target_y = y + yk;
          int target_x = x + xk;
          if (target_y > 0 && target_y < 8 && target_x > 0 && target_y < 8) {

            int pixel = target_x + target_y * 8;
            //Serial.print(pixel);
            //Serial.print(",");
            sum_r += (uncorrected_pixels[pixel * 3]) * conv_blur[yk+1][xk+1];
            sum_g += (uncorrected_pixels[pixel * 3 + 1]) * conv_blur[yk+1][xk+1];
            sum_b += (uncorrected_pixels[pixel * 3 + 2]) * conv_blur[yk+1][xk+1];
          }
        }
      }
      sum_r /= 16;
      sum_g /= 16;
      sum_b /= 16;
      int pix = x + y * 8;
      new_uncorrected_pixels[pix*3] = (uint8_t) min((uint16_t)255,  (uint16_t) sum_r);
      new_uncorrected_pixels[pix*3 + 1] = (uint8_t) min((uint16_t)255,  (uint16_t) sum_g);
      new_uncorrected_pixels[pix*3 + 2] = (uint8_t) min((uint16_t)255,  (uint16_t) sum_b);
    }
  }
  for (uint16_t i = 0; i < 3 * 8 * 8; i++) {
    uncorrected_pixels[i] = new_uncorrected_pixels[i];
  }
}

// Take the uncorrected pixels, do gamma correction and set the pixels in the
void gamma_correct_set_pixels() {
  uint8_t *cur_pixels = pixels.getPixels();

  for (uint8_t x = 0; x < 8; x++) {
    for (uint8_t y = 0; y < 8; y++) {
      uint8_t pixel;
      if (y % 2 == 0) {
        pixel = y * 8 + x;
      } else {
        pixel = y * 8 + (7-x);
      }
      cur_pixels[pixel * 3 + 0] = uncorrected_pixels[(y * 8 + x) * 3 + 0];
      cur_pixels[pixel * 3 + 1] = uncorrected_pixels[(y * 8 + x) * 3 + 1];
      cur_pixels[pixel * 3 + 2] = uncorrected_pixels[(y * 8 + x) * 3 + 2];
    }
  }

  pixels.show();
}

// Hash for determining position and color.
uint16_t calculate_mac_hash(uint8_t mac_address[3]) {

  uint16_t hash = 17;
  for(int i = 0; i < 3; i++) {
      hash = 31 * hash + mac_address[i] * 19;
  }
  return hash;
}


typedef struct RgbColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RgbColor;

typedef struct HsvColor {
    uint8_t h;
    uint8_t s;
    uint8_t v;
} HsvColor;

RgbColor hsv_to_rgb(HsvColor hsv) {
    RgbColor rgb;
    uint8_t region, remainder, p, q, t;

    if (hsv.s == 0) {
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        return rgb;
    }

    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6;

    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }
    return rgb;
}


void light_led(uint8_t x, uint8_t y, uint8_t hue) {

  uint8_t led_id = (y * 8 + x) * 3;

  // Convert hue to RGB value.
  HsvColor hsv;
  hsv.h = hue;
  hsv.s = 255;
  hsv.v = 255;

  RgbColor rgb = hsv_to_rgb(hsv);

  uint8_t *p = uncorrected_pixels;
  p[led_id + 0] = rgb.r;
  p[led_id + 1] = rgb.g;
  p[led_id + 2] = rgb.b;

#ifdef DEBUG
  Serial.print(hue);
  Serial.println();

  Serial.print(rgb.r);
  Serial.print(" ");
  Serial.print(rgb.g);
  Serial.print(" ");
  Serial.print(rgb.b);
  Serial.println();
#endif
}

// void shine_at(int location)
// {
//   uint8_t *p = pixels.getPixels();
//   p[location] = BRIGHTNESS;
//
//   //Fade all pixels
//   for (int i = NPOS - 1; i >=0 ; --i){
//     if (p[i] > DECAY) {
//        p[i] -= DECAY;
//     } else {
//       p[i] = 0;
//     }
//   }

//   pixels.show();
// }


void packet_callback(uint8_t *buf, uint16_t len) {
  //len == 128 for beacons

  int i = 0;
  uint16_t seq_n_new = 0;

  struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
  //Is data or QOS?
  if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) {
    struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
//    if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) {
//      activity_by(ci.station);
//    }
#ifdef DEBUG
    for (int i = 0; i < ETH_MAC_LEN; i++) {
      Serial.print(ci.bssid[i], HEX);
      Serial.print(sniffer->ampdu_info[0].address3[i], HEX);

    }
#endif
    Serial.println();

    uint8_t *mac_address = sniffer->ampdu_info[0].address3;
    uint16_t mac_hash = calculate_mac_hash(mac_address);
    // Serial.println(mac_hash);


    // Positioning
    uint16_t h = mac_hash;
    uint8_t x = h & ((1 << 3) -1); // bit-mask for 3 right-most bits.
    uint8_t y = (h & (((1 << 3) -1) << 3)) >> 3; // bit-mask for bits 3 to 6.
      
    uint8_t hue = (h >> 8) & ((1 << 8) -1); // bit-mask for last 6 bits.
#ifdef DEBUG
    Serial.print(x);
    Serial.print(" ");
    Serial.print(y);
    Serial.print(" ");
    Serial.print(hue);
    Serial.println();
    Serial.println();
#endif

    light_led(x, y, hue);
  }
}



extern "C" {
#include "user_interface.h"
}

void setup() {
  pixels.begin();

  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(9);
  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(packet_callback);   // Set up promiscuous callback
  wifi_promiscuous_enable(1);

  pinMode(BUTTON_PIN, INPUT);

  Serial.begin(115200);

  /*
  for(uint16_t i=0; i<pixels.numPixels(); i++) {
    pixels.setPixelColor(i, pixels.Color(255, 0, 0));
    pixels.show();
    delay(300);
  }
  */
}

int channel = 1;

void loop() {
  int buttonVal = digitalRead(BUTTON_PIN);
  if (buttonVal > 0) {
    channel = (channel + 1) % 14;

    wifi_set_channel(channel);
#ifdef DEBUG
    Serial.print("Channel ");Serial.print(channel);Serial.println();
    delay(500);
#endif
  }
  blur();
  move_to_target();
  gamma_correct_set_pixels();
  delay(50);  // 50 fps.
}
