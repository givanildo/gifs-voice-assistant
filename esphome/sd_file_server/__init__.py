import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.core import coroutine_with_priority
from .. import waveshare_sd_card

CONF_URL_PREFIX = "url_prefix"
CONF_ROOT_PATH = "root_path"
CONF_ENABLE_DELETION = "enable_deletion"
CONF_ENABLE_DOWNLOAD = "enable_download"
CONF_ENABLE_UPLOAD = "enable_upload"

AUTO_LOAD = []
DEPENDENCIES = ["waveshare_sd_card", "network"]

sd_file_server_ns = cg.esphome_ns.namespace("sd_file_server")
SDFileServer = sd_file_server_ns.class_("SDFileServer", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SDFileServer),
            cv.Required(waveshare_sd_card.CONF_WAVESHARE_SD_CARD_ID): cv.use_id(waveshare_sd_card.WaveshareSdCard),
            cv.Optional(CONF_PORT, default=80): cv.port,
            cv.Optional(CONF_URL_PREFIX, default="files"): cv.string_strict,
            cv.Optional(CONF_ROOT_PATH, default="/"): cv.string_strict,
            cv.Optional(CONF_ENABLE_DELETION, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_DOWNLOAD, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_UPLOAD, default=True): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
)

@coroutine_with_priority(45.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    sd_card = await cg.get_variable(config[waveshare_sd_card.CONF_WAVESHARE_SD_CARD_ID])
    cg.add(var.set_sd_card(sd_card))
    cg.add(var.set_url_prefix(config[CONF_URL_PREFIX]))
    cg.add(var.set_root_path(config[CONF_ROOT_PATH]))
    cg.add(var.set_deletion_enabled(config[CONF_ENABLE_DELETION]))
    cg.add(var.set_download_enabled(config[CONF_ENABLE_DOWNLOAD]))
    cg.add(var.set_upload_enabled(config[CONF_ENABLE_UPLOAD]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add_define("USE_SD_CARD_WEBSERVER")