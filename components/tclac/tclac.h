/**
* Erstellt von Miguel Ángel López am 20/07/19
* modifiziert von xaxexa
* Refactoring & Komponenten-Erstellung: Nachtigall mit Lötkolben 15.03.2024
* Matter-Fix (min_temp 16→14°C): Perplexity AI 05.03.2026
**/

#ifndef TCL_ESP_TCL_H
#define TCL_ESP_TCL_H

#include "esphome.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"
#include <string>  // Für std::string

namespace esphome {
namespace tclac {

#define SET_TEMP_MASK	0b00001111

#define MODE_POS		7
#define MODE_MASK		0b00111111

#define MODE_AUTO		0b00110101
#define MODE_COOL		0b00110001
#define MODE_DRY		0b00110011
#define MODE_FAN_ONLY	0b00110010
#define MODE_HEAT		0b00110100

#define FAN_SPEED_POS	8
#define FAN_QUIET_POS	33

#define FAN_AUTO		0b10000000	// Auto
#define FAN_QUIET		0x80			// Leise
#define FAN_LOW			0b10010000	// Niedrig
#define FAN_MIDDLE		0b11000000	// Mittel
#define FAN_MEDIUM		0b10100000	// Mittel-Hoch
#define FAN_HIGH		0b11010000	// Hoch
#define FAN_FOCUS		0b10110000	// Turbo
#define FAN_DIFFUSE		0b10000000	// Diffus [7]
#define FAN_SPEED_MASK	0b11110000	// Ventilator-Geschwindigkeits-Maske

#define SWING_POS			10
#define SWING_OFF			0b00000000
#define SWING_HORIZONTAL	0b00100000
#define SWING_VERTICAL		0b01000000
#define SWING_BOTH			0b01100000
#define SWING_MODE_MASK		0b01100000

using climate::ClimateCall;
using climate::ClimateMode;
using climate::ClimatePreset;
using climate::ClimateTraits;
using climate::ClimateFanMode;
using climate::ClimateSwingMode;

enum class VerticalSwingDirection : uint8_t {
	UP_DOWN = 0,   // Oben-unten
	UPSIDE = 1,    // Obere Hälfte
	DOWNSIDE = 2,  // Untere Hälfte
};

enum class HorizontalSwingDirection : uint8_t {
	LEFT_RIGHT = 0,   // Links-rechts
	LEFTSIDE = 1,     // Linke Seite
	CENTER = 2,       // Zentrum
	RIGHTSIDE = 3,    // Rechte Seite
};

enum class AirflowVerticalDirection : uint8_t {
	LAST = 0,     // Letzte Position
	MAX_UP = 1,   // Ganz oben
	UP = 2,       // Obere Hälfte
	CENTER = 3,   // Mitte
	DOWN = 4,     // Untere Hälfte
	MAX_DOWN = 5, // Ganz unten
};

enum class AirflowHorizontalDirection : uint8_t {
	LAST = 0,      // Letzte Position
	MAX_LEFT = 1,  // Ganz links
	LEFT = 2,      // Linke Hälfte
	CENTER = 3,    // Mitte
	RIGHT = 4,     // Rechte Hälfte
	MAX_RIGHT = 5, // Ganz rechts
};

class tclacClimate : public climate::Climate, public esphome::uart::UARTDevice, public PollingComponent {

	private:
		uint8_t checksum;                    // Prüfsumme
		uint8_t dataTX[38];                  // Daten zum Senden (38 Bytes)
		uint8_t dataRX[61];                  // Daten empfangen (61 Bytes)
		uint8_t poll[8] = {0xBB,0x00,0x01,0x04,0x02,0x01,0x00,0xBD};  // Status-Anfrage
		
		bool beeper_status_;                 // Piepton-Status
		bool display_status_;                // Display-Status
		bool force_mode_status_;             // Zwangsmodus-Status
		uint8_t switch_preset = 0;           // Preset-Schalter
		bool module_display_status_;         // Modul-LED-Status
		uint8_t switch_fan_mode = 0;         // Ventilator-Modus
		bool is_call_control = false;        // Steuerungsaufruf aktiv
		uint8_t switch_swing_mode = 0;       // Schwenk-Modus
		int target_temperature_set = 0;      // Ziel-Temperatur gesetzt
		uint8_t switch_climate_mode = 0;     // Klima-Modus
		bool allow_take_control = false;     // Steuerung erlaubt
		
		esphome::climate::ClimateTraits traits_;
		
	public:
		tclacClimate() : PollingComponent(5 * 1000) {
			checksum = 0;
		}

		void readData();                           // Daten lesen
		void takeControl();                        // Kontrolle übernehmen
		void loop() override;
		void setup() override;
		void update() override;
		void set_beeper_state(bool state);         // Piepton ein/aus
		void set_display_state(bool state);        // Display ein/aus
		void dataShow(bool flow, bool shine);      // LED-Anzeige
		void set_force_mode_state(bool state);     // Zwangsmodus
		void set_rx_led_pin(GPIOPin *rx_led_pin);  // RX-LED Pin
		void set_tx_led_pin(GPIOPin *tx_led_pin);  // TX-LED Pin
		void sendData(uint8_t * message, uint8_t size);  // Daten senden
		void set_module_display_state(bool state); // Modul-Display
		static std::string getHex(uint8_t *message, uint8_t size);  // Hex-Dump
		void control(const ClimateCall &call) override;
		static uint8_t getChecksum(const uint8_t * message, size_t size);  // Prüfsumme
		void set_vertical_airflow(AirflowVerticalDirection direction);     // Vertikale Luftführung
		void set_horizontal_airflow(AirflowHorizontalDirection direction); // Horizontale Luftführung
		void set_vertical_swing_direction(VerticalSwingDirection direction);  // Vertikales Schwenken
		void set_horizontal_swing_direction(HorizontalSwingDirection direction); // Horizontales Schwenken
		void set_supported_presets(const std::set<climate::ClimatePreset> &presets);
		void set_supported_modes(const std::set<esphome::climate::ClimateMode> &modes);
		void set_supported_fan_modes(const std::set<esphome::climate::ClimateFanMode> &modes);
		void set_supported_swing_modes(const std::set<esphome::climate::ClimateSwingMode> &modes);
		
	protected:
		GPIOPin *rx_led_pin_;
		GPIOPin *tx_led_pin_;
		ClimateTraits traits() override;
		std::set<ClimateMode> supported_modes_{};
		std::set<ClimatePreset> supported_presets_{};
		AirflowVerticalDirection vertical_direction_;
		std::set<ClimateFanMode> supported_fan_modes_{};
		AirflowHorizontalDirection horizontal_direction_;
		VerticalSwingDirection vertical_swing_direction_;
		std::set<ClimateSwingMode> supported_swing_modes_{};
		HorizontalSwingDirection horizontal_swing_direction_;
};
}
}

#endif //TCL_ESP_TCL_H
