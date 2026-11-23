
#ifndef HASEE_FRAME_PACKET_H
#define HASEE_FRAME_PACKET_H

#include <stdint.h>

#define HASEE_MAGIC 0x48415345u
#define HASEE_MAX_OBJS 16
#define HASEE_NUM_FRAMES 100

#define HASEE_CLS_UNKNOWN    0
#define HASEE_CLS_CAR        1
#define HASEE_CLS_PEDESTRIAN 2
#define HASEE_CLS_CONE       3

#define HASEE_HEALTH_OK  0
#define HASEE_HEALTH_OOD 1

typedef struct {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint8_t cls;
    uint8_t reserved[3];
    float conf;
} hasee_obj_t;

typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint32_t frame_id;

    uint16_t img_w;
    uint16_t img_h;
    uint16_t num_objs;
    uint16_t reserved0;

    float uncertainty;
    uint32_t latency_ms;
    uint32_t health;

    hasee_obj_t objs[HASEE_MAX_OBJS];
} hasee_frame_packet_t;

#endif