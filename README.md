#Play MP3/Flac Files and WIFI AP SDCard server

Long press to start/stop WIFI AP - server is on 1.2.3.4

rec = pause
mode = new random file
play = previous file
set = next file
vol+ vol- = volume


You need to modify adf/components/esp_http_server/src/httpd_uri.c so it can handle wildcard pages:

#Mod Code

static httpd_uri_t* httpd_find_uri_handler2(httpd_err_resp_t *err, struct httpd_data *hd,   const char *uri, size_t uri_len,   httpd_method_t method)
    {
    *err = 0;
    for (int i = 0; i < hd->config.max_uri_handlers; i++)
        {
        if (hd->hd_calls[i])
            {
            ESP_LOGD(TAG, LOG_FMT("[%d] = %s"), i, hd->hd_calls[i]->uri);
            if (hd->hd_calls[i]->method == method)
            if (strcmp(hd->hd_calls[i]->uri, "/*") == 0)
                return hd->hd_calls[i];
                
            if ((strlen(hd->hd_calls[i]->uri) == uri_len) &&            // First match uri length
                (strncmp(hd->hd_calls[i]->uri, uri, uri_len) == 0))
                {  // Then match uri strings
                if (hd->hd_calls[i]->method == method)
                    {               // Finally match methods
                    return hd->hd_calls[i];
                    }
                /* URI found but method not allowed.
                 * If URI IS found later then this
                 * error is to be neglected */
                *err = HTTPD_405_METHOD_NOT_ALLOWED;
                }
                
                
            }
        }
    if (*err == 0) {
        *err = HTTPD_404_NOT_FOUND;
        }
    return NULL;
    }
