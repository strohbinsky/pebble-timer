// Timer 0.9 — Timer und Stoppuhren für die Pebble Time 2, Bedienung und Aussehen wie Simzeit und Linie 8.
// Ansichten: Water (Standard seit 0.9.1: Sprungbrett, Springer, Punktrichter, Planschbecken), Papier, Phosphor.
// Läuft beim Schließen etwas, öffnet die App wieder in der letzten Ansicht (Timer bzw. Stoppuhr).
// Startliste: NEUER TIMER · NEUE STOPPUHR · laufende Timer und Stoppuhren · gespeicherte Timer (nach Dauer).
// Bis zu 5 Timer und 5 Stoppuhren gleichzeitig, 10 gespeicherte Timer. Alles läuft bei geschlossener App weiter;
// abgelaufene Timer öffnen die App per Wecker. Zeiteingabe: Minuten zuerst, Zurück = Stunden, Select = Sekunden.
// Tasten in Timer und Stoppuhr: Hoch (oben rechts) Start/Stopp, Runter Reset (gestoppt) bzw. Runde (Stoppuhr läuft),
// Select kurz oder lang = Menü (Zeit anpassen, Zurücksetzen, Speichern, Entfernen, Einstellungen), Zurück = Liste.
// Ablauf: Ton und Vibration (Dauer einstellbar), danach Nachklingeln; jede Taste stellt ab.
#include <pebble.h>
#include "led_font.h"

#define BREITE 200
#define HOEHE 228
#define BLINK_MS 450
#define RUHE_MS 30000                 // ohne Taste und nichts läuft: App zu, wenn eingestellt (Standard an)
#define HINWEIS_MS 2000
#define MAXT 5                        // Timer gleichzeitig
#define MAXS 5                        // Stoppuhren gleichzeitig
#define MAXG 10                       // gespeicherte Timer
#define MAXR 10                       // Runden je Stoppuhr (die letzten)
#define MAX_DAUER (24 * 3600 - 1)
#define MIN_REST 10                   // Zeit anpassen: weniger Rest geht nicht
#define NACH_MAX 3600                 // Nachklingeln höchstens 1 h nach Ablauf
#define KLINGEL_TAKT 2000

// Farben zur Laufzeit (farben_setzen): Papier schwarz auf Weiß, Phosphor weiß auf Schwarz, Invertiert tauscht
#define P_TEXT   s_pal.p_text
#define P_GRAU   s_pal.p_grau
#define P_GITTER s_pal.p_gitter
#define P_HELL   s_pal.p_hell
#define P_CYAN   s_pal.cyan
#define P_WARN   s_pal.p_warn
// Papier: nie Grau in der Schrift (Zweitschrift = Tinte), nur Linien grau
#define K_TEXT   s_pal.tinte
#define K_ZWEIT  s_pal.tinte
#define K_LINIE  s_pal.k_linie
#define K_ORANGE s_pal.k_orange
#define K_CYAN   s_pal.cyan
#define C_HG     s_pal.hg
#define C_ROT    s_pal.rot            // abgelaufen: Zeit zählt hoch
#define ZEILE 22                      // Listen: Zeilenhöhe
#define LZEILEN 8                     // Listen: sichtbare Zeilen ohne Infozeilen

// Papier (Standard): Systemschrift fett, Ziffern Roboto. Phosphor: Pixelschrift, weiß. Klar (1) bis 0.6 → Papier.
// Water (0.9.0, Standard seit 0.9.1): Fredoka, Eisblau, Springer und Punktrichter
enum { L_PHOSPHOR = 0, L_PAPIER = 2, L_WATER = 3 };
enum { M_LISTE, M_EIN, M_TIMER, M_STOPP, M_ANPASSEN, M_FRAGE };
enum { Z_FREI = 0, Z_BEREIT, Z_LAEUFT, Z_PAUSE, Z_UM };   // Zustand von Timer und Stoppuhr (Z_UM nur Timer)
enum { PK_FORMAT = 1, PK_LAYOUT, PK_TON, PK_LAUT, PK_VIB, PK_KLINGEL, PK_NACH, PK_RUHE, PK_LETZTE, PK_GESP, PK_TIMER,
       PK_STOPP = 20, PK_ANSICHT = 30, PK_KLANG = 31, PK_INVERS = 32, PK_KONTRAST = 33,
       PK_RUHE2 = 34, PK_ZU = 35 };   // 0.8.0: Ruhe mit neuer Bedeutung und Standard an, daher neuer Schlüssel   // PK_TON (bis 0.3) ungenutzt
#define FORMAT 3                      // 3 ab 0.3: Daten der zusammengelegten 0.2 (andere Schlüssel) werden verworfen

// Timer: ende = Endzeit (läuft) bzw. Zeitpunkt des Ablaufs (Z_UM); rest gilt bereit und angehalten.
// gesamt = Dauer dieses Laufs inkl. Anpassung (Balken), basis = eingestellte Dauer
typedef struct { int32_t basis, gesamt, rest, ende; uint8_t z, quitt, nach, frei; } Timer;
// Stoppuhr: akku = Millisekunden bis zum letzten Anhalten; rd = Rundenzeit, rg = Gesamtzeit bei der Runde (Ring)
typedef struct { int32_t start_s, akku, erst; int16_t start_ms; uint8_t z, nr; uint16_t rn, frei; int32_t rd[MAXR], rg[MAXR]; } Stopp;
typedef struct { int32_t dauer; uint16_t nutzung, frei; } Gesp;   // dauer 0 = leer

static const int16_t W_KLINGEL[] = { 30, 60, 120 };   // Sekunden
static const int16_t W_NACH[] = { 0, 2, 5, 10 };      // Minuten, 0 = aus
static const int16_t W_ZU[] = { 0, 10, 30, 60 };      // Sekunden nach dem Abstellen ohne Taste: App zu, 0 = nie
static const uint8_t W_LAUT[] = { 30, 60, 100 };

enum { LI_WAHL, LI_START, LI_STOPP, LI_MENUE, LI_EINST, LI_ANSICHT, LI_TON, LI_LAUT, LI_KLINGEL, LI_NACH, LI_RUNDEN, LI_ERSETZEN, LI_ZU };
enum { A_NEU_T, A_NEU_S, A_T, A_S, A_G, A_G_AENDERN, A_G_LOESCHEN, A_G_NEU, A_T_ENTF, A_S_ENTF, A_ANPASSEN, A_RESET,
       A_T_SPEICHERN, A_RUNDEN, A_EINST, A_ANSICHT, A_TON, A_LAUT, A_VIB, A_KLINGEL, A_NACH, A_TEST, A_RUHE,
       A_SET_ANSICHT, A_SET_TON, A_SET_LAUT, A_SET_KLINGEL, A_SET_NACH, A_RUNDE, A_ERSETZEN,
       A_BEREICH_T, A_BEREICH_S, A_ALLE_S, A_INVERS, A_KONTRAST, A_ZU, A_SET_ZU };
enum { FR_SPEICHERN, FR_ERSETZEN, FR_G_LOESCHEN, FR_T_ENTF, FR_S_ENTF };
enum { FL_NEU, FL_G_NEU, FL_G_AENDERN, FL_T_SPEICHERN };

static Window *s_win;
static Layer *s_layer;
// Einstellungen
static int s_layout = L_WATER;
static bool s_invers, s_kontrast;     // Farben getauscht; hoher Kontrast (Phosphor: keine graue Schrift)
typedef struct { GColor hg, tinte, k_linie, k_orange, cyan, rot, p_text, p_grau, p_gitter, p_hell, p_warn; } Palette;
static Palette s_pal;
static bool s_vib = true;
static int s_klang;                   // Ton beim Ablauf: 0 aus (Standard), 1 Piepser hoch, 2 Piepser tiefer
static int s_laut = 2, s_klingel = 1, s_nach = 2, s_ruhe = 1, s_zu = 1;
// Daten
static Timer s_t[MAXT];
static Stopp s_s[MAXS];
static Gesp s_g[MAXG];
static int32_t s_letzte = 300;        // zuletzt eingegebene Dauer (Vorschlag der nächsten Eingabe)
// Eingabe und Abläufe
static int s_h, s_m, s_sek, s_feld, s_fluss, s_g_edit, s_wdh_n;
static int32_t s_neu_dauer;
static int s_fr, s_fr_wert, s_wahl;
// Ansicht
static int s_modus = M_LISTE, s_akt, s_von;   // s_akt: gezeigter Timer bzw. Stoppuhr; s_von: Herkunft des Menüs
static bool s_blink = true;
static AppTimer *s_zu_timer;          // nach dem Abstellen eines Klingelns: App zu, wenn keine Taste mehr kommt
static AppTimer *s_blink_timer, *s_ruhe_timer, *s_lauf_timer, *s_klingel_timer, *s_schnell_timer, *s_hinweis_timer;
static int s_lauf_off;
static time_t s_klingel_bis;
static char s_hinweis[40];
static int s_start_akt, s_start_wert, s_letzte_ansicht = -1;   // letzte: 16 + Timer bzw. 32 + Stoppuhr
static int s_stopp_zuletzt = -1;      // zuletzt gezeigte Stoppuhr: die zeigt der Bereich Stoppuhr zuerst
static bool s_einst_aus_menue;
// Listen
#define LMAX 24
static int s_lart, s_lanz, s_lsel, s_loben;
static char s_ltitel[28], s_ltext[LMAX][40], s_linfo[LMAX][2][30];
static bool s_linfo_an;
static int8_t s_lakt[LMAX];
static int16_t s_lwert[LMAX];
static GFont s_f14, s_f18, s_f18b, s_f24b, s_f28b, s_leco[4];

#define PAPIER (s_layout == L_PAPIER)
#define WATER (s_layout == L_WATER)
#define KLAR (PAPIER || WATER)        // Aufbau mit Textschrift, Texte gemischt geschrieben (bis 0.6 auch Ansicht Klar)
#define T(p, k) (KLAR ? (k) : (p))
#define C_NORMAL (KLAR ? K_TEXT : P_TEXT)
#define C_VOR    (KLAR ? K_CYAN : P_CYAN)
#define C_WARN   (KLAR ? K_ORANGE : P_WARN)
#define C_GEDIMMT (KLAR ? K_ZWEIT : P_GRAU)
// Water: Farben, Schrift Fredoka (erst bei Bedarf geladen), Bewegung
#define W_NAVY     GColorFromHEX(0x000055)
#define W_ROSA     GColorFromHEX(0xFF55AA)
#define W_GRUEN    GColorFromHEX(0x55FF55)
#define W_GRUEN_D  GColorFromHEX(0x00AA55)
#define W_WASSER   GColorFromHEX(0x55AAFF)
#define W_WASSER_D GColorFromHEX(0x0055AA)
#define W_HELLBLAU GColorFromHEX(0x55AAFF)  // Blinken: Ziffer aus
#define W_HAUT     GColorFromHEX(0xFFAAAA)
#define W_KAPPE    GColorFromHEX(0xFFFF00)
#define W_HOSE     GColorFromHEX(0x0055AA)
static GFont s_wf15, s_wf18, s_wf26;
static AppTimer *s_anim_timer;
static time_t s_taste;                // letzte Taste oder Öffnen: danach 30 s flüssige Bewegung

static void start_zeigen(int akt, int wert);
static void wahl_zeigen(int akt);
static void stopp_neu(void);
// Papier: fette Systemschrift, Ziffern aus Roboto (Doppelpunkt mittig, tools/ziffern_schrift.py), erst bei Bedarf geladen
static GFont s_roboto[4];
static void schriften_setzen(void) {
  static const uint32_t R[4] = { RESOURCE_ID_ROBOTO_BOLD_92, RESOURCE_ID_ROBOTO_BOLD_64, RESOURCE_ID_ROBOTO_BOLD_50, RESOURCE_ID_ROBOTO_BOLD_36 };
  for (int i = 0; i < 4; i++) {
    if (PAPIER && !s_roboto[i]) s_roboto[i] = fonts_load_custom_font(resource_get_handle(R[i]));
    s_leco[i] = s_roboto[i];
  }
  if (WATER && !s_wf15) {                    // Water: Fredoka 15/18 halbfett, 26 fett (nur Ziffern)
    s_wf15 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FREDOKA_15));
    s_wf18 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FREDOKA_18));
    s_wf26 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FREDOKA_BOLD_26));
  }
  // Water nutzt den Textaufbau von Papier, dort mit Fredoka statt Gothic
  s_f14 = WATER ? s_wf15 : fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_f18 = WATER ? s_wf15 : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_f18b = WATER ? s_wf18 : fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_f24b = WATER ? s_wf18 : fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_f28b = WATER ? s_wf18 : fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
}
// Farben wie Linie 8 0.13: Papier normal hell, Phosphor normal dunkel; Invertiert tauscht Grund und Schrift
static GColor hex(uint32_t c) { return GColorFromHEX(c); }
static void farben_setzen(void) {
  if (WATER) {                                // Eisblau, Schrift nachtblau; Invertiert gilt hier nicht
    s_pal.hg = hex(0xAAFFFF);
    s_pal.tinte = s_pal.p_text = s_pal.p_hell = hex(0x000055);
    s_pal.k_linie = s_pal.p_gitter = hex(0x55AAAA);
    s_pal.p_grau = s_pal.cyan = hex(0x0055AA);
    s_pal.k_orange = s_pal.p_warn = hex(0xFF5500);
    s_pal.rot = hex(0xFF0055);
    if (s_win) window_set_background_color(s_win, s_pal.hg);
    return;
  }
  const bool hell = PAPIER != s_invers;
  s_pal.hg = hell ? GColorWhite : GColorBlack;
  s_pal.tinte = s_pal.p_text = s_pal.p_hell = hell ? GColorBlack : GColorWhite;
  s_pal.k_linie = s_pal.p_gitter = hex(hell ? 0xAAAAAA : 0x555555);
  s_pal.p_grau = hex(hell ? 0x555555 : 0xAAAAAA);
  s_pal.k_orange = hex(hell ? 0xAA5500 : 0xFFAA00);
  s_pal.p_warn = hex(hell ? 0xAA5500 : 0xFFFF00);
  s_pal.cyan = hex(hell ? 0x0055AA : 0x55FFFF);
  s_pal.rot = hex(hell ? 0xAA0000 : 0xFF0000);
  if (s_kontrast) s_pal.p_grau = s_pal.p_text;   // nur Schrift; Gitter und Balken bleiben
  if (s_win) window_set_background_color(s_win, s_pal.hg);
}
static void wecker_stellen(void);
static void timer_reset(Timer *t);

// ---------- Zeit ----------
static void uhrzeit(char *b, size_t n, time_t t) {
  struct tm *tm = localtime(&t);
  snprintf(b, n, "%02d:%02d", tm->tm_hour, tm->tm_min);
}
static void dauer_text(char *b, size_t n, int32_t s) {   // M:SS, ab einer Stunde H:MM:SS
  if (s < 0) s = 0;
  if (s >= 3600) snprintf(b, n, "%d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
  else snprintf(b, n, "%d:%02d", (int)(s / 60), (int)(s % 60));
}
static void ms_text(char *b, size_t n, int32_t ms) {     // mit Zehnteln
  char d[12];
  if (ms < 0) ms = 0;
  dauer_text(d, sizeof d, ms / 1000);
  snprintf(b, n, "%s.%d", d, (int)(ms / 100 % 10));
}
static void plus_text(char *b, size_t n, int32_t s) {    // "+2:00" / "-1:00" / "-"
  char d[12];
  if (s == 0) { snprintf(b, n, "-"); return; }
  dauer_text(d, sizeof d, s < 0 ? -s : s);
  snprintf(b, n, "%c%s", s < 0 ? '-' : '+', d);
}
static int32_t rest_von(const Timer *t, time_t now) {
  if (t->z == Z_LAEUFT) return t->ende > now ? t->ende - now : 0;
  return t->z == Z_UM ? 0 : t->rest;
}
static int32_t stopp_ms(const Stopp *p) {
  if (p->z != Z_LAEUFT) return p->akku;
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  int32_t d = (int32_t)(s - p->start_s) * 1000 + (int32_t)ms - p->start_ms;
  if (d < 0) d = 0;
  const int32_t g = p->akku + d;
  return g > 359999999 ? 359999999 : g;      // höchstens 99:59:59.9
}
static int32_t runde_vor(const Stopp *p) { return p->rn ? p->rg[(p->rn - 1) % MAXR] : 0; }

// ---------- Phosphor: Pixeltext ----------
static const Glyph *glyph(const Glyph *f, int n, char c) {
  for (int i = 0; i < n; i++) if (f[i].c == c) return &f[i];
  return NULL;
}
static int text_w(const Glyph *f, int n, const char *s, int sc) {
  int w = 0;
  for (; *s; s++) { const Glyph *g = glyph(f, n, *s); w += ((g ? g->w : 2) + 1) * sc; }
  return w > 0 ? w - sc : 0;
}
// Pixeltext; Pixel links von minx bzw. ab maxx werden nicht gezeichnet (Lauftext)
static void p_text_clip(GContext *ctx, const Glyph *f, int n, int h, const char *s, int x, int y, int sc, GColor c, int minx, int maxx) {
  graphics_context_set_fill_color(ctx, c);
  for (; *s; s++) {
    const Glyph *g = glyph(f, n, *s);
    if (!g) { x += 3 * sc; continue; }
    for (int gy = 0; gy < h; gy++)
      for (int gx = 0; gx < g->w; gx++)
        if ((g->rows[gy] & (1 << (g->w - 1 - gx))) && x + gx * sc >= minx && x + gx * sc + sc <= maxx)
          graphics_fill_rect(ctx, GRect(x + gx * sc, y + gy * sc, sc, sc), 0, GCornerNone);
    x += (g->w + 1) * sc;
  }
}
static void p_text(GContext *ctx, const Glyph *f, int n, int h, const char *s, int x, int y, int sc, GColor c) {
  p_text_clip(ctx, f, n, h, s, x, y, sc, c, -1000, 1000);
}
#define P_W(s, sc)  text_w(F35, F35_N, s, sc)
#define P_GW(s, sc) text_w(F57, F57_N, s, sc)
#define P_KLEIN(s, x, y, c)    p_text(ctx, F35, F35_N, F35_H, s, x, y, 2, c)
#define P_KLEIN_R(s, x, y, c)  p_text(ctx, F35, F35_N, F35_H, s, (x) - P_W(s, 2), y, 2, c)
#define P_KLEIN_M(s, y, c)     p_text(ctx, F35, F35_N, F35_H, s, (BREITE - P_W(s, 2)) / 2, y, 2, c)
#define P_MITTEL(s, x, y, c)   p_text(ctx, F35, F35_N, F35_H, s, x, y, 3, c)
#define P_MITTEL_M(s, y, c)    p_text(ctx, F35, F35_N, F35_H, s, (BREITE - P_W(s, 3)) / 2, y, 3, c)
#define P_GROSS(s, x, y, sc, c) p_text(ctx, F57, F57_N, F57_H, s, x, y, sc, c)

static void fuellen(GContext *ctx, GRect r, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, r, 0, GCornerNone);
}
static void p_punktlinie(GContext *ctx, int y) {
  graphics_context_set_fill_color(ctx, P_GITTER);
  for (int x = 0; x < BREITE; x += 2) graphics_fill_rect(ctx, GRect(x, y, 1, 1), 0, GCornerNone);
}
static void k_linie(GContext *ctx, int y) {
  graphics_context_set_stroke_color(ctx, K_LINIE);
  graphics_draw_line(ctx, GPoint(6, y), GPoint(194, y));
}

