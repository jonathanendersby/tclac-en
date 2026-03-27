#include "esphome.h"
#include "esphome/core/defines.h"
#include "tclac.h"

namespace esphome{
namespace tclac{


ClimateTraits tclacClimate::traits() {
	auto traits = climate::ClimateTraits();

	traits.set_supports_current_temperature(true);

	for (auto mode : this->supported_modes_)
		traits.add_supported_mode(mode);
	for (auto preset : this->supported_presets_)
		traits.add_supported_preset(preset);
	for (auto fan_mode : this->supported_fan_modes_)
		traits.add_supported_fan_mode(fan_mode);
	for (auto swing_mode : this->supported_swing_modes_)
		traits.add_supported_swing_mode(swing_mode);

	traits.add_supported_mode(climate::CLIMATE_MODE_OFF);			// Off mode always available
	traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);			// Auto mode always available
	traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);		// Auto fan mode always available
	traits.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);	// Swing off always available
	traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);// No preset as safety default


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

void tclacClimate::loop()  {
	// If there is data in the UART buffer, read it
	if (esphome::uart::UARTDevice::available() > 0) {
		dataShow(0, true);
		dataRX[0] = esphome::uart::UARTDevice::read();
		// If the received byte is not the header (0xBB), just return
		if (dataRX[0] != 0xBB) {
			ESP_LOGD("TCL", "Wrong byte");
			dataShow(0,0);
			return;
		}
		// If the header (0xBB) matches, read the next 4 bytes
		delay(5);
		dataRX[1] = esphome::uart::UARTDevice::read();
		delay(5);
		dataRX[2] = esphome::uart::UARTDevice::read();
		delay(5);
		dataRX[3] = esphome::uart::UARTDevice::read();
		delay(5);
		dataRX[4] = esphome::uart::UARTDevice::read();

		// The 5th byte contains the message length
		esphome::uart::UARTDevice::read_array(dataRX+5, dataRX[4]+1);

		byte check = getChecksum(dataRX, sizeof(dataRX));

		// Verify checksum
		if (check != dataRX[60]) {
			ESP_LOGD("TCL", "Invalid checksum %x", check);
			tclacClimate::dataShow(0,0);
			return;
		} else {
			//ESP_LOGD("TCL", "checksum OK %x", check);
		}
		tclacClimate::dataShow(0,0);
		// After reading the buffer, parse the data
		tclacClimate::readData();
	}
}

void tclacClimate::update() {
	tclacClimate::dataShow(1,1);
	this->esphome::uart::UARTDevice::write_array(poll, sizeof(poll));
	tclacClimate::dataShow(1,0);
}

