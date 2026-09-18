#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    void *pointer;
    size_t size;
} allocation;

static allocation allocations[4096];
static int allocation_calls;
static int fail_allocation;
static int allocator_errors;
static int checks;
static int failures;

static void check(int condition, const char *description)
{
    checks++;
    if (!condition) {
        failures++;
        fprintf(stderr, "FAIL: %s\n", description);
    }
}

static int find_allocation(void *pointer)
{
    for (int i = 0; i < 4096; i++) {
        if (allocations[i].pointer == pointer) {
            return i;
        }
    }
    return -1;
}

static void remember(void *pointer, size_t size)
{
    if (!pointer) {
        return;
    }
    int slot = find_allocation(NULL);
    if (slot < 0) {
        fprintf(stderr, "Allocation tracking exhausted\n");
        exit(2);
    }
    allocations[slot].pointer = pointer;
    allocations[slot].size = size;
}

static int allocation_should_fail(void)
{
    allocation_calls++;
    return fail_allocation && allocation_calls == fail_allocation;
}

static void *tracked_malloc(size_t size)
{
    if (allocation_should_fail()) {
        return NULL;
    }
    void *pointer = malloc(size);
    if (pointer) {
        // Make initialization failures sensitive to uninitialized-pointer cleanup.
        memset(pointer, 0xa5, size);
    }
    remember(pointer, size);
    return pointer;
}

static void tracked_free(void *pointer)
{
    if (!pointer) {
        return;
    }
    int slot = find_allocation(pointer);
    if (slot < 0) {
        allocator_errors++;
        fprintf(stderr, "Free of untracked pointer\n");
        return;
    }
    allocations[slot].pointer = NULL;
    allocations[slot].size = 0;
    free(pointer);
}

static void *tracked_realloc(void *pointer, size_t size)
{
    if (allocation_should_fail()) {
        return NULL;
    }
    int slot = pointer ? find_allocation(pointer) : -1;
    if (pointer && slot < 0) {
        fprintf(stderr, "Realloc of untracked pointer\n");
        exit(2);
    }
    void *replacement = realloc(pointer, size);
    if (!replacement) {
        return NULL;
    }
    if (slot < 0) {
        remember(replacement, size);
    } else {
        allocations[slot].pointer = replacement;
        allocations[slot].size = size;
    }
    return replacement;
}

// Instrument only the real parser's allocations, not the test or tokenizer.
#define malloc tracked_malloc
#define realloc tracked_realloc
#define free tracked_free
#include XML_PARSER_SOURCE
#undef malloc
#undef realloc
#undef free

void log_error(const char *message, const char *parameter, int number)
{
    (void) message;
    (void) parameter;
    (void) number;
}

static char last_text[256];

static void capture_text(const char *text)
{
    snprintf(last_text, sizeof(last_text), "%s", text);
}

static const xml_parser_element elements[] = {
    { "campaign_localization", 0, 0, 0, capture_text },
    { "name", 0, 0, "campaign_localization", capture_text },
    { "description", 0, 0, "campaign_localization", capture_text },
};

static const char *VALID = "<campaign_localization version=\"1\"><name>Campaign</name></campaign_localization>";
static const char *MISMATCH = "<campaign_localization version=\"1\"><name>Campaign</description></campaign_localization>";

static int init_parser(void)
{
    return xml_parser_init(elements, sizeof(elements) / sizeof(elements[0]), 1);
}

static int parse(const char *xml)
{
    return xml_parser_parse(xml, (unsigned int) strlen(xml), 1);
}

static void check_no_allocations(const char *label)
{
    int count = 0;
    size_t bytes = 0;
    for (int i = 0; i < 4096; i++) {
        if (allocations[i].pointer) {
            count++;
            bytes += allocations[i].size;
        }
    }
    printf("%s: outstanding=%d bytes=%zu\n", label, count, bytes);
    check(count == 0, label);
    // Isolate baseline failures so later cases still run independently.
    for (int i = 0; i < 4096; i++) {
        if (allocations[i].pointer) {
            free(allocations[i].pointer);
            memset(&allocations[i], 0, sizeof(allocations[i]));
        }
    }
}

static void test_parse_and_free(const char *label, const char *xml, int expected_parse)
{
    check(init_parser(), "initialize parser");
    check(parse(xml) == expected_parse, label);
    xml_parser_free();
    xml_parser_free();
    check_no_allocations(label);
}

static void test_reset(int after_error)
{
    check(init_parser(), "initialize before reset");
    check(parse(after_error ? MISMATCH : VALID) == !after_error, "parse before reset");
    xml_parser_reset();
    last_text[0] = 0;
    check(parse("<campaign_localization version=\"1\"><name>Second</name></campaign_localization>"),
        "parse after reset");
    check(strcmp(last_text, "Second") == 0, "reset discards previous document text");
    xml_parser_reset();
    xml_parser_free();
    check_no_allocations(after_error ? "reset-after-error" : "reset-after-success");
}

static void test_init_failures(void)
{
    for (int failure = 1; failure <= 4; failure++) {
        check(init_parser(), "initialize before failed reinitialization");
        check(!parse(MISMATCH), "leave pending text before failed reinitialization");
        allocation_calls = 0;
        fail_allocation = failure;
        check(!init_parser(), "allocation failure makes init fail");
        check(allocation_calls >= failure, "injected init allocation failure was reached");
        fail_allocation = 0;
        xml_parser_free();
        check_no_allocations("failed-reinitialization");
        check(init_parser(), "recover after failed initialization");
        check(parse(VALID), "parse after failed initialization");
        xml_parser_free();
        check_no_allocations("recovered-initialization");
    }
    xml_parser_element invalid_elements[] = { { 0 } };
    check(!xml_parser_init(invalid_elements, 1, 1), "invalid element name makes init fail");
    xml_parser_free();
    check_no_allocations("invalid-element-initialization");
}

int main(void)
{
    test_parse_and_free("valid-control", VALID, 1);
    test_parse_and_free("mismatched-close", MISMATCH, 0);
    test_parse_and_free("truncated", "<campaign_localization version=\"1\"><name>Campaign", 0);
    test_parse_and_free("unknown-child", "<campaign_localization><name>Campaign<invalid/></name></campaign_localization>", 0);

    char long_mismatch[8500];
    const char *prefix = "<campaign_localization><name>";
    size_t prefix_length = strlen(prefix);
    memcpy(long_mismatch, prefix, prefix_length);
    memset(long_mismatch + prefix_length, 'a', 8192);
    strcpy(long_mismatch + prefix_length + 8192, "</description></campaign_localization>");
    test_parse_and_free("long-mismatched-close", long_mismatch, 0);

    for (int i = 0; i < 20; i++) {
        check(init_parser(), "initialize repeated invalid document");
        check(!parse(MISMATCH), "reject repeated invalid document");
        xml_parser_free();
    }
    check_no_allocations("repeated-invalid-documents");
    test_reset(0);
    test_reset(1);
    test_init_failures();
    check(allocator_errors == 0, "no invalid free or uninitialized-pointer cleanup");
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
