#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h>

#define ETH_MAC_LEN 6

uint8_t broadcast1[3] = { 0x01, 0x00, 0x5e };
uint8_t broadcast2[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
uint8_t broadcast3[3] = { 0x33, 0x33, 0x00 };



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

////////PIXELS
#define PIN D2
#define BUTTON_PIN D8
#define NPIXELS 8*8
#define NPOS NPIXELS*3 
#define DECAY 5
#define BRIGHTNESS 200

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NPIXELS, PIN, NEO_GRB + NEO_KHZ800);

void shine_at(int location)
{
  uint8_t *p = pixels.getPixels();
  p[location] = BRIGHTNESS;
  
  //Fade all pixels
  for(int i = NPOS -1; i >=0 ; --i){
    if(p[i] > DECAY) {
       p[i] -= DECAY;      
    } else {
      p[i] = 0;
    }
  }
  pixels.show();
}


void activity_by(uint8_t *mac) {
  int loc = 0;
  for(int i = 0; i < ETH_MAC_LEN; ++i) {
    loc += mac[i];
    loc = loc % NPOS;
  }
  shine_at(loc);
}

void packet_callback(uint8_t *buf, uint16_t len)
{
  //len == 128 for beacons
  
  int i = 0;
  uint16_t seq_n_new = 0;
  
  struct sniffer_buf *sniffer = (struct sniffer_buf*) buf;
  //Is data or QOS?
  if ((sniffer->buf[0] == 0x08) || (sniffer->buf[0] == 0x88)) {
    struct clientinfo ci = parse_data(sniffer->buf, 36, sniffer->rx_ctrl.rssi, sniffer->rx_ctrl.channel);
//    if (memcmp(ci.bssid, ci.station, ETH_MAC_LEN)) {
      activity_by(ci.station);
//    }
  }
  
}

extern "C" {
#include "user_interface.h"
}

void setup() {
  pixels.begin();

  wifi_set_opmode(STATION_MODE);            // Promiscuous works only with station mode
  wifi_set_channel(1);
  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(packet_callback);   // Set up promiscuous callback
  wifi_promiscuous_enable(1);
  
  pinMode(BUTTON_PIN, INPUT);
}

int channel = 1;

void loop() {
  int buttonVal = digitalRead(BUTTON_PIN);
  if(buttonVal > 0) {
    channel += 1;
    if(channel > 13) {
      channel = 1;
    }
    wifi_set_channel(channel);
    for(int i = 0; i < 20 ; ++i){
      shine_at(channel * 3);
    }
    delay(1000);
  } else {
    delay(100);
  }
}