void tclacClimate::readData() {

	current_temperature = float((( (dataRX[17] << 8) | dataRX[18] ) / 374 - 32)/1.8);
	target_temperature = (dataRX[FAN_SPEED_POS] & SET_TEMP_MASK) + 16;

	if (dataRX[MODE_POS] & ( 1 << 4)) {
		// AC is on, parse data for display
		uint8_t modeswitch = MODE_MASK & dataRX[MODE_POS];
		uint8_t fanspeedswitch = FAN_SPEED_MASK & dataRX[FAN_SPEED_POS];
		uint8_t swingmodeswitch = SWING_MODE_MASK & dataRX[SWING_POS];

		switch (modeswitch) {
			case MODE_AUTO:
				mode = climate::CLIMATE_MODE_AUTO;
				break;
			case MODE_COOL:
				mode = climate::CLIMATE_MODE_COOL;
				break;
			case MODE_DRY:
				mode = climate::CLIMATE_MODE_DRY;
				break;
			case MODE_FAN_ONLY:
				mode = climate::CLIMATE_MODE_FAN_ONLY;
				break;
			case MODE_HEAT:
				mode = climate::CLIMATE_MODE_HEAT;
				break;
			default:
				mode = climate::CLIMATE_MODE_AUTO;
		}

		if ( dataRX[FAN_QUIET_POS] & FAN_QUIET) {
			fan_mode = climate::CLIMATE_FAN_QUIET;
		} else if (dataRX[MODE_POS] & FAN_DIFFUSE){
			fan_mode = climate::CLIMATE_FAN_DIFFUSE;
		} else {
			switch (fanspeedswitch) {
				case FAN_AUTO:
					fan_mode = climate::CLIMATE_FAN_AUTO;
					break;
				case FAN_LOW:
					fan_mode = climate::CLIMATE_FAN_LOW;
					break;
				case FAN_MIDDLE:
					fan_mode = climate::CLIMATE_FAN_MIDDLE;
					break;
				case FAN_MEDIUM:
					fan_mode = climate::CLIMATE_FAN_MEDIUM;
					break;
				case FAN_HIGH:
					fan_mode = climate::CLIMATE_FAN_HIGH;
					break;
				case FAN_FOCUS:
					fan_mode = climate::CLIMATE_FAN_FOCUS;
					break;
				default:
					fan_mode = climate::CLIMATE_FAN_AUTO;
			}
		}

		switch (swingmodeswitch) {
			case SWING_OFF:
				swing_mode = climate::CLIMATE_SWING_OFF;
				break;
			case SWING_HORIZONTAL:
				swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
				break;
			case SWING_VERTICAL:
				swing_mode = climate::CLIMATE_SWING_VERTICAL;
				break;
			case SWING_BOTH:
				swing_mode = climate::CLIMATE_SWING_BOTH;
				break;
		}

		// Parse preset data
		preset = ClimatePreset::CLIMATE_PRESET_NONE;
		if (dataRX[7] & (1 << 6)){
			preset = ClimatePreset::CLIMATE_PRESET_ECO;
		} else if (dataRX[9] & (1 << 2)){
			preset = ClimatePreset::CLIMATE_PRESET_COMFORT;
		} else if (dataRX[19] & (1 << 0)){
			preset = ClimatePreset::CLIMATE_PRESET_SLEEP;
		}

	} else {
		// AC is off, show all modes as off
		mode = climate::CLIMATE_MODE_OFF;
		swing_mode = climate::CLIMATE_SWING_OFF;
		preset = ClimatePreset::CLIMATE_PRESET_NONE;
	}
	// Publish state
	this->publish_state();
	allow_take_control = true;
   }

// Climate control
void tclacClimate::control(const ClimateCall &call) {
	// Get data from the climate mode selector
	if (call.get_mode().has_value()){
		switch_climate_mode = call.get_mode().value();
		ESP_LOGD("TCL", "Get MODE from call");
	} else {
		switch_climate_mode = mode;
		ESP_LOGD("TCL", "Get MODE from AC");
	}

	// Get data from the preset selector
	if (call.get_preset().has_value()){
		switch_preset = call.get_preset().value();
	} else {
		switch_preset = preset.value();
	}

	// Get data from the fan mode selector
	if (call.get_fan_mode().has_value()){
		switch_fan_mode = call.get_fan_mode().value();
	} else {
		switch_fan_mode = fan_mode.value();
	}

	// Get data from the swing mode selector
	if (call.get_swing_mode().has_value()){
		switch_swing_mode = call.get_swing_mode().value();
	} else {
		// If empty, use last polled value (nothing changed)
		switch_swing_mode = swing_mode;
	}

	// Calculate temperature
	if (call.get_target_temperature().has_value()) {
		target_temperature_set = 31-(int)call.get_target_temperature().value();
	} else {
		target_temperature_set = 31-(int)target_temperature;
	}

	is_call_control = true;
	takeControl();
	allow_take_control = true;
}