// ---------- Klar: Systemschrift ----------
static void k_text(GContext *ctx, const char *t, GFont f, GRect r, GTextAlignment a, GColor c) {
  graphics_context_set_text_color(ctx, c);
  graphics_draw_text(ctx, t, f, r, GTextOverflowModeTrailingEllipsis, a, NULL);
}
static int k_breite(const char *t, GFont f) {
  return graphics_text_layout_get_content_size(t, f, GRect(0, 0, 1000, 80), GTextOverflowModeFill, GTextAlignmentLeft).w;
}

// ---------- Ansicht Water: Bausteine ----------
// Pille: Rechteck mit Halbkreisen (Ecken von graphics_fill_rect höchstens 8 px). Ungerade Höhe, dann rund
static void w_pille(GContext *ctx, int x, int y, int w, int h, GColor c) {
  const int r = h / 2;
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(x + r, y, w - 2 * r, h), 0, GCornerNone);
  graphics_fill_circle(ctx, GPoint(x + r, y + r), r);
  graphics_fill_circle(ctx, GPoint(x + w - 1 - r, y + r), r);
}
static void w_kreis(GContext *ctx, GPoint m, int r, GColor fuell, GColor rand) {
  graphics_context_set_fill_color(ctx, fuell);
  graphics_fill_circle(ctx, m, r);
  graphics_context_set_stroke_color(ctx, rand);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_circle(ctx, m, r);
}
static void w_poly(GContext *ctx, GPoint *p, int n, GColor fuell, GColor rand) {
  GPath g = { .num_points = n, .points = p, .rotation = 0, .offset = GPointZero };
  graphics_context_set_fill_color(ctx, fuell);
  gpath_draw_filled(ctx, &g);
  graphics_context_set_stroke_color(ctx, rand);
  graphics_context_set_stroke_width(ctx, 1);
  gpath_draw_outline(ctx, &g);
}
static void w_rasen(GContext *ctx, int y) {
  fuellen(ctx, GRect(0, y, BREITE, HOEHE - y), W_GRUEN);
  fuellen(ctx, GRect(0, y, BREITE, 1), W_GRUEN_D);
}
// Text auf dem Rasen (grund: Grundlinie); zu lang für Fredoka 15: Systemschrift 14 fett
static void w_rasen_text(GContext *ctx, const char *t, int grund) {
  if (k_breite(t, s_wf15) <= 192) k_text(ctx, t, s_wf15, GRect(4, grund - 15, 192, 20), GTextAlignmentLeft, W_NAVY);
  else k_text(ctx, t, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(4, grund - 13, 192, 18), GTextAlignmentLeft, W_NAVY);
}
// Kopf: Titel in einer Pille, Uhrzeit rechts. Fredoka: Grundlinie = Oberkante des Rahmens + Schriftgröße
static void w_kopf(GContext *ctx, const char *titel, GColor pille, GColor schrift) {
  char u[8];
  uhrzeit(u, sizeof u, time(NULL));
  fuellen(ctx, GRect(0, 0, BREITE, HOEHE), C_HG);
  const int uw = k_breite(u, s_wf18), maxw = 196 - uw - 8 - 24;
  int tw = k_breite(titel, s_wf15);
  if (tw > maxw) tw = maxw;
  w_pille(ctx, 4, 3, tw + 20, 21, pille);
  k_text(ctx, titel, s_wf15, GRect(14, 18 - 15, tw + 2, 20), GTextAlignmentLeft, schrift);
  k_text(ctx, u, s_wf18, GRect(196 - uw - 4, 19 - 18, uw + 4, 24), GTextAlignmentRight, W_NAVY);
}

// ---------- Kopf, Fuß, Zeilen (Linie 8: Titel links, Uhrzeit oben rechts) ----------
static void kopf(GContext *ctx, const char *p_titel, const char *k_titel) {
  if (WATER) { w_kopf(ctx, k_titel, W_ROSA, W_NAVY); return; }
  char u[8];
  uhrzeit(u, sizeof u, time(NULL));
  fuellen(ctx, GRect(0, 0, BREITE, HOEHE), C_HG);
  if (KLAR) {
    const int uw = k_breite(u, s_f18b) + 8;
    k_text(ctx, u, s_f18b, GRect(194 - uw, -1, uw, 24), GTextAlignmentRight, K_TEXT);
    k_text(ctx, k_titel, s_f18b, GRect(6, -1, 188 - uw, 24), GTextAlignmentLeft, K_TEXT);
    fuellen(ctx, GRect(0, 24, BREITE, 2), K_LINIE);
  } else {
    P_KLEIN_R(u, 196, 6, P_HELL);
    P_KLEIN(p_titel, 4, 6, P_GRAU);
    p_punktlinie(ctx, 20);
  }
}
static void fuss(GContext *ctx, const char *p_links, const char *k_links) {
  if (WATER) { w_rasen(ctx, 206); w_rasen_text(ctx, k_links, 222); return; }
  if (KLAR) {
    k_linie(ctx, 209);
    // Papier: fett, nur wenn es in die Zeile passt — sonst normal statt abgeschnitten
    const GFont f = PAPIER && k_breite(k_links, s_f14) > 188 ? fonts_get_system_font(FONT_KEY_GOTHIC_14) : s_f14;
    k_text(ctx, k_links, f, GRect(6, 209, 188, 18), GTextAlignmentLeft, K_ZWEIT);
  } else {
    p_punktlinie(ctx, 207);
    P_KLEIN(p_links, 4, 213, P_GRAU);
  }
}
static bool zu_lang(const char *t) { return KLAR ? k_breite(t, s_f18b) > (WATER ? 180 : 188) : P_W(t, 2) > 188; }
// Auswahlzeilen wie Linie 8: gewählte Zeile invers, zu lang: läuft durch
static void zeile(GContext *ctx, const char *t, int y, bool gewaehlt) {
  const bool lang = gewaehlt && zu_lang(t);
  if (WATER) {                                // gewählte Zeile: rosa Pille, zu lang: läuft darin durch
    if (gewaehlt) w_pille(ctx, 2, y + 1, 196, 21, W_ROSA);
    if (!lang) { k_text(ctx, t, s_wf18, GRect(10, y + 17 - 18, 182, 24), GTextAlignmentLeft, W_NAVY); return; }
    const int w = k_breite(t, s_wf18), per = w + 40, x = 10 - (s_lauf_off * 3) % per;
    graphics_context_set_text_color(ctx, W_NAVY);
    graphics_draw_text(ctx, t, s_wf18, GRect(x, y - 1, w + 4, 24), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, t, s_wf18, GRect(x + per, y - 1, w + 4, 24), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    return;
  }
  if (KLAR) {
    if (!gewaehlt) {
      k_text(ctx, t, s_f18, GRect(6, y - 3, 188, 24), GTextAlignmentLeft, K_TEXT);
      graphics_context_set_stroke_color(ctx, K_LINIE);
      graphics_draw_line(ctx, GPoint(6, y + ZEILE - 1), GPoint(194, y + ZEILE - 1));
      return;
    }
    fuellen(ctx, GRect(0, y, BREITE, ZEILE), K_TEXT);
    if (!lang) { k_text(ctx, t, s_f18b, GRect(6, y - 3, 188, 24), GTextAlignmentLeft, C_HG); return; }
    const int w = k_breite(t, s_f18b), per = w + 40, x = 6 - (s_lauf_off * 3) % per;
    graphics_context_set_text_color(ctx, C_HG);
    graphics_draw_text(ctx, t, s_f18b, GRect(x, y - 3, w + 4, 24), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, t, s_f18b, GRect(x + per, y - 3, w + 4, 24), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    return;
  }
  if (!gewaehlt) { p_text_clip(ctx, F35, F35_N, F35_H, t, 6, y + 6, 2, P_TEXT, 0, 196); return; }
  fuellen(ctx, GRect(0, y + 1, BREITE, ZEILE - 2), P_TEXT);
  if (!lang) { P_KLEIN(t, 6, y + 6, C_HG); return; }
  const int per = P_W(t, 2) + 40, x = 6 - (s_lauf_off * 2) % per;
  p_text_clip(ctx, F35, F35_N, F35_H, t, x, y + 6, 2, C_HG, 4, 196);
  p_text_clip(ctx, F35, F35_N, F35_H, t, x + per, y + 6, 2, C_HG, 4, 196);
}

// ---------- große Zeit aus Teilen ("1", ":", "05", ":", "00") ----------
// sel: Teil, der blinkt und unterstrichen ist (-1 keiner). zehntel >= 0: kleine Ziffer mit Punkt rechts unten.
// Mittig um y_mitte; Phosphor so groß wie möglich bis sc_max, Klar LECO 60/42/38/28 (60 nur bei sc_max >= 7)
static void gross_teile(GContext *ctx, const char *const *t, int n, int sel, int zehntel, int y_mitte, int sc_max, GColor c) {
  char z[2] = { (char)('0' + (zehntel < 0 ? 0 : zehntel)), 0 };
  if (KLAR) {
    static const int OB_K[4] = { 18, 13, 11, 8 }, ZH_K[4] = { 42, 29, 27, 20 };   // Rahmen → Ziffern, Ziffernhöhe (LECO)
    static const int OB_P[4] = { 26, 18, 14, 10 }, ZH_P[4] = { 66, 46, 36, 26 };  // Papier: Roboto 92/64/50/36
    const int *OB = PAPIER ? OB_P : OB_K, *ZH = PAPIER ? ZH_P : ZH_K;
    int f = sc_max >= 7 ? 0 : 1, w = 0;
    for (; f < 4; f++) {
      w = zehntel >= 0 ? k_breite(z, s_leco[3]) + 10 : 0;
      for (int i = 0; i < n; i++) w += k_breite(t[i], s_leco[f]) + (t[i][0] == ':' ? 6 : 0);
      if (w <= 192) break;
    }
    if (f > 3) f = 3;
    const int y = y_mitte - ZH[f] / 2;
    int x = (BREITE - w) / 2;
    for (int i = 0; i < n; i++) {
      const int wi = k_breite(t[i], s_leco[f]) + (t[i][0] == ':' ? 6 : 0);
      k_text(ctx, t[i], s_leco[f], GRect(x - 4, y - OB[f], wi + 8, 120), GTextAlignmentCenter, i == sel && !s_blink ? K_LINIE : c);
      if (i == sel) fuellen(ctx, GRect(x, y + ZH[f] + 6, wi, 4), K_CYAN);
      x += wi;
    }
    if (zehntel >= 0) {
      fuellen(ctx, GRect(x + 2, y + ZH[f] - 4, 4, 4), c);
      k_text(ctx, z, s_leco[3], GRect(x + 8, y + ZH[f] - ZH[3] - OB[3], 30, 40), GTextAlignmentLeft, c);
    }
    return;
  }
  int sc = sc_max, w = 0, scz = 2;
  for (; sc > 2; sc--) {
    scz = sc * 3 / 5 < 2 ? 2 : sc * 3 / 5;
    w = 0;
    for (int i = 0; i < n; i++) w += P_GW(t[i], sc) + (i ? 2 * sc : 0);
    if (zehntel >= 0) w += 2 * sc + 2 * scz + P_GW(z, scz);   // Abstand, Punkt mit Abstand, Ziffer
    if (w <= 196) break;
  }
  const int y = y_mitte - F57_H * sc / 2;
  int x = (BREITE - w) / 2;
  for (int i = 0; i < n; i++) {
    if (i) x += 2 * sc;
    const int wi = P_GW(t[i], sc);
    P_GROSS(t[i], x, y, sc, i == sel && !s_blink ? P_GITTER : c);
    if (i == sel) fuellen(ctx, GRect(x, y + F57_H * sc + 5, wi, 3), P_CYAN);
    x += wi;
  }
  if (zehntel >= 0) {
    x += 2 * sc;
    fuellen(ctx, GRect(x, y + F57_H * sc - scz, scz, scz), c);
    x += 2 * scz;
    P_GROSS(z, x, y + F57_H * sc - F57_H * scz, scz, c);
  }
}
// Dauer in Teile zerlegen: M:SS, ab einer Stunde (oder immer_h) H:MM:SS
typedef struct { char p[5][4]; const char *z[6]; int n; } Teile;   // z: Platz für ein "+" davor
static void teile_dauer(Teile *t, int32_t s, bool immer_h) {
  if (s < 0) s = 0;
  t->n = 0;
  if (immer_h || s >= 3600) {
    snprintf(t->p[t->n++], 4, "%d", (int)(s / 3600 % 100));
    snprintf(t->p[t->n++], 4, ":");
    snprintf(t->p[t->n++], 4, "%02d", (int)(s / 60 % 60));
  } else snprintf(t->p[t->n++], 4, "%d", (int)(s / 60));
  snprintf(t->p[t->n++], 4, ":");
  snprintf(t->p[t->n++], 4, "%02d", (int)(s % 60));
  for (int i = 0; i < t->n; i++) t->z[i] = t->p[i];
}
// unterer Block mit drei Werten
static void drei_werte(GContext *ctx, const char *const lab[3], const char *const wert[3], int hervor) {
  if (KLAR) {
    k_linie(ctx, 154);
    const GTextAlignment a[3] = { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight };
    const int x[3] = { 4, 60, 116 };
    for (int i = 0; i < 3; i++) {
      k_text(ctx, lab[i], s_f14, GRect(x[i] + 2, 155, 76, 18), a[i], K_ZWEIT);
      k_text(ctx, wert[i], s_f28b, GRect(x[i], 166, 80, 34), a[i], i == hervor ? K_TEXT : K_ZWEIT);
    }
    return;
  }
  p_punktlinie(ctx, 162);
  P_KLEIN(lab[0], 4, 168, P_GRAU);
  P_KLEIN_M(lab[1], 168, P_GRAU);
  P_KLEIN_R(lab[2], 196, 168, P_GRAU);
  P_MITTEL(wert[0], 4, 184, hervor == 0 ? P_TEXT : P_GRAU);
  P_MITTEL(wert[1], (BREITE - P_W(wert[1], 3)) / 2, 184, hervor == 1 ? P_TEXT : P_GRAU);
  P_MITTEL(wert[2], 196 - P_W(wert[2], 3), 184, hervor == 2 ? P_TEXT : P_HELL);
}
// Balken aus 16 Blöcken, num/den = gefüllter Anteil (beim Timer: Rest, läuft leer)
static void balken(GContext *ctx, int y, int h, int32_t num, int32_t den) {
  const int n = 16, w = 12, x0 = (BREITE - n * w) / 2;
  if (den <= 0) { den = 1; num = 0; }
  if (num < 0) num = 0;
  const int voll = num >= den ? n : (int)(((int64_t)num * n + den - 1) / den);   // angebrochener Block zählt
  for (int i = 0; i < n; i++) {
    const GColor c = KLAR ? (i < voll ? K_TEXT : K_LINIE) : (i < voll ? P_TEXT : P_GITTER);
    fuellen(ctx, GRect(x0 + i * w, y, w - 2, h), c);
  }
}
// Zeile unter dem Balken: "ENDE 14:35" — sagt, wofür der Balken steht
static void unter_balken(GContext *ctx, const char *lab, const char *zeit, int y) {
  char b[36];
  if (KLAR) {
    snprintf(b, sizeof b, "%s %s", lab, zeit);
    k_text(ctx, b, s_f18b, GRect(0, y - 7, BREITE, 24), GTextAlignmentCenter, K_ZWEIT);
    return;
  }
  snprintf(b, sizeof b, "%s ", lab);
  P_KLEIN(b, 100 - (P_W(b, 2) + P_W(zeit, 2)) / 2, y, P_GRAU);
  P_KLEIN(zeit, 100 + (P_W(b, 2) - P_W(zeit, 2)) / 2, y, P_HELL);
}
// Kasten in der Mitte (Hinweis, Klingeln)
static void kasten(GContext *ctx, int y, int h, GColor rand) {
  if (WATER) {                                // weiß mit rundem, farbigem Rand
    graphics_context_set_fill_color(ctx, rand);
    graphics_fill_rect(ctx, GRect(10, y, 180, h), 8, GCornersAll);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(13, y + 3, 174, h - 6), 6, GCornersAll);
    return;
  }
  fuellen(ctx, GRect(10, y, 180, h), C_HG);
  fuellen(ctx, GRect(10, y, 180, 3), rand); fuellen(ctx, GRect(10, y + h - 3, 180, 3), rand);
  fuellen(ctx, GRect(10, y, 3, h), rand); fuellen(ctx, GRect(187, y, 3, h), rand);
}

// ---------- Daten: gespeicherte und aktive Einträge ----------
static int gesp_index(int32_t d) { for (int i = 0; i < MAXG; i++) if (s_g[i].dauer == d) return i; return -1; }
static int gesp_frei(void) { return gesp_index(0); }
static int gesp_sortiert(int8_t *ix, bool selten_oben) {   // nach Dauer bzw. selten genutzte oben
  int n = 0;
  for (int i = 0; i < MAXG; i++) if (s_g[i].dauer > 0) ix[n++] = i;
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0; b--) {
      const Gesp *x = &s_g[ix[b - 1]], *y = &s_g[ix[b]];
      const bool tauschen = selten_oben ? (x->nutzung > y->nutzung || (x->nutzung == y->nutzung && x->dauer > y->dauer))
                                        : x->dauer > y->dauer;
      if (!tauschen) break;
      const int8_t h = ix[b]; ix[b] = ix[b - 1]; ix[b - 1] = h;
    }
  return n;
}
static int timer_anzahl(void) { int n = 0; for (int i = 0; i < MAXT; i++) if (s_t[i].z != Z_FREI) n++; return n; }
static int timer_frei(void) { for (int i = 0; i < MAXT; i++) if (s_t[i].z == Z_FREI) return i; return -1; }
static int stopp_frei(void) { for (int i = 0; i < MAXS; i++) if (s_s[i].z == Z_FREI) return i; return -1; }
static int timer_sortiert(int8_t *ix, time_t now) {   // abgelaufene zuerst, dann nach Rest
  int n = 0;
  for (int i = 0; i < MAXT; i++) if (s_t[i].z != Z_FREI) ix[n++] = i;
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0; b--) {
      const Timer *x = &s_t[ix[b - 1]], *y = &s_t[ix[b]];
      const int32_t kx = x->z == Z_UM ? -1 : rest_von(x, now), ky = y->z == Z_UM ? -1 : rest_von(y, now);
      if (kx <= ky) break;
      const int8_t h = ix[b]; ix[b] = ix[b - 1]; ix[b - 1] = h;
    }
  return n;
}
static int stopp_sortiert(int8_t *ix) {
  int n = 0;
  for (int nr = 1; nr <= MAXS; nr++)
    for (int i = 0; i < MAXS; i++) if (s_s[i].z != Z_FREI && s_s[i].nr == nr) ix[n++] = i;
  return n;
}
static int offener_timer(void) {             // abgelaufen und noch nicht abgestellt, der früheste
  int b = -1;
  for (int i = 0; i < MAXT; i++)
    if (s_t[i].z == Z_UM && !s_t[i].quitt && (b < 0 || s_t[i].ende < s_t[b].ende)) b = i;
  return b;
}
static void speichern(void) {
  persist_write_int(PK_FORMAT, FORMAT);
  persist_write_data(PK_TIMER, s_t, sizeof s_t);
  for (int i = 0; i < MAXS; i++) persist_write_data(PK_STOPP + i, &s_s[i], sizeof s_s[i]);
  persist_write_data(PK_GESP, s_g, sizeof s_g);
}
static void einst_speichern(void) {
  persist_write_int(PK_LAYOUT, s_layout);
  persist_write_int(PK_KLANG, s_klang);
  persist_write_int(PK_LAUT, s_laut);
  persist_write_int(PK_VIB, s_vib);
  persist_write_int(PK_KLINGEL, s_klingel);
  persist_write_int(PK_NACH, s_nach);
  persist_write_int(PK_RUHE2, s_ruhe);
  persist_write_int(PK_ZU, s_zu);
  persist_write_int(PK_INVERS, s_invers);
  persist_write_int(PK_KONTRAST, s_kontrast);
}

