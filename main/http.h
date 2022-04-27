#ifndef HTTP_H
#define HTTP_H

//https://thingpulse.com/embed-binary-data-on-esp32/



// remember place the file in your project directory structure and register it as an IDF component
// in CMakeLists  set(COMPONENT_EMBED_TXTFILES server_root_cert.pem)
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

cJSON *  http_request(char *url,char* request);


#endif