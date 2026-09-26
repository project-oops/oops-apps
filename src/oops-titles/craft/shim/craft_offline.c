/*
 * What `auth.c` and `client.c` export, for a port that compiles neither.
 *
 * Both are the multiplayer half: `auth.c` fetches an access token over HTTPS through
 * libcurl, and `client.c` talks to a server over BSD sockets. This port is
 * single-player, so the Makefile excludes both rather than shimming libcurl and a
 * socket layer, and these supply the symbols `main.c` calls.
 *
 * `main.c` calls the client unconditionally and guards nothing on the connection, so
 * every entry point has to exist. They do nothing, which is what upstream's own
 * `client.c` does while the client is disabled - `get_client_enabled` answers 0 and
 * `client_recv` answers NULL, so `main.c` takes its offline branch at every point it
 * asks.
 */

#include <stddef.h>

#include "auth.h"
#include "client.h"

int get_access_token(char *result, int length, char *username, char *identity_token) {
    (void)username;
    (void)identity_token;
    if (result && length > 0)
        result[0] = '\0';
    return 0; /* upstream's own "no token", which main.c already handles */
}

/* The client is never enabled, so nothing below is reached with a live connection. */
int get_client_enabled(void) {
    return 0;
}

void client_enable(void) {}
void client_disable(void) {}
void client_connect(char *hostname, int port) {
    (void)hostname;
    (void)port;
}
void client_start(void) {}
void client_stop(void) {}
void client_send(char *data) {
    (void)data;
}

/* NULL is upstream's "nothing arrived", which `main.c` polls for and skips. */
char *client_recv(void) {
    return NULL;
}

void client_version(int version) {
    (void)version;
}
void client_login(const char *username, const char *identity_token) {
    (void)username;
    (void)identity_token;
}
void client_position(float x, float y, float z, float rx, float ry) {
    (void)x;
    (void)y;
    (void)z;
    (void)rx;
    (void)ry;
}
void client_chunk(int p, int q, int key) {
    (void)p;
    (void)q;
    (void)key;
}
void client_block(int x, int y, int z, int w) {
    (void)x;
    (void)y;
    (void)z;
    (void)w;
}
void client_light(int x, int y, int z, int w) {
    (void)x;
    (void)y;
    (void)z;
    (void)w;
}
void client_sign(int x, int y, int z, int face, const char *text) {
    (void)x;
    (void)y;
    (void)z;
    (void)face;
    (void)text;
}
void client_talk(const char *text) {
    (void)text;
}
