#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// sdk includes
#include "esp_http_server.h"
#include "esp_log.h"
#include "http_server.h"
#include "cJSON.h"
// app includes
#include "config.h"
#include "urldecode.h"
#include "info.h"

#define TAG "http_server.c"

static httpd_handle_t server = NULL;

static int save_wifi_post_data(const char *httpd_data, char *httpd_status)
{
    int httpd_code;
    ESP_LOGI(TAG, "save_wifi_post_data:%s", httpd_data);
    cJSON *json_data = cJSON_Parse(httpd_data);
    if (json_data == NULL)
    {
        ESP_LOGE(TAG, "cJSON Parse error");
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG, "Error: %s\n", error_ptr);
        }
        strcpy(httpd_status, HTTPD_500);
        httpd_code = 500;
        goto end;
    }
    cJSON *json_ssid = cJSON_GetObjectItemCaseSensitive(json_data, "ssid");
    if (cJSON_IsString(json_ssid) && json_ssid->valuestring != NULL)
    {
        ESP_LOGI(TAG, "Received ssid:%s", json_ssid->valuestring);
    }
    else
    {
        ESP_LOGE(TAG, "Bad request. ssid not present.");
        strcpy(httpd_status, HTTPD_400);
        httpd_code = 400;
        goto end;
    }

    cJSON *json_pass = cJSON_GetObjectItemCaseSensitive(json_data, "pass");
    if (cJSON_IsString(json_pass) && json_pass->valuestring != NULL)
    {
        ESP_LOGI(TAG, "Received pass:%s", json_pass->valuestring);
    }
    else
    {
        ESP_LOGE(TAG, "Bad request. pass not present.");
        strcpy(httpd_status, HTTPD_400);
        httpd_code = 400;
        goto end;
    }

    if (set_config_str(CONFIG_NAMESPACE, CONFIG_WIFI_SSID_KEY, json_ssid->valuestring) != ESP_OK)
    {
        ESP_LOGE(TAG, "Error in set_config_str:%s %s", CONFIG_WIFI_SSID_KEY, json_ssid->valuestring);
        strcpy(httpd_status, HTTPD_500);
        httpd_code = 500;
        goto end;
    }

    if (set_config_str(CONFIG_NAMESPACE, CONFIG_WIFI_PASS_KEY, json_pass->valuestring) != ESP_OK)
    {
        ESP_LOGE(TAG, "Error in set_config_str:%s %s", CONFIG_WIFI_SSID_KEY, json_ssid->valuestring);
        strcpy(httpd_status, HTTPD_500);
        httpd_code = 500;
        goto end;
    }
    if (commit_info(CONFIG_NAMESPACE) != ESP_OK)
    {
        ESP_LOGE(TAG, "commit_info:%s", CONFIG_WIFI_SSID_KEY);
        strcpy(httpd_status, HTTPD_500);
        httpd_code = 500;
        goto end;
    }

    httpd_code = 200;
    strcpy(httpd_status, HTTPD_200);

end:
    return httpd_code;
    /* Delete a cJSON entity and all subentities. */
    cJSON_Delete(json_data);
}

static char *get_wifi_response(char **str_response)
{
    // heap allocated inside function you are required to free it after use.

    cJSON *json_wifi_ssid = NULL;
    cJSON *json_wifi_pass = NULL;

    cJSON *json_wifi = cJSON_CreateObject();
    if (json_wifi == NULL)
    {
        cJSON_Delete(json_wifi);
        return HTTPD_500;
    }
    //++wifi_ssid
    char *wifi_ssid = (char *)get_config_str(CONFIG_NAMESPACE, CONFIG_WIFI_SSID_KEY);
    if (wifi_ssid == NULL)
    {
        cJSON_Delete(json_wifi);
        return HTTPD_404;
    }
    json_wifi_ssid = cJSON_CreateString(wifi_ssid);
    if (json_wifi_ssid == NULL)
    {
        free(wifi_ssid);
        cJSON_Delete(json_wifi);
        return HTTPD_500;
    }
    cJSON_AddItemToObject(json_wifi, "ssid", json_wifi_ssid);
    //--wifi_ssid

    //++wifi_pass
    char *wifi_pass = (char *)get_config_str(CONFIG_NAMESPACE, CONFIG_WIFI_PASS_KEY);
    if (wifi_pass == NULL)
    {
        free(wifi_pass);
        cJSON_Delete(json_wifi);
        return HTTPD_404;
    }

    json_wifi_pass = cJSON_CreateString(wifi_pass);
    if (json_wifi_ssid == NULL)
    {
        cJSON_Delete(json_wifi);
        return HTTPD_500;
    }
    cJSON_AddItemToObject(json_wifi, "pass", json_wifi_pass);
    //--wifi_pass

    char *str_wifi = cJSON_Print(json_wifi);

    ESP_LOGD(TAG, "str_wifi:%s", str_wifi);
    if (str_wifi != NULL)
    {
        *str_response = (char *)malloc(strlen(str_wifi) + 1);
        memset(*str_response, 0, strlen(str_wifi) + 1);
        strcpy(*str_response, str_wifi);
        ESP_LOGD(TAG, "str_response:%s", *str_response);
    }

    cJSON_Delete(json_wifi);
    free(str_wifi);
    return HTTPD_200;
}

