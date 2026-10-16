// Automatisch erzeugt von tools/fallschirm_bilder.py — nicht von Hand ändern.
#pragma once
#include <pebble.h>

enum { FB_STEH, FB_HOCKE, FB_FLUG, FB_SEITE, FB_HAENG, FB_WINK, FB_ZIEH, FB_UHR, FB_KLEIN, FB_FLUGZEUG, FB_GESICHT, FB_VOGEL_A, FB_VOGEL_B, FB_TAUBE, FB_ENTE, FB_JUMBO, FB_N };
static const uint32_t FB_RES[FB_N] = { RESOURCE_ID_FS_STEH, RESOURCE_ID_FS_HOCKE, RESOURCE_ID_FS_FLUG, RESOURCE_ID_FS_SEITE, RESOURCE_ID_FS_HAENG, RESOURCE_ID_FS_WINK, RESOURCE_ID_FS_ZIEH, RESOURCE_ID_FS_UHR, RESOURCE_ID_FS_KLEIN, RESOURCE_ID_FS_FLUGZEUG, RESOURCE_ID_FS_GESICHT, RESOURCE_ID_FS_VOGEL_A, RESOURCE_ID_FS_VOGEL_B, RESOURCE_ID_FS_TAUBE, RESOURCE_ID_FS_ENTE, RESOURCE_ID_FS_JUMBO };
// Anker im Bild (x, y): dorthin zeigt die Position beim Zeichnen; Größe nur zur Info
static const int16_t FB_ANKER[FB_N][2] = {
  { 32, 48 },   // STEH 52 x 75
  { 28, 47 },   // HOCKE 52 x 69
  { 17, 69 },   // FLUG 40 x 100
  { 18, 36 },   // SEITE 51 x 113
  { 34, 64 },   // HAENG 55 x 92
  { 34, 63 },   // WINK 55 x 91
  { 17, 69 },   // ZIEH 40 x 100
  { 33, 55 },   // UHR 67 x 88
  { 7, 12 },   // KLEIN 16 x 38
  { 0, -104 },   // FLUGZEUG 200 x 120
  { 33, 84 },   // GESICHT 66 x 133
  { 15, 13 },   // VOGEL_A 32 x 20
  { 15, 6 },   // VOGEL_B 32 x 13
  { 22, 15 },   // TAUBE 41 x 27
  { 14, 12 },   // ENTE 28 x 18
  { 3, 9 },   // JUMBO 42 x 20
};
