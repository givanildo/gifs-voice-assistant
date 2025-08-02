# gifs-voice-assistant
gifs for voice assistant
board/////////.... (https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.85) ....///

 ////////////.....yaml use ...esp-idf...//////
 ..................................

waveshare_sd_card:

  id: my_sd_card

  clk_pin: GPIO14

  cmd_pin: GPIO17

  data0_pin: GPIO16

  cs_pin:

    pca9554: ext_io

    number: 2

  total_space:

    name: "SD Card Total Space"

  used_space:

    name: "SD Card Used Space"

  free_space:

    name: "SD Card Free Space"



sd_file_server:

  id: my_sd_file_server
  
  waveshare_sd_card_id: my_sd_card
  
  port: 81
  
  url_prefix: files
  
  root_path: /
  
  enable_deletion: true
  
  enable_download: true
  
  enable_upload: true
  
