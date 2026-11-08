/* wi - suckless wikipedia reader. C99 + POSIX.1-2008, libcurl. */
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <curl/curl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <wchar.h>
#define VERSION "2.0"
typedef struct { char lang[32], api[512], ua[256]; int w, timeout, title, intro; } Cfg;
typedef struct { char *s; size_t n, cap; } Buf;
static void die(const char *m) { fprintf(stderr, "wi: %s\n", m); exit(1); }
static void add(Buf *b, const char *s, size_t n) {
	if (b->n + n + 1 > b->cap) { b->cap = b->cap ? b->cap : 4096; while (b->cap < b->n + n + 1) b->cap *= 2; if (!(b->s = realloc(b->s, b->cap))) die("out of memory"); }
	memcpy(b->s + b->n, s, n); b->n += n; b->s[b->n] = 0; }
static int hex(int c) { return c <= '9' ? c - '0' : (c | 32) - 'a' + 10; }
static void put8(Buf *b, unsigned c) { char s[4]; int n;
	if (c < 0x80) s[0] = c, n = 1; else if (c < 0x800) s[0] = 0xc0 | c >> 6, s[1] = 0x80 | (c & 63), n = 2; else if (c < 0x10000) s[0] = 0xe0 | c >> 12, s[1] = 0x80 | (c >> 6 & 63), s[2] = 0x80 | (c & 63), n = 3; else s[0] = 0xf0 | c >> 18, s[1] = 0x80 | (c >> 12 & 63), s[2] = 0x80 | (c >> 6 & 63), s[3] = 0x80 | (c & 63), n = 4;
	add(b, s, n); }
static char *jget(const char *j, const char *key) {
	char pat[32]; const char *p; Buf b = {0};
	snprintf(pat, sizeof pat, "\"%s\":\"", key);
	if (!(p = strstr(j, pat))) return NULL;
	for (p += strlen(pat); *p && *p != '"'; p++) {
		if (*p != '\\') { add(&b, p, 1); continue; }
		if (*++p == 'u') { unsigned c = 0, lo; int i;
			for (i = 0; i < 4 && p[1]; i++) c = c * 16 + hex(*++p);
			if (c >= 0xd800 && c <= 0xdbff && p[1] == '\\' && p[2] == 'u') {
				for (lo = 0, i = 0; i < 4 && p[3 + i]; i++) lo = lo * 16 + hex(p[3 + i]);
				if (lo >= 0xdc00 && lo <= 0xdfff) c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00), p += 6;
			}
			put8(&b, c);
		} else add(&b, *p == 'n' ? "\n" : *p == 't' ? "\t" : *p == 'r' ? "\r" : p, 1);
	}
	return b.s ? b.s : strdup(""); }
static size_t sink(char *p, size_t s, size_t n, void *u) { add(u, p, s * n); return s * n; }
static char *fetch(const char *url, Cfg *c) { CURL *h = curl_easy_init(); Buf b = {0}; long code = 0;
	if (!h) die("can not initialize curl");
	curl_easy_setopt(h, CURLOPT_URL, url); curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L); curl_easy_setopt(h, CURLOPT_USERAGENT, c->ua);
	curl_easy_setopt(h, CURLOPT_TIMEOUT, (long)c->timeout); curl_easy_setopt(h, CURLOPT_ACCEPT_ENCODING, ""); curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, sink); curl_easy_setopt(h, CURLOPT_WRITEDATA, &b);
	if (curl_easy_perform(h) != CURLE_OK) { curl_easy_cleanup(h); die("network error"); }
	curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code); curl_easy_cleanup(h); if (code != 200) die("http error");
	return b.s ? b.s : strdup(""); }
static int termw(void) { struct winsize ws; const char *c = getenv("COLUMNS");
	if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col) return ws.ws_col;
	return c && atoi(c) > 0 ? atoi(c) : 80; }
static int cols(const char *s, size_t n) { mbstate_t st = {0}; wchar_t wc; size_t i = 0, r; int w = 0, c;
	while (i < n && (r = mbrtowc(&wc, s + i, n - i, &st)) != 0) { if (r == (size_t)-1 || r == (size_t)-2) { memset(&st, 0, sizeof st); i++; w++; continue; } c = wcwidth(wc); w += c < 0 ? 1 : c; i += r; }
	return w; }
static void wrap(const char *s, int width) {
	while (*s) { const char *e = strchr(s, '\n'), *t, *u; size_t n = e ? (size_t)(e - s) : strlen(s), l; int col = 0, cw;
		while (n && strchr(" \t\r", s[n - 1])) n--;
		for (t = s; t < s + n;) { while (t < s + n && (*t == ' ' || *t == '\t')) t++;
			for (u = t; t < s + n && *t != ' ' && *t != '\t'; t++);
			if (!(l = (size_t)(t - u))) break;
			cw = cols(u, l); if (col && col + 1 + cw > width) { putchar('\n'); col = 0; }
			if (col) { putchar(' '); col++; } fwrite(u, 1, l, stdout); col += cw; }
		putchar('\n'); if (!e) break; s = e + 1; } }
