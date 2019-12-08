# Play MP3 files from microSD with touch pad control 

The demo plays MP3 files stored on the SD card using audio pipeline API.

The playback control is done using ESP32 touch pad functionality. User can start, stop, pause, resume playback and advance to the next song as well as adjust volume. When playing, the application automatically advances to the next song once previous finishes.

To run this example you need ESP32 LyraT or compatible board:

- Connect speakers or headphones to the board. 
- Insert a microSD card loaded with a MP3 files 'test.mp3', 'test1.mp3' and 'test2.mp3' into board's slot.


You need to modify adf/components/esp_http_server/src/httpd_uri.c so it can handle wildcard pages:



static httpd_uri_t* httpd_find_uri_handler2(httpd_err_resp_t *err,
    struct httpd_data *hd,
    const char *uri, size_t uri_len,
    httpd_method_t method)
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
