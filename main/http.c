#include <string.h>
#include <stdlib.h>
#include <time.h>
// sdk includes
#include "esp_tls.h"
#include "esp_log.h"
#include "cJSON.h"

#include "http.h"
#include "info.h"

#define TAG "http"

#define MIN_HTTP_RESPONSE 16


static char * read_response(struct esp_tls *tls_conn)
{
    //allocated heap memory for full response, must be free!.
    char buffer[512] = {0};
    int bytes_read = 0;
    int total_bytes_read = 0;
    char * response = NULL;

    do
    {
        bzero(buffer, sizeof(buffer));
        int retval = esp_tls_conn_read(tls_conn, (char *)buffer, sizeof(buffer) - 1);

        if (retval == ESP_TLS_ERR_SSL_WANT_READ  || retval == ESP_TLS_ERR_SSL_WANT_WRITE)
            continue;
        if(retval < 0)
        {
            ESP_LOGE(TAG, "esp_tls_conn_read  returned -0x%x", -retval);
            return NULL;
        }
        if(retval == 0)
        {
            ESP_LOGI(TAG, "connection closed");
            break;
        }

        bytes_read = retval;
        response = (char*) realloc(response,(total_bytes_read+bytes_read));
        memcpy(response + total_bytes_read,buffer,bytes_read);
        total_bytes_read += bytes_read;

    } while(1);

    //null terminated
    response = (char*) realloc(response,total_bytes_read+1);
    response[total_bytes_read] = '\0';

    return response;

}

static int write_request(struct esp_tls *tls_conn,char * request){

    //write request
    size_t written_bytes = 0;
    do {
        int ret = esp_tls_conn_write(tls_conn,request + written_bytes,strlen(request) - written_bytes);
        if (ret >= 0) {
            written_bytes += ret;
        } else if (ret != ESP_TLS_ERR_SSL_WANT_READ  && ret != ESP_TLS_ERR_SSL_WANT_WRITE)
        {
            ESP_LOGE(TAG, "esp_tls_conn_write  returned 0x%x", ret);
            return ESP_FAIL;
        }
    } while(written_bytes < strlen(request));
    return ESP_OK;


}

static char * get_body(char * response){
    char header_end[5] = "\r\n\r\n";
    char *body = NULL;
     body = strstr(response,header_end);
    if (body == NULL){
        ESP_LOGE(TAG, "body null!");
        return NULL;

    }
    body += strlen(header_end);
    return body;
}

static int validate_response(const char * response){
    char  protocol[9]={ 0 };
    char  http_code[4]={ 0 };
    if (response == NULL){
        ESP_LOGI(TAG,"response NULL");
        return ESP_FAIL;

    }
    int response_len = strlen(response);
    if (response_len < MIN_HTTP_RESPONSE){
        ESP_LOGE(TAG, "response len:%d less than min http response:%d",response_len,MIN_HTTP_RESPONSE);
        return ESP_FAIL;
    }
    strncpy(protocol, response, 8);
    if ((strcmp(protocol,"HTTP/1.0") != 0) && (strcmp(protocol,"HTTP/1.1") != 0) ){
        ESP_LOGE(TAG,"response protocol error");
        return ESP_FAIL;
    }
    strncpy(http_code, response + 9, 3);
    if (strcmp(http_code,"200") != 0){
        ESP_LOGE(TAG,"response http_code error");
        return ESP_FAIL;
    }
    return ESP_OK;
}

cJSON * http_request(char *url, char * request)
{
    //if succesfull return a cJSON ptr that must be free, else return NULL.
    char *response = NULL;
    char *body_response = NULL;
    cJSON *json_response = NULL;
    struct esp_tls *tls_conn;
    esp_tls_cfg_t cfg = {
        .cacert_pem_buf = server_root_cert_pem_start,
        .cacert_pem_bytes = server_root_cert_pem_end - server_root_cert_pem_start,
    };
    cfg.timeout_ms = 30000;
    ESP_LOGI(TAG,"Trying to connect...");
    tls_conn= esp_tls_conn_http_new(url, &cfg);
    if(tls_conn != NULL) {
        ESP_LOGI(TAG, "http connection established:%s",url);
    } else {
        ESP_LOGE(TAG, "connection failed:%s",url);
        goto exit;
    }
    if (write_request(tls_conn,request) != ESP_OK){
        goto exit;
    }
    response = read_response(tls_conn);
    if (validate_response(response) != ESP_OK){
        goto exit;
    }
    //ESP_LOGI(TAG,"response:%s",response);
    body_response = get_body(response);
    if (body_response == NULL){
        goto exit;
    }
    json_response = cJSON_Parse(body_response);
    if (json_response == NULL)
    {
        ESP_LOGE(TAG, "Error in parse JSON");
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG, "Error:%s\n", error_ptr);
        }
        cJSON_Delete(json_response);
    }
    exit:
        esp_tls_conn_delete(tls_conn);
        free(response);
    memory_info("end http_request");
    return json_response;
}