#ifndef HTTP_H
#define HTTP_H

//https://thingpulse.com/embed-binary-data-on-esp32/



// remember place the file in your project directory structure and register it as an IDF component
// in CMakeLists  set(COMPONENT_EMBED_TXTFILES server_root_cert.pem)
/* Root cert for howsmyssl.com, taken from server_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

cJSON *  http_request(char *url,char* request);


#endif