void tclacClimate::takeControl() {

	dataTX[7]  = 0b00000000;
	dataTX[8]  = 0b00000000;
	dataTX[9]  = 0b00000000;
	dataTX[10] = 0b00000000;
	dataTX[11] = 0b00000000;
	dataTX[19] = 0b00000000;
	dataTX[32] = 0b00000000;
	dataTX[33] = 0b00000000;

	if (is_call_control != true){
		ESP_LOGD("TCL", "Get MODE from AC for force config");
		switch_climate_mode = mode;
		switch_preset = preset.value();
		switch_fan_mode = fan_mode.value();
		switch_swing_mode = swing_mode;
		target_temperature_set = 31-(int)target_temperature;
	}

	// Enable or disable beeper based on settings switch
	if (beeper_status_){
		ESP_LOGD("TCL", "Beep mode ON");
		dataTX[7] += 0b00100000;
	} else {
		ESP_LOGD("TCL", "Beep mode OFF");
		dataTX[7] += 0b00000000;
	}

	// Enable or disable display on the AC unit based on settings switch
	// Only enable display if AC is in an active mode
	// WARNING: When the display is turned off, the AC forces itself into auto mode!
	if ((display_status_) && (switch_climate_mode != climate::CLIMATE_MODE_OFF)){
		ESP_LOGD("TCL", "Dispaly turn ON");
		dataTX[7] += 0b01000000;
	} else {
		ESP_LOGD("TCL", "Dispaly turn OFF");
		dataTX[7] += 0b00000000;
	}

	// Set AC operating mode
	switch (switch_climate_mode) {
		case climate::CLIMATE_MODE_OFF:
			dataTX[7] += 0b00000000;
			dataTX[8] += 0b00000000;
			break;
		case climate::CLIMATE_MODE_AUTO:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00001000;
			break;
		case climate::CLIMATE_MODE_COOL:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000011;
			break;
		case climate::CLIMATE_MODE_DRY:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000010;
			break;
		case climate::CLIMATE_MODE_FAN_ONLY:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000111;
			break;
		case climate::CLIMATE_MODE_HEAT:
			dataTX[7] += 0b00000100;
			dataTX[8] += 0b00000001;
			break;
	}

	// Set fan mode
	switch(switch_fan_mode) {
		case climate::CLIMATE_FAN_AUTO:
			dataTX[8]	+= 0b00000000;
			dataTX[10]	+= 0b00000000;
			break;
		case climate::CLIMATE_FAN_QUIET:
			dataTX[8]	+= 0b10000000;
			dataTX[10]	+= 0b00000000;
			break;
		case climate::CLIMATE_FAN_LOW:
			dataTX[8]	+= 0b00000000;
			dataTX[10]	+= 0b00000001;
			break;
		case climate::CLIMATE_FAN_MIDDLE:
			dataTX[8]	+= 0b00000000;
			dataTX[10]	+= 0b00000110;
			break;
		case climate::CLIMATE_FAN_MEDIUM:
			dataTX[8]	+= 0b00000000;
			dataTX[10]	+= 0b00000011;
			break;
		case climate::CLIMATE_FAN_HIGH:
			dataTX[8]	+= 0b00000000;
			dataTX[10]	+= 0b00000111;
			break;
		case climate::CLIMATE_FAN_FOCUS:
			dataTX[8]	+= 0b00000000;
			dataTX[10]	+= 0b00000101;
			break;
		case climate::CLIMATE_FAN_DIFFUSE:
			dataTX[8]	+= 0b01000000;
			dataTX[10]	+= 0b00000000;
			break;
	}

	// Set swing mode
	switch(switch_swing_mode) {
		case climate::CLIMATE_SWING_OFF:
			dataTX[10]	+= 0b00000000;
			dataTX[11]	+= 0b00000000;
			break;
		case climate::CLIMATE_SWING_VERTICAL:
			dataTX[10]	+= 0b00111000;
			dataTX[11]	+= 0b00000000;
			break;
		case climate::CLIMATE_SWING_HORIZONTAL:
			dataTX[10]	+= 0b00000000;
			dataTX[11]	+= 0b00001000;
			break;
		case climate::CLIMATE_SWING_BOTH:
			dataTX[10]	+= 0b00111000;
			dataTX[11]	+= 0b00001000;
			break;
	}

	// Set presets
	switch(switch_preset) {
		case ClimatePreset::CLIMATE_PRESET_NONE:
			break;
		case ClimatePreset::CLIMATE_PRESET_ECO:
			dataTX[7]	+= 0b10000000;
			break;
		case ClimatePreset::CLIMATE_PRESET_SLEEP:
			dataTX[19]	+= 0b00000001;
			break;
		case ClimatePreset::CLIMATE_PRESET_COMFORT:
			dataTX[8]	+= 0b00010000;
			break;
	}

// LOUVER MODES
//	Vertical louver
//		Vertical louver swing [byte 10, mask 00111000]:
//			000 - swing OFF, louver at last position or fixed
//			111 - swing ON in selected mode
//		Vertical swing mode (fix mode irrelevant when swing ON) [byte 32, mask 00011000]:
//			01 - top to bottom, DEFAULT
//			10 - upper half
//			11 - lower half
//		Vertical fix mode (swing mode irrelevant when swing OFF) [byte 32, mask 00000111]:
//			000 - no fixing, DEFAULT
//			001 - max up
//			010 - between up and center
//			011 - center
//			100 - between center and down
//			101 - max down
//	Horizontal louvers
//		Horizontal louver swing [byte 11, mask 00001000]:
//			0 - swing OFF, louvers at last position or fixed
//			1 - swing ON in selected mode
//		Horizontal swing mode (fix mode irrelevant when swing ON) [byte 33, mask 00111000]:
//			001 - left to right, DEFAULT
//			010 - left
//			011 - center
//			100 - right
//		Horizontal fix mode (swing mode irrelevant when swing OFF) [byte 33, mask 00000111]:
//			000 - no fixing, DEFAULT
//			001 - max left
//			010 - between left and center
//			011 - center
//			100 - between center and right
//			101 - max right


	// Set vertical louver swing mode
	switch(vertical_swing_direction_) {
		case VerticalSwingDirection::UP_DOWN:
			dataTX[32]	+= 0b00001000;
			ESP_LOGD("TCL", "Vertical swing: up-down");
			break;
		case VerticalSwingDirection::UPSIDE:
			dataTX[32]	+= 0b00010000;
			ESP_LOGD("TCL", "Vertical swing: upper");
			break;
		case VerticalSwingDirection::DOWNSIDE:
			dataTX[32]	+= 0b00011000;
			ESP_LOGD("TCL", "Vertical swing: downer");
			break;
	}
	// Set horizontal louver swing mode
	switch(horizontal_swing_direction_) {
		case HorizontalSwingDirection::LEFT_RIGHT:
			dataTX[33]	+= 0b00001000;
			ESP_LOGD("TCL", "Horizontal swing: left-right");
			break;
		case HorizontalSwingDirection::LEFTSIDE:
			dataTX[33]	+= 0b00010000;
			ESP_LOGD("TCL", "Horizontal swing: lefter");
			break;
		case HorizontalSwingDirection::CENTER:
			dataTX[33]	+= 0b00011000;
			ESP_LOGD("TCL", "Horizontal swing: center");
			break;
		case HorizontalSwingDirection::RIGHTSIDE:
			dataTX[33]	+= 0b00100000;
			ESP_LOGD("TCL", "Horizontal swing: righter");
			break;
	}
	// Set vertical louver fixed position
	switch(vertical_direction_) {
		case AirflowVerticalDirection::LAST:
			dataTX[32]	+= 0b00000000;
			ESP_LOGD("TCL", "Vertical fix: last position");
			break;
		case AirflowVerticalDirection::MAX_UP:
			dataTX[32]	+= 0b00000001;
			ESP_LOGD("TCL", "Vertical fix: up");
			break;
		case AirflowVerticalDirection::UP:
			dataTX[32]	+= 0b00000010;
			ESP_LOGD("TCL", "Vertical fix: upper");
			break;
		case AirflowVerticalDirection::CENTER:
			dataTX[32]	+= 0b00000011;
			ESP_LOGD("TCL", "Vertical fix: center");
			break;
		case AirflowVerticalDirection::DOWN:
			dataTX[32]	+= 0b00000100;
			ESP_LOGD("TCL", "Vertical fix: downer");
			break;
		case AirflowVerticalDirection::MAX_DOWN:
			dataTX[32]	+= 0b00000101;
			ESP_LOGD("TCL", "Vertical fix: down");
			break;
	}
	// Set horizontal louver fixed position
	switch(horizontal_direction_) {
		case AirflowHorizontalDirection::LAST:
			dataTX[33]	+= 0b00000000;
			ESP_LOGD("TCL", "Horizontal fix: last position");
			break;
		case AirflowHorizontalDirection::MAX_LEFT:
			dataTX[33]	+= 0b00000001;
			ESP_LOGD("TCL", "Horizontal fix: left");
			break;
		case AirflowHorizontalDirection::LEFT:
			dataTX[33]	+= 0b00000010;
			ESP_LOGD("TCL", "Horizontal fix: lefter");
			break;
		case AirflowHorizontalDirection::CENTER:
			dataTX[33]	+= 0b00000011;
			ESP_LOGD("TCL", "Horizontal fix: center");
			break;
		case AirflowHorizontalDirection::RIGHT:
			dataTX[33]	+= 0b00000100;
			ESP_LOGD("TCL", "Horizontal fix: righter");
			break;
		case AirflowHorizontalDirection::MAX_RIGHT:
			dataTX[33]	+= 0b00000101;
			ESP_LOGD("TCL", "Horizontal fix: right");
			break;
	}

	// Set temperature
	dataTX[9] = target_temperature_set;

	// Build byte array for sending to AC
	dataTX[0] = 0xBB;	// header start byte
	dataTX[1] = 0x00;	// header
	dataTX[2] = 0x01;	// header
	dataTX[3] = 0x03;	// 0x03 = control, 0x04 = poll
	dataTX[4] = 0x20;	// 0x20 = control, 0x19 = poll
	dataTX[5] = 0x03;	// ??
	dataTX[6] = 0x01;	// ??
	//dataTX[7]  = eco,display,beep,ontimerenable,offtimerenable,power,0,0
	//dataTX[8]  = mute,0,turbo,health,mode(4)
	//dataTX[9]  = 0,0,0,0,temp(4) settemp = 31 - x
	//dataTX[10] = 0,timerindicator,swingv(3),fan(3)
	//dataTX[11] = 0,offtimer(6),0
	dataTX[12] = 0x00;	// fahrenheit,ontimer(6),0  cf 80=f 0=c
	dataTX[13] = 0x01;	// ??
	dataTX[14] = 0x00;	// 0,0,halfdegree,0,0,0,0,0
	dataTX[15] = 0x00;	// ??
	dataTX[16] = 0x00;	// ??
	dataTX[17] = 0x00;	// ??
	dataTX[18] = 0x00;	// ??
	//dataTX[19] = sleep on=1 off=0
	dataTX[20] = 0x00;	// ??
	dataTX[21] = 0x00;	// ??
	dataTX[22] = 0x00;	// ??
	dataTX[23] = 0x00;	// ??
	dataTX[24] = 0x00;	// ??
	dataTX[25] = 0x00;	// ??
	dataTX[26] = 0x00;	// ??
	dataTX[27] = 0x00;	// ??
	dataTX[28] = 0x00;	// ??
	dataTX[30] = 0x00;	// ??
	dataTX[31] = 0x00;	// ??
	//dataTX[32] = 0,0,0,vertical_swing_mode(2),vertical_fix_mode(3)
	//dataTX[33] = 0,0,horizontal_swing_mode(3),horizontal_fix_mode(3)
	dataTX[34] = 0x00;	// ??
	dataTX[35] = 0x00;	// ??
	dataTX[36] = 0x00;	// ??
	dataTX[37] = 0xFF;	// checksum
	dataTX[37] = tclacClimate::getChecksum(dataTX, sizeof(dataTX));

	tclacClimate::sendData(dataTX, sizeof(dataTX));
	allow_take_control = false;
	is_call_control = false;
}