// ---------- Ansichten ----------
static void eingabe(GContext *ctx, time_t now) {
  Teile t;
  char b[36], z[8];
  const int32_t d = s_h * 3600 + s_m * 60 + s_sek;
  teile_dauer(&t, d, true);
  const char *titel = s_fluss == FL_NEU ? T("NEUER TIMER", "Neuer Timer") : s_fluss == FL_G_AENDERN ? T("DAUER AENDERN", "Dauer ändern")
                                                                          : T("TIMER SPEICHERN", "Timer speichern");
  static const char *const WAS_P[3] = { "STUNDEN WAEHLEN", "MINUTEN WAEHLEN", "SEKUNDEN WAEHLEN" };
  static const char *const WAS_K[3] = { "Stunden wählen", "Minuten wählen", "Sekunden wählen" };
  static const char *const SCHRITT_P[3] = { "1 STD", "1 MIN", "5 S" };
  static const char *const FUSS_P[3] = { "SEL: MIN  ZURUECK: ABBRUCH", "SEL: SEK  ZURUECK: STD", "SEL: FERTIG  ZURUECK: MIN" };
  static const char *const FUSS_K[3] = { "Select: Minuten · Zurück: abbrechen", "Select: Sekunden · Zurück: Stunden",
                                         "Select: fertig · Zurück: Minuten" };
  if (d > 0) { uhrzeit(z, sizeof z, now + d); snprintf(b, sizeof b, T("BEI START: ENDE %s", "bei Start: Ende %s"), z); }
  else snprintf(b, sizeof b, "%s", T("DAUER IST NOCH 0", "Dauer ist noch 0"));
  kopf(ctx, titel, titel);
  if (KLAR) {
    k_text(ctx, WAS_K[s_feld], s_f24b, GRect(8, 30, 184, 30), GTextAlignmentLeft, K_TEXT);
    gross_teile(ctx, t.z, t.n, s_feld * 2, -1, 98, 6, K_TEXT);
    k_text(ctx, b, s_f18b, GRect(4, 150, 192, 24), GTextAlignmentCenter, K_ZWEIT);
    k_text(ctx, s_feld == 2 ? "Hoch/Runter: 5 s" : s_feld == 1 ? "Hoch/Runter: 1 min, halten: schneller" : "Hoch/Runter: 1 h",
           s_f14, GRect(8, 178, 184, 20), GTextAlignmentLeft, K_ZWEIT);
    fuss(ctx, "", FUSS_K[s_feld]);
  } else {
    P_KLEIN(WAS_P[s_feld], 4, 32, P_CYAN);
    gross_teile(ctx, t.z, t.n, s_feld * 2, -1, 94, 7, P_TEXT);
    P_KLEIN_M(b, 150, P_HELL);
    P_KLEIN("HOCH/RUNTER", 4, 176, P_GRAU);
    P_KLEIN_R(SCHRITT_P[s_feld], 196, 176, P_GRAU);
    fuss(ctx, FUSS_P[s_feld], "");
  }
}

static void timer_ansicht(GContext *ctx, time_t now) {
  const Timer *t = &s_t[s_akt];
  Teile tl;
  char titel[24], d[16], uz[8], w[3][16];
  const char *lab[3], *wert[3] = { w[0], w[1], w[2] }, *ober, *ulab, *f_p, *f_k;
  const int32_t rest = rest_von(t, now);
  GColor gc = C_NORMAL, oc = C_GEDIMMT;
  int32_t gross = rest;
  dauer_text(d, sizeof d, t->basis);
  snprintf(titel, sizeof titel, T("TIMER %s", "Timer %s"), d);
  switch (t->z) {
    case Z_BEREIT:                            // bereit und angehalten: normale Farbe (Wunsch Seb 0.7.0, vorher cyan)
      ober = T("BEREIT", "Bereit");
      ulab = T("BEI START: ENDE", "bei Start: Ende"); uhrzeit(uz, sizeof uz, now + rest);
      f_p = "OBEN: START  SEL: MENUE"; f_k = "Oben: Start · Select: Menü";
      break;
    case Z_LAEUFT:
      ober = T("LAEUFT", "Läuft");
      ulab = T("ENDE", "Ende"); uhrzeit(uz, sizeof uz, t->ende);
      f_p = "OBEN: STOPP  SEL: MENUE"; f_k = "Oben: Stopp · Select: Menü";
      break;
    case Z_PAUSE:
      ober = T("ANGEHALTEN", "Angehalten");
      ulab = T("BEI WEITER: ENDE", "bei weiter: Ende"); uhrzeit(uz, sizeof uz, now + rest);
      f_p = "OBEN: WEITER UNTEN: RESET"; f_k = "Oben: weiter · Unten: Reset";
      break;
    default:
      // zählt hoch, rot, mit "+", blinkt. Hoch und Runter tun hier nichts (kein versehentlicher Neustart)
      ober = T("ZEIT UM SEIT", "Zeit um seit"); gross = now - t->ende; gc = oc = C_ROT;
      ulab = T("ABGELAUFEN UM", "abgelaufen um"); uhrzeit(uz, sizeof uz, t->ende);
      f_p = "TASTE: AUS"; f_k = "Taste: aus";   // steht nur hinter dem Kasten (0.7.1)
      break;
  }
  lab[0] = T("DAUER", "Dauer"); dauer_text(w[0], sizeof w[0], t->basis);
  lab[1] = T("ANGEPASST", "Angepasst"); plus_text(w[1], sizeof w[1], t->gesamt - t->basis);
  lab[2] = T("ENDE", "Ende");
  if (t->z == Z_LAEUFT || t->z == Z_UM) uhrzeit(w[2], sizeof w[2], t->ende); else snprintf(w[2], sizeof w[2], "-");
  teile_dauer(&tl, gross, false);
  const bool um = t->z == Z_UM, zeigen = !um;   // abgelaufen: Überzeit steht im Kasten, Ziffern ragten seitlich heraus
  if (um) {                                   // "+" vor die Zeit
    for (int i = tl.n; i > 0; i--) tl.z[i] = tl.z[i - 1];
    tl.z[0] = "+"; tl.n++;
  }
  kopf(ctx, titel, titel);
  if (KLAR) {
    // Papier: größere Ziffern (50–116), Balken dafür schmaler, wie Simzeit
    k_text(ctx, ober, s_f18b, GRect(0, 24, BREITE, 24), GTextAlignmentCenter, oc);
    if (zeigen) gross_teile(ctx, tl.z, tl.n, -1, -1, 83, 9, gc);
    balken(ctx, 122, 10, rest, t->gesamt);
    unter_balken(ctx, ulab, uz, 136);
  } else {
    P_KLEIN_M(ober, 28, oc);
    if (zeigen) gross_teile(ctx, tl.z, tl.n, -1, -1, 80, 10, gc);
    balken(ctx, 124, 12, rest, t->gesamt);
    unter_balken(ctx, ulab, uz, 146);
  }
  drei_werte(ctx, lab, wert, t->z == Z_LAEUFT ? 2 : 0);
  fuss(ctx, f_p, f_k);
}

static void stopp_ansicht(GContext *ctx, time_t now) {
  const Stopp *p = &s_s[s_akt];
  const int32_t ms = stopp_ms(p);
  Teile tl;
  char titel[24], b[24], z[16];
  const char *ober = p->z == Z_LAEUFT ? T("LAEUFT", "Läuft") : p->z == Z_PAUSE ? T("ANGEHALTEN", "Angehalten") : T("BEREIT", "Bereit");
  const GColor gc = C_NORMAL;                 // seit 0.7.0 auch bereit und angehalten normal
  snprintf(titel, sizeof titel, T("STOPPUHR %d", "Stoppuhr %d"), p->nr);
  teile_dauer(&tl, ms / 1000, false);
  kopf(ctx, titel, titel);
  // Runden: die laufende (bzw. letzte Teilstrecke) und die drei letzten fertigen
  const int zeilen = p->rn > 3 ? 3 : p->rn;
  const int y0 = KLAR && !PAPIER ? 120 : 124, dy = KLAR ? 21 : 18;   // Papier: Ziffern reichen bis 116
  if (KLAR) {
    k_text(ctx, ober, s_f18b, GRect(0, PAPIER ? 24 : 26, BREITE, 24), GTextAlignmentCenter, gc);
    gross_teile(ctx, tl.z, tl.n, -1, (int)(ms / 100 % 10), PAPIER ? 83 : 80, 9, gc);
    k_linie(ctx, y0 - 3);
  } else {
    P_KLEIN_M(ober, 28, gc);
    gross_teile(ctx, tl.z, tl.n, -1, (int)(ms / 100 % 10), 76, 9, gc);
    p_punktlinie(ctx, y0 - 5);
  }
  for (int k = -1; k < zeilen; k++) {
    const int y = y0 + (k + 1) * dy;
    int32_t rz;
    if (k < 0) {
      if (p->z == Z_BEREIT) break;
      rz = ms - runde_vor(p);
      snprintf(b, sizeof b, T("RUNDE %d", "Runde %d"), p->rn + 1);
    } else {
      const int nr = p->rn - k;               // 1-basiert
      rz = p->rd[(nr - 1) % MAXR];
      snprintf(b, sizeof b, T("RUNDE %d", "Runde %d"), nr);
    }
    ms_text(z, sizeof z, rz);
    const GColor c = k < 0 ? C_VOR : C_GEDIMMT;
    if (KLAR) {
      k_text(ctx, b, s_f18b, GRect(8, y - 4, 100, 24), GTextAlignmentLeft, c);
      k_text(ctx, z, s_f18b, GRect(92, y - 4, 100, 24), GTextAlignmentRight, k < 0 ? K_TEXT : K_ZWEIT);
    } else {
      P_KLEIN(b, 6, y, c);
      P_KLEIN_R(z, 194, y, k < 0 ? P_TEXT : P_GRAU);
    }
  }
  if (p->z == Z_BEREIT) {
    if (KLAR) k_text(ctx, "Oben rechts starten", s_f18b, GRect(0, y0, BREITE, 24), GTextAlignmentCenter, K_ZWEIT);
    else P_KLEIN_M("OBEN RECHTS STARTEN", y0 + 4, P_GRAU);
  }
  const char *f_p = p->z == Z_LAEUFT ? "OBEN: STOPP  UNTEN: RUNDE" : p->z == Z_PAUSE ? "OBEN: WEITER UNTEN: RESET" : "OBEN: START  SEL: MENUE";
  const char *f_k = p->z == Z_LAEUFT ? "Oben: Stopp · Unten: Runde"
                  : p->z == Z_PAUSE ? "Oben: weiter · Unten: Reset" : "Oben: Start · Select: Menü";
  fuss(ctx, f_p, f_k);
  (void)now;
}

static void anpassen_ansicht(GContext *ctx, time_t now) {
  const Timer *t = &s_t[s_akt];
  Teile tl;
  char uz[8], b[32], d[16];
  const int32_t rest = rest_von(t, now);
  teile_dauer(&tl, rest, false);
  plus_text(d, sizeof d, t->gesamt - t->basis);
  snprintf(b, sizeof b, T("ANGEPASST %s", "angepasst %s"), d);
  if (t->z == Z_LAEUFT) uhrzeit(uz, sizeof uz, t->ende); else uhrzeit(uz, sizeof uz, now + rest);
  kopf(ctx, T("ZEIT ANPASSEN", "Zeit anpassen"), T("ZEIT ANPASSEN", "Zeit anpassen"));
  if (KLAR) {
    k_text(ctx, "Hoch: +1 min · Runter: -1 min", s_f18b, GRect(0, PAPIER ? 24 : 26, BREITE, 24), GTextAlignmentCenter, K_CYAN);
    gross_teile(ctx, tl.z, tl.n, -1, -1, PAPIER ? 83 : 80, 9, K_CYAN);
    balken(ctx, PAPIER ? 122 : 112, PAPIER ? 10 : 14, rest, t->gesamt);
    unter_balken(ctx, t->z == Z_LAEUFT ? "Ende" : "bei Start: Ende", uz, 136);
    k_text(ctx, b, s_f24b, GRect(0, 160, BREITE, 30), GTextAlignmentCenter, K_TEXT);
    fuss(ctx, "", "Select oder Zurück: fertig");
  } else {
    P_KLEIN_M("HOCH +1  RUNTER -1 MIN", 28, P_CYAN);
    gross_teile(ctx, tl.z, tl.n, -1, -1, 80, 10, P_CYAN);
    balken(ctx, 124, 12, rest, t->gesamt);
    unter_balken(ctx, t->z == Z_LAEUFT ? "ENDE" : "BEI START: ENDE", uz, 146);
    p_punktlinie(ctx, 162);
    if (P_W(b, 3) <= 192) P_MITTEL_M(b, 176, P_TEXT); else P_KLEIN_M(b, 180, P_TEXT);
    fuss(ctx, "SEL ODER ZURUECK: FERTIG", "");
  }
}

static int lz(void) { return s_linfo_an ? 6 : LZEILEN; }
static void liste(GContext *ctx) {
  char b[32];
  kopf(ctx, s_ltitel, s_ltitel);
  const int y0 = KLAR ? 28 : 26;
  for (int k = 0; k < lz() && s_loben + k < s_lanz; k++)
    zeile(ctx, s_ltext[s_loben + k], y0 + k * ZEILE, s_loben + k == s_lsel);
  if (s_linfo_an && s_lanz > 0) {
    if (KLAR) {
      k_text(ctx, s_linfo[s_lsel][0], s_f18, GRect(6, 161, 188, 24), GTextAlignmentLeft, K_ZWEIT);
      k_text(ctx, s_linfo[s_lsel][1], s_f18, GRect(6, 181, 188, 24), GTextAlignmentLeft, K_ZWEIT);
    } else {
      p_punktlinie(ctx, 162);
      P_KLEIN(s_linfo[s_lsel][0], 4, 168, P_HELL);
      P_KLEIN(s_linfo[s_lsel][1], 4, 186, P_HELL);
    }
  }
  b[0] = '\0';
  if (s_lanz > lz()) snprintf(b, sizeof b, "%d/%d", s_lsel + 1, s_lanz);
  fuss(ctx, b, b);
}

// ---------- Rückfrage: sicherer Eintrag zuerst, Select führt aus, Zurück bricht ab ----------
static void frage_texte(const char **titel, const char **frage, char *info, size_t ni, const char **z0, const char **z1) {
  char d[16];
  info[0] = '\0';
  *titel = T("TIMER", "Timer");
  switch (s_fr) {
    case FR_SPEICHERN:
      *frage = T("TIMER SPEICHERN?", "Timer speichern?");
      dauer_text(d, sizeof d, s_neu_dauer);
      snprintf(info, ni, T("%s IN DIE LISTE", "%s in die Liste"), d);
      *z0 = T("NEIN", "Nein"); *z1 = T("JA, SPEICHERN", "Ja, speichern");
      break;
    case FR_ERSETZEN:
    case FR_G_LOESCHEN:
      *frage = s_fr == FR_ERSETZEN ? T("TIMER ERSETZEN?", "Timer ersetzen?") : T("TIMER LOESCHEN?", "Timer löschen?");
      dauer_text(d, sizeof d, s_g[s_fr_wert].dauer);
      snprintf(info, ni, T("GESPEICHERT: %s", "gespeichert: %s"), d);
      *z0 = s_fr == FR_ERSETZEN ? T("NICHT ERSETZEN", "Nicht ersetzen") : T("BEHALTEN", "Behalten");
      *z1 = s_fr == FR_ERSETZEN ? T("ERSETZEN", "Ersetzen") : T("LOESCHEN", "Löschen");
      break;
    case FR_T_ENTF: {
      const Timer *t = &s_t[s_fr_wert];
      *frage = T("TIMER ENTFERNEN?", "Timer entfernen?");
      dauer_text(d, sizeof d, rest_von(t, time(NULL)));
      if (t->z == Z_LAEUFT) snprintf(info, ni, T("LAEUFT NOCH %s", "läuft noch %s"), d);
      else if (t->z == Z_PAUSE) snprintf(info, ni, T("ANGEHALTEN BEI %s", "angehalten bei %s"), d);
      else snprintf(info, ni, "%s", T("GESPEICHERTE BLEIBEN", "Gespeicherte bleiben"));
      *z0 = T("BEHALTEN", "Behalten"); *z1 = T("ENTFERNEN", "Entfernen");
      break;
    }
    default: {                                // FR_S_ENTF
      const Stopp *p = &s_s[s_fr_wert];
      *titel = T("STOPPUHR", "Stoppuhr");
      *frage = T("STOPPUHR ENTFERNEN?", "Stoppuhr entfernen?");
      ms_text(d, sizeof d, stopp_ms(p));
      snprintf(info, ni, T("STOPPUHR %d: %s", "Stoppuhr %d: %s"), p->nr, d);
      *z0 = T("BEHALTEN", "Behalten"); *z1 = T("ENTFERNEN", "Entfernen");
      break;
    }
  }
}
// Frage groß, Phosphor bei Bedarf auf zwei Zeilen umbrochen
static void p_frage_text(GContext *ctx, const char *t, int y) {
  if (P_W(t, 3) <= 192) { P_MITTEL(t, 4, y, P_HELL); return; }
  char a[32];
  int teil = 0;
  for (int i = 0; t[i]; i++) {
    if (t[i] != ' ') continue;
    snprintf(a, sizeof a, "%.*s", i, t);
    if (P_W(a, 3) <= 192) teil = i;
  }
  snprintf(a, sizeof a, "%.*s", teil, t);
  P_MITTEL(a, 4, y, P_HELL);
  P_MITTEL(t + teil + 1, 4, y + 22, P_HELL);
}
static void frage_ansicht(GContext *ctx) {
  const char *titel, *frage, *z0, *z1;
  char info[40];
  frage_texte(&titel, &frage, info, sizeof info, &z0, &z1);
  kopf(ctx, titel, titel);
  if (KLAR) {
    graphics_context_set_text_color(ctx, K_TEXT);
    graphics_draw_text(ctx, frage, s_f24b, GRect(8, 32, 184, 60), GTextOverflowModeWordWrap, GTextAlignmentLeft, NULL);
    k_text(ctx, info, s_f18, GRect(8, 96, 184, 24), GTextAlignmentLeft, K_ZWEIT);
  } else {
    p_frage_text(ctx, frage, 40);
    P_KLEIN(info, 4, 100, P_GRAU);
  }
  zeile(ctx, z0, 140, s_wahl == 0);
  zeile(ctx, z1, 140 + ZEILE, s_wahl == 1);
  fuss(ctx, "ZURUECK: ABBRECHEN", "Zurück: abbrechen");
}

