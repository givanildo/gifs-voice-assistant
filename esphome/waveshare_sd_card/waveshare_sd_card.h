#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include <vector>
#include <string>

namespace esphome {
namespace waveshare_sd_card {

struct FileInfo {
  std::string name;
  bool is_directory;
  uint32_t size;
};

class WaveshareSdCard : public Component {
 public:
  void set_clk_pin(uint8_t pin) { clk_pin_ = pin; }
  void set_cmd_pin(uint8_t pin) { cmd_pin_ = pin; }
  void set_data0_pin(uint8_t pin) { data0_pin_ = pin; }
  void set_cs_pin(GPIOPin *pin) { cs_pin_ = pin; }
  void set_total_space_sensor(sensor::Sensor *s) { total_space_sensor_ = s; }
  void set_used_space_sensor(sensor::Sensor *s) { used_space_sensor_ = s; }
  void set_free_space_sensor(sensor::Sensor *s) { free_space_sensor_ = s; }

  void setup() override;
  void dump_config() override;
  void loop() override;
  
  // A prioridade LATE força este componente a arrancar depois dos outros, como o pca9554.
  float get_setup_priority() const override { return setup_priority::LATE; }

  bool is_directory(const char *path);
  std::vector<FileInfo> list_files(const char *path);
  bool write_file(const char *path, const uint8_t *data, size_t len);
  bool append_file(const char *path, const uint8_t *data, size_t len);

 protected:
  void update_sensors_();

  uint8_t clk_pin_;
  uint8_t cmd_pin_;
  uint8_t data0_pin_;
  GPIOPin *cs_pin_{nullptr};
  sensor::Sensor *total_space_sensor_{nullptr};
  sensor::Sensor *used_space_sensor_{nullptr};
  sensor::Sensor *free_space_sensor_{nullptr};
  sdmmc_card_t *card_{nullptr};
  uint32_t last_update_{0};
};

}  // namespace waveshare_sd_card
}  // namespace esphome