// Send data to AC
void tclacClimate::sendData(byte * message, byte size) {
	tclacClimate::dataShow(1,1);
	this->esphome::uart::UARTDevice::write_array(message, size);
	ESP_LOGD("TCL", "Message to TCL sended...");
	tclacClimate::dataShow(1,0);
}

// Convert byte array to readable hex format
String tclacClimate::getHex(byte *message, byte size) {
	String raw;
	for (int i = 0; i < size; i++) {
		raw += "\n" + String(message[i]);
	}
	raw.toUpperCase();
	return raw;
}

// Calculate checksum
byte tclacClimate::getChecksum(const byte * message, size_t size) {
	byte position = size - 1;
	byte crc = 0;
	for (int i = 0; i < position; i++)
		crc ^= message[i];
	return crc;
}

// Flash LEDs for data flow indication
void tclacClimate::dataShow(bool flow, bool shine) {
	if (module_display_status_){
		if (flow == 0){
			if (shine == 1){
#ifdef CONF_RX_LED
				this->rx_led_pin_->digital_write(true);
#endif
			} else {
#ifdef CONF_RX_LED
				this->rx_led_pin_->digital_write(false);
#endif
			}
		}
		if (flow == 1) {
			if (shine == 1){
#ifdef CONF_TX_LED
				this->tx_led_pin_->digital_write(true);
#endif
			} else {
#ifdef CONF_TX_LED
				this->tx_led_pin_->digital_write(false);
#endif
			}
		}
	}
}