// Abgelaufen: Kasten über allem, bis eine Taste gedrückt wird
static void klingel_kasten(GContext *ctx, time_t now) {
  const int i = offener_timer();
  if (i < 0) return;
  char d[16], b[32];
  dauer_text(d, sizeof d, s_t[i].basis);
  snprintf(b, sizeof b, T("TIMER %s", "Timer %s"), d);
  dauer_text(d, sizeof d, now - s_t[i].ende);
  kasten(ctx, 46, 128, C_ROT);
  if (KLAR) {
    k_text(ctx, "Zeit um", s_f28b, GRect(10, 50, 180, 34), GTextAlignmentCenter, C_ROT);
    k_text(ctx, b, s_f24b, GRect(10, 82, 180, 30), GTextAlignmentCenter, K_TEXT);
    char s[24];
    snprintf(s, sizeof s, "+%s", d);          // Überzeit zählt hoch, rot, blinkt
    if (s_blink) k_text(ctx, s, s_f28b, GRect(10, 106, 180, 34), GTextAlignmentCenter, C_ROT);
    k_text(ctx, "Taste: aus", s_f18b, GRect(10, 144, 180, 24), GTextAlignmentCenter, K_CYAN);
  } else {
    P_MITTEL_M("ZEIT UM", 60, C_ROT);
    P_KLEIN_M(b, 90, P_TEXT);
    char s[24];
    snprintf(s, sizeof s, "+%s", d);
    if (s_blink) P_MITTEL_M(s, 110, C_ROT);
    P_KLEIN_M("TASTE: AUS", 150, P_CYAN);
  }
}
static void hinweis_kasten(GContext *ctx) {
  kasten(ctx, 84, 60, WATER ? W_ROSA : C_VOR);
  if (KLAR) {
    graphics_context_set_text_color(ctx, K_TEXT);
    graphics_draw_text(ctx, s_hinweis, s_f18b, GRect(16, 92, 168, 48), GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  } else if (P_W(s_hinweis, 2) <= 168) P_KLEIN_M(s_hinweis, 110, P_HELL);
  else {                                      // auf zwei Zeilen
    char a[40];
    int teil = 0;
    for (int i = 0; s_hinweis[i]; i++) {
      if (s_hinweis[i] != ' ') continue;
      snprintf(a, sizeof a, "%.*s", i, s_hinweis);
      if (P_W(a, 2) <= 168) teil = i;
    }
    snprintf(a, sizeof a, "%.*s", teil, s_hinweis);
    P_KLEIN_M(a, 100, P_HELL);
    P_KLEIN_M(s_hinweis + teil + 1, 118, P_HELL);
  }
}

// ---------- Ansicht Water (0.9.0): Sprungbrett, Springer, Punktrichter, Planschbecken, Game Over ----------
// Der Springer springt beim Start ab und kommt bei 0 im Planschbecken an (Flugbahn = Anteil der Laufzeit), taucht
// ein und ist weg, das Wasser spritzt. Drei Richter halten Stunden, Minuten, Sekunden hoch; wechselt ein Wert,
// senkt der Richter das Schild und hebt es mit dem neuen. Entwurf und Vorlage: Skripte in ~/.local/share/timer-skin/
#define W_PX 122                      // Planschbecken: Mitte
#define W_PY 196                      // Wasserlinie = Oberkante des vorderen Rands
#define W_YT 170                      // Richtertisch: Oberkante (Timer, Eingabe, Zeit anpassen)
#define W_BRETT 62                    // Sprungbrett: Oberkante
#define W_RASEN 204
#define KK(v) ((int32_t)((v) * 12))   // Körperraster des Springers → 1/10 px (Figur 1,2-fach)
#define RAD(r) ((int32_t)((r) * 10430))   // Bogenmaß → Winkeleinheiten (65536 / 2 pi)
#define WN(a) ((a) & (TRIG_MAX_ANGLE - 1))

static int32_t abs32(int32_t v) { return v < 0 ? -v : v; }
// Winkel nach ms bei pro_s Einheiten je Sekunde (Sekunden in Stunden gefaltet, damit nichts überläuft)
static int32_t w_winkel(int32_t ms, int32_t pro_s) { return WN(ms / 1000 % 3600 * pro_s + ms % 1000 * pro_s / 1000); }
static int32_t w_sin(int32_t amp, int32_t winkel) { return amp * sin_lookup(WN(winkel)) / TRIG_MAX_RATIO; }
static int32_t w_ms_seit(time_t t0) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (int32_t)(s - t0) * 1000 + ms;
}
static int32_t w_ease(int32_t x) { return x * x / 1000 * (3000 - 2 * x) / 1000; }   // ‰ → ‰, weich an den Enden
static void w_hms(char w[3][4], int32_t s) {
  if (s < 0) s = 0;
  snprintf(w[0], 4, "%02d", (int)(s / 3600 % 100));
  snprintf(w[1], 4, "%02d", (int)(s / 60 % 60));
  snprintf(w[2], 4, "%02d", (int)(s % 60));
}

// Punktrichter hinter dem Tisch, je ein zweistelliges Schild
typedef struct {
  int yt, bein, p, sel, hoch, kb;     // Tischkante, Tischbeine bis, ms seit dem Wechsel, gewähltes Schild, Jubel, halbe Breite
  int32_t takt;                       // ms fürs Wippen, 0: still
  char neu[3][4], alt[3][4], zusatz[4];
  GColor rahmen;
} Richter;
static const uint32_t R_HAAR[3] = { 0x000000, 0xAA5500, 0xAAAAAA }, R_ANZUG[3] = { 0x555555, 0x0000AA, 0x550055 };
static const int R_X[3] = { 38, 100, 162 };
static void w_richter(GContext *ctx, const Richter *r) {
  for (int i = 0; i < 3; i++) {
    const int x = R_X[i];
    const bool wechsel = r->p < 450 && strcmp(r->neu[i], r->alt[i]) != 0;
    const int senk = wechsel ? w_sin(24, r->p * (TRIG_MAX_ANGLE / 2) / 450) : 0;
    const char *wert = wechsel && r->p < 220 ? r->alt[i] : r->neu[i];
    const int v = r->takt ? w_sin(15, w_winkel(r->takt, 52153) + i * RAD(2)) : 0, wipp = (v + (v < 0 ? -5 : 5)) / 10;
    const int hy = r->yt - 24 + wipp, ky = r->yt - 72 + senk - r->hoch + wipp - (i == r->sel ? 8 : 0);
    const GRect leib = GRect(x - 10, hy + 6, 21, r->yt + 3 - hy - 6);
    graphics_context_set_fill_color(ctx, GColorFromHEX(R_ANZUG[i]));
    graphics_fill_rect(ctx, leib, 4, GCornersTop);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_round_rect(ctx, leib, 4);
    graphics_context_set_stroke_color(ctx, W_HAUT);                          // Arme zum Schild
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_line(ctx, GPoint(x - 8, hy + 9), GPoint(x - 15, ky + 30));
    graphics_draw_line(ctx, GPoint(x + 8, hy + 9), GPoint(x + 15, ky + 30));
    w_kreis(ctx, GPoint(x, hy), 7, W_HAUT, GColorBlack);                      // Kopf, Haare oben
    graphics_context_set_fill_color(ctx, GColorFromHEX(R_HAAR[i]));
    graphics_fill_radial(ctx, GRect(x - 7, hy - 9, 15, 15), GOvalScaleModeFitCircle, 8, DEG_TO_TRIGANGLE(270), TRIG_MAX_ANGLE);
    graphics_fill_radial(ctx, GRect(x - 7, hy - 9, 15, 15), GOvalScaleModeFitCircle, 8, 0, DEG_TO_TRIGANGLE(90));
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_circle(ctx, GPoint(x + 3, hy + 1), 1);
    graphics_fill_circle(ctx, GPoint(x - 2, hy + 1), 1);
    graphics_context_set_fill_color(ctx, W_HAUT);                             // Kinn
    graphics_fill_rect(ctx, GRect(x + 1, hy + 2, 6, 5), 2, GCornersAll);
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_draw_round_rect(ctx, GRect(x + 1, hy + 2, 6, 5), 2);
    const GRect k = GRect(x - r->kb, ky, 2 * r->kb + 1, 31);                  // Schild
    if (i == r->sel) {
      graphics_context_set_fill_color(ctx, W_ROSA);
      graphics_fill_rect(ctx, grect_inset(k, GEdgeInsets(-3)), 6, GCornersAll);
    }
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, k, 3, GCornersAll);
    graphics_context_set_stroke_color(ctx, r->rahmen);
    graphics_draw_round_rect(ctx, k, 3);
    const GColor tc = i == r->sel && !s_blink ? W_HELLBLAU : W_NAVY;
    if (i == 2 && r->zusatz[0]) {                                            // Stoppuhr: Zehntel klein dahinter
      const int w1 = k_breite(wert, s_wf26), w2 = k_breite(r->zusatz, s_wf15), x0 = x - (w1 + w2) / 2;
      k_text(ctx, wert, s_wf26, GRect(x0, ky + 25 - 26, w1 + 4, 32), GTextAlignmentLeft, tc);
      k_text(ctx, r->zusatz, s_wf15, GRect(x0 + w1, ky + 25 - 15, w2 + 4, 20), GTextAlignmentLeft, tc);
    } else k_text(ctx, wert, s_wf26, GRect(x - r->kb, ky + 25 - 26, 2 * r->kb + 1, 32), GTextAlignmentCenter, tc);
  }
  graphics_context_set_stroke_color(ctx, GColorFromHEX(0x555555));         // Tisch mit Beinen
  graphics_context_set_stroke_width(ctx, 1);
  for (int k = 0; k < 10 && r->bein > r->yt + 16; k++)
    graphics_draw_line(ctx, GPoint(20 + k * 18, r->yt + 16), GPoint(20 + k * 18, r->bein));
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(10, r->yt, 181, 17), 2, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, GRect(10, r->yt, 181, 17), 2);
  graphics_context_set_fill_color(ctx, W_ROSA);
  for (int k = 0; k < 10; k++) graphics_fill_rect(ctx, GRect(14 + k * 18, r->yt + 2, 8, 13), 0, GCornerNone);
}

// Springer im Cartoon-Stil: Badekappe, Schwimmbrille, Bäuchlein, schwarze Kontur. Körperpunkte in 1/10 px
typedef struct { int32_t ca, sa; int cx, cy; } Dreh;
static GPoint d_pt(const Dreh *d, int32_t x, int32_t y) {
  const int32_t rx = (x * d->ca - y * d->sa) / TRIG_MAX_RATIO, ry = (x * d->sa + y * d->ca) / TRIG_MAX_RATIO;
  return GPoint(d->cx + (rx + (rx < 0 ? -5 : 5)) / 10, d->cy + (ry + (ry < 0 ? -5 : 5)) / 10);
}
static void d_poly(GContext *ctx, const Dreh *d, const int16_t *xy, int n, GColor fuell) {
  GPoint p[8];
  for (int i = 0; i < n; i++) p[i] = d_pt(d, xy[2 * i], xy[2 * i + 1]);
  w_poly(ctx, p, n, fuell, GColorBlack);
}
// Arm oder Bein ab Gelenk (sx, sy); th: Winkel im Körper (0 = nach oben). ende 1: Hand, 2: Fuß quer nach vorn
static void d_glied(GContext *ctx, const Dreh *d, int32_t sx, int32_t sy, int32_t th, int32_t len, int32_t dicke, int ende) {
  const int32_t dx = sin_lookup(WN(th)), dy = -cos_lookup(WN(th));
  const int32_t ex = sx + dx * len / TRIG_MAX_RATIO, ey = sy + dy * len / TRIG_MAX_RATIO;
  const int32_t wx = -dy * dicke / 2 / TRIG_MAX_RATIO, wy = dx * dicke / 2 / TRIG_MAX_RATIO;
  const int16_t g[8] = { sx + wx, sy + wy, ex + wx, ey + wy, ex - wx, ey - wy, sx - wx, sy - wy };
  d_poly(ctx, d, g, 4, W_HAUT);
  if (ende == 1) w_kreis(ctx, d_pt(d, ex, ey), 3, W_HAUT, GColorBlack);
  if (ende != 2) return;
  const int32_t fx = dy, fy = -dx;
#define FP(a, b) ex + (dx * (a) + fx * (b)) / TRIG_MAX_RATIO, ey + (dy * (a) + fy * (b)) / TRIG_MAX_RATIO
  const int16_t f[8] = { FP(-KK(1), -KK(2)), FP(-KK(1), KK(5.5)), FP(KK(1.8), KK(5.5)), FP(KK(1.8), -KK(2.5)) };
#undef FP
  d_poly(ctx, d, f, 4, W_HAUT);
}
static const int16_t S_RUMPF[8] = { KK(-7), KK(-11), KK(6), KK(-11), KK(7), KK(9), KK(-7), KK(9) };
static const int16_t S_HOSE[8] = { KK(-7), KK(4), KK(9), KK(4), KK(7), KK(12), KK(-7), KK(12) };
static const int16_t S_KINN[8] = { KK(2), KK(-16), KK(8), KK(-14), KK(8), KK(-11), KK(2), KK(-11) };
static const int16_t S_KAPPE[14] = { KK(-7), KK(-18), KK(-6), KK(-23), KK(-2), KK(-26), KK(3), KK(-26), KK(6), KK(-23),
                                     KK(7), KK(-20), KK(-1), KK(-21) };
static const int16_t S_BAND[8] = { KK(-6), KK(-19.5), KK(0.5), KK(-19.5), KK(0.5), KK(-18.5), KK(-6), KK(-18.5) };
// a: Drehung des Körpers (0 = Kopf oben); Arme 0 = hoch; Beine 0 = gerade nach unten
static void w_springer(GContext *ctx, int cx, int cy, int32_t a, int32_t arm_l, int32_t arm_r, int32_t bein_l, int32_t bein_r) {
  const Dreh d = { cos_lookup(WN(a)), sin_lookup(WN(a)), cx, cy };
  d_glied(ctx, &d, KK(-3), KK(9), TRIG_MAX_ANGLE / 2 + bein_l, KK(14), KK(4.5), 2);
  d_glied(ctx, &d, KK(3), KK(9), TRIG_MAX_ANGLE / 2 + bein_r, KK(14), KK(4.5), 2);
  d_poly(ctx, &d, S_RUMPF, 4, W_HAUT);
  w_kreis(ctx, d_pt(&d, KK(4), 0), 7, W_HAUT, GColorBlack);
  d_poly(ctx, &d, S_HOSE, 4, W_HOSE);
  d_glied(ctx, &d, KK(-5), KK(-9), arm_l, KK(13), KK(3.5), 1);
  w_kreis(ctx, d_pt(&d, 0, KK(-18)), 8, W_HAUT, GColorBlack);
  d_poly(ctx, &d, S_KINN, 4, W_HAUT);
  d_poly(ctx, &d, S_KAPPE, 7, W_KAPPE);
  w_kreis(ctx, d_pt(&d, KK(2.5), KK(-19)), 2, W_WASSER, GColorBlack);
  w_kreis(ctx, d_pt(&d, KK(6.5), KK(-19)), 2, W_WASSER, GColorBlack);
  d_poly(ctx, &d, S_BAND, 4, GColorBlack);
  d_glied(ctx, &d, KK(5), KK(-9), arm_r, KK(13), KK(3.5), 1);
}

static void w_brett(GContext *ctx, int d) {   // d: Spitze biegt sich (px)
  const GRect stuetze = GRect(0, W_BRETT + 4, 15, W_RASEN - W_BRETT - 4);
  fuellen(ctx, stuetze, GColorFromHEX(0xAAAAAA));
  graphics_context_set_stroke_color(ctx, W_NAVY);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, stuetze);
  GPoint p[4] = { GPoint(0, W_BRETT), GPoint(66, W_BRETT + d), GPoint(66, W_BRETT + 6 + d), GPoint(0, W_BRETT + 6) };
  w_poly(ctx, p, 4, GColorWhite, W_NAVY);
}
static void w_becken_hinten(GContext *ctx) {
  w_pille(ctx, W_PX - 21, W_PY - 7, 43, 13, W_WASSER_D);
  w_pille(ctx, W_PX - 20, W_PY - 6, 41, 11, W_WASSER);
}
static void w_becken_vorn(GContext *ctx) {
  const GRect r = GRect(W_PX - 22, W_PY, 45, 13);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, r, 6, GCornersAll);
  graphics_context_set_fill_color(ctx, W_ROSA);
  for (int k = -2; k <= 2; k++) graphics_fill_rect(ctx, GRect(W_PX + k * 9 - 2, W_PY + 1, 5, 11), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, W_NAVY);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_round_rect(ctx, r, 6);
}
static void w_schild(GContext *ctx, const char *t) {   // rechts auf Höhe der oberen Taste, Pfeil zur Taste
  const int w = k_breite(t, s_wf15), x = 184 - w;
  w_pille(ctx, x - 10, 40, 240 - x, 23, W_ROSA);
  k_text(ctx, t, s_wf15, GRect(x, 56 - 15, w + 4, 20), GTextAlignmentLeft, W_NAVY);
  GPoint p[3] = { GPoint(188, 46), GPoint(195, 51), GPoint(188, 56) };
  w_poly(ctx, p, 3, W_NAVY, W_NAVY);
}

static const int16_t W_TROPFEN[13][2] = { { -55, -120 }, { -35, -150 }, { -18, -170 }, { 0, -185 }, { 18, -165 }, { 36, -145 },
                                          { 55, -115 }, { -75, -80 }, { 75, -85 }, { -28, -100 }, { 28, -105 }, { -8, -135 }, { 10, -128 } };
