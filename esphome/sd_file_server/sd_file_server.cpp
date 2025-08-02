#include "sd_file_server.h"
#include <sstream>
#include <map>
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstdio>
#include <sys/stat.h>

namespace esphome {
namespace sd_file_server {

static const char *const TAG = "sd_file_server";

SDFileServer::SDFileServer() = default;

void SDFileServer::setup() {
  if (this->sd_card_ == nullptr) {
    ESP_LOGE(TAG, "Componente SD Card não configurado!");
    this->mark_failed();
    return;
  }

  ESP_LOGCONFIG(TAG, "Inicializando servidor HTTP...");
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = this->port_;
  config.max_uri_handlers = 10;
  config.stack_size = 8192;
  config.uri_match_fn = httpd_uri_match_wildcard;

  if (httpd_start(&this->server_, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Falha ao iniciar servidor HTTP");
    this->mark_failed();
    return;
  }

  // Registra os handlers para os métodos HTTP.
  httpd_uri_t get_uri = {.uri = "/*", .method = HTTP_GET, .handler = http_get_handler, .user_ctx = this};
  httpd_register_uri_handler(this->server_, &get_uri);

  httpd_uri_t delete_uri = {.uri = "/*", .method = HTTP_DELETE, .handler = http_delete_handler, .user_ctx = this};
  httpd_register_uri_handler(this->server_, &delete_uri);

  httpd_uri_t post_uri = {.uri = "/*", .method = HTTP_POST, .handler = http_post_handler, .user_ctx = this};
  httpd_register_uri_handler(this->server_, &post_uri);

  ESP_LOGI(TAG, "Servidor HTTP iniciado! Acesse: http://%s:%u%s", network::get_use_address().c_str(), this->port_, this->build_prefix().c_str());
}

void SDFileServer::dump_config() {
  ESP_LOGCONFIG(TAG, "SD File Server:");
  ESP_LOGCONFIG(TAG, "  Porta: %u", this->port_);
  ESP_LOGCONFIG(TAG, "  Prefixo da URL: %s", this->build_prefix().c_str());
  ESP_LOGCONFIG(TAG, "  Caminho Raiz no SD: %s", this->root_path_.c_str());
  ESP_LOGCONFIG(TAG, "  Deleção Habilitada: %s", TRUEFALSE(this->deletion_enabled_));
  ESP_LOGCONFIG(TAG, "  Download Habilitado: %s", TRUEFALSE(this->download_enabled_));
  ESP_LOGCONFIG(TAG, "  Upload Habilitado: %s", TRUEFALSE(this->upload_enabled_));
}

void SDFileServer::set_url_prefix(const std::string &prefix) { this->url_prefix_ = prefix; }
void SDFileServer::set_root_path(const std::string &path) { this->root_path_ = path; }
void SDFileServer::set_sd_card(waveshare_sd_card::WaveshareSdCard *card) { this->sd_card_ = card; }
void SDFileServer::set_deletion_enabled(bool allow) { this->deletion_enabled_ = allow; }
void SDFileServer::set_download_enabled(bool allow) { this->download_enabled_ = allow; }
void SDFileServer::set_upload_enabled(bool allow) { this->upload_enabled_ = allow; }
void SDFileServer::set_port(uint16_t port) { this->port_ = port; }

std::string SDFileServer::build_prefix() const { return "/" + this->url_prefix_; }

std::string SDFileServer::extract_path_from_url(const std::string &url) const {
  std::string prefix = this->build_prefix();
  if (url.rfind(prefix, 0) == 0) {
    return url.substr(prefix.length());
  }
  return url;
}

std::string SDFileServer::build_absolute_path(const std::string &relative_path) const {
    std::string path = this->root_path_ + relative_path;
    while (path.length() > 1 && path.find("//") != std::string::npos) {
        path.replace(path.find("//"), 2, "/");
    }
    return "/sdcard" + path;
}

void SDFileServer::send_response(httpd_req_t *req, int status, const char *content_type, const char *body, size_t body_len) const {
    char status_str[16];
    snprintf(status_str, sizeof(status_str), "%d", status);
    httpd_resp_set_status(req, status_str);
    httpd_resp_set_type(req, content_type);
    httpd_resp_send(req, body, body_len);
}

// Gera a página HTML para listar os arquivos.
void SDFileServer::handle_index(httpd_req_t *req, const std::string &path, const std::string& relative_path) const {
    if (!this->sd_card_->is_directory(path.c_str())) {
        send_response(req, 404, "text/plain", "Not a directory", 15);
        return;
    }

    std::stringstream response;
    response << R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 File Server</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; margin: 0; background-color: #f4f4f9; color: #333; }
        .container { max-width: 800px; margin: 2rem auto; padding: 1rem; background-color: #fff; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1, h2 { color: #444; border-bottom: 2px solid #eee; padding-bottom: 10px; }
        table { width: 100%; border-collapse: collapse; margin-top: 20px; }
        th, td { padding: 12px 15px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background-color: #f8f8f8; }
        tr:hover { background-color: #f1f1f1; }
        a { text-decoration: none; color: #007bff; }
        a:hover { text-decoration: underline; }
        .icon { display: inline-block; width: 20px; text-align: center; margin-right: 10px; }
        .delete-btn { color: #dc3545; cursor: pointer; }
        .upload-form { margin-top: 30px; padding: 20px; background-color: #fdfdfd; border: 1px dashed #ccc; border-radius: 5px; }
        .upload-form input[type='file'] { margin-bottom: 10px; }
        .upload-form input[type='submit'] { padding: 10px 15px; background-color: #28a745; color: white; border: none; border-radius: 5px; cursor: pointer; }
        .upload-form input[type='submit']:hover { background-color: #218838; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Conteúdo de: )rawliteral" << (relative_path.empty() || relative_path == "/" ? "/" : relative_path) << R"rawliteral(</h1>
        <table>
            <thead><tr><th></th><th>Nome</th><th>Tamanho</th><th>Ações</th></tr></thead>
            <tbody>
    )rawliteral";

    if (!relative_path.empty() && relative_path != "/") {
        response << "<tr><td><span class='icon'>&#128193;</span></td><td colspan='3'><a href='" << build_prefix() << Path::parent_path(relative_path) << "'>.. (Voltar)</a></td></tr>";
    }

    for (const auto &info : this->sd_card_->list_files(path.c_str())) {
        std::string full_relative_path = Path::join(relative_path, info.name);
        response << "<tr>";
        if (info.is_directory) {
            response << "<td><span class='icon'>&#128193;</span></td>"; // Ícone de pasta
            response << "<td><a href='" << build_prefix() << full_relative_path << "'>" << info.name << "</a></td>";
            response << "<td>-</td><td></td>";
        } else {
            response << "<td><span class='icon'>&#128441;</span></td>"; // Ícone de arquivo
            response << "<td><a href='" << build_prefix() << full_relative_path << "'>" << info.name << "</a></td>";
            response << "<td>" << info.size << " B</td>";
            response << "<td>";
            if (this->deletion_enabled_) {
                 response << "<a href='#' class='delete-btn' onclick='if(confirm(\"Deletar " << info.name << "?\")){fetch(\"" << build_prefix() << full_relative_path << "\", {method:\"DELETE\"}).then(()=>location.reload())}'>Deletar</a>";
            }
            response << "</td>";
        }
        response << "</tr>";
    }
    
    response << R"rawliteral(
            </tbody>
        </table>
    )rawliteral";

    if (this->upload_enabled_) {
        response << R"rawliteral(
        <div class="upload-form">
            <h2>Upload de Arquivo</h2>
            <form method='post' action='' enctype='multipart/form-data'>
                <input type='file' name='file' required>
                <input type='submit' value='Enviar'>
            </form>
        </div>
    )rawliteral";
    }

    response << R"rawliteral(
    </div>
</body>
</html>
    )rawliteral";

    std::string body = response.str();
    send_response(req, 200, "text/html", body.c_str(), body.length());
}


// Handler principal para requisições GET.
void SDFileServer::handle_get(httpd_req_t *req) const {
  std::string relative_path = this->extract_path_from_url(req->uri);
  std::string absolute_path = this->build_absolute_path(relative_path);
  
  if (this->sd_card_->is_directory(absolute_path.c_str())) {
      handle_index(req, absolute_path, relative_path);
  } else {
      handle_download(req, absolute_path);
  }
}

// Handler para download de arquivos.
void SDFileServer::handle_download(httpd_req_t *req, const std::string &path) const {
    if (!this->download_enabled_) {
        send_response(req, 403, "text/plain", "Download desabilitado", 21);
        return;
    }
    FILE* f = fopen(path.c_str(), "r");
    if (f == nullptr) {
        send_response(req, 404, "text/plain", "Arquivo não encontrado", 22);
        return;
    }
    
    httpd_resp_set_type(req, Path::mime_type(path));
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
            fclose(f);
            return;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, nullptr, 0); // Finaliza a resposta.
}

// Handler para deletar arquivos.
void SDFileServer::handle_delete(httpd_req_t *req) const {
    if (!this->deletion_enabled_) {
        send_response(req, 403, "text/plain", "Deleção desabilitada", 21);
        return;
    }
    std::string path = this->build_absolute_path(this->extract_path_from_url(req->uri));
    if (remove(path.c_str()) != 0) {
        send_response(req, 500, "text/plain", "Falha ao deletar", 16);
        return;
    }
    send_response(req, 200, "text/plain", "Deletado com sucesso", 21);
}

// Handler para upload de arquivos (implementação básica).
static const char *memmem(const char *haystack, size_t hlen, const char *needle, size_t nlen) {
    if (nlen == 0) return haystack;
    if (hlen < nlen) return NULL;
    const char *end = haystack + hlen - nlen;
    for (const char *p = haystack; p <= end; ++p) {
        if (memcmp(p, needle, nlen) == 0) {
            return p;
        }
    }
    return NULL;
}

void SDFileServer::handle_upload(httpd_req_t *req) const {
    if (!this->upload_enabled_) {
        send_response(req, 403, "text/plain", "Upload desabilitado.", 20);
        return;
    }

    // Buffer para receber os dados
    char buf[1500];
    int received;

    // Obter o "boundary" do cabeçalho Content-Type
    char content_type[100];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type)) != ESP_OK) {
        send_response(req, 400, "text/plain", "Cabeçalho Content-Type ausente.", 31);
        return;
    }
    char *boundary_ptr = strstr(content_type, "boundary=");
    if (!boundary_ptr) {
        send_response(req, 400, "text/plain", "Boundary não encontrado.", 24);
        return;
    }
    std::string boundary = "--" + std::string(boundary_ptr + 9);
    std::string boundary_end = boundary + "--";

    FILE *f = nullptr;
    std::string filename;
    bool header_found = false;
    int remaining = req->content_len;
    
    while (remaining > 0) {
        received = httpd_req_recv(req, buf, std::min(remaining, (int)sizeof(buf)));
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
            if (f) fclose(f);
            send_response(req, 500, "text/plain", "Erro ao receber arquivo.", 26);
            return;
        }

        const char* current_buf = buf;
        int current_len = received;

        if (!header_found) {
            const char *header_start = memmem(current_buf, current_len, "filename=\"", 10);
            if (header_start) {
                const char *filename_start = header_start + 10;
                const char *filename_end = memmem(filename_start, current_len - (filename_start - current_buf), "\"", 1);
                if (filename_end) {
                    filename.assign(filename_start, filename_end - filename_start);
                    std::string full_path = this->build_absolute_path(Path::join(this->extract_path_from_url(req->uri), filename));
                    f = fopen(full_path.c_str(), "w");
                    if (!f) {
                        send_response(req, 500, "text/plain", "Falha ao criar arquivo.", 23);
                        return;
                    }

                    const char *body_start = memmem(filename_end, current_len - (filename_end - current_buf), "\r\n\r\n", 4);
                    if (body_start) {
                        body_start += 4;
                        header_found = true;
                        int data_to_write = current_len - (body_start - current_buf);
                        
                        // Verifica se o boundary final está neste mesmo buffer
                        const char *end_marker = memmem(body_start, data_to_write, boundary.c_str(), boundary.length());
                        if (end_marker) {
                            fwrite(body_start, 1, end_marker - body_start - 2 /* remove \r\n */, f);
                        } else {
                            fwrite(body_start, 1, data_to_write, f);
                        }
                    }
                }
            }
        } else {
            // Verifica se o boundary final está neste buffer
            const char *end_marker = memmem(current_buf, current_len, boundary.c_str(), boundary.length());
            if (end_marker) {
                fwrite(current_buf, 1, end_marker - current_buf - 2 /* remove \r\n */, f);
            } else {
                fwrite(current_buf, 1, current_len, f);
            }
        }
        remaining -= received;
    }

    if (f) {
        fclose(f);
    }

    // Redireciona de volta para a página de arquivos
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", req->uri);
    httpd_resp_send(req, NULL, 0);
}

// Funções estáticas que chamam os métodos da instância da classe.
esp_err_t SDFileServer::http_get_handler(httpd_req_t *req) {
  ((SDFileServer *)req->user_ctx)->handle_get(req);
  return ESP_OK;
}

esp_err_t SDFileServer::http_delete_handler(httpd_req_t *req) {
  ((SDFileServer *)req->user_ctx)->handle_delete(req);
  return ESP_OK;
}

esp_err_t SDFileServer::http_post_handler(httpd_req_t *req) {
  ((SDFileServer *)req->user_ctx)->handle_upload(req);
  return ESP_OK;
}

// Implementações da estrutura Path.
std::string Path::file_name(const std::string &path) {
  size_t pos = path.find_last_of(separator);
  return pos == std::string::npos ? path : path.substr(pos + 1);
}

std::string Path::parent_path(const std::string &path) {
    if (path.empty() || path == "/") return "/";
    size_t pos = path.find_last_of(separator);
    if (pos == 0) return "/";
    if (pos == std::string::npos) return "/";
    return path.substr(0, pos);
}

std::string Path::join(const std::string &base, const std::string &file) {
    if (base.empty() || base == "/") {
        return "/" + file;
    }
    return base + "/" + file;
}

const char *Path::mime_type(const std::string &path) {
  size_t pos = path.find_last_of('.');
  if(pos == std::string::npos) return "application/octet-stream";
  std::string ext = path.substr(pos);
  if (ext == ".html" || ext == ".htm") return "text/html";
  if (ext == ".css") return "text/css";
  if (ext == ".js") return "application/javascript";
  if (ext == ".json") return "application/json";
  if (ext == ".txt") return "text/plain";
  if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
  if (ext == ".png") return "image/png";
  if (ext == ".gif") return "image/gif";
  if (ext == ".svg") return "image/svg+xml";
  if (ext == ".ico") return "image/x-icon";
  return "application/octet-stream";
}

}  // namespace sd_file_server
}  // namespace esphome
