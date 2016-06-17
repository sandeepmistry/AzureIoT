// Copyright (c) Arduino. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <time.h>
#include <sys/time.h>

#include <ArduinoHttpClient.h>

#include "sdk/httpapi.h"
#include "iot_logging.h"

#include "AzureIoTHubClient.h"


HttpClient* httpsClient = NULL;

HTTPAPI_RESULT HTTPAPI_Init(void)
{
    return HTTPAPI_OK;
}

void HTTPAPI_Deinit(void)
{
}

HTTP_HANDLE HTTPAPI_CreateConnection(const char* hostName)
{
    HttpClient* client = NULL;

    if (httpsClient == NULL) {
        httpsClient = new HttpClient(*AzureIoTHubClient::sslClient, hostName, 443);
        httpsClient->connectionKeepAlive();
        httpsClient->noDefaultRequestHeaders();
        httpsClient->setHttpResponseTimeout(10000);
    } else {
        httpsClient->stop();
    }

    client = httpsClient;

    return ((HTTP_HANDLE)client);
}

void HTTPAPI_CloseConnection(HTTP_HANDLE handle)
{
    HttpClient* client = (HttpClient*)handle;

    client->stop();
}

static const char* HTTPRequestTypes[] = {
    "GET",
    "POST",
    "PUT",
    "DELETE",
    "PATCH"
};

HTTPAPI_RESULT HTTPAPI_ExecuteRequest(HTTP_HANDLE handle,
        HTTPAPI_REQUEST_TYPE requestType, const char* relativePath,
        HTTP_HEADERS_HANDLE httpHeadersHandle, const unsigned char* content,
        size_t contentLength, unsigned int* statusCode,
        HTTP_HEADERS_HANDLE responseHeadersHandle,
        BUFFER_HANDLE responseContent)
{
    int result;
    size_t headersCount;
    char* header;

    HttpClient* client = (HttpClient*)handle;

    client->beginRequest();

    result = client->startRequest(relativePath, HTTPRequestTypes[requestType]);
    if (result) {
        LogError("HTTPS send request failed\n");
        return HTTPAPI_SEND_REQUEST_FAILED;
    }

    HTTPHeaders_GetHeaderCount(httpHeadersHandle, &headersCount);

    for (size_t i = 0; i < headersCount; i++) {
        HTTPHeaders_GetHeader(httpHeadersHandle, i, &header);
        client->sendHeader(header);
        free(header);
    }

    client->endRequest();

    result = client->write(content, contentLength);
    if (result < contentLength) {
        LogError("HTTPS send body failed\n");
        return HTTPAPI_SEND_REQUEST_FAILED;
    }

    result = client->responseStatusCode();
    if (result == -1) {
        return HTTPAPI_STRING_PROCESSING_ERROR;
    }
    *statusCode = result;

    while (client->headerAvailable()) {
        String headerName = client->readHeaderName();
        String headerValue = client->readHeaderValue();

        HTTPHeaders_AddHeaderNameValuePair(responseHeadersHandle, headerName.c_str(), headerValue.c_str());
    }

    if (result == -1) {
        LogError("HTTPS header parsing failed\n");
        return HTTPAPI_HTTP_HEADERS_FAILED;
    }

    contentLength = client->contentLength();
    if (contentLength) {
        BUFFER_pre_build(responseContent, contentLength);
        client->read(BUFFER_u_char(responseContent), contentLength);
    }

    return HTTPAPI_OK;
}

HTTPAPI_RESULT HTTPAPI_SetOption(HTTP_HANDLE handle, const char* optionName,
        const void* value)
{
    HttpClient* client = (HttpClient*)handle;
    HTTPAPI_RESULT result = HTTPAPI_INVALID_ARG;

    if (strcmp("timeout", optionName) == 0) {
        client->setHttpResponseTimeout(*(const unsigned int*)value);

        result = HTTPAPI_OK;
    }

    return result;
}

HTTPAPI_RESULT HTTPAPI_CloneOption(const char* optionName, const void* value,
        const void** savedValue)
{
    HTTPAPI_RESULT result = HTTPAPI_INVALID_ARG;

    if (strcmp("timeout", optionName) == 0) {
        unsigned int* timeout = (unsigned int*)malloc(sizeof(unsigned int));

        *timeout = *(const unsigned int*)value;

        *savedValue = &timeout;

        result = HTTPAPI_OK;
    }

    return result;
}