static void w_timer(GContext *ctx, time_t now) {
  const Timer *t = &s_t[s_akt];
  char titel[32], a[16], e[8], b[48];
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  // Restzeit genau, Flugzeit seit dem Start (ohne Pausen), Fortschritt in Promille
  int32_t rest_ms = t->z == Z_LAEUFT ? (int32_t)(t->ende - s) * 1000 - ms : t->z == Z_UM ? 0 : t->rest * 1000;
  if (rest_ms < 0) rest_ms = 0;
  const int32_t ges = t->gesamt > 0 ? t->gesamt : 1, flug = ges * 1000 - rest_ms, uhr = (int32_t)(s % 86400) * 1000 + ms;
  int32_t prog = 1000 - rest_ms / ges;
  if (prog < 0) prog = 0;
  if (prog > 1000) prog = 1000;
  // Kopf: Dauer in der Pille rechts neben der Uhrzeit, links steht der Springer auf dem Brett
  char u[8];
  uhrzeit(u, sizeof u, now);
  dauer_text(titel, sizeof titel, t->basis);
  fuellen(ctx, GRect(0, 0, BREITE, HOEHE), C_HG);
  const int uw = k_breite(u, s_wf18), tw = k_breite(titel, s_wf15), kx = 196 - uw - 8 - tw - 20;
  w_pille(ctx, kx, 3, tw + 20, 21, W_ROSA);
  k_text(ctx, titel, s_wf15, GRect(kx + 10, 18 - 15, tw + 2, 20), GTextAlignmentLeft, W_NAVY);
  k_text(ctx, u, s_wf18, GRect(196 - uw - 4, 19 - 18, uw + 4, 24), GTextAlignmentRight, W_NAVY);
  if (t->gesamt != t->basis) plus_text(a, sizeof a, t->gesamt - t->basis); else a[0] = '\0';
  // Brett wippt, solange der Springer bereit steht, und schwingt nach dem Absprung aus
  int biege = 0;
  if (t->z == Z_BEREIT) biege = 3 * abs32(sin_lookup(w_winkel(uhr, 27307))) / TRIG_MAX_RATIO;
  else if (t->z != Z_UM && flug < 1500) { const int32_t f = 1500 - flug; biege = w_sin(50 * f / 1500 * f / 1500, w_winkel(flug, 229477)) / 10; }
  w_brett(ctx, biege);
  // Richter: Restzeit
  Richter r = { .yt = W_YT, .bein = W_RASEN, .p = 1000, .sel = -1, .kb = 24, .takt = uhr, .rahmen = GColorBlack };
  const int32_t rest = t->z == Z_LAEUFT ? (t->ende > s ? (int32_t)(t->ende - s) : 0) : t->z == Z_UM ? 0 : t->rest;
  w_hms(r.neu, rest);
  memcpy(r.alt, r.neu, sizeof r.alt);
  if (t->z == Z_LAEUFT && rest < t->gesamt) { w_hms(r.alt, rest + 1); r.p = ms; }
  if (t->z == Z_UM) {
    const int32_t seit = w_ms_seit(t->ende);
    if (seit >= 0 && seit < 1000) { w_hms(r.alt, 1); r.p = seit; }
    r.hoch = 3;
  }
  w_richter(ctx, &r);
  w_becken_hinten(ctx);
  // Springer: steht und pendelt mit den Armen, fliegt mit rudernden Armen, taucht ein
  const int y_steh = W_BRETT - 30;
  if (t->z == Z_BEREIT) {
    const int32_t pendel = w_sin(RAD(0.35), w_winkel(uhr, 54613));
    w_springer(ctx, 54, y_steh + biege, 0, TRIG_MAX_ANGLE / 2 - RAD(0.25) + pendel, TRIG_MAX_ANGLE / 2 + RAD(0.25) + pendel, 0, 0);
  } else if (t->z != Z_UM) {
    const int x = 54 + (W_PX - 54) * prog / 1000;
    const int y = y_steh - 40 * prog / 1000 + (W_PY - 12 - y_steh + 40) * prog / 1000 * prog / 1000;
    const int32_t a = TRIG_MAX_ANGLE / 2 * w_ease(prog * 11 / 10 > 1000 ? 1000 : prog * 11 / 10) / 1000
                      + w_sin(RAD(0.25), prog * 93873 / 1000);
    w_springer(ctx, x, y, a, w_winkel(flug, 114688), -w_winkel(flug, 88660) + RAD(2.1) + w_sin(RAD(0.8), w_winkel(flug, 177340)),
               w_sin(RAD(0.5), w_winkel(flug, 146028)), -w_sin(RAD(0.6), w_winkel(flug, 114737) + RAD(1)));
  } else {
    const int32_t tau = w_ms_seit(t->ende);
    if (tau >= 0 && tau < 700)
      w_springer(ctx, W_PX, W_PY - 12 + 60 * tau / 1000 + 200 * tau / 1000 * tau / 1000,
                 TRIG_MAX_ANGLE / 2 + w_sin(RAD(0.1), w_winkel(tau, 208600)), 0, 0,
                 w_sin(RAD(0.8), w_winkel(tau, 312900)), -w_sin(RAD(0.8), w_winkel(tau, 312900) + RAD(0.6)));
  }
  // Rasen und vorderer Beckenrand verdecken alles unter der Wasserlinie: das Becken ist unendlich tief
  w_rasen(ctx, W_RASEN);
  w_becken_vorn(ctx);
  if (t->z == Z_UM) {                         // Spritzer
    const int32_t tau = w_ms_seit(t->ende);
    if (tau >= 0 && tau < 1800) {
      const int32_t z = tau * 8 / 10;
      for (unsigned k = 0; k < ARRAY_LENGTH(W_TROPFEN); k++) {
        const int x = W_PX + W_TROPFEN[k][0] * z * 20 / 10000;
        const int y = W_PY - 2 + 70 * W_TROPFEN[k][1] * z / 100000 + z * z / 1000 * 143 / 1000;
        if (y < W_RASEN - 2) w_kreis(ctx, GPoint(x, y), 3 - (k & 1), W_WASSER, W_WASSER_D);
      }
    }
  }
  if (t->z == Z_BEREIT) w_schild(ctx, "Start");
  else if (t->z == Z_PAUSE) w_schild(ctx, "Weiter");
  switch (t->z) {
    case Z_BEREIT: uhrzeit(e, sizeof e, now + t->rest); snprintf(b, sizeof b, "bei Start: Ende %s", e); break;
    case Z_LAEUFT:
      uhrzeit(e, sizeof e, t->ende);
      if (a[0]) snprintf(b, sizeof b, "Ende %s · angepasst %s", e, a); else snprintf(b, sizeof b, "Ende %s", e);
      break;
    case Z_PAUSE: snprintf(b, sizeof b, "%s", "Pause · unten: Reset"); break;
    default: uhrzeit(e, sizeof e, t->ende); snprintf(b, sizeof b, "abgelaufen um %s", e); break;
  }
  w_rasen_text(ctx, b, 222);
}

// Eingabe: die Richter halten die Dauer hoch, das gewählte Schild hebt sich und blinkt
static void w_eingabe(GContext *ctx, time_t now) {
  char b[36], z[8];
  const int32_t d = s_h * 3600 + s_m * 60 + s_sek;
  static const char *const WAS[3] = { "Stunden wählen", "Minuten wählen", "Sekunden wählen" };
  static const char *const SCHRITT[3] = { "Hoch/Runter: 1 Stunde", "Hoch/Runter: 1 Minute", "Hoch/Runter: 5 Sekunden" };
  static const char *const FUSS[3] = { "Sel: Minuten · Zurück: Abbruch", "Sel: Sekunden · Zurück: Stunden",
                                       "Sel: fertig · Zurück: Minuten" };
  if (d > 0) { uhrzeit(z, sizeof z, now + d); snprintf(b, sizeof b, "bei Start: Ende %s", z); }
  else snprintf(b, sizeof b, "%s", "Dauer ist noch 0");
  w_kopf(ctx, s_fluss == FL_NEU ? "Neuer Timer" : s_fluss == FL_G_AENDERN ? "Dauer ändern" : "Timer speichern", W_ROSA, W_NAVY);
  k_text(ctx, WAS[s_feld], s_wf18, GRect(8, 44 - 18, 184, 24), GTextAlignmentLeft, W_NAVY);
  k_text(ctx, b, s_wf15, GRect(8, 62 - 15, 184, 20), GTextAlignmentLeft, W_NAVY);
  k_text(ctx, SCHRITT[s_feld], s_wf15, GRect(8, 79 - 15, 184, 20), GTextAlignmentLeft, W_WASSER_D);
  Richter r = { .yt = W_YT, .bein = W_RASEN, .p = 1000, .sel = s_feld, .kb = 24, .rahmen = GColorBlack };
  snprintf(r.neu[0], 4, "%02d", s_h);
  snprintf(r.neu[1], 4, "%02d", s_m);
  snprintf(r.neu[2], 4, "%02d", s_sek);
  memcpy(r.alt, r.neu, sizeof r.alt);
  w_richter(ctx, &r);
  w_rasen(ctx, W_RASEN);
  w_rasen_text(ctx, FUSS[s_feld], 222);
}

static void w_anpassen(GContext *ctx, time_t now) {
  const Timer *t = &s_t[s_akt];
  char uz[8], b[40], d[16];
  const int32_t rest = rest_von(t, now);
  w_kopf(ctx, "Zeit anpassen", W_ROSA, W_NAVY);
  k_text(ctx, "Oben +1 min, unten -1 min", s_wf15, GRect(8, 44 - 15, 184, 20), GTextAlignmentLeft, W_WASSER_D);
  plus_text(d, sizeof d, t->gesamt - t->basis);
  snprintf(b, sizeof b, "angepasst %s", d);
  k_text(ctx, b, s_wf18, GRect(8, 64 - 18, 184, 24), GTextAlignmentLeft, W_NAVY);
  uhrzeit(uz, sizeof uz, t->z == Z_LAEUFT ? t->ende : now + rest);
  snprintf(b, sizeof b, t->z == Z_LAEUFT ? "Ende %s" : "bei Start: Ende %s", uz);
  k_text(ctx, b, s_wf15, GRect(8, 82 - 15, 184, 20), GTextAlignmentLeft, W_NAVY);
  Richter r = { .yt = W_YT, .bein = W_RASEN, .p = 1000, .sel = -1, .kb = 24, .rahmen = W_ROSA };
  w_hms(r.neu, rest);
  memcpy(r.alt, r.neu, sizeof r.alt);
  w_richter(ctx, &r);
  w_rasen(ctx, W_RASEN);
  w_rasen_text(ctx, "Select oder Zurück: fertig", 222);
}

// Stoppuhr: Richter oben (Stunden, Minuten, Sekunden mit Zehnteln), darunter die Runden
static void w_stopp(GContext *ctx, time_t now) {
  const Stopp *p = &s_s[s_akt];
  const int32_t ms = stopp_ms(p);
  char titel[24], b[24], z[16];
  time_t s;
  uint16_t m;
  time_ms(&s, &m);
  snprintf(titel, sizeof titel, "Stoppuhr %d", p->nr);
  w_kopf(ctx, titel, W_GRUEN_D, GColorWhite);
  Richter r = { .yt = 104, .bein = 104, .p = 1000, .sel = -1, .kb = 26, .rahmen = GColorBlack };
  r.takt = p->z == Z_LAEUFT ? (int32_t)(s % 86400) * 1000 + m : 0;
  w_hms(r.neu, ms / 1000);
  memcpy(r.alt, r.neu, sizeof r.alt);
  if (p->z == Z_LAEUFT && ms >= 1000) { w_hms(r.alt, ms / 1000 - 1); r.p = ms % 1000; }
  snprintf(r.zusatz, sizeof r.zusatz, ".%d", (int)(ms / 100 % 10));
  w_richter(ctx, &r);
  const int zeilen = p->rn > 3 ? 3 : p->rn;
  for (int k = -1; k < zeilen; k++) {        // laufende Runde und die drei letzten
    const int y = 126 + (k + 1) * 20;
    int32_t rz;
    if (k < 0) {
      if (p->z == Z_BEREIT) break;
      rz = ms - runde_vor(p);
      snprintf(b, sizeof b, "Runde %d", p->rn + 1);
    } else {
      const int nr = p->rn - k;
      rz = p->rd[(nr - 1) % MAXR];
      snprintf(b, sizeof b, "Runde %d", nr);
    }
    ms_text(z, sizeof z, rz);
    w_pille(ctx, 6, y, 189, 19, k < 0 ? W_ROSA : GColorWhite);
    k_text(ctx, b, s_wf15, GRect(16, y + 14 - 15, 100, 20), GTextAlignmentLeft, W_NAVY);
    k_text(ctx, z, s_wf15, GRect(84, y + 14 - 15, 100, 20), GTextAlignmentRight, W_NAVY);
  }
  if (p->z == Z_BEREIT) k_text(ctx, "Oben rechts starten", s_wf18, GRect(0, 162 - 18, BREITE, 24), GTextAlignmentCenter, W_NAVY);
  w_rasen(ctx, 206);
  w_rasen_text(ctx, p->z == Z_LAEUFT ? "Oben: Stopp · Unten: Runde" : p->z == Z_PAUSE ? "Oben: weiter · Unten: Reset"
                                                                                     : "Oben: Start · Select: Menü", 222);
  (void)now;
}

// Zeit um: Kasten im Game-Boy-Grün mit Pixelschrift, Überzeit zählt hoch und blinkt
static void w_gameboy(GContext *ctx, time_t now) {
  const int i = offener_timer();
  if (i < 0) return;
  char d[16], b[32], u[24];
  const GColor dunkel = GColorFromHEX(0x005500);
  dauer_text(d, sizeof d, s_t[i].basis);
  snprintf(b, sizeof b, "TIMER %s", d);
  dauer_text(d, sizeof d, now - s_t[i].ende);
  snprintf(u, sizeof u, "+%s", d);
  fuellen(ctx, GRect(10, 30, 180, 124), GColorFromHEX(0x555500));
  fuellen(ctx, GRect(14, 34, 172, 116), GColorFromHEX(0xAAAA00));
  graphics_context_set_stroke_color(ctx, dunkel);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_rect(ctx, GRect(18, 38, 164, 108));
  p_text(ctx, F35, F35_N, F35_H, "GAME OVER", (BREITE - P_W("GAME OVER", 3)) / 2, 46, 3, dunkel);
  P_KLEIN_M(b, 70, dunkel);
  if (s_blink) {
    int sc = 4;
    while (sc > 2 && P_GW(u, sc) > 156) sc--;
    P_GROSS(u, (BREITE - P_GW(u, sc)) / 2, 88, sc, dunkel);
  }
  P_KLEIN_M("TASTE: AUS", 128, dunkel);
}

static void zeichnen(Layer *layer, GContext *ctx) {
  const time_t now = time(NULL);
  if (WATER) graphics_context_set_antialiased(ctx, true);
  switch (s_modus) {
    case M_EIN: if (WATER) w_eingabe(ctx, now); else eingabe(ctx, now); break;
    case M_TIMER: if (WATER) w_timer(ctx, now); else timer_ansicht(ctx, now); break;
    case M_STOPP: if (WATER) w_stopp(ctx, now); else stopp_ansicht(ctx, now); break;
    case M_ANPASSEN: if (WATER) w_anpassen(ctx, now); else anpassen_ansicht(ctx, now); break;
    case M_FRAGE: frage_ansicht(ctx); break;
    default: liste(ctx); break;
  }
  if (offener_timer() >= 0) { if (WATER) w_gameboy(ctx, now); else klingel_kasten(ctx, now); }
  else if (s_hinweis[0]) hinweis_kasten(ctx);
}

// ---------- Zustand der Anzeige ----------
static void neu_zeichnen(void) { if (s_layer) layer_mark_dirty(s_layer); }
static void zum_watchface(void) {
  exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);
  window_stack_pop_all(true);
}
static bool etwas_laeuft(void) {
  for (int i = 0; i < MAXT; i++) if (s_t[i].z == Z_LAEUFT) return true;
  for (int i = 0; i < MAXS; i++) if (s_s[i].z == Z_LAEUFT) return true;
  return false;
}
// 30 s ohne Taste: zu, wenn eingestellt, kein Klingeln offen und weder Timer noch Stoppuhr läuft
static void ruhe_cb(void *d) {
  s_ruhe_timer = NULL;
  if (!s_ruhe || offener_timer() >= 0 || etwas_laeuft()) { s_ruhe_timer = app_timer_register(RUHE_MS, ruhe_cb, NULL); return; }
  zum_watchface();
}
// Klingeln abgestellt, danach keine Taste (also auch kein neuer Start): zu. Andere laufende laufen per Wecker weiter
static void zu_cb(void *d) {
  s_zu_timer = NULL;
  if (offener_timer() < 0) zum_watchface();
}
static void zu_aus(void) { if (s_zu_timer) { app_timer_cancel(s_zu_timer); s_zu_timer = NULL; } }
static void ruhe_neu(void) {                  // nach jeder Taste
  if (s_ruhe_timer) app_timer_cancel(s_ruhe_timer);
  s_ruhe_timer = app_timer_register(RUHE_MS, ruhe_cb, NULL);
}
// Blinkt: Feld in der Eingabe; Zeit eines abgelaufenen Timers (an 600 ms, aus 300 ms)
static bool um_gezeigt(void) { return offener_timer() >= 0 || (s_modus == M_TIMER && s_t[s_akt].z == Z_UM); }
static void blink_cb(void *d) {
  s_blink_timer = NULL;
  const bool um = um_gezeigt();
  if (s_modus != M_EIN && !um) { s_blink = true; return; }
  s_blink = !s_blink;
  neu_zeichnen();
  s_blink_timer = app_timer_register(um ? (s_blink ? 600 : 300) : BLINK_MS, blink_cb, NULL);
}
static void blink_neu(void) {                 // nach Tastendruck: Feld sofort sichtbar, Takt von vorn
  s_blink = true;
  if (s_blink_timer) { app_timer_cancel(s_blink_timer); s_blink_timer = NULL; }
  if (s_modus == M_EIN) s_blink_timer = app_timer_register(BLINK_MS * 2, blink_cb, NULL);
  else if (um_gezeigt()) s_blink_timer = app_timer_register(600, blink_cb, NULL);
}
static void lauf_cb(void *d) {
  s_lauf_timer = NULL;
  if (s_modus != M_LISTE) return;
  s_lauf_off++;
  neu_zeichnen();
  s_lauf_timer = app_timer_register(120, lauf_cb, NULL);
}
static void lauf_neu(void) {                  // gewählte Zeile zu lang: nach 1 s läuft sie durch
  s_lauf_off = 0;
  if (s_lauf_timer) { app_timer_cancel(s_lauf_timer); s_lauf_timer = NULL; }
  if (s_modus == M_LISTE && s_lanz > 0 && zu_lang(s_ltext[s_lsel])) s_lauf_timer = app_timer_register(1000, lauf_cb, NULL);
}
static void schnell_cb(void *d) {             // laufende Stoppuhr: Zehntel
  s_schnell_timer = NULL;
  if (s_modus != M_STOPP || s_s[s_akt].z != Z_LAEUFT) return;
  neu_zeichnen();
  s_schnell_timer = app_timer_register(100, schnell_cb, NULL);
}
static void schnell_neu(void) { if (!s_schnell_timer) schnell_cb(NULL); }
// Water: flüssig (100 ms) nur 30 s nach einer Taste oder dem Öffnen, in den letzten 10 s und beim Eintauchen,
// sonst im Sekundentakt (Akku)
static bool w_anim_noetig(void) {
  if (!WATER || s_modus != M_TIMER) return false;
  const Timer *t = &s_t[s_akt];
  const time_t now = time(NULL);
  if (t->z == Z_UM) return now - t->ende < 2;
  if (t->z == Z_LAEUFT && t->ende - now <= 10) return true;
  return (t->z == Z_BEREIT || t->z == Z_LAEUFT) && now - s_taste < 30;
}
static void anim_cb(void *d) {
  s_anim_timer = NULL;
  if (!w_anim_noetig()) return;
  neu_zeichnen();
  s_anim_timer = app_timer_register(100, anim_cb, NULL);
}
static void anim_neu(void) { if (!s_anim_timer) anim_cb(NULL); }
// Letzte Ansicht mit etwas Laufendem: beim nächsten Öffnen wieder dort
static void letzte_merken(void) {
  const int v = s_modus == M_TIMER ? 16 + s_akt : s_modus == M_STOPP ? 32 + s_akt : -1;
  if (v >= 0 && v != s_letzte_ansicht) { s_letzte_ansicht = v; persist_write_int(PK_ANSICHT, v); }
  if (s_modus == M_STOPP) s_stopp_zuletzt = s_akt;
}
static void modus(int m) {
  s_modus = m;
  letzte_merken();
  lauf_neu();
  blink_neu();
  schnell_neu();
  anim_neu();
  neu_zeichnen();
}
static void hinweis_cb(void *d) { s_hinweis_timer = NULL; s_hinweis[0] = '\0'; neu_zeichnen(); }
static void hinweis(const char *t) {
  snprintf(s_hinweis, sizeof s_hinweis, "%s", t);
  if (s_hinweis_timer) app_timer_cancel(s_hinweis_timer);
  s_hinweis_timer = app_timer_register(HINWEIS_MS, hinweis_cb, NULL);
  neu_zeichnen();
}

