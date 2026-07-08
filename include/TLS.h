
#ifndef TLS_H
#define TLS_H
 
#include <openssl/ssl.h>
#include <openssl/err.h>
 
/* Global SSL context (initialized once at server startup) */
extern SSL_CTX *tls_server_ctx;
extern SSL_CTX *tls_client_ctx;
 
// tls for server
int tls_init_server(const char *cert_file, const char *key_file);

// tls for client
int tls_init_client(void);

// tls handshake that occurs
int tls_handshake(SSL *ssl, int is_server);
 
const char *tls_get_error_string(void);
 
// clean up the tls 
void tls_cleanup(void);
 
#endif
