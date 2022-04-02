#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "http_server.h"
#include "config.h"
#include "urldecode.h"
#include "info.h"



#define TAG "http_server.c" 

#define HEADER_1 "Host"
#define HEADER_2 "User-Agent"
#define HEADER_3 "Accept"

static httpd_handle_t server = NULL;


/* An HTTP GET handler */
esp_err_t hello_get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    uint8_t num_headers = 4;
    const char *headers[num_headers];
    headers[0] = "Host";
    headers[1] = "User-Agent";
    headers[2] = "Accept";
    headers[3] = "Content-Type";
    memory_info("start hello_get_handler");
    for (int i=0;i<num_headers;i++){
         /* Get header value string length and allocate memory for length + 1,
         * extra byte for null termination */
        ESP_LOGI(TAG, "header: => %s",headers[i]);
        buf_len = httpd_req_get_hdr_value_len(req, headers[i]) + 1;
        ESP_LOGI(TAG, "req_get_hdr_value => %d",buf_len);
        if (buf_len > 0)
        {
            buf = malloc(buf_len);
            /* Copy null terminated value string into buffer */
            if (httpd_req_get_hdr_value_str(req, headers[i], buf, buf_len) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found header => %s: %s",headers[i], buf);
            }
            free(buf);
        }


    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            char* decoded_param = NULL;
            /* Get value of expected key from query string */
            
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
                decoded_param = urlDecode(param);
                ESP_LOGI(TAG, "Decoded parameter => query1=%s", decoded_param);
                free(decoded_param);

            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
                decoded_param = urlDecode(param);
                ESP_LOGI(TAG, "Decoded parameter => query1=%s", decoded_param);
                free(decoded_param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char *resp_str = (const char *)req->user_ctx;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0)
    {
        ESP_LOGI(TAG, "Request headers lost");
    }
    memory_info("end hello_get_handler");
    return ESP_OK;
}

httpd_uri_t hello = {
    .uri = "/hello",
    .method = HTTP_GET,
    .handler = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx = "Hello Pr.Falken"};

void start_webserver(void)
{

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
    }
    else{
        ESP_LOGE(TAG, "Error starting server!");

    }

    
}

void stop_webserver()
{
    // Stop the httpd server
    httpd_stop(server);
}
