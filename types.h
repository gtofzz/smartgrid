#ifndef TYPES_H
#define TYPES_H
#include <time.h>
#include <stdbool.h>

typedef struct {
    int ID;
    int c1;      // carga 1 (0/1)
    int c2;      // carga 2 (0/1)
    int solar;   // 0/1
    int active;  // 0/1 (sempre c1||c2 ao publicar)
    int shed;    // 0,1,2
    int noOffer; // 0/1
    int ts;      // epoch do envio
} HouseState;

typedef struct {
    int H;       // casas com active=1
    int G;       // geradores ON
    int Loff;    // somatório de shed
    int Cap;     // 4 + G + floor(Loff/4)
    int Gap;     // H - Cap
    int epoch;   // epoch atual
    int targetOff;
} GridVars;

typedef struct {
    int epoch;
    int targetOff;
    time_t seen_at;
} SheddingCmd;

typedef enum {
    CMD_TOGGLE_C1 = 1,
    CMD_TOGGLE_C2 = 2,
    CMD_TOGGLE_SOLAR = 3,
    CMD_SET_ID = 4,
    CMD_QUIT = 9,
    CMD_INTERNAL_SHED = 100,
    CMD_INTERNAL_RESTORE = 101
} CommandType;

typedef struct { CommandType type; int arg; } Command;

#endif
