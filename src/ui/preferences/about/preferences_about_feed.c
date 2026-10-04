#include "preferences_about_feed.h"
#include "bongo_cat/safe_ffi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parsing, base64 and avatar decoding run in the bongo-safe Rust crate:
   the SVG embeds contributor-controlled network data. This file only moves
   the results into the UI-owned structures. */

static bool about_cancelled(void *userdata) {
    return SDL_GetAtomicInt((SDL_AtomicInt *)userdata) != 0;
}

BongoCatAboutFeed *bongo_cat_about_feed_parse(char *svg, SDL_AtomicInt *cancel) {
    if (!svg || !cancel) return NULL;
    bongo_safe_about_person *persons = NULL;
    int count = bongo_safe_about_feed_parse(svg, strlen(svg), about_cancelled,
        cancel, BONGO_ABOUT_AVATAR_SIZE, BONGO_ABOUT_CONTRIBUTOR_CAP, &persons);
    if (count < 0) return NULL;
    BongoCatAboutFeed *feed = calloc(1, sizeof(*feed));
    if (!feed) {
        bongo_safe_free_persons(persons, count);
        return NULL;
    }
    for (int i = 0; i < count; ++i) {
        BongoCatAboutContributor *person = &feed->people[feed->count++];
        snprintf(person->name, sizeof(person->name), "%s",
            persons[i].name ? persons[i].name : "");
        snprintf(person->profile, sizeof(person->profile), "%s",
            persons[i].profile ? persons[i].profile : "");
        person->pixels = persons[i].pixels;
    }
    bongo_safe_free_persons(persons, count);
    return feed;
}

void bongo_cat_about_feed_free(BongoCatAboutFeed *feed) {
    if (!feed) return;
    for (int i = 0; i < feed->count; i++)
        bongo_safe_free_pixels(feed->people[i].pixels,
            (size_t)BONGO_ABOUT_AVATAR_SIZE * BONGO_ABOUT_AVATAR_SIZE * 4);
    free(feed);
}
