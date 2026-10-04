#include "bongo_cat/file.h"
#include "bongo_cat/image.h"
#include "bongo_cat/safe_ffi.h"
#include "bongo_cat/sha256.h"
#include "test.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int bongo_cat_test_failures;

/* Exercises the bongo-safe Rust FFI through the C wrappers: SHA-256
   vectors, image decoding bounds, the about-page feed parser, and the
   Live2D expression parser. */

static void sha256_vectors(void) {
    char digest[65];
    bongo_cat_sha256_bytes("", 0, digest);
    CHECK(!strcmp(digest,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    bongo_cat_sha256_bytes("abc", 3, digest);
    CHECK(!strcmp(digest,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    bongo_cat_sha256_bytes(NULL, 0, digest);
    CHECK(!strcmp(digest,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

static void sha256_file_roundtrip(void) {
    /* Keep the test hermetic: write next to the process's working directory. */
    char path[BONGO_CAT_PATH_CAP];
    SDL_snprintf(path, sizeof(path), "bongo-safe-hash-test.tmp");
    const char *payload = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(bongo_cat_file_append(path, payload, strlen(payload)));
    char digest[65];
    BongoCatResult result = bongo_cat_sha256_file(path, digest, NULL);
    CHECK(result == BONGO_CAT_OK);
    CHECK(!strcmp(digest,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    CHECK(bongo_cat_file_remove(path));
    CHECK(bongo_cat_sha256_file(path, digest, NULL) == BONGO_CAT_ERROR_IO);
}

/* 1x1 opaque red PNG. */
static const unsigned char RED_PNG[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xDE, 0x00, 0x00, 0x00,
    0x0C, 0x49, 0x44, 0x41, 0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
    0x00, 0x03, 0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D, 0xB0, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

/* 1x1 opaque red PNG, base64 encoded. */
#define RED_PNG_BASE64 \
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAADElEQVR4nGP4z8AAAAMBAQ" \
    "DJ/pLvAAAAAElFTkSuQmCC"

static void image_decode_bounds(void) {
    int width = 0, height = 0;
    unsigned char *pixels = bongo_safe_image_decode(RED_PNG, sizeof(RED_PNG),
        &width, &height);
    CHECK(width == 1 && height == 1);
    CHECK(pixels && pixels[3] == 255);
    bongo_safe_free_pixels(pixels, (size_t)width * height * 4);
    width = height = -1;
    CHECK(!bongo_safe_image_info((const unsigned char *)"garbage-bytes", 13,
        &width, &height));
    CHECK(!width && !height);
    CHECK(bongo_safe_image_info(RED_PNG, sizeof(RED_PNG), &width, &height));
    CHECK(width == 1 && height == 1);
    CHECK(!bongo_safe_image_decode((const unsigned char *)"garbage-bytes", 13,
        &width, &height));
    CHECK(!width && !height);
}

static bool not_cancelled(void *userdata) {
    (void)userdata;
    return false;
}

static void feed_parses(void) {
    const char *head =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
        "<a href=\"https://github.com/octo\"><title>O&amp;cto</title>"
        "<image xlink:href=\"data:image/png;base64,";
    const char *tail =
        "\"/></a>"
        "<a href=\"https://github.com/empty\"><title>Nobody</title></a>"
        "</svg>";
    const size_t head_length = strlen(head), base64_length = strlen(RED_PNG_BASE64),
        tail_length = strlen(tail);
    char *svg = malloc(head_length + base64_length + tail_length + 1);
    CHECK(svg);
    if (!svg) return;
    memcpy(svg, head, head_length);
    memcpy(svg + head_length, RED_PNG_BASE64, base64_length);
    memcpy(svg + head_length + base64_length, tail, tail_length + 1);
    bongo_safe_about_person *persons = NULL;
    int count = bongo_safe_about_feed_parse(svg, strlen(svg), not_cancelled,
        NULL, 8, 64, &persons);
    CHECK(count == 1); /* contributors without a decoded avatar are dropped */
    if (count == 1) {
        CHECK(persons && persons[0].name && persons[0].profile &&
            persons[0].pixels);
        CHECK(!strcmp(persons[0].name, "O&cto"));
        CHECK(!strcmp(persons[0].profile, "https://github.com/octo"));
        const unsigned char *pixels = persons[0].pixels;
        CHECK(pixels[(4 * 8 + 4) * 4 + 3] == 255); /* center stays inside the circle */
        CHECK(pixels[3] == 0); /* corners fall outside the inscribed circle */
        bongo_safe_free_pixels(persons[0].pixels, 8 * 8 * 4);
        bongo_safe_free_persons(persons, count);
    }
    free(svg);
    /* Hostile inputs fail cleanly. */
    const char *nested =
        "<svg><a href=\"https://a\"><a href=\"https://b\"/></a></svg>";
    persons = (bongo_safe_about_person *)(size_t)1;
    CHECK(bongo_safe_about_feed_parse(nested, strlen(nested), not_cancelled,
        NULL, 8, 64, &persons) == -1);
    CHECK(persons == (bongo_safe_about_person *)(size_t)1);
    const char *doctype = "<!DOCTYPE svg><svg><a href=\"https://a\"/></svg>";
    CHECK(bongo_safe_about_feed_parse(doctype, strlen(doctype), not_cancelled,
        NULL, 8, 64, &persons) == -1);
}

/* Expression files route through the Rust crate: the bundled Cubism JSON
   parser cannot read the scientific notation Cubism Editor exports
   (upstream issue #68). */
static void expression_parses(void) {
    const char *json =
        "{\n"
        "  \"Type\": \"Live2D Expression\",\n"
        "  \"FadeInTime\": 0.25,\n"
        "  \"FadeOutTime\": 0.5,\n"
        "  \"Parameters\": [\n"
        "    {\"Id\": \"Param707\", \"Value\": -2.980232238769531e-7, \"Blend\": \"Add\"},\n"
        "    {\"Id\": \"ParamAngleX\", \"Value\": 15, \"Blend\": \"Multiply\"},\n"
        "    {\"Id\": \"ParamMouthOpenY\", \"Value\": 0.75, \"Blend\": \"Overwrite\"}\n"
        "  ]\n"
        "}\n";
    BongoSafeExpression *expression = (BongoSafeExpression *)(size_t)1;
    CHECK(bongo_safe_expression_parse((const unsigned char *)json, strlen(json),
        &expression));
    if (expression != (BongoSafeExpression *)(size_t)1) {
        CHECK(fabsf(expression->fade_in_seconds - 0.25f) < 1e-6f);
        CHECK(fabsf(expression->fade_out_seconds - 0.5f) < 1e-6f);
        CHECK(expression->parameter_count == 3);
        const BongoSafeExpressionParameter *parameters = expression->parameters;
        CHECK(parameters && !strcmp(parameters[0].id, "Param707"));
        CHECK(fabsf(parameters[0].value + 2.980232238769531e-7f) < 1e-12f);
        CHECK(parameters[0].blend == BONGO_SAFE_EXPRESSION_BLEND_ADD);
        CHECK(!strcmp(parameters[1].id, "ParamAngleX"));
        CHECK(fabsf(parameters[1].value - 15.0f) < 1e-6f);
        CHECK(parameters[1].blend == BONGO_SAFE_EXPRESSION_BLEND_MULTIPLY);
        CHECK(!strcmp(parameters[2].id, "ParamMouthOpenY"));
        CHECK(fabsf(parameters[2].value - 0.75f) < 1e-6f);
        CHECK(parameters[2].blend == BONGO_SAFE_EXPRESSION_BLEND_OVERWRITE);
        bongo_safe_free_expression(expression);
    }
    /* Missing fades default to one second; unknown shapes stay empty. */
    const char *defaults = "{}";
    expression = NULL;
    CHECK(bongo_safe_expression_parse((const unsigned char *)defaults,
        strlen(defaults), &expression));
    CHECK(expression && expression->parameter_count == 0 &&
        fabsf(expression->fade_in_seconds - 1.0f) < 1e-6f &&
        fabsf(expression->fade_out_seconds - 1.0f) < 1e-6f);
    bongo_safe_free_expression(expression);
    /* Hostile inputs fail cleanly and leave the output untouched. */
    expression = (BongoSafeExpression *)(size_t)1;
    CHECK(!bongo_safe_expression_parse((const unsigned char *)"garbage", 7,
        &expression));
    CHECK(expression == (BongoSafeExpression *)(size_t)1);
    CHECK(!bongo_safe_expression_parse(NULL, 4, &expression));
    CHECK(!bongo_safe_expression_parse((const unsigned char *)"{}", 2, NULL));
    bongo_safe_free_expression(NULL);
}

int main(void) {
    sha256_vectors();
    sha256_file_roundtrip();
    image_decode_bounds();
    feed_parses();
    expression_parses();
    if (bongo_cat_test_failures) {
        fprintf(stderr, "%d checks failed\n", bongo_cat_test_failures);
        return 1;
    }
    puts("all safe-ffi checks passed");
    return 0;
}