static void trim(char *s) { char *e = s + strlen(s); while (e > s && isspace((unsigned char)e[-1])) *--e = 0; }
static void cfgfile(Cfg *c, const char *path) { FILE *f = fopen(path, "r"); char *l = NULL, k[32], v[512]; size_t z = 0;
	if (!f) return;
	while (getline(&l, &z, f) != -1) { if (sscanf(l, " %31[^= \t] = %511[^\n]", k, v) != 2) continue; trim(v);
		if (!strcmp(k, "language") || !strcmp(k, "lang")) snprintf(c->lang, sizeof c->lang, "%s", v);
		else if (!strcmp(k, "api")) snprintf(c->api, sizeof c->api, "%s", v);
		else if (!strcmp(k, "user_agent")) snprintf(c->ua, sizeof c->ua, "%s", v);
		else if (!strcmp(k, "width")) c->w = atoi(v); else if (!strcmp(k, "timeout")) c->timeout = atoi(v);
		else if (!strcmp(k, "title")) c->title = atoi(v) != 0; else if (!strcmp(k, "intro")) c->intro = atoi(v) != 0; }
	free(l); fclose(f); }
static void cfg(Cfg *c) { const char *x = getenv("XDG_CONFIG_HOME"), *h = getenv("HOME"); char p[1024];
	cfgfile(c, "/etc/wi/config");
	if (x && *x) { snprintf(p, sizeof p, "%s/wi/config", x); cfgfile(c, p); }
	else if (h && *h) { snprintf(p, sizeof p, "%s/.config/wi/config", h); cfgfile(c, p); } }
static void use(int s) { fprintf(s ? stderr : stdout, "usage: wi [-ilqv] [-l lang] [-w width] query...\n"); exit(s); }
int main(int argc, char *argv[]) {
	Cfg c = { .lang = "en", .api = "https://%s.wikipedia.org/w/api.php", .ua = "wi/" VERSION " (suckless wikipedia reader)", .timeout = 15, .title = 1 };
	const char *lang = NULL, *q; char api[512], *u, *j, *txt, *title, *enc, *p; Buf b = {0}; int i, w;
	setlocale(LC_CTYPE, ""); cfg(&c);
	for (i = 1; i < argc; i++) { char *a = argv[i];
		if (*a != '-' || !a[1]) break;
		if (!strcmp(a, "-h")) use(0); else if (!strcmp(a, "-v")) { puts("wi-" VERSION); return 0; }
		else if (!strcmp(a, "-i")) c.intro = 1; else if (!strcmp(a, "-q")) c.title = 0;
		else if (!strcmp(a, "-l") && i + 1 < argc) lang = argv[++i];
		else if (!strcmp(a, "-w") && i + 1 < argc) c.w = atoi(argv[++i]);
		else die("unknown option"); } if (i >= argc) { use(1); } if (lang) snprintf(c.lang, sizeof c.lang, "%s", lang);
	for (; i < argc; i++) { if (b.n) add(&b, " ", 1); add(&b, argv[i], strlen(argv[i])); } q = b.s ? b.s : "";
	if ((p = strstr(c.api, "%s"))) snprintf(api, sizeof api, "%.*s%s%s", (int)(p - c.api), c.api, c.lang, p + 2); else snprintf(api, sizeof api, "%s", c.api);
	enc = malloc(3 * strlen(q) + 1); u = malloc(strlen(api) + strlen(enc) + 256);
	for (p = enc; *q; q++) { unsigned char ch = (unsigned char)*q;
		if (isalnum(ch) || strchr("-_.~", ch)) *p++ = ch;
		else { *p++ = '%'; *p++ = "0123456789ABCDEF"[ch >> 4]; *p++ = "0123456789ABCDEF"[ch & 15]; } }
	*p = 0;
	sprintf(u, "%s%saction=query&format=json&formatversion=2&generator=search&gsrnamespace=0" "&gsrlimit=1&gsrsearch=%s&prop=extracts&explaintext=1&redirects=1&exlimit=1%s", api, strchr(api, '?') ? "&" : "?", enc, c.intro ? "&exintro=1" : "");
	curl_global_init(CURL_GLOBAL_DEFAULT); j = fetch(u, &c); curl_global_cleanup();
	txt = jget(j, "extract"); if (!txt || !*txt) die("article not found");
	title = jget(j, "title"); w = c.w > 0 ? c.w : termw();
	if (c.title && title && *title) printf("%s\n\n", title);
	wrap(txt, w); return 0; }