// ---------- Signal: Ton und Vibration ----------
// Piepser wie eine Sportuhr mit Piezo: Vierergruppen kurzer, heller Sinustöne. Lang (Klingeln): zwei Gruppen,
// kurz (Nachklingeln): eine. Hoch ≈ 3,95 kHz (MIDI 107), tiefer ≈ 2,64 kHz (MIDI 100)
static void ton(bool lang) {
#if defined(_PBL_API_EXISTS_speaker_play_notes)
  if (!s_klang) return;
  static SpeakerNote n[17];
  const uint8_t note = s_klang == 1 ? 107 : 100;
  int k = 0;
  for (int g = 0; g < (lang ? 2 : 1); g++) {
    if (g) n[k++] = (SpeakerNote){ 0, SpeakerWaveformSine, 400, 0, 0 };
    for (int i = 0; i < 4; i++) {
      n[k++] = (SpeakerNote){ note, SpeakerWaveformSine, 70, 0, 0 };
      if (i < 3) n[k++] = (SpeakerNote){ 0, SpeakerWaveformSine, 50, 0, 0 };
    }
  }
  speaker_play_notes(n, k, W_LAUT[s_laut]);
#endif
}
static void signal(bool lang) {
  static const uint32_t SEG[] = { 150, 100, 150, 100, 150 };
  light_enable_interaction();
  if (s_vib) vibes_enqueue_custom_pattern((VibePattern){ .durations = SEG, .num_segments = lang ? ARRAY_LENGTH(SEG) : 1 });
  ton(lang);
}
static void klingel_cb(void *d) {
  s_klingel_timer = NULL;
  if (offener_timer() < 0 || time(NULL) >= s_klingel_bis) return;
  signal(true);
  s_klingel_timer = app_timer_register(KLINGEL_TAKT, klingel_cb, NULL);
}
static void signal_aus(void) {
  if (s_klingel_timer) { app_timer_cancel(s_klingel_timer); s_klingel_timer = NULL; }
  vibes_cancel();
#if defined(_PBL_API_EXISTS_speaker_stop)
  speaker_stop();
#endif
}
static void klingeln(int i) {
  s_klingel_bis = time(NULL) + W_KLINGEL[s_klingel];
  if (!s_klingel_timer) klingel_cb(NULL);
  // aus Liste, Timer und Stoppuhr direkt zum abgelaufenen Timer; Eingaben, Menüs und Rückfragen bleiben stehen
  if ((s_modus == M_LISTE && s_lart <= LI_STOPP) || s_modus == M_TIMER || s_modus == M_STOPP) { s_akt = i; modus(M_TIMER); }
}
// Jede Taste stellt ab und zeigt den Timer (von überall), wieder bereit mit voller Zeit; bewirkt sonst nichts
static bool quittieren(void) {
  const int k = offener_timer();
  if (k < 0) return false;
  // 0.7.1 (Wunsch Seb): abgelaufene stehen danach wieder mit voller Zeit bereit, Hoch startet neu
  for (int i = 0; i < MAXT; i++) if (s_t[i].z == Z_UM) timer_reset(&s_t[i]);
  signal_aus();
  speichern();
  wecker_stellen();
  s_akt = k;
  modus(M_TIMER);
  return true;
}

// ---------- Ablauf prüfen, Wecker ----------
static time_t nach_zeit(const Timer *t, int k) {   // k-tes Nachklingeln (ab 1)
  return t->ende + W_KLINGEL[s_klingel] + (time_t)k * W_NACH[s_nach] * 60;
}
static void pruefen(void) {
  const time_t now = time(NULL);
  bool geaendert = false, kurz = false;
  int neu = -1;
  for (int i = 0; i < MAXT; i++) {
    Timer *t = &s_t[i];
    if (t->z == Z_LAEUFT && now >= t->ende) {
      t->z = Z_UM; t->quitt = 0; t->nach = 0; geaendert = true;
      if (now - t->ende < 120 && (neu < 0 || t->ende < s_t[neu].ende)) neu = i;   // später entdeckt: still, Kasten bleibt
    } else if (t->z == Z_UM && !t->quitt && W_NACH[s_nach]) {
      const int32_t iv = W_NACH[s_nach] * 60;
      const time_t b = t->ende + W_KLINGEL[s_klingel];
      if (now >= b + iv) {
        const int k = (now - b) / iv;
        if (k > t->nach) {
          if (nach_zeit(t, k) <= t->ende + NACH_MAX && now - nach_zeit(t, k) < 60) kurz = true;
          t->nach = k > 255 ? 255 : k;
          geaendert = true;
        }
      }
    }
  }
  if (geaendert) { speichern(); wecker_stellen(); }
  if (neu >= 0) klingeln(neu);
  else if (kurz && !s_klingel_timer) signal(true);
  if (geaendert || neu >= 0) neu_zeichnen();
  if (um_gezeigt() && !s_blink_timer) blink_neu();   // gezeigter Timer still abgelaufen (etwa beim Öffnen)
}
// Wecker: Ende jedes laufenden Timers und das nächste Nachklingeln. Höchstens 8 je App, keiner innerhalb
// 1 min eines anderen (auch anderer Apps) — dann 30 s später versuchen; bei offener App zählt ohnehin der Takt
static void wecker_stellen(void) {
  wakeup_cancel_all();
  const time_t now = time(NULL);
  time_t z[MAXT];
  int n = 0;
  for (int i = 0; i < MAXT; i++) {
    const Timer *t = &s_t[i];
    if (t->z == Z_LAEUFT && t->ende > now) z[n++] = t->ende;
    else if (t->z == Z_UM && !t->quitt && W_NACH[s_nach]) {
      const time_t nx = nach_zeit(t, t->nach + 1);
      if (nx <= t->ende + NACH_MAX && nx > now) z[n++] = nx;
    }
  }
  for (int a = 1; a < n; a++)
    for (int b = a; b > 0 && z[b - 1] > z[b]; b--) { const time_t h = z[b]; z[b] = z[b - 1]; z[b - 1] = h; }
  int gestellt = 0;
  for (int k = 0; k < n && gestellt < 8; k++)
    for (int v = 0; v < 4; v++) {
      const WakeupId id = wakeup_schedule(z[k] + v * 30, 0, true);
      if (id >= 0) { gestellt++; break; }
      if (id != E_RANGE) break;
    }
}

// ---------- Listen ----------
static void liste_beginnen(int art, const char *titel) {
  s_lart = art;
  s_lanz = 0;
  s_linfo_an = false;
  snprintf(s_ltitel, sizeof s_ltitel, "%s", titel);
}
static void eintrag(const char *t, int aktion, int wert) {
  if (s_lanz >= LMAX) return;
  snprintf(s_ltext[s_lanz], sizeof s_ltext[0], "%s", t);
  s_linfo[s_lanz][0][0] = s_linfo[s_lanz][1][0] = '\0';
  s_lakt[s_lanz] = aktion;
  s_lwert[s_lanz] = wert;
  s_lanz++;
}
static void info(const char *a, const char *b) {   // Infozeilen zum zuletzt angelegten Eintrag
  if (!s_lanz) return;
  snprintf(s_linfo[s_lanz - 1][0], sizeof s_linfo[0][0], "%s", a);
  snprintf(s_linfo[s_lanz - 1][1], sizeof s_linfo[0][1], "%s", b);
  s_linfo_an = true;
}
static int liste_index(int aktion, int wert) {   // wert < 0: egal; nicht gefunden: -1
  for (int i = 0; i < s_lanz; i++) if (s_lakt[i] == aktion && (wert < 0 || s_lwert[i] == wert)) return i;
  return -1;
}
static void liste_fertig(int sel) {
  s_lsel = sel >= 0 && sel < s_lanz ? sel : 0;
  s_loben = s_lsel >= lz() ? s_lsel - lz() + 1 : 0;
  modus(M_LISTE);
}

// Bereich Stoppuhr zeigt: die zuletzt gezeigte, sonst die erste laufende, sonst die erste; keine: -1
static int stopp_wahl(void) {
  if (s_stopp_zuletzt >= 0 && s_stopp_zuletzt < MAXS && s_s[s_stopp_zuletzt].z != Z_FREI) return s_stopp_zuletzt;
  int8_t ix[MAXS];
  const int n = stopp_sortiert(ix);
  for (int k = 0; k < n; k++) if (s_s[ix[k]].z == Z_LAEUFT) return ix[k];
  return n ? ix[0] : -1;
}
static int stopp_anzahl(void) { int n = 0; for (int i = 0; i < MAXS; i++) if (s_s[i].z != Z_FREI) n++; return n; }

// Listen Auswahl, Timer, Stoppuhren: Texte der Zeilen, jede Sekunde neu (Restzeiten)
static void start_text(int k, time_t now) {
  char a[16], b[16], *t = s_ltext[k], *i0 = s_linfo[k][0], *i1 = s_linfo[k][1];
  const int w = s_lwert[k];
  i0[0] = i1[0] = '\0';
  switch (s_lakt[k]) {
    case A_BEREICH_T: {
      int nl = 0, np = 0, nu = 0, ng = 0, fr = -1;
      for (int i = 0; i < MAXT; i++) {
        const Timer *x = &s_t[i];
        if (x->z == Z_LAEUFT) { nl++; if (fr < 0 || x->ende < s_t[fr].ende) fr = i; }
        else if (x->z == Z_PAUSE) np++;
        else if (x->z == Z_UM) nu++;
      }
      for (int i = 0; i < MAXG; i++) if (s_g[i].dauer) ng++;
      snprintf(t, 40, "%s", T("TIMER >", "Timer >"));
      if (nu) snprintf(i0, 30, T("%d ABGELAUFEN", "%d abgelaufen"), nu);
      else if (nl) {
        uhrzeit(a, sizeof a, s_t[fr].ende);
        if (nl == 1) snprintf(i0, 30, T("1 LAEUFT, ENDE %s", "1 läuft, Ende %s"), a);
        else snprintf(i0, 30, T("%d LAUFEN, ENDE %s", "%d laufen, Ende %s"), nl, a);
      } else if (np) snprintf(i0, 30, T("%d ANGEHALTEN", "%d angehalten"), np);
      else snprintf(i0, 30, "%s", T("KEINER LAEUFT", "keiner läuft"));
      if (np && (nl || nu)) snprintf(i1, 30, T("%d ANGEHALTEN", "%d angehalten"), np);
      else if (ng) snprintf(i1, 30, T("%d GESPEICHERT", "%d gespeichert"), ng);
      else snprintf(i1, 30, "%s", T("NOCH KEINER GESPEICHERT", "noch keiner gespeichert"));
      break;
    }
    case A_BEREICH_S: {
      int n = 0, nl = 0;
      for (int i = 0; i < MAXS; i++) if (s_s[i].z != Z_FREI) { n++; if (s_s[i].z == Z_LAEUFT) nl++; }
      snprintf(t, 40, "%s", T("STOPPUHR >", "Stoppuhr >"));
      const int k = stopp_wahl();
      if (k < 0 || s_s[k].z == Z_BEREIT) {
        snprintf(i0, 30, "%s", T("OEFFNET GROSS,", "öffnet groß,"));
        snprintf(i1, 30, "%s", T("DANN OBEN RECHTS: START", "dann oben rechts: Start"));
        break;
      }
      dauer_text(a, sizeof a, stopp_ms(&s_s[k]) / 1000);
      snprintf(i0, 30, T("STOPPUHR %d: %s", "Stoppuhr %d: %s"), s_s[k].nr, a);
      if (n > 1 && nl == 1) snprintf(i1, 30, T("1 VON %d LAEUFT", "1 von %d läuft"), n);
      else if (n > 1) snprintf(i1, 30, T("%d VON %d LAUFEN", "%d von %d laufen"), nl, n);
      else snprintf(i1, 30, "%s", nl ? T("LAEUFT", "läuft") : T("ANGEHALTEN", "angehalten"));
      break;
    }
    case A_NEU_T:
      snprintf(t, 40, "%s", T("NEUER TIMER >", "Neuer Timer >"));
      snprintf(i0, 30, "%s", T("ZEIT EINSTELLEN,", "Zeit einstellen,"));
      snprintf(i1, 30, "%s", T("DANN OBEN RECHTS: START", "dann oben rechts: Start"));
      break;
    case A_NEU_S:
      snprintf(t, 40, "%s", T("NEUE STOPPUHR", "Neue Stoppuhr"));
      snprintf(i0, 30, "%s", T("BIS ZU 5 GLEICHZEITIG,", "Bis zu 5 gleichzeitig,"));
      snprintf(i1, 30, "%s", T("LAUFEN IMMER WEITER", "laufen immer weiter"));
      break;
    case A_T: {
      const Timer *x = &s_t[w];
      dauer_text(a, sizeof a, rest_von(x, now));
      dauer_text(b, sizeof b, x->basis);
      if (x->z == Z_LAEUFT) {
        snprintf(t, 40, T("NOCH %s VON %s", "noch %s von %s"), a, b);
        uhrzeit(a, sizeof a, x->ende);
        snprintf(i0, 30, T("LAEUFT, ENDE %s", "läuft, Ende %s"), a);
      } else if (x->z == Z_PAUSE) {
        snprintf(t, 40, T("HALT %s VON %s", "angehalten %s von %s"), a, b);
        snprintf(i0, 30, "%s", T("ANGEHALTEN", "angehalten"));
      } else if (x->z == Z_UM) {
        snprintf(t, 40, T("ZEIT UM: %s", "Zeit um: %s"), b);
        dauer_text(a, sizeof a, now - x->ende);
        snprintf(i0, 30, T("SEIT %s", "seit %s"), a);
      } else {
        snprintf(t, 40, T("BEREIT: %s", "bereit: %s"), b);
        snprintf(i0, 30, "%s", T("NOCH NICHT GESTARTET", "noch nicht gestartet"));
      }
      if (x->gesamt != x->basis) { plus_text(a, sizeof a, x->gesamt - x->basis); snprintf(i1, 30, T("ANGEPASST %s", "angepasst %s"), a); }
      break;
    }
    case A_S: {
      const Stopp *p = &s_s[w];
      dauer_text(a, sizeof a, stopp_ms(p) / 1000);
      snprintf(t, 40, T("STOPPUHR %d: %s", "Stoppuhr %d: %s"), p->nr, a);
      if (p->z == Z_LAEUFT) { uhrzeit(a, sizeof a, p->erst); snprintf(i0, 30, T("LAEUFT, START %s", "läuft, Start %s"), a); }
      else snprintf(i0, 30, "%s", p->z == Z_PAUSE ? T("ANGEHALTEN", "angehalten") : T("BEREIT", "bereit"));
      if (p->rn == 1) snprintf(i1, 30, "%s", T("1 RUNDE", "1 Runde"));
      else if (p->rn) snprintf(i1, 30, T("%d RUNDEN", "%d Runden"), p->rn);
      break;
    }
    case A_G: {
      const Gesp *g = &s_g[w];
      dauer_text(a, sizeof a, g->dauer);
      snprintf(t, 40, T("TIMER %s", "Timer %s"), a);
      uhrzeit(a, sizeof a, now + g->dauer);
      snprintf(i0, 30, T("BEI START: ENDE %s", "bei Start: Ende %s"), a);
      if (g->nutzung) snprintf(i1, 30, T("%dX GESTARTET", "%dx gestartet"), g->nutzung);
      else snprintf(i1, 30, "%s", T("NOCH NIE GESTARTET", "noch nie gestartet"));
      break;
    }
  }
}
static void start_texte(void) {
  const time_t now = time(NULL);
  for (int k = 0; k < s_lanz; k++) start_text(k, now);
}
// Beim Zeigen der Startliste: nie gestartete bzw. zurückgesetzte und gesehene abgelaufene Einträge fallen weg
static void aufraeumen(void) {
  bool weg = false;
  for (int i = 0; i < MAXT; i++)
    if (s_t[i].z == Z_BEREIT || (s_t[i].z == Z_UM && s_t[i].quitt)) { s_t[i].z = Z_FREI; weg = true; }
  for (int i = 0; i < MAXS; i++) if (s_s[i].z == Z_BEREIT) { s_s[i].z = Z_FREI; weg = true; }
  if (weg) speichern();
}
static void start_zeigen(int akt, int wert) {
  int8_t ix[MAXG];
  const time_t now = time(NULL);
  aufraeumen();
  liste_beginnen(LI_START, T("TIMER", "Timer"));
  eintrag("", A_NEU_T, 0);
  int n = timer_sortiert(ix, now);
  for (int k = 0; k < n; k++) eintrag("", A_T, ix[k]);
  n = gesp_sortiert(ix, false);
  for (int k = 0; k < n; k++) eintrag("", A_G, ix[k]);
  s_linfo_an = true;
  start_texte();
  liste_fertig(liste_index(akt, wert));
}
// Erste Seite: Timer oder Stoppuhr
static void wahl_zeigen(int akt) {
  aufraeumen();
  liste_beginnen(LI_WAHL, T("START", "Start"));
  eintrag("", A_BEREICH_T, 0);
  eintrag("", A_BEREICH_S, 0);
  s_linfo_an = true;
  start_texte();
  liste_fertig(liste_index(akt, -1));
}
// Alle Stoppuhren (aus dem Menü der Stoppuhr), zum Wechseln
static void stopps_zeigen(void) {
  int8_t ix[MAXS];
  liste_beginnen(LI_STOPP, T("STOPPUHREN", "Stoppuhren"));
  if (stopp_frei() >= 0) eintrag("", A_NEU_S, 0);
  const int n = stopp_sortiert(ix);
  for (int k = 0; k < n; k++) eintrag("", A_S, ix[k]);
  s_linfo_an = true;
  start_texte();
  liste_fertig(liste_index(A_S, s_akt));
}
// Bereich Stoppuhr: gleich groß; gibt es keine, eine neue bereite
static void stopp_bereich(void) {
  aufraeumen();
  const int k = stopp_wahl();
  if (k < 0) { stopp_neu(); return; }
  s_akt = k;
  modus(M_STOPP);
}
static void ersetzen_zeigen(void) {           // alle Plätze belegt: selten genutzte oben
  int8_t ix[MAXG];
  char a[16], b[30];
  liste_beginnen(LI_ERSETZEN, T("WELCHEN ERSETZEN?", "Welchen ersetzen?"));
  const int n = gesp_sortiert(ix, true);
  for (int k = 0; k < n; k++) {
    dauer_text(a, sizeof a, s_g[ix[k]].dauer);
    snprintf(b, sizeof b, T("TIMER %s", "Timer %s"), a);
    eintrag(b, A_ERSETZEN, ix[k]);
    snprintf(b, sizeof b, T("%dX GESTARTET", "%dx gestartet"), s_g[ix[k]].nutzung);
    info(b, T("SELTEN GENUTZTE OBEN", "selten genutzte oben"));
  }
  liste_fertig(0);
}
static void runden_zeigen(void) {             // neueste oben
  const Stopp *p = &s_s[s_akt];
  char a[16], b[40];
  int beste = -1;
  const int alt = p->rn > MAXR ? p->rn - MAXR + 1 : 1;
  for (int nr = alt; nr <= p->rn; nr++) if (beste < 0 || p->rd[(nr - 1) % MAXR] < p->rd[(beste - 1) % MAXR]) beste = nr;
  snprintf(b, sizeof b, T("RUNDEN STOPPUHR %d", "Runden Stoppuhr %d"), p->nr);
  liste_beginnen(LI_RUNDEN, b);
  for (int nr = p->rn; nr >= alt; nr--) {
    ms_text(a, sizeof a, p->rd[(nr - 1) % MAXR]);
    snprintf(b, sizeof b, T("RUNDE %d: %s", "Runde %d: %s"), nr, a);
    eintrag(b, A_RUNDE, nr);
    ms_text(a, sizeof a, p->rg[(nr - 1) % MAXR]);
    char c[30], e[30];
    snprintf(c, sizeof c, T("GESAMT %s", "gesamt %s"), a);
    if (nr == beste) snprintf(e, sizeof e, "%s", T("BESTE RUNDE", "beste Runde"));
    else e[0] = '\0';
    info(c, e);
  }
  liste_fertig(0);
}

