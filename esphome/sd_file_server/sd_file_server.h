#pragma once

#include "esphome/core/component.h"
#include "esphome/components/network/util.h"
// Inclui o cabeçalho do componente do cartão SD a partir do diretório irmão.
#include "../waveshare_sd_card/waveshare_sd_card.h"
#include "esp_http_server.h"
#include <vector>
#include <string>

namespace esphome {
namespace sd_file_server {

// Classe principal para o servidor de arquivos.
class SDFileServer : public Component {
 public:
  SDFileServer();
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  // Setters para configurar o servidor a partir do YAML.
  void set_url_prefix(const std::string &prefix);
  void set_root_path(const std::string &path);
  void set_sd_card(waveshare_sd_card::WaveshareSdCard *card);
  void set_deletion_enabled(bool allow);
  void set_download_enabled(bool allow);
  void set_upload_enabled(bool allow);
  void set_port(uint16_t port);

 protected:
  // Handlers para as requisições HTTP.
  void handle_index(httpd_req_t *req, const std::string &path, const std::string& relative_path) const;
  void handle_get(httpd_req_t *req) const;
  void handle_delete(httpd_req_t *req) const;
  void handle_upload(httpd_req_t *req) const;
  void handle_download(httpd_req_t *req, const std::string &path) const;

  // Funções de utilidade.
  std::string build_prefix() const;
  std::string extract_path_from_url(const std::string &url) const;
  std::string build_absolute_path(const std::string &path) const;
  void send_response(httpd_req_t *req, int status, const char *content_type, const char *body, size_t body_len) const;

  // Handlers estáticos para o servidor HTTP do ESP-IDF.
  static esp_err_t http_get_handler(httpd_req_t *req);
  static esp_err_t http_delete_handler(httpd_req_t *req);
  static esp_err_t http_post_handler(httpd_req_t *req);

  // Variáveis membro.
  httpd_handle_t server_{nullptr};
  waveshare_sd_card::WaveshareSdCard *sd_card_{nullptr};
  std::string url_prefix_;
  std::string root_path_;
  bool deletion_enabled_{false};
  bool download_enabled_{false};
  bool upload_enabled_{false};
  uint16_t port_{80};
};

// Estrutura de utilidade para manipulação de caminhos.
struct Path {
  static constexpr char separator = '/';
  static std::string file_name(const std::string &path);
  static std::string parent_path(const std::string &path);
  static std::string join(const std::string &base, const std::string &file);
  static const char *mime_type(const std::string &path);
};

}  // namespace sd_file_server
}  // namespace esphome
