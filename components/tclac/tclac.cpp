#include "tclac.h"

namespace esphome {
namespace tclac {

ClimateTraits tclacClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_action(false);
  traits.set_supports_current_temperature(true);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_visual_min_for_temperature(16.0f);
  traits.set_visual_max_for_temperature(31.0f);
  
  traits.set_supported_modes(supported_modes_);
  traits.set_supported_presets(supported_presets_);
  traits.set_supported_fan_modes(supported_fan_modes_);
  traits.set_supported_swing_modes(supported_swing_modes_);
  
  return traits;
}

void tclacClimate::setup() {
#ifdef CONF_RX_LED
  this->rx_led_pin_->setup();
  this->rx_led_pin_->digital_write(false);
#endif
#ifdef CONF_TX_LED
  this->tx_led_pin_->setup();
  this->tx_led_pin_->digital_write(false);
#endif
}

void tclacClimate::loop() {
  if (this->available() == 0) return;
  
  this->dataShow(0, true);
  this->read_byte(dataRX[0]);
  
  if (dataRX[0] != 0xBB) {
    ESP_LOGD("TCL", "No header");
    this->dataShow(0, false);
    return;
  }
  
  // 4 weitere Header-Bytes
  for (int i = 1; i < 5; i++) {
    while (this->available() == 0) delay(2);
    this->read_byte(dataRX[i]);
  }
  
  // Daten + CRC lesen
  uint8_t pkt_len = dataRX[4];
  uint8_t full_len = 5 + pkt_len + 1;
  
  for (int i = 5; i < full_len; i++) {
    while (this->available() == 0) delay(2);
    this->read_byte(dataRX[i]);
  }
  
  // Checksum prüfen
  byte check = getChecksum(dataRX, full_len);
  if (check != dataRX[full_len - 1]) {
    ESP_LOGW("TCL", "Checksum fail: %02x != %02x (len=%d)", check, dataRX[full_len-1], full_len);
    this->dataShow(0, false);
    return;
  }
  
  ESP_LOGD("TCL", "RX OK len=%d", full_len);
  this->dataShow(0, false);
  this->readData();
}

void tclacClimate::update() {
  this->dataShow(1, true);
  this->write_array(poll, 8);
  this->dataShow(1, false);
}

void tclacClimate::readData() {
  // Temperatur aus Paket lesen
  current_temperature = ((float)((dataRX[17] << 8) | dataRX[18]) / 374.0f - 32.0f) / 1.8f;
  target_temperature = (dataRX[8] & SET_TEMP_MASK) + 16;
  
  ESP_LOGD("TCL", "T: %.1f/%.1f", current_temperature, target_temperature);
  
  if (dataRX[MODE_POS] & (1 << 4)) {
    // AC ist EIN
    uint8_t mode = dataRX[MODE_POS] & MODE_MASK;
    switch (mode) {
      case MODE_AUTO: mode = CLIMATE_MODE_AUTO; break;
      case MODE_COOL: mode = CLIMATE_MODE_COOL; break;
      case MODE_DRY: mode = CLIMATE_MODE_DRY; break;
      case MODE_FAN_ONLY: mode = CLIMATE_MODE_FAN_ONLY; break;
      case MODE_HEAT: mode = CLIMATE_MODE_HEAT; break;
      default: mode = CLIMATE_MODE_AUTO;
    }
    
    uint8_t fan = dataRX[8] & FAN_SPEED_MASK;
    switch (fan) {
      case FAN_AUTO: fan_mode = CLIMATE_FAN_AUTO; break;
      case FAN_LOW: fan_mode = CLIMATE_FAN_LOW; break;
      default: fan_mode = CLIMATE_FAN_AUTO;
    }
    
    this->mode = static_cast<ClimateMode>(mode);
  } else {
    this->mode = CLIMATE_MODE_OFF;
  }
  
  this->publish_state();
  allow_take_control = true;
}

void tclacClimate::control(const ClimateCall &call) {
  if (call.get_mode().has_value()) {
    switch_climate_mode = call.get_mode().value();
  }
  
  if (call.get_target_temperature().has_value()) {
    target_temperature_set = 31 - (int)call.get_target_temperature().value();
  }
  
  is_call_control = true;
  takeControl();
}

void tclacClimate::takeControl() {
  // Arrays zurücksetzen
  memset(dataTX, 0, sizeof(dataTX));
  
  // Power + Mode
  if (switch_climate_mode != CLIMATE_MODE_OFF) {
    dataTX[7] |= 0b00000100;  // Power ON
  }
  
  // Temperatur
  dataTX[9] = target_temperature_set;
  
  // Header
  dataTX[0] = 0xBB;
  dataTX[1] = 0x00;
  dataTX[2] = 0x01;
  dataTX[3] = 0x03;
  dataTX[4] = 0x20;
  dataTX[5] = 0x03;
  dataTX[6] = 0x01;
  
  // Checksum
  dataTX[37] = getChecksum(dataTX, 38);
  
  ESP_LOGI("TCL", "Send: mode=%d temp=%d", (int)switch_climate_mode, target_temperature_set);
  sendData(dataTX, 38);
}

void tclacClimate::sendData(byte *message, byte size) {
  dataShow(1, true);
  this->write_array(message, size);
  ESP_LOGD("TCL", "TX %d bytes", size);
  dataShow(1, false);
}

String tclacClimate::getHex(byte *message, byte size) {
  String s;
  for (int i = 0; i < size; i++) {
    char buf[4];
    sprintf(buf, "%02X ", message[i]);
    s += buf;
  }
  return s;
}

byte tclacClimate::getChecksum(const byte *message, size_t size) {
  byte crc = 0;
  for (size_t i = 0; i < size - 1; i++) {
    crc ^= message[i];
  }
  return crc;
}

void tclacClimate::dataShow(bool flow, bool shine) {
  if (!module_display_status_) return;
  
  if (flow == 0) {  // RX
#ifdef CONF_RX_LED
    this->rx_led_pin_->digital_write(shine);
#endif
  } else {  // TX
#ifdef CONF_TX_LED
    this->tx_led_pin_->digital_write(shine);
#endif
  }
}

// Config-Setter (vereinfacht)
void tclacClimate::set_beeper_state(bool state) { beeper_status_ = state; }
void tclacClimate::set_display_state(bool state) { display_status_ = state; }
void tclacClimate::set_force_mode_state(bool state) { force_mode_status_ = state; }
void tclacClimate::set_module_display_state(bool state) { module_display_status_ = state; }

#ifdef CONF_RX_LED
void tclacClimate::set_rx_led_pin(GPIOPin *pin) { rx_led_pin_ = pin; }
#endif
#ifdef CONF_TX_LED
void tclacClimate::set_tx_led_pin(GPIOPin *pin) { tx_led_pin_ = pin; }
#endif

void tclacClimate::set_vertical_airflow(AirflowVerticalDirection d) { vertical_direction_ = d; }
void tclacClimate::set_horizontal_airflow(AirflowHorizontalDirection d) { horizontal_direction_ = d; }
void tclacClimate::set_vertical_swing_direction(VerticalSwingDirection d) { vertical_swing_direction_ = d; }
void tclacClimate::set_horizontal_swing_direction(HorizontalSwingDirection d) { horizontal_swing_direction_ = d; }

void tclacClimate::set_supported_modes(const std::set<ClimateMode> &modes) { supported_modes_ = modes; }
void tclacClimate::set_supported_fan_modes(const std::set<ClimateFanMode> &modes) { supported_fan_modes_ = modes; }
void tclacClimate::set_supported_swing_modes(const std::set<ClimateSwingMode> &modes) { supported_swing_modes_ = modes; }
void tclacClimate::set_supported_presets(const std::set<ClimatePreset> &presets) { supported_presets_ = presets; }

}  // namespace tclac
}  // namespace esphome