static void menue_zeigen(int sel_aktion) {
  liste_beginnen(LI_MENUE, T("MENUE", "Menü"));
  if (s_von == M_LISTE) {
    if (s_start_akt == A_G) {
      eintrag(T("DAUER AENDERN >", "Dauer ändern >"), A_G_AENDERN, s_start_wert);
      eintrag(T("TIMER LOESCHEN", "Timer löschen"), A_G_LOESCHEN, s_start_wert);
    } else if (s_start_akt == A_T) eintrag(T("TIMER ENTFERNEN", "Timer entfernen"), A_T_ENTF, s_start_wert);
    else if (s_start_akt == A_S) eintrag(T("STOPPUHR ENTFERNEN", "Stoppuhr entfernen"), A_S_ENTF, s_start_wert);
    if (gesp_frei() >= 0) eintrag(T("NEUEN TIMER SPEICHERN >", "Neuen Timer speichern >"), A_G_NEU, 0);
  } else if (s_von == M_TIMER) {
    const Timer *t = &s_t[s_akt];
    eintrag(T("ZEIT ANPASSEN >", "Zeit anpassen >"), A_ANPASSEN, 0);
    if (t->z == Z_PAUSE || t->z == Z_UM || (t->z == Z_BEREIT && t->rest != t->basis))
      eintrag(T("ZURUECKSETZEN", "Zurücksetzen"), A_RESET, 0);
    if (gesp_index(t->basis) < 0) eintrag(T("TIMER SPEICHERN", "Timer speichern"), A_T_SPEICHERN, 0);
    eintrag(T("TIMER ENTFERNEN", "Timer entfernen"), A_T_ENTF, s_akt);
  } else if (s_von == M_STOPP) {
    const Stopp *p = &s_s[s_akt];
    if (p->z != Z_BEREIT && stopp_frei() >= 0) eintrag(T("NEUE STOPPUHR", "Neue Stoppuhr"), A_NEU_S, 0);
    if (stopp_anzahl() > 1) eintrag(T("ALLE STOPPUHREN >", "Alle Stoppuhren >"), A_ALLE_S, 0);
    if (p->rn) eintrag(T("RUNDEN ANZEIGEN >", "Runden anzeigen >"), A_RUNDEN, 0);
    if (p->z == Z_PAUSE) eintrag(T("ZURUECKSETZEN", "Zurücksetzen"), A_RESET, 0);
    eintrag(T("STOPPUHR ENTFERNEN", "Stoppuhr entfernen"), A_S_ENTF, s_akt);
  }
  eintrag(T("EINSTELLUNGEN >", "Einstellungen >"), A_EINST, 0);
  liste_fertig(liste_index(sel_aktion, -1));
}
static void einst_zeigen(int sel_aktion) {
  char b[40];
  liste_beginnen(LI_EINST, T("EINSTELLUNGEN", "Einstellungen"));
  eintrag(WATER ? "Ansicht: Water >" : T("ANSICHT: PHOSPHOR >", "Ansicht: Papier >"), A_ANSICHT, 0);
  if (!WATER)                                 // Water: eigene Farben, Invertiert gilt dort nicht
    eintrag(s_invers ? T("INVERTIERT: AN", "Invertiert: an") : T("INVERTIERT: AUS", "Invertiert: aus"), A_INVERS, 0);
  if (s_layout == L_PHOSPHOR)                 // Papier und Water haben ohnehin kein Grau
    eintrag(s_kontrast ? "HOHER KONTRAST: AN" : "HOHER KONTRAST: AUS", A_KONTRAST, 0);
  static const char *const TP[3] = { "AUS", "PIEPSER HOCH", "PIEPSER TIEFER" }, *const TK[3] = { "aus", "Piepser hoch", "Piepser tiefer" };
  snprintf(b, sizeof b, T("TON: %s >", "Ton: %s >"), T(TP[s_klang], TK[s_klang]));
  eintrag(b, A_TON, 0);
  if (s_klang) {                              // Lautstärke nur, wenn es einen Ton gibt
    static const char *const LP[3] = { "LEISE", "MITTEL", "LAUT" }, *const LK[3] = { "leise", "mittel", "laut" };
    snprintf(b, sizeof b, T("LAUTSTAERKE: %s >", "Lautstärke: %s >"), T(LP[s_laut], LK[s_laut]));
    eintrag(b, A_LAUT, 0);
  }
  eintrag(s_vib ? T("VIBRATION: AN", "Vibration: an") : T("VIBRATION: AUS", "Vibration: aus"), A_VIB, 0);
  if (W_KLINGEL[s_klingel] < 60) snprintf(b, sizeof b, T("KLINGELN: %d S >", "Klingeln: %d s >"), W_KLINGEL[s_klingel]);
  else snprintf(b, sizeof b, T("KLINGELN: %d MIN >", "Klingeln: %d min >"), W_KLINGEL[s_klingel] / 60);
  eintrag(b, A_KLINGEL, 0);
  if (W_NACH[s_nach]) snprintf(b, sizeof b, T("NACHKLINGELN: ALLE %d MIN >", "Nachklingeln: alle %d min >"), W_NACH[s_nach]);
  else snprintf(b, sizeof b, "%s", T("NACHKLINGELN: AUS >", "Nachklingeln: aus >"));
  eintrag(b, A_NACH, 0);
  eintrag(T("ALARM TESTEN", "Alarm testen"), A_TEST, 0);
  if (W_ZU[s_zu]) snprintf(b, sizeof b, T("NACH ABLAUF ZU: %d S >", "Nach Ablauf zu: %d s >"), W_ZU[s_zu]);
  else snprintf(b, sizeof b, "%s", T("NACH ABLAUF ZU: NIE >", "Nach Ablauf zu: nie >"));
  eintrag(b, A_ZU, 0);
  eintrag(s_ruhe ? T("OHNE TASTE ZU: 30 S", "Ohne Taste zu: 30 s") : T("OHNE TASTE ZU: NIE", "Ohne Taste zu: nie"), A_RUHE, 0);
  liste_fertig(liste_index(sel_aktion, -1));
}
static void ansicht_zeigen(void) {
  liste_beginnen(LI_ANSICHT, T("ANSICHT", "Ansicht"));
  eintrag(T("WATER", "Water"), A_SET_ANSICHT, L_WATER);
  info(T("STANDARD: SPRINGER,", "Standard: Springer,"), T("PUNKTRICHTER, PLANSCHBECKEN", "Punktrichter, Planschbecken"));
  eintrag(T("PAPIER", "Papier"), A_SET_ANSICHT, L_PAPIER);
  info(T("SYSTEMSCHRIFT FETT,", "Systemschrift fett,"), T("SCHWARZ AUF WEISS", "schwarz auf weiß"));
  eintrag(T("PHOSPHOR", "Phosphor"), A_SET_ANSICHT, L_PHOSPHOR);
  info(T("PIXELSCHRIFT,", "Pixelschrift,"), T("WEISS AUF SCHWARZ", "weiß auf Schwarz"));
  liste_fertig(WATER ? 0 : PAPIER ? 1 : 2);
}
static void ton_zeigen(void) {
  liste_beginnen(LI_TON, T("TON", "Ton"));
  eintrag(T("AUS", "aus"), A_SET_TON, 0);
  eintrag(T("PIEPSER HOCH", "Piepser hoch"), A_SET_TON, 1);
  eintrag(T("PIEPSER TIEFER", "Piepser tiefer"), A_SET_TON, 2);
  for (int k = 0; k < s_lanz; k++) {
    snprintf(s_linfo[k][0], sizeof s_linfo[0][0], "%s", T("WIE EINE SPORTUHR, BEI", "Wie eine Sportuhr. Bei der"));
    snprintf(s_linfo[k][1], sizeof s_linfo[0][1], "%s", T("DER WAHL EINMAL ZUR PROBE", "Wahl einmal zur Probe"));
  }
  s_linfo_an = true;
  liste_fertig(s_klang);
}
static void laut_zeigen(void) {
  liste_beginnen(LI_LAUT, T("LAUTSTAERKE", "Lautstärke"));
  eintrag(T("LEISE", "leise"), A_SET_LAUT, 0);
  eintrag(T("MITTEL", "mittel"), A_SET_LAUT, 1);
  eintrag(T("LAUT", "laut"), A_SET_LAUT, 2);
  info(T("BEI DER WAHL KLINGT", "Bei der Wahl klingt"), T("ES EINMAL ZUR PROBE", "es einmal zur Probe"));
  for (int k = 0; k < 2; k++) {
    snprintf(s_linfo[k][0], sizeof s_linfo[0][0], "%s", s_linfo[2][0]);
    snprintf(s_linfo[k][1], sizeof s_linfo[0][1], "%s", s_linfo[2][1]);
  }
  liste_fertig(s_laut);
}
static void klingel_zeigen(void) {
  liste_beginnen(LI_KLINGEL, T("KLINGELN", "Klingeln"));
  eintrag(T("30 SEKUNDEN", "30 Sekunden"), A_SET_KLINGEL, 0);
  eintrag(T("1 MINUTE", "1 Minute"), A_SET_KLINGEL, 1);
  eintrag(T("2 MINUTEN", "2 Minuten"), A_SET_KLINGEL, 2);
  liste_fertig(s_klingel);
}
static void zu_zeigen(void) {
  liste_beginnen(LI_ZU, T("NACH ABLAUF ZU", "Nach Ablauf zu"));
  eintrag(T("NIE", "nie"), A_SET_ZU, 0);
  eintrag(T("NACH 10 SEKUNDEN", "nach 10 Sekunden"), A_SET_ZU, 1);
  eintrag(T("NACH 30 SEKUNDEN", "nach 30 Sekunden"), A_SET_ZU, 2);
  eintrag(T("NACH 1 MINUTE", "nach 1 Minute"), A_SET_ZU, 3);
  for (int k = 0; k < s_lanz; k++) {
    snprintf(s_linfo[k][0], sizeof s_linfo[0][0], "%s", T("KLINGELN AUS, DANN KEINE", "Klingeln abgestellt, danach"));
    snprintf(s_linfo[k][1], sizeof s_linfo[0][1], "%s", T("TASTE: ZUM WATCHFACE", "keine Taste: zum Watchface"));
  }
  s_linfo_an = true;
  liste_fertig(s_zu);
}
static void nach_zeigen(void) {
  liste_beginnen(LI_NACH, T("NACHKLINGELN", "Nachklingeln"));
  eintrag(T("AUS", "aus"), A_SET_NACH, 0);
  eintrag(T("ALLE 2 MINUTEN", "alle 2 Minuten"), A_SET_NACH, 1);
  eintrag(T("ALLE 5 MINUTEN", "alle 5 Minuten"), A_SET_NACH, 2);
  eintrag(T("ALLE 10 MINUTEN", "alle 10 Minuten"), A_SET_NACH, 3);
  for (int k = 0; k < s_lanz; k++) {
    snprintf(s_linfo[k][0], sizeof s_linfo[0][0], "%s", T("NICHT ABGESTELLT: KURZ,", "Nicht abgestellt: kurz,"));
    snprintf(s_linfo[k][1], sizeof s_linfo[0][1], "%s", T("HOECHSTENS 1 STUNDE", "höchstens 1 Stunde lang"));
  }
  s_linfo_an = true;
  liste_fertig(s_nach);
}

// ---------- Abläufe ----------
static void frage_zeigen(int fr, int wert) { s_fr = fr; s_fr_wert = wert; s_wahl = 0; modus(M_FRAGE); }
static void timer_zeigen(int i) { s_akt = i; modus(M_TIMER); }
static void timer_neu(int32_t d) {            // aktiver Timer, bereit; Start mit Hoch
  const int i = timer_frei();
  if (i < 0) { start_zeigen(A_NEU_T, -1); hinweis(T("HOECHSTENS 5 TIMER", "Höchstens 5 Timer")); return; }
  s_t[i] = (Timer){ .basis = d, .gesamt = d, .rest = d, .z = Z_BEREIT };
  speichern();
  timer_zeigen(i);
}
static void eingabe_zeigen(int fluss, int32_t d) {
  s_fluss = fluss;
  s_h = d / 3600; s_m = d / 60 % 60; s_sek = d % 60;
  s_feld = 1;                                 // Minuten zuerst
  modus(M_EIN);
}
static void gesp_neu(int i, int32_t d) { s_g[i] = (Gesp){ .dauer = d }; speichern(); }
// Nach dem Speichern (oder Nein) je nach Ablauf weiter
static void fluss_weiter(void) {
  switch (s_fluss) {
    case FL_NEU: timer_neu(s_neu_dauer); break;
    case FL_T_SPEICHERN: timer_zeigen(s_akt); break;
    default: start_zeigen(A_G, gesp_index(s_neu_dauer)); break;
  }
}
static void speichern_oder_ersetzen(void) {
  const int i = gesp_frei();
  if (i >= 0) { gesp_neu(i, s_neu_dauer); fluss_weiter(); if (s_fluss != FL_NEU) hinweis(T("GESPEICHERT", "Gespeichert")); }
  else ersetzen_zeigen();
}
static void eingabe_fertig(void) {            // Select in den Sekunden
  const int32_t d = s_h * 3600 + s_m * 60 + s_sek;
  if (d <= 0) { hinweis(T("DAUER IST NOCH 0", "Dauer ist noch 0")); return; }
  s_neu_dauer = d;
  s_letzte = d;                               // letzte Eingabe von Hand: Vorschlag der nächsten (alle Abläufe, 0.7.0)
  persist_write_int(PK_LETZTE, d);
  switch (s_fluss) {
    case FL_NEU:
      if (gesp_index(d) >= 0) timer_neu(d);
      else frage_zeigen(FR_SPEICHERN, 0);
      break;
    case FL_G_NEU:
      if (gesp_index(d) >= 0) { start_zeigen(A_G, gesp_index(d)); hinweis(T("SCHON GESPEICHERT", "Schon gespeichert")); }
      else speichern_oder_ersetzen();
      break;
    case FL_G_AENDERN: {
      const int j = gesp_index(d);
      if (j >= 0 && j != s_g_edit) {          // gibt es schon: zusammenlegen
        s_g[j].nutzung += s_g[s_g_edit].nutzung;
        s_g[s_g_edit].dauer = 0;
        hinweis(T("SCHON GESPEICHERT", "Schon gespeichert"));
      } else s_g[s_g_edit].dauer = d;
      speichern();
      start_zeigen(A_G, gesp_index(d));
      break;
    }
  }
}
static void eingabe_abbrechen(void) {         // Zurück in den Stunden
  if (s_fluss == FL_G_AENDERN) start_zeigen(A_G, s_g_edit);
  else if (s_fluss == FL_G_NEU) start_zeigen(A_NEU_T, -1);
  else start_zeigen(A_NEU_T, -1);
}
static void timer_reset(Timer *t) {
  t->z = Z_BEREIT; t->rest = t->gesamt = t->basis; t->quitt = 0; t->nach = 0;
}
static void stopp_reset(Stopp *p) { p->z = Z_BEREIT; p->akku = 0; p->rn = 0; }
static void stopp_neu(void) {
  const int i = stopp_frei();
  if (i < 0) { hinweis(T("HOECHSTENS 5 STOPPUHREN", "Höchstens 5 Stoppuhren")); return; }
  int nr = 1;
  for (bool belegt = true; belegt && nr <= MAXS; ) {
    belegt = false;
    for (int k = 0; k < MAXS; k++) if (s_s[k].z != Z_FREI && s_s[k].nr == nr) { belegt = true; nr++; break; }
  }
  memset(&s_s[i], 0, sizeof s_s[i]);
  s_s[i].nr = nr;
  s_s[i].z = Z_BEREIT;
  speichern();
  s_akt = i;
  modus(M_STOPP);
}
static void menue_oeffnen(void) {             // Select lang (Timer und Stoppuhr: auch kurz)
  if (s_modus == M_LISTE && s_lart == LI_WAHL) {   // Auswahl: nur Einstellungen
    s_start_akt = s_lakt[s_lsel]; s_start_wert = 0;
    s_von = M_LISTE;
    s_einst_aus_menue = false;
    einst_zeigen(-1);
    return;
  }
  if (s_modus == M_LISTE && s_lart == LI_START) {
    s_start_akt = s_lakt[s_lsel]; s_start_wert = s_lwert[s_lsel];
    s_von = M_LISTE;
    s_einst_aus_menue = true;
    menue_zeigen(-1);
    return;
  }
  if (s_modus == M_LISTE || s_modus == M_FRAGE || s_modus == M_ANPASSEN) return;
  s_von = s_modus;
  s_einst_aus_menue = s_modus != M_EIN;
  if (s_einst_aus_menue) menue_zeigen(-1);
  else einst_zeigen(-1);                      // in der Eingabe gibt es nur die Einstellungen
}
static void menue_verlassen(void) {
  if (s_von == M_LISTE && (s_start_akt == A_BEREICH_T || s_start_akt == A_BEREICH_S)) wahl_zeigen(s_start_akt);
  else if (s_von == M_LISTE) start_zeigen(s_start_akt, s_start_wert);
  else modus(s_von);
}
static void beenden(void) {
  exit_reason_set(APP_EXIT_ACTION_PERFORMED_SUCCESSFULLY);   // zurück aufs Watchface
  window_stack_pop(true);
}

