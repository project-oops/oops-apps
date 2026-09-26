/*
 * The one function `auth.c` exports, for a port that does not compile it.
 *
 * `upstream/src/auth.c` fetches a multiplayer access token over HTTPS through libcurl.
 * This port is single-player, so the Makefile excludes the file rather than shimming
 * libcurl, and this supplies the symbol `main.c` calls. It returns 0, upstream's own
 * "no token" on a network error, and `main.c` continues with the local world.
 */

/* Upstream's prototype from `upstream/src/auth.h`, declared here so this file builds
 * without the fetched tree. */
int get_access_token(char *result, int length, char *username, char *identity_token);

int get_access_token(char *result, int length, char *username, char *identity_token) {
    (void)username;
    (void)identity_token;
    if (result && length > 0)
        result[0] = '\0';
    return 0; /* upstream's own "no token", which main.c already handles */
}