/* An HTTP POST handler */
esp_err_t wifi_post_handler(httpd_req_t *req)
{
    char *httpd_data = NULL;
    int ret;

    int bytes_read = 0;
    int content_len = req->content_len;
    ESP_LOGI(TAG, "content_len:%d", content_len);
    httpd_data = (char *)malloc(content_len + 1);
    memset(httpd_data, 0, content_len + 1);
    do
    {

        ret = httpd_req_recv(req, httpd_data + bytes_read, content_len - bytes_read);
        if (ret < 0)
        {
            ESP_LOGE(TAG, "Error reading post data:%d", ret);
            return ESP_FAIL;
        }
        else if (ret == 0)
        {
            ESP_LOGI(TAG, "finish reading post data");
            break;
        }
        else
        {
            ESP_LOGI(TAG, " %d bytes read in post data", ret);
            bytes_read += ret;
            ESP_LOGI(TAG, "total  bytes read %d in post data", bytes_read);
        }

    } while (bytes_read < content_len);

    char httpd_status[32] = {0};
    int http_code = save_wifi_post_data(httpd_data, httpd_status);
    free(httpd_data);
    httpd_resp_set_status(req, httpd_status);
    char response[64] = {0};
    if (http_code == 200)
    {
        sprintf(response, "{\"ok\":\"%s\"}", httpd_status);
    }
    else
    {
        sprintf(response, "{\"error\":\"%s\"}", httpd_status);
    }

    httpd_resp_send(req, response, strlen(response));
    return ESP_OK;
}

/* An HTTP GET handler */
esp_err_t wifi_get_handler(httpd_req_t *req)
{
    char *buf;
    size_t buf_len;
    uint8_t num_headers = 4;
    const char *headers[num_headers];
    char *http_response = NULL;
    headers[0] = "Host";
    headers[1] = "User-Agent";
    headers[2] = "Accept";
    headers[3] = "Content-Type";

    memory_info("start wifi_get_handler");
    for (int i = 0; i < num_headers; i++)
    {
        /* Get header value string length and allocate memory for length + 1,
         * extra byte for null termination */
        ESP_LOGI(TAG, "header: => %s", headers[i]);
        buf_len = httpd_req_get_hdr_value_len(req, headers[i]) + 1;
        ESP_LOGI(TAG, "req_get_hdr_value => %d", buf_len);
        if (buf_len > 0)
        {
            buf = malloc(buf_len);
            /* Copy null terminated value string into buffer */
            if (httpd_req_get_hdr_value_str(req, headers[i], buf, buf_len) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found header => %s: %s", headers[i], buf);
            }
            free(buf);
        }
    }

    /* Set some custom headers */
    // httpd_resp_set_hdr(req, "Content-Type", "application/json");
    httpd_resp_set_type(req, HTTPD_TYPE_JSON);

    char *http_code_status = get_wifi_response(&http_response);
    ESP_LOGI(TAG, "http_code:%s", http_code_status);
    httpd_resp_set_status(req, http_code_status);
    if (http_response != NULL)
    {
        httpd_resp_send(req, http_response, strlen(http_response));
    }
    else
    {
        char error_response[64] = {0};
        sprintf(error_response, "{\"error\":\"%s\"}", http_code_status);
        httpd_resp_send(req, error_response, strlen(error_response));
    }

    free(http_response);
    memory_info("end wifi_get_handler");
    return ESP_OK;
}

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
    for (int i = 0; i < num_headers; i++)
    {
        /* Get header value string length and allocate memory for length + 1,
         * extra byte for null termination */
        ESP_LOGI(TAG, "header: => %s", headers[i]);
        buf_len = httpd_req_get_hdr_value_len(req, headers[i]) + 1;
        ESP_LOGI(TAG, "req_get_hdr_value => %d", buf_len);
        if (buf_len > 0)
        {
            buf = malloc(buf_len);
            /* Copy null terminated value string into buffer */
            if (httpd_req_get_hdr_value_str(req, headers[i], buf, buf_len) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found header => %s: %s", headers[i], buf);
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
            char *decoded_param = NULL;
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

httpd_uri_t get_wifi = {
    .uri = "/wifi",
    .method = HTTP_GET,
    .handler = wifi_get_handler,
    .user_ctx = NULL};

httpd_uri_t post_wifi = {
    .uri = "/wifi",
    .method = HTTP_POST,
    .handler = wifi_post_handler,
    .user_ctx = NULL};

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
        httpd_register_uri_handler(server, &get_wifi);
        httpd_register_uri_handler(server, &post_wifi);
    }
    else
    {
        ESP_LOGE(TAG, "Error starting server!");
    }
}

void stop_webserver()
{
    // Stop the httpd server
    httpd_stop(server);
}
