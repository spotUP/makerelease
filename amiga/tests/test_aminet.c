#include "aminet.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)

/* Scripted FTP server: control replies are fed in order after each command; data
 * connections swallow what is sent. Everything sent is logged per handle. */
static char ctl_log[2048], data_log[2048];
static int pasv_fail, stor_code;
static const char *pending;       /* control bytes waiting to be read */
static int opens, closes;

static int f_open(void *c, const char *host, int port, char *e, size_t ec) {
    (void)c; (void)e; (void)ec;
    opens++;
    if (opens == 1) { CHECK(!strcmp(host, "main.aminet.net") && port == 21); pending = "220-hi\r\n220 ready\r\n"; return 10; }
    CHECK(!strcmp(host, "1.2.3.4") && port == 1029);
    return 11;
}
static int f_send(void *c, int h, const char *d, size_t n) {
    char *log = h == 10 ? ctl_log : data_log;
    (void)c;
    snprintf(log + strlen(log), 2048 - strlen(log), "%.*s", (int)n, d);
    if (h == 10) {
        if (!strncmp(d, "USER", 4)) pending = "331 pw\r\n";
        else if (!strncmp(d, "PASS", 4)) pending = "230 ok\r\n";
        else if (!strncmp(d, "TYPE", 4)) pending = "200 ok\r\n";
        else if (!strncmp(d, "CWD", 3)) pending = "250 ok\r\n";
        else if (!strncmp(d, "PASV", 4)) pending = pasv_fail ? "502 no\r\n" : "227 Entering Passive Mode (1,2,3,4,4,5)\r\n";
        else if (!strncmp(d, "STOR", 4)) pending = stor_code ? "553 denied\r\n" : "150 go\r\n";
        else pending = "221 bye\r\n";
    }
    return 0;
}
static int f_recv(void *c, int h, char *b, size_t cap) {
    size_t n = pending ? strlen(pending) : 0;
    (void)c; (void)h;
    if (!n) return 0;
    if (n > cap) n = cap;
    memcpy(b, pending, n); pending += n;
    return (int)n;
}
static void f_close(void *c, int h) {
    (void)c;
    closes++;
    /* closing a data connection makes the server confirm the transfer */
    if (h == 11) pending = "226 done\r\n";
}
static const rp_tcp_ops ops = { f_open, f_send, f_recv, f_close };

int main(void) {
    char a[64], r[64], msg[600];
    char *t;
    const char *good = "Short:        A demo\nUploader:     me@x.org (Me)\nType:         demo/aga\nArchitecture: m68k-amigaos\n\nbody\n";

    CHECK(!rp_aminet_names("UP-HYPOC.LHA", a, r, sizeof a));
    CHECK(!strcmp(a, "up-hypoc.lha") && !strcmp(r, "up-hypoc.readme"));
    t = rp_aminet_normalize_readme("a\xE4\r\nb\xE9\xFF\r");
    CHECK(t && !strcmp(t, "aae\nbe?\n")); free(t);

    {
        /* golden from the TS renderAminetReadme (app/src/shared/aminet-readme.ts), same input */
        static const rp_aminet_credit cr[] = {{"Code", "Spot"}, {"Music", "Dipswitch"}, {"", ""}};
        static const char *gold = "Short:        A500 HDD demo from Chaos Constructions\nUploader:     spot@uprough.net (Spot)\nAuthor:       Up Rough (Spot, Dipswitch)\nType:         demo/aga\nArchitecture: m68k-amigaos\nDistribution: Aminet\n\nHypocrisy, our AGA demo for Chaos Constructions 2025.\n\nNeeds 2MB chip and a hard drive.\nTested on A1200/030. word word word word word word word word word word word\nword word word word word word word word word\n\nCredits:\n  Code: Spot\n  Music: Dipswitch\n\nReleased by Up Rough - https://www.uprough.net\n";
        rp_aminet_readme_in in = {"A500 HDD demo from Chaos Constructions", "demo/aga", "m68k-amigaos",
            "spot@uprough.net", "Spot", "Up Rough", "https://www.uprough.net", "Spot, Dipswitch",
            "Hypocrisy, our AGA demo for Chaos Constructions 2025.",
            "\nHypocrisy, our AGA demo for Chaos Constructions 2025.\n\nNeeds 2MB chip and a hard drive.  \n"
            "Tested on A1200/030. word word word word word word word word word word word word word word word word word word word word\n",
            cr, 3};
        t = rp_aminet_render_readme(&in);
        CHECK(t && !strcmp(t, gold)); free(t);
    }

    CHECK(rp_aminet_problems(a, r, good, msg, sizeof msg) == 0);
    CHECK(rp_aminet_problems(a, r, "Short: \nUploader: bad\nType: Demo\n", msg, sizeof msg) >= 4);
    CHECK(strstr(msg, "Short: is empty") && strstr(msg, "Architecture: is empty"));
    CHECK(rp_aminet_problems("a b.lha", r, good, msg, sizeof msg) == 1);

    CHECK(rp_aminet_upload(&ops, NULL, "UP-HYPOC.LHA", "me@x.org", good, "DATA", 4, msg, sizeof msg) == 0);
    CHECK(strstr(msg, "uploaded up-hypoc.lha and up-hypoc.readme"));
    CHECK(strstr(ctl_log, "PASS me@x.org\r\n") && strstr(ctl_log, "CWD /new/\r\n"));
    CHECK(strstr(ctl_log, "STOR up-hypoc.lha") < strstr(ctl_log, "STOR up-hypoc.readme"));
    CHECK(strstr(data_log, "DATA") == data_log && strstr(data_log, "Short:"));
    CHECK(opens == 3 && closes == 3);

    ctl_log[0] = 0; opens = 0; pasv_fail = 1;
    CHECK(rp_aminet_upload(&ops, NULL, "UP-HYPOC.LHA", "me@x.org", good, "D", 1, msg, sizeof msg) == -1);
    CHECK(strstr(msg, "Aminet upload of up-hypoc.lha failed: passive mode refused"));
    pasv_fail = 0; stor_code = 1; opens = 0;
    CHECK(rp_aminet_upload(&ops, NULL, "UP-HYPOC.LHA", "me@x.org", good, "D", 1, msg, sizeof msg) == -1);
    CHECK(strstr(msg, "553 denied"));
    CHECK(rp_aminet_upload(&ops, NULL, "UP-HYPOC.LHA", "nomail", good, "D", 1, msg, sizeof msg) == -1);
    CHECK(strstr(msg, "email is missing"));

    {
        rp_aminet_prefs p, q;
        memset(&p, 0, sizeof p);
        strcpy(p.directory, "demo/aga"); strcpy(p.architecture, "m68k-amigaos"); strcpy(p.uploader, "Me"); strcpy(p.email, "me@x.org");
        CHECK(rp_aminet_prefs_save("/tmp/rp_aminet.prefs", &p) == 0);
        CHECK(rp_aminet_prefs_load("/tmp/rp_aminet.prefs", &q) == 0);
        CHECK(!strcmp(q.directory, "demo/aga") && !strcmp(q.architecture, "m68k-amigaos") && !strcmp(q.uploader, "Me") && !strcmp(q.email, "me@x.org"));
        CHECK(rp_aminet_prefs_load("/tmp/rp_no_such_file", &q) == -1);
        remove("/tmp/rp_aminet.prefs");
    }
    if (failures) return 1;
    puts("aminet ok");
    return 0;
}