// Actions with data from configuration

// Get beeper state
void tclacClimate::set_beeper_state(bool state) {
	this->beeper_status_ = state;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Get AC display state
void tclacClimate::set_display_state(bool state) {
	this->display_status_ = state;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Get force config mode state
void tclacClimate::set_force_mode_state(bool state) {
	this->force_mode_status_ = state;
}
// Set RX LED pin
#ifdef CONF_RX_LED
void tclacClimate::set_rx_led_pin(GPIOPin *rx_led_pin) {
	this->rx_led_pin_ = rx_led_pin;
}
#endif
// Set TX LED pin
#ifdef CONF_TX_LED
void tclacClimate::set_tx_led_pin(GPIOPin *tx_led_pin) {
	this->tx_led_pin_ = tx_led_pin;
}
#endif
// Get module LED state
void tclacClimate::set_module_display_state(bool state) {
	this->module_display_status_ = state;
}
// Set vertical louver fixed position
void tclacClimate::set_vertical_airflow(AirflowVerticalDirection direction) {
	this->vertical_direction_ = direction;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Set horizontal louver fixed position
void tclacClimate::set_horizontal_airflow(AirflowHorizontalDirection direction) {
	this->horizontal_direction_ = direction;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Set vertical swing direction
void tclacClimate::set_vertical_swing_direction(VerticalSwingDirection direction) {
	this->vertical_swing_direction_ = direction;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Get supported climate modes
void tclacClimate::set_supported_modes(const std::set<climate::ClimateMode> &modes) {
	this->supported_modes_ = modes;
}
// Set horizontal swing direction
void tclacClimate::set_horizontal_swing_direction(HorizontalSwingDirection direction) {
	horizontal_swing_direction_ = direction;
	if (force_mode_status_){
		if (allow_take_control){
			tclacClimate::takeControl();
		}
	}
}
// Get supported fan speeds
void tclacClimate::set_supported_fan_modes(const std::set<climate::ClimateFanMode> &modes){
	this->supported_fan_modes_ = modes;
}
// Get supported swing modes
void tclacClimate::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
	this->supported_swing_modes_ = modes;
}
// Get supported presets
void tclacClimate::set_supported_presets(const std::set<climate::ClimatePreset> &presets) {
  this->supported_presets_ = presets;
}

}
}
