/*
 * The one function `auth.c` exported, for a port that does not compile it.
 *
 * # Why the file is excluded rather than the dependency stubbed
 *
 * `upstream/src/auth.c` is the only file in Craft that includes <curl/curl.h>. It does
 * one thing: POST a username and an identity token to a multiplayer server over HTTPS
 * and hand back an access token. Porting that means porting libcurl and a TLS stack to
 * this target, for a feature this port does not have - Craft on a console is the
 * single-player game.
 *
 * The alternative to excluding it would be a libcurl shim whose `curl_easy_perform`
 * fails, and that is worse in a specific way: the game would offer online play, try it,
 * and fail in a dialog that says nothing about why. **A login that always fails is a
 * bug report; a login that is not there is a scope.**
 *
 * So `Makefile` filters `auth.c` out of the source list, and this supplies the symbol
 * `main.c` calls. It returns 0 - the same "could not get a token" that upstream returns
 * on a network error - and `main.c` already handles that: it carries on with the local
 * world, which is exactly where this port wants to be.
 */

/* Upstream's prototype, from `upstream/src/auth.h`. Not included, because the header is
 * inside the fetched tree and this file is deliberately buildable without a particular
 * one. */
int get_access_token(char *result, int length, char *username, char *identity_token);

int get_access_token(char *result, int length, char *username, char *identity_token) {
    (void)username;
    (void)identity_token;
    if (result && length > 0)
        result[0] = '\0';
    return 0; /* upstream's own "no token", which main.c already handles */
}