static void liste_wahl(void) {
  if (!s_lanz) return;
  const int a = s_lakt[s_lsel], w = s_lwert[s_lsel];
  switch (a) {
    case A_BEREICH_T: start_zeigen(A_NEU_T, -1); break;
    case A_BEREICH_S: stopp_bereich(); break;
    case A_ALLE_S: stopps_zeigen(); break;
    case A_NEU_T:
      if (timer_anzahl() >= MAXT) hinweis(T("HOECHSTENS 5 TIMER", "Höchstens 5 Timer"));
      else eingabe_zeigen(FL_NEU, s_letzte);
      break;
    case A_NEU_S: stopp_neu(); break;
    case A_T: timer_zeigen(w); break;
    case A_S: s_akt = w; modus(M_STOPP); break;
    case A_G:
      if (timer_anzahl() >= MAXT) hinweis(T("HOECHSTENS 5 TIMER", "Höchstens 5 Timer"));
      else timer_neu(s_g[w].dauer);
      break;
    case A_G_AENDERN: s_g_edit = w; eingabe_zeigen(FL_G_AENDERN, s_g[w].dauer); break;
    case A_G_LOESCHEN: frage_zeigen(FR_G_LOESCHEN, w); break;
    case A_G_NEU: eingabe_zeigen(FL_G_NEU, s_letzte); break;
    case A_T_ENTF: frage_zeigen(FR_T_ENTF, w); break;
    case A_S_ENTF: frage_zeigen(FR_S_ENTF, w); break;
    case A_ANPASSEN: modus(M_ANPASSEN); break;
    case A_RESET:
      if (s_von == M_TIMER) timer_reset(&s_t[s_akt]); else stopp_reset(&s_s[s_akt]);
      speichern(); wecker_stellen();
      modus(s_von);
      break;
    case A_T_SPEICHERN: s_fluss = FL_T_SPEICHERN; s_neu_dauer = s_t[s_akt].basis; speichern_oder_ersetzen(); break;
    case A_RUNDEN: runden_zeigen(); break;
    case A_EINST: einst_zeigen(-1); break;
    case A_ANSICHT: ansicht_zeigen(); break;
    case A_TON: ton_zeigen(); break;
    case A_LAUT: laut_zeigen(); break;
    case A_VIB: s_vib = !s_vib; einst_speichern(); einst_zeigen(A_VIB); break;
    case A_KLINGEL: klingel_zeigen(); break;
    case A_NACH: nach_zeigen(); break;
    case A_TEST: signal(true); break;
    case A_RUHE: s_ruhe = !s_ruhe; einst_speichern(); einst_zeigen(A_RUHE); break;
    case A_ZU: zu_zeigen(); break;
    case A_SET_ZU: s_zu = w; einst_speichern(); einst_zeigen(A_ZU); break;
    case A_SET_ANSICHT:
      s_layout = w; schriften_setzen(); farben_setzen();
      einst_speichern(); einst_zeigen(A_ANSICHT); break;
    case A_INVERS: s_invers = !s_invers; farben_setzen(); einst_speichern(); einst_zeigen(A_INVERS); break;
    case A_KONTRAST: s_kontrast = !s_kontrast; farben_setzen(); einst_speichern(); einst_zeigen(A_KONTRAST); break;
    case A_SET_TON: s_klang = w; einst_speichern(); ton(false); einst_zeigen(A_TON); break;
    case A_SET_LAUT: s_laut = w; einst_speichern(); ton(false); einst_zeigen(A_LAUT); break;
    case A_SET_KLINGEL: s_klingel = w; einst_speichern(); einst_zeigen(A_KLINGEL); break;
    case A_SET_NACH: s_nach = w; einst_speichern(); wecker_stellen(); einst_zeigen(A_NACH); break;
    case A_ERSETZEN: frage_zeigen(FR_ERSETZEN, w); break;
    default: break;                           // A_RUNDE: nur ansehen
  }
}
static void liste_zurueck(void) {
  switch (s_lart) {
    case LI_WAHL: beenden(); break;
    case LI_START: wahl_zeigen(A_BEREICH_T); break;
    case LI_STOPP: s_von = M_STOPP; menue_zeigen(A_ALLE_S); break;
    case LI_MENUE: menue_verlassen(); break;
    case LI_EINST: if (s_einst_aus_menue) menue_zeigen(A_EINST); else menue_verlassen(); break;
    case LI_ANSICHT: einst_zeigen(A_ANSICHT); break;
    case LI_TON: einst_zeigen(A_TON); break;
    case LI_LAUT: einst_zeigen(A_LAUT); break;
    case LI_KLINGEL: einst_zeigen(A_KLINGEL); break;
    case LI_NACH: einst_zeigen(A_NACH); break;
    case LI_ZU: einst_zeigen(A_ZU); break;
    case LI_RUNDEN: menue_zeigen(A_RUNDEN); break;
    case LI_ERSETZEN:                         // nichts ersetzen
      if (s_fluss == FL_NEU) frage_zeigen(FR_SPEICHERN, 0);
      else if (s_fluss == FL_T_SPEICHERN) menue_zeigen(A_T_SPEICHERN);
      else { s_feld = 2; modus(M_EIN); }
      break;
  }
}
static void frage_ausfuehren(void) {
  const bool tun = s_wahl == 1;
  switch (s_fr) {
    case FR_SPEICHERN: if (tun) speichern_oder_ersetzen(); else fluss_weiter(); break;
    case FR_ERSETZEN:
      if (!tun) { ersetzen_zeigen(); break; }
      gesp_neu(s_fr_wert, s_neu_dauer);
      fluss_weiter();
      if (s_fluss != FL_NEU) hinweis(T("GESPEICHERT", "Gespeichert"));
      break;
    case FR_G_LOESCHEN:
      if (!tun) { start_zeigen(A_G, s_fr_wert); break; }
      s_g[s_fr_wert].dauer = 0;
      speichern();
      start_zeigen(A_NEU_T, -1);
      break;
    case FR_T_ENTF:
    case FR_S_ENTF:
      if (!tun) { menue_zeigen(s_fr == FR_T_ENTF ? A_T_ENTF : A_S_ENTF); break; }
      if (s_fr == FR_T_ENTF) s_t[s_fr_wert].z = Z_FREI; else s_s[s_fr_wert].z = Z_FREI;
      speichern();
      wecker_stellen();
      if (s_fr == FR_T_ENTF) start_zeigen(A_NEU_T, -1); else wahl_zeigen(A_BEREICH_S);
      break;
  }
}
static void frage_abbrechen(void) {
  switch (s_fr) {
    case FR_SPEICHERN: s_feld = 2; modus(M_EIN); break;   // zurück in die Eingabe
    case FR_ERSETZEN: ersetzen_zeigen(); break;
    default: s_wahl = 0; frage_ausfuehren(); break;      // wie der sichere Eintrag
  }
}

// ---------- Timer und Stoppuhr bedienen ----------
static void zaehlen(int32_t d) {              // gespeicherten Timer dieser Dauer: einmal mehr gestartet
  const int g = gesp_index(d);
  if (g >= 0 && s_g[g].nutzung < 60000) s_g[g].nutzung++;
}
static void timer_hoch(void) {                // Start / Stopp / weiter
  Timer *t = &s_t[s_akt];
  const time_t now = time(NULL);
  switch (t->z) {
    case Z_BEREIT: if (t->rest == t->basis) zaehlen(t->basis); t->ende = now + t->rest; t->z = Z_LAEUFT; break;
    case Z_LAEUFT: t->rest = t->ende - now > 1 ? t->ende - now : 1; t->z = Z_PAUSE; break;
    case Z_PAUSE: t->ende = now + t->rest; t->z = Z_LAEUFT; break;
    case Z_UM: return;                        // nur hinter dem Kasten; die Taste stellt erst ab
  }
  speichern();
  wecker_stellen();
  neu_zeichnen();
}
static void timer_runter(void) {              // Reset, nur angehalten bzw. abgelaufen
  Timer *t = &s_t[s_akt];
  if (t->z == Z_LAEUFT) { hinweis(T("ERST ANHALTEN: OBEN", "Erst anhalten: oben")); return; }
  if (t->z == Z_UM) return;
  if (t->z == Z_BEREIT && t->rest == t->basis && t->gesamt == t->basis) return;
  timer_reset(t);
  speichern();
  wecker_stellen();
  neu_zeichnen();
}
static void stopp_hoch(void) {
  Stopp *p = &s_s[s_akt];
  if (p->z == Z_LAEUFT) { p->akku = stopp_ms(p); p->z = Z_PAUSE; }
  else {
    time_t s;
    uint16_t ms;
    time_ms(&s, &ms);
    if (p->z == Z_BEREIT) p->erst = s;
    p->start_s = s; p->start_ms = ms; p->z = Z_LAEUFT;
  }
  speichern();
  schnell_neu();
  neu_zeichnen();
}
static void stopp_runter(void) {              // läuft: Runde; angehalten: Reset
  Stopp *p = &s_s[s_akt];
  if (p->z == Z_PAUSE) stopp_reset(p);
  else if (p->z == Z_LAEUFT) {
    const int32_t g = stopp_ms(p), i = p->rn % MAXR;
    p->rd[i] = g - runde_vor(p);
    p->rg[i] = g;
    if (p->rn < 60000) p->rn++;
  } else return;
  speichern();
  neu_zeichnen();
}
static void anpassen(int d) {                 // ±1 min am gezeigten Timer
  Timer *t = &s_t[s_akt];
  const time_t now = time(NULL);
  const int32_t rest = rest_von(t, now), neu = rest + 60 * d;
  if (neu < MIN_REST) { hinweis(T("WENIGER GEHT NICHT", "Weniger geht nicht")); return; }
  if (neu > MAX_DAUER) return;
  if (t->z == Z_LAEUFT) t->ende += 60 * d;
  else if (t->z == Z_UM) { t->z = Z_LAEUFT; t->ende = now + neu; t->quitt = 0; t->nach = 0; }
  else t->rest = neu;
  t->gesamt += 60 * d;
  if (t->gesamt < neu) t->gesamt = neu;
  speichern();
  wecker_stellen();
  neu_zeichnen();
}

// ---------- Tasten ----------
static int schritt5(int v, int d) {          // auf das nächste Vielfache von 5 in Richtung d, 0–59
  return ((d > 0 ? v / 5 * 5 + 5 : (v + 4) / 5 * 5 - 5) + 60) % 60;
}
static void hoch_runter(int d, bool wdh) {
  s_wdh_n = wdh ? s_wdh_n + 1 : 0;
  switch (s_modus) {
    case M_EIN: {
      if (s_feld == 0) s_h = (s_h + d + 24) % 24;
      else if (s_feld == 1) s_m = s_wdh_n > 8 ? schritt5(s_m, d) : (s_m + d + 60) % 60;   // lange gehalten: 5er-Schritte
      else s_sek = schritt5(s_sek, d);
      blink_neu();
      break;
    }
    case M_FRAGE: if (wdh) return; s_wahl = !s_wahl; break;
    case M_ANPASSEN: if (wdh && s_wdh_n % 3) return; anpassen(d); return;
    case M_LISTE:
      s_lsel += d > 0 ? -1 : 1;              // Hoch = Zeile darüber
      if (s_lsel < 0) s_lsel = wdh ? 0 : s_lanz - 1;
      if (s_lsel >= s_lanz) s_lsel = wdh ? s_lanz - 1 : 0;
      if (s_lsel < s_loben) s_loben = s_lsel;
      if (s_lsel >= s_loben + lz()) s_loben = s_lsel - lz() + 1;
      lauf_neu();
      break;
    case M_TIMER: if (!wdh) { if (d > 0) timer_hoch(); else timer_runter(); } return;
    case M_STOPP: if (!wdh) { if (d > 0) stopp_hoch(); else stopp_runter(); } return;
    default: return;
  }
  neu_zeichnen();
}
static bool vorweg(ClickRecognizerRef r) {    // jede Taste: Ruhe neu; stellt ein Klingeln ab und ist dann verbraucht
  ruhe_neu();
  s_taste = time(NULL);
  if (quittieren()) {                         // abgestellt: ohne weitere Taste nach W_ZU zum Watchface
    zu_aus();
    if (W_ZU[s_zu]) s_zu_timer = app_timer_register(W_ZU[s_zu] * 1000, zu_cb, NULL);
    return true;
  }
  zu_aus();                                   // jede weitere Taste: App bleibt
  return false;
}
static void hoch(ClickRecognizerRef r, void *c) { if (!vorweg(r)) hoch_runter(1, click_recognizer_is_repeating(r)); anim_neu(); }
static void runter(ClickRecognizerRef r, void *c) { if (!vorweg(r)) hoch_runter(-1, click_recognizer_is_repeating(r)); anim_neu(); }
static void auswahl(ClickRecognizerRef r, void *c) {
  if (vorweg(r)) return;
  switch (s_modus) {
    case M_EIN:
      if (s_feld < 2) { s_feld++; blink_neu(); neu_zeichnen(); }
      else eingabe_fertig();
      break;
    case M_TIMER: case M_STOPP: menue_oeffnen(); break;
    case M_ANPASSEN: modus(M_TIMER); break;
    case M_LISTE: liste_wahl(); break;
    case M_FRAGE: frage_ausfuehren(); break;
  }
  anim_neu();
}
static void auswahl_lang(ClickRecognizerRef r, void *c) { if (!vorweg(r)) menue_oeffnen(); anim_neu(); }
static void zurueck(ClickRecognizerRef r, void *c) {
  if (vorweg(r)) return;
  switch (s_modus) {
    case M_EIN:
      if (s_feld > 0) { s_feld--; blink_neu(); neu_zeichnen(); }
      else eingabe_abbrechen();
      break;
    case M_TIMER: start_zeigen(A_T, s_akt); break;
    case M_STOPP: wahl_zeigen(A_BEREICH_S); break;
    case M_ANPASSEN: modus(M_TIMER); break;
    case M_LISTE: liste_zurueck(); break;
    case M_FRAGE: frage_abbrechen(); break;
  }
  anim_neu();
}
static void tasten(void *ctx) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, hoch);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, runter);
  window_single_click_subscribe(BUTTON_ID_SELECT, auswahl);
  window_long_click_subscribe(BUTTON_ID_SELECT, 600, auswahl_lang, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, zurueck);
}

// ---------- Takt und Wecker ----------
static void takt(struct tm *t, TimeUnits u) {
  pruefen();
  if (s_modus == M_LISTE && s_lart <= LI_STOPP) start_texte();
  if (s_modus != M_EIN) neu_zeichnen();       // Eingabe: Blinktakt zeichnet
  anim_neu();                                 // Water: Finale, Eintauchen
}
static void geweckt(WakeupId id, int32_t cookie) { pruefen(); }

// ---------- Start ----------
static void fenster_laden(Window *w) {
  Layer *root = window_get_root_layer(w);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, zeichnen);
  layer_add_child(root, s_layer);
}
static void fenster_weg(Window *w) { layer_destroy(s_layer); s_layer = NULL; }
static int lesen(uint32_t k, int sonst) { return persist_exists(k) ? persist_read_int(k) : sonst; }
static int lesen_bis(uint32_t k, int sonst, int max) { const int v = lesen(k, sonst); return v >= 0 && v <= max ? v : sonst; }

static void init(void) {
  if (lesen(PK_FORMAT, 0) != FORMAT) {        // fremdes oder altes Format: alles weg, sonst falsche Einstellungen
    for (uint32_t k = 1; k < 260; k++) persist_delete(k);
    persist_write_int(PK_FORMAT, FORMAT);
  }
  s_layout = lesen(PK_LAYOUT, L_WATER);
  if (s_layout == 1) s_layout = L_PAPIER;    // Klar (bis 0.6) → Papier
  else if (s_layout != L_PHOSPHOR && s_layout != L_PAPIER) s_layout = L_WATER;
  s_invers = lesen(PK_INVERS, 0) ? true : false;
  s_kontrast = lesen(PK_KONTRAST, 0) ? true : false;
  schriften_setzen();
  farben_setzen();
  s_klang = lesen_bis(PK_KLANG, 0, 2);
  s_laut = lesen_bis(PK_LAUT, 2, 2);
  s_vib = lesen(PK_VIB, 1) ? true : false;
  s_klingel = lesen_bis(PK_KLINGEL, 1, ARRAY_LENGTH(W_KLINGEL) - 1);
  s_nach = lesen_bis(PK_NACH, 2, ARRAY_LENGTH(W_NACH) - 1);
  s_ruhe = lesen_bis(PK_RUHE2, 1, 1);         // PK_RUHE (bis 0.7, Standard aus) bleibt ungenutzt
  s_zu = lesen_bis(PK_ZU, 1, ARRAY_LENGTH(W_ZU) - 1);
  s_letzte = lesen(PK_LETZTE, 300);
  if (s_letzte <= 0 || s_letzte > MAX_DAUER) s_letzte = 300;
  if (lesen(PK_FORMAT, 0) == FORMAT) {
    if (persist_read_data(PK_TIMER, s_t, sizeof s_t) != (int)sizeof s_t) memset(s_t, 0, sizeof s_t);
    for (int i = 0; i < MAXS; i++)
      if (persist_read_data(PK_STOPP + i, &s_s[i], sizeof s_s[i]) != (int)sizeof s_s[i]) memset(&s_s[i], 0, sizeof s_s[i]);
    if (persist_read_data(PK_GESP, s_g, sizeof s_g) != (int)sizeof s_g) memset(s_g, 0, sizeof s_g);
  }
  for (int i = 0; i < MAXT; i++) {            // Plausibilität; abgestellte abgelaufene (0.7.0) wieder bereit
    if (s_t[i].z > Z_UM || s_t[i].basis <= 0 || s_t[i].basis > MAX_DAUER || s_t[i].gesamt <= 0) s_t[i].z = Z_FREI;
    else if (s_t[i].z == Z_UM && s_t[i].quitt) timer_reset(&s_t[i]);
  }
  for (int i = 0; i < MAXS; i++) if (s_s[i].z > Z_PAUSE || s_s[i].nr < 1 || s_s[i].nr > MAXS) s_s[i].z = Z_FREI;
  for (int i = 0; i < MAXG; i++) if (s_g[i].dauer < 0 || s_g[i].dauer > MAX_DAUER) s_g[i].dauer = 0;

  s_win = window_create();
  window_set_background_color(s_win, C_HG);
  window_set_click_config_provider(s_win, tasten);
  window_set_window_handlers(s_win, (WindowHandlers){ .load = fenster_laden, .unload = fenster_weg });
  window_stack_push(s_win, true);

  // Erste Ansicht: die letzte, wenn dort noch etwas läuft, sonst die Startliste; ein abgelaufener Timer holt sich
  // danach selbst nach vorn (pruefen)
  s_taste = time(NULL);                       // Öffnen zählt wie eine Taste (Water: 30 s flüssig)
  s_letzte_ansicht = lesen(PK_ANSICHT, -1);
  if (s_letzte_ansicht >= 32 && s_letzte_ansicht < 32 + MAXS) s_stopp_zuletzt = s_letzte_ansicht - 32;
  const int la = s_letzte_ansicht / 16, li = s_letzte_ansicht % 16;
  if (la == 1 && li < MAXT && s_t[li].z >= Z_LAEUFT) { s_akt = li; modus(M_TIMER); }
  else if (la == 2 && li < MAXS && s_s[li].z >= Z_LAEUFT) { s_akt = li; modus(M_STOPP); }
  else wahl_zeigen(la == 2 ? A_BEREICH_S : A_BEREICH_T);
  pruefen();                                  // abgelaufene melden (bei Wecker: klingeln)
  wecker_stellen();
  ruhe_neu();
  tick_timer_service_subscribe(SECOND_UNIT, takt);
  wakeup_service_subscribe(geweckt);
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  if (s_blink_timer) app_timer_cancel(s_blink_timer);
  if (s_ruhe_timer) app_timer_cancel(s_ruhe_timer);
  zu_aus();
  if (s_lauf_timer) app_timer_cancel(s_lauf_timer);
  if (s_schnell_timer) app_timer_cancel(s_schnell_timer);
  if (s_hinweis_timer) app_timer_cancel(s_hinweis_timer);
  if (s_anim_timer) app_timer_cancel(s_anim_timer);
  signal_aus();
  speichern();
  wecker_stellen();
  window_destroy(s_win);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
