#include "TLS.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

/* Global SSL contexts */
SSL_CTX *tls_server_ctx = NULL;
SSL_CTX *tls_client_ctx = NULL;

/* Static buffer for error strings */
static char tls_error_buffer[256] = {0};

/**
 * Get human-readable error from OpenSSL error queue
 */
const char *tls_get_error_string(void)
{
    unsigned long err = ERR_get_error();
    if (err == 0) {
        snprintf(tls_error_buffer, sizeof(tls_error_buffer), "No error");
        return tls_error_buffer;
    }
    
    ERR_error_string_n(err, tls_error_buffer, sizeof(tls_error_buffer));
    return tls_error_buffer;
}

/**
 * Initialize server-side SSL context
 */
int tls_init_server(const char *cert_file, const char *key_file)
{
    if (tls_server_ctx != NULL) {
        fprintf(stderr, "[TLS] Server context already initialized\n");
        return -1;
    }
    
    if (cert_file == NULL || key_file == NULL) {
        fprintf(stderr, "[TLS] Certificate and key files required\n");
        return -1;
    }
    
    /* Initialize OpenSSL library (call once) */
    static int ssl_initialized = 0;
    if (!ssl_initialized) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        ssl_initialized = 1;
    }
    
    /* Create server context using TLS 1.2 or higher */
    tls_server_ctx = SSL_CTX_new(TLS_server_method());
    if (tls_server_ctx == NULL) {
        fprintf(stderr, "[TLS] Failed to create server context: %s\n",
                tls_get_error_string());
        return -1;
    }
    
    /* Set minimum TLS version */
    SSL_CTX_set_min_proto_version(tls_server_ctx, TLS1_2_VERSION);
    
    /* Load certificate file */
    if (SSL_CTX_use_certificate_file(tls_server_ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "[TLS] Failed to load certificate '%s': %s\n",
                cert_file, tls_get_error_string());
        SSL_CTX_free(tls_server_ctx);
        tls_server_ctx = NULL;
        return -1;
    }
    
    /* Load private key file */
    if (SSL_CTX_use_PrivateKey_file(tls_server_ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        fprintf(stderr, "[TLS] Failed to load private key '%s': %s\n",
                key_file, tls_get_error_string());
        SSL_CTX_free(tls_server_ctx);
        tls_server_ctx = NULL;
        return -1;
    }
    
    /* Verify that cert and key match */
    if (!SSL_CTX_check_private_key(tls_server_ctx)) {
        fprintf(stderr, "[TLS] Private key does not match certificate\n");
        SSL_CTX_free(tls_server_ctx);
        tls_server_ctx = NULL;
        return -1;
    }
    
    printf("[TLS] Server context initialized successfully\n");
    printf("[TLS] Certificate: %s\n", cert_file);
    printf("[TLS] Private key: %s\n", key_file);
    
    return 0;
}

/**
 * Initialize client-side SSL context
 */
int tls_init_client(void)
{
    if (tls_client_ctx != NULL) {
        fprintf(stderr, "[TLS] Client context already initialized\n");
        return -1;
    }
    
    /* Initialize OpenSSL library (call once) */
    static int ssl_initialized = 0;
    if (!ssl_initialized) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        ssl_initialized = 1;
    }
    
    /* Create client context using TLS 1.2 or higher */
    tls_client_ctx = SSL_CTX_new(TLS_client_method());
    if (tls_client_ctx == NULL) {
        fprintf(stderr, "[TLS] Failed to create client context: %s\n",
                tls_get_error_string());
        return -1;
    }
    
    /* Set minimum TLS version */
    SSL_CTX_set_min_proto_version(tls_client_ctx, TLS1_2_VERSION);
    
    /* For testing: disable peer cert verification (NOT for production!) */
    SSL_CTX_set_verify(tls_client_ctx, SSL_VERIFY_NONE, NULL);
    
    printf("[TLS] Client context initialized successfully\n");
    
    return 0;
}

/**
 * Perform TLS handshake
 * Handles both blocking and non-blocking modes
 */
int tls_handshake(SSL *ssl, int is_server)
{
    if (ssl == NULL)
        return -1;
    
    int result;
    
    if (is_server) {
        result = SSL_accept(ssl);
    } else {
        result = SSL_connect(ssl);
    }
    
    /* Handshake complete */
    if (result == 1) {
        return 1;
    }
    
    /* Check error */
    int err = SSL_get_error(ssl, result);
    
    switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            /* Handshake in progress, try again later */
            return 0;
        
        case SSL_ERROR_ZERO_RETURN:
            /* Peer closed connection during handshake */
            fprintf(stderr, "[TLS] Handshake failed: peer closed connection\n");
            return -1;
        
        case SSL_ERROR_SSL:
            fprintf(stderr, "[TLS] Handshake failed: %s\n", tls_get_error_string());
            return -1;
        
        default:
            fprintf(stderr, "[TLS] Handshake failed with error code %d\n", err);
            return -1;
    }
}

/**
 * Cleanup SSL contexts at shutdown
 */
void tls_cleanup(void)
{
    if (tls_server_ctx != NULL) {
        SSL_CTX_free(tls_server_ctx);
        tls_server_ctx = NULL;
    }
    
    if (tls_client_ctx != NULL) {
        SSL_CTX_free(tls_client_ctx);
        tls_client_ctx = NULL;
    }
    
    ERR_free_strings();
    EVP_cleanup();
    
    printf("[TLS] SSL cleanup complete\n");
}