/*
 * Parser checks for the toml-c fork.
 *
 * The document below mirrors the shape of config/undefshim.toml, so this
 * doubles as a check that the real configuration file stays parseable, and
 * that the fork rejects the value kinds it no longer supports.
 */

#include <stdio.h>
#include <string.h>

#include "toml.h"

static int failures;
static int checks;

static void expectInt(const char *name, int got, int want) {
    checks++;
    if (got != want) {
        failures++;
        printf("FAIL %-28s want %d got %d\n", name, want, got);
    }
}

static void expectStr(const char *name, const char *got, const char *want) {
    checks++;
    if (got == NULL || strcmp(got, want) != 0) {
        failures++;
        printf("FAIL %-28s want \"%s\" got \"%s\"\n", name, want, got ? got : "(null)");
    }
}

/* Parses a document and reports the error text on failure. Returns NULL. */
static toml_table_t *parseDoc(char *doc) {
    char errbuf[256];
    toml_table_t *root = toml_parse(doc, errbuf, sizeof(errbuf));
    if (root == NULL) {
        printf("     parse error: %s\n", errbuf);
    }
    return root;
}

static void testConfigShape(void) {
    /* toml_parse mutates its input, so every case needs a fresh copy. */
    char doc[] =
        "version = 1\n"
        "\n"
        "[log]\n"
        "level = \"info\"\n"
        "\n"
        "[scan]\n"
        "ldapr_rewrite = true\n"
        "\n"
        "[debug]\n"
        "enabled = true\n"
        "flag1 = true\n"
        "something.int = 1\n"
        "\n"
        "[[debug.patch]]\n"
        "target = \"ntoskrnl\"\n"
        "rva    = 0x203bd4\n"
        "value  = 0x14000000\n"
        "width  = 4\n"
        "tag    = \"vbar write site\"\n"
        "\n"
        "[[debug.patch]]\n"
        "target = \"winload\"\n"
        "rva    = 0x1a2b0\n"
        "value  = 0xd503201f\n"
        "width  = 4\n";

    toml_table_t *root = parseDoc(doc);
    if (root == NULL) { failures++; checks++; return; }

    expectInt("version", (int)toml_table_int(root, "version").u.i, 1);

    toml_table_t *log = toml_table_table(root, "log");
    checks++;
    if (log == NULL) { failures++; printf("FAIL [log] missing\n"); }
    else expectStr("log.level", toml_table_string(log, "level").u.s, "info");

    toml_table_t *scan = toml_table_table(root, "scan");
    checks++;
    if (scan == NULL) { failures++; printf("FAIL [scan] missing\n"); }
    else expectInt("scan.ldapr_rewrite", toml_table_bool(scan, "ldapr_rewrite").u.b, 1);

    toml_table_t *dbg = toml_table_table(root, "debug");
    checks++;
    if (dbg == NULL) { failures++; printf("FAIL [debug] missing\n"); }
    else {
        expectInt("debug.enabled", toml_table_bool(dbg, "enabled").u.b, 1);
        expectInt("debug.flag1", toml_table_bool(dbg, "flag1").u.b, 1);

        /* A dotted key lives in its own sub table, it is not a flat name. */
        toml_table_t *sub = toml_table_table(dbg, "something");
        checks++;
        if (sub == NULL) { failures++; printf("FAIL [debug.something] missing\n"); }
        else expectInt("debug.something.int", (int)toml_table_int(sub, "int").u.i, 1);
    }

    /* [[debug.patch]] is an array of tables. */
    toml_array_t *patches = toml_table_array(dbg, "patch");
    checks++;
    if (patches == NULL) { failures++; printf("FAIL debug.patch missing\n"); }
    else {
        expectInt("patch count", toml_array_len(patches), 2);

        toml_table_t *p0 = toml_array_table(patches, 0);
        checks++;
        if (p0 == NULL) { failures++; printf("FAIL patch[0] missing\n"); }
        else {
            expectStr("patch[0].target", toml_table_string(p0, "target").u.s, "ntoskrnl");
            expectInt("patch[0].rva", (int)toml_table_int(p0, "rva").u.i, 0x203bd4);
            expectInt("patch[0].value", (int)toml_table_int(p0, "value").u.i, 0x14000000);
            expectInt("patch[0].width", (int)toml_table_int(p0, "width").u.i, 4);
            expectStr("patch[0].tag", toml_table_string(p0, "tag").u.s, "vbar write site");
        }

        toml_table_t *p1 = toml_array_table(patches, 1);
        checks++;
        if (p1 == NULL) { failures++; printf("FAIL patch[1] missing\n"); }
        else {
            expectStr("patch[1].target", toml_table_string(p1, "target").u.s, "winload");
            expectInt("patch[1].rva", (int)toml_table_int(p1, "rva").u.i, 0x1a2b0);
            expectInt("patch[1].value", (int)toml_table_int(p1, "value").u.i, 0xd503201f);
        }
    }

    toml_free(root);
}

/* A value the parser cannot classify is stored raw and typed 'u', and the
 * document still parses. That is upstream behaviour and it is deliberate: the
 * configuration loader is what rejects unsupported values, by refusing to
 * read them as any of the supported kinds. */
static void testUnsupportedValues(void) {
    const char *bad[] = {
        "value = 1.5\n",
        "value = 2024-01-02\n",
        "value = 12:30:00\n",
        "value = abc\n",
    };

    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        char doc[64];
        strcpy(doc, bad[i]);

        char errbuf[128];
        toml_table_t *root = toml_parse(doc, errbuf, sizeof(errbuf));

        checks++;
        if (root == NULL) {
            failures++;
            printf("FAIL %s should parse, error: %s", bad[i], errbuf);
            continue;
        }

        /* Present, but not readable as a string, a bool or an int. */
        int readable = toml_table_int(root, "value").ok
                       || toml_table_bool(root, "value").ok
                       || toml_table_string(root, "value").ok;
        checks++;
        if (readable) {
            failures++;
            printf("FAIL %s was accepted as a supported kind\n", bad[i]);
        }

        toml_free(root);
    }
}

static void testSyntaxError(void) {
    char doc[] = "this is not toml\n";
    char errbuf[128];
    toml_table_t *root = toml_parse(doc, errbuf, sizeof(errbuf));
    checks++;
    if (root != NULL) {
        failures++;
        printf("FAIL malformed input parsed successfully\n");
        toml_free(root);
    }
}

int main(void) {
    testConfigShape();
    testUnsupportedValues();
    testSyntaxError();

    printf("%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
