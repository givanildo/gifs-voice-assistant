#include "waveshare_sd_card.h"
#include "esphome/core/log.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h" // Alterado de sdmmc_host.h
#include "driver/spi_common.h" // Necessário para o barramento SPI
#include "sdmmc_cmd.h"         // Mantido para a estrutura do cartão
#include "ff.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>

namespace esphome {
namespace waveshare_sd_card {

static const char *const TAG = "waveshare_sd";

void WaveshareSdCard::setup() {
  ESP_LOGCONFIG(TAG, "Configurando o cartão SD (modo SPI)...");

  if (this->cs_pin_ != nullptr) {
    this->cs_pin_->setup();
    this->cs_pin_->digital_write(true);
    vTaskDelay(pdMS_TO_TICKS(100));
    this->cs_pin_->digital_write(false);
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  // --- Início da nova configuração do driver SPI ---
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  
  // ****** ESTA É A ALTERAÇÃO CRUCIAL ******
  // O display provavelmente está a usar SPI2_HOST. Vamos usar SPI3_HOST para evitar conflitos.
  spi_host_device_t spi_bus = SPI3_HOST;
  host.slot = spi_bus;

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = this->cmd_pin_,   // CMD é o nosso MOSI
      .miso_io_num = this->data0_pin_, // D0 é o nosso MISO
      .sclk_io_num = this->clk_pin_,   // CLK é o nosso SCLK
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4096,
  };

  esp_err_t ret = spi_bus_initialize(spi_bus, &bus_cfg, SDSPI_DEFAULT_DMA);
  if (ret != ESP_OK) {
    // Se o barramento já estiver inicializado, não há problema, podemos tentar usá-lo.
    if (ret != ESP_ERR_INVALID_STATE) {
      ESP_LOGE(TAG, "Falha ao inicializar o barramento SPI (%s)", esp_err_to_name(ret));
      this->mark_failed();
      return;
    }
    ESP_LOGW(TAG, "O barramento SPI %d já estava inicializado. A tentar reutilizá-lo.", spi_bus);
  }

  sdspi_device_config_t device_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
  device_cfg.gpio_cs = GPIO_NUM_NC; // O nosso CS é controlado externamente pelo PCA9554
  device_cfg.host_id = spi_bus;
  // --- Fim da nova configuração do driver SPI ---

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024
  };
  
  // Usa a função de montagem específica para SPI
  ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &device_cfg, &mount_config, &this->card_);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao inicializar o cartão em modo SPI (%s).", esp_err_to_name(ret));
    spi_bus_free(spi_bus);
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Cartão SD montado com sucesso em /sdcard (modo SPI)!");
  sdmmc_card_print_info(stdout, this->card_);
  this->update_sensors_();
}

// ... (o resto do ficheiro permanece o mesmo) ...
void WaveshareSdCard::loop() {
  if (millis() - this->last_update_ > 30000) {
    this->last_update_ = millis();
    this->update_sensors_();
  }
}

void WaveshareSdCard::update_sensors_() {
  if (this->is_failed() || this->card_ == nullptr)
    return;

  FATFS *fs;
  DWORD fre_clust;
  if (f_getfree("0:", &fre_clust, &fs) != FR_OK) {
    ESP_LOGW(TAG, "Falha ao obter informações de espaço do cartão SD.");
    return;
  }

  uint64_t total_bytes = (uint64_t)(fs->n_fatent - 2) * fs->csize * this->card_->csd.sector_size;
  uint64_t free_bytes = (uint64_t)fre_clust * fs->csize * this->card_->csd.sector_size;
  uint64_t used_bytes = total_bytes - free_bytes;

  if (this->total_space_sensor_ != nullptr)
    this->total_space_sensor_->publish_state(total_bytes);
  if (this->used_space_sensor_ != nullptr)
    this->used_space_sensor_->publish_state(used_bytes);
  if (this->free_space_sensor_ != nullptr)
    this->free_space_sensor_->publish_state(free_bytes);
}

bool WaveshareSdCard::is_directory(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    return S_ISDIR(st.st_mode);
  }
  return false;
}

std::vector<FileInfo> WaveshareSdCard::list_files(const char *path) {
  std::vector<FileInfo> files;
  DIR *dir = opendir(path);
  if (dir == nullptr) {
    ESP_LOGE(TAG, "Falha ao abrir diretório %s", path);
    return files;
  }

  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_name[0] == '.') {
      continue;
    }

    FileInfo info;
    info.name = entry->d_name;
    info.is_directory = (entry->d_type == DT_DIR);
    
    if (!info.is_directory) {
      std::string full_path = std::string(path) + "/" + info.name;
      struct stat st;
      if (stat(full_path.c_str(), &st) == 0) {
        info.size = st.st_size;
      } else {
        info.size = 0;
      }
    } else {
        info.size = 0;
    }
    
    files.push_back(info);
  }

  closedir(dir);
  return files;
}

void WaveshareSdCard::dump_config() {
  ESP_LOGCONFIG(TAG, "Componente SD Card Genérico");
  ESP_LOGCONFIG(TAG, "  CLK Pin: %u", this->clk_pin_);
  ESP_LOGCONFIG(TAG, "  CMD Pin: %u", this->cmd_pin_);
  ESP_LOGCONFIG(TAG, "  D0 Pin: %u", this->data0_pin_);
  LOG_PIN("  CS Pin: ", this->cs_pin_);
  LOG_SENSOR("  ", "Total Space Sensor", this->total_space_sensor_);
  LOG_SENSOR("  ", "Used Space Sensor", this->used_space_sensor_);
  LOG_SENSOR("  ", "Free Space Sensor", this->free_space_sensor_);
}

bool WaveshareSdCard::write_file(const char *path, const uint8_t *data, size_t len) {
  FILE *f = fopen(path, "w");
  if (f == nullptr) {
    ESP_LOGE(TAG, "Falha ao abrir arquivo %s para escrita", path);
    return false;
  }
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len;
}

bool WaveshareSdCard::append_file(const char *path, const uint8_t *data, size_t len) {
  FILE *f = fopen(path, "a");
  if (f == nullptr) {
    ESP_LOGE(TAG, "Falha ao abrir arquivo %s para anexar", path);
    return false;
  }
  size_t written = fwrite(data, 1, len, f);
  fclose(f);
  return written == len;
}

}  // namespace waveshare_sd_card
}  // namespace esphome
