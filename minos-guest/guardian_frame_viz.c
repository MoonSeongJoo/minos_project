#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "hasee_frame_packet.h"

#define GUARDIAN_PORT 7777
#define OUT_DIR "/demo"

#define SVG_W 1280
#define SVG_H 720

typedef enum {
    STATE_NORMAL = 0,
    STATE_PROBATION = 1,
    STATE_SEVERED = 2
} guardian_state_t;

static const char *cls_name(uint8_t c)
{
    switch (c) {
    case HASEE_CLS_CAR: return "car";
    case HASEE_CLS_PEDESTRIAN: return "pedestrian";
    case HASEE_CLS_CONE: return "cone";
    default: return "unknown";
    }
}

static const char *health_name(uint32_t h)
{
    return h == HASEE_HEALTH_OK ? "OK" : "OOD";
}

static const char *state_name(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return "NORMAL";
    case STATE_PROBATION: return "PROBATION";
    case STATE_SEVERED: return "SEVERED";
    default: return "UNKNOWN";
    }
}

static const char *safety_state_title(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return "AI OUTPUT ACCEPTED";
    case STATE_PROBATION: return "AI OUTPUT RESTRICTED";
    case STATE_SEVERED: return "VM2 OUTPUT PATH CUT";
    default: return "UNKNOWN";
    }
}

static const char *active_path_text(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return "VM2 AI DOMAIN";
    case STATE_PROBATION: return "VM2 AI DOMAIN - RESTRICTED";
    case STATE_SEVERED: return "VM1 FALLBACK ONLY";
    default: return "UNKNOWN";
    }
}

static const char *vm2_gate_text(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return "OPEN";
    case STATE_PROBATION: return "RESTRICTED";
    case STATE_SEVERED: return "CLOSED";
    default: return "UNKNOWN";
    }
}

static const char *fallback_text(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return "STANDBY";
    case STATE_PROBATION: return "PRE-ARMED";
    case STATE_SEVERED: return "ACTIVE";
    default: return "UNKNOWN";
    }
}

static int ai_authority_pct(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return 100;
    case STATE_PROBATION: return 50;
    case STATE_SEVERED: return 0;
    default: return 0;
    }
}

static int fallback_authority_pct(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return 0;
    case STATE_PROBATION: return 50;
    case STATE_SEVERED: return 100;
    default: return 0;
    }
}

static const char *state_color(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return "#16a34a";
    case STATE_PROBATION: return "#eab308";
    case STATE_SEVERED: return "#dc2626";
    default: return "#64748b";
    }
}

static const char *state_dark_color(guardian_state_t s)
{
    switch (s) {
    case STATE_NORMAL: return "#166534";
    case STATE_PROBATION: return "#854d0e";
    case STATE_SEVERED: return "#991b1b";
    default: return "#334155";
    }
}

static const char *box_color(uint8_t c)
{
    switch (c) {
    case HASEE_CLS_CAR: return "#00ff66";
    case HASEE_CLS_PEDESTRIAN: return "#ffcc00";
    case HASEE_CLS_CONE: return "#ff6600";
    default: return "#ffffff";
    }
}

static void build_risk_reason(const hasee_frame_packet_t *pkt,
                              char *buf,
                              size_t len)
{
    int first = 1;

    if (len == 0)
        return;

    buf[0] = '\0';

    if (pkt->uncertainty >= 0.80f) {
        strncat(buf, "High uncertainty", len - strlen(buf) - 1);
        first = 0;
    }

    if (pkt->latency_ms >= 40) {
        if (!first)
            strncat(buf, " + ", len - strlen(buf) - 1);
        strncat(buf, "Latency violation", len - strlen(buf) - 1);
        first = 0;
    }

    if (pkt->health != HASEE_HEALTH_OK) {
        if (!first)
            strncat(buf, " + ", len - strlen(buf) - 1);
        strncat(buf, "AI health OOD", len - strlen(buf) - 1);
        first = 0;
    }

    if (first)
        strncat(buf, "No risk detected", len - strlen(buf) - 1);
}

static void draw_authority_bar(FILE *f,
                               int x,
                               int y,
                               int w,
                               const char *label,
                               int pct,
                               const char *fill)
{
    int fill_w = (w * pct) / 100;

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#cbd5e1\" font-size=\"15\" "
        "font-family=\"Arial\" font-weight=\"bold\">%s</text>\n",
        x, y, label);

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#e5e7eb\" font-size=\"15\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"end\">%d%%</text>\n",
        x + w, y, pct);

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"14\" rx=\"7\" "
        "fill=\"#334155\"/>\n",
        x, y + 10, w);

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"14\" rx=\"7\" "
        "fill=\"%s\"/>\n",
        x, y + 10, fill_w, fill);
}

static void draw_runtime_flow(FILE *f, guardian_state_t state)
{
    int x = 30;
    int y = 545;
    int w = 1210;
    int h = 145;
    const char *vm2_line_color;
    const char *gate_fill;

    vm2_line_color = (state == STATE_SEVERED) ? "#ef4444" :
                     (state == STATE_PROBATION) ? "#eab308" : "#22c55e";
    gate_fill = (state == STATE_SEVERED) ? "#991b1b" :
                (state == STATE_PROBATION) ? "#854d0e" : "#166534";

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"16\" "
        "fill=\"#111827\" stroke=\"#334155\" stroke-width=\"2\"/>\n",
        x, y, w, h);

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#e5e7eb\" font-size=\"20\" "
        "font-family=\"Arial\" font-weight=\"bold\">Runtime Safety Path</text>\n",
        x + 24, y + 32);

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#94a3b8\" font-size=\"13\" "
        "font-family=\"Arial\">AI output is accepted only while runtime health is safe</text>\n",
        x + 24, y + 54);

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"170\" height=\"42\" rx=\"9\" "
        "fill=\"#1e293b\" stroke=\"#94a3b8\" stroke-width=\"1.5\"/>\n",
        x + 24, y + 76);
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#e5e7eb\" font-size=\"16\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">VM2 AI DOMAIN</text>\n",
        x + 109, y + 102);

    fprintf(f,
        "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
        "stroke=\"%s\" stroke-width=\"7\" stroke-linecap=\"round\"/>\n",
        x + 200, y + 97, x + 390, y + 97, vm2_line_color);

    if (state == STATE_SEVERED) {
        fprintf(f,
            "<circle cx=\"%d\" cy=\"%d\" r=\"21\" fill=\"#dc2626\" stroke=\"#fee2e2\" stroke-width=\"2\"/>\n",
            x + 295, y + 97);
        fprintf(f,
            "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"28\" "
            "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">X</text>\n",
            x + 295, y + 107);
    }

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"220\" height=\"42\" rx=\"9\" "
        "fill=\"%s\" stroke=\"#e5e7eb\" stroke-width=\"1.5\"/>\n",
        x + 400, y + 76, gate_fill);
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"16\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">OUTPUT GATE: %s</text>\n",
        x + 510, y + 102, vm2_gate_text(state));

    fprintf(f,
        "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" "
        "stroke=\"%s\" stroke-width=\"7\" stroke-linecap=\"round\"/>\n",
        x + 630, y + 97, x + 815, y + 97,
        (state == STATE_SEVERED) ? "#475569" : vm2_line_color);

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"190\" height=\"42\" rx=\"9\" "
        "fill=\"#1e293b\" stroke=\"#94a3b8\" stroke-width=\"1.5\"/>\n",
        x + 825, y + 76);
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#e5e7eb\" font-size=\"16\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">CONTROL PATH</text>\n",
        x + 920, y + 102);

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"160\" height=\"42\" rx=\"9\" "
        "fill=\"%s\" stroke=\"#94a3b8\" stroke-width=\"1.5\"/>\n",
        x + 1030, y + 76,
        (state == STATE_SEVERED) ? "#075985" :
        (state == STATE_PROBATION) ? "#854d0e" : "#1e293b");
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"15\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">VM1 FALLBACK</text>\n",
        x + 1110, y + 102);

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"%s\" font-size=\"13\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">%s</text>\n",
        x + 1110, y + 132,
        (state == STATE_SEVERED) ? "#38bdf8" :
        (state == STATE_PROBATION) ? "#fde68a" : "#94a3b8",
        fallback_text(state));
}

static void draw_rejected_stamp(FILE *f, int cx, int cy)
{
    fprintf(f,
        "<g transform=\"translate(%d,%d) rotate(-14)\">\n"
        "<rect x=\"-95\" y=\"-20\" width=\"190\" height=\"40\" rx=\"6\" "
        "fill=\"#7f1d1d\" fill-opacity=\"0.92\" stroke=\"#fecaca\" stroke-width=\"2\"/>\n"
        "<text x=\"0\" y=\"8\" fill=\"#ffffff\" font-size=\"22\" font-family=\"Arial\" "
        "font-weight=\"bold\" text-anchor=\"middle\">REJECTED</text>\n"
        "</g>\n",
        cx, cy);
}

static void write_svg(const char *path,
                      const hasee_frame_packet_t *pkt,
                      guardian_state_t state,
                      int bad_streak,
                      int good_streak,
                      int cut_event)
{
    FILE *f;
    int i;
    int x0 = 30;
    int y0 = 110;
    int vw = 720;
    int vh = 405;
    int panel_x = 785;
    int panel_y = 110;
    int panel_w = 455;
    int panel_h = 405;
    char image_href[128];
    char risk_reason[160];

    snprintf(image_href, sizeof(image_href), "frames/frame_%04u.jpg", pkt->frame_id);
    build_risk_reason(pkt, risk_reason, sizeof(risk_reason));

    f = fopen(path, "w");
    if (!f) {
        perror("[VM1 GuardianViz] fopen svg");
        return;
    }

    fprintf(f,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n",
        SVG_W, SVG_H, SVG_W, SVG_H);

    fprintf(f,
        "<rect x=\"0\" y=\"0\" width=\"%d\" height=\"%d\" fill=\"#0f172a\"/>\n",
        SVG_W, SVG_H);

    fprintf(f,
        "<text x=\"30\" y=\"45\" fill=\"#f8fafc\" font-size=\"30\" "
        "font-family=\"Arial\" font-weight=\"bold\" letter-spacing=\"2\">"
        "H-A-SEE Runtime Safety Gate Demo</text>\n");

    fprintf(f,
        "<text x=\"30\" y=\"75\" fill=\"#94a3b8\" font-size=\"17\" "
        "font-family=\"Arial\">"
        "VM2 AI output is accepted only while runtime health is safe</text>\n");

    fprintf(f,
        "<rect x=\"930\" y=\"24\" width=\"310\" height=\"44\" rx=\"10\" fill=\"%s\"/>\n",
        state_dark_color(state));
    fprintf(f,
        "<text x=\"1085\" y=\"53\" fill=\"#ffffff\" font-size=\"20\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">%s</text>\n",
        safety_state_title(state));

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#000000\" "
        "stroke=\"#94a3b8\" stroke-width=\"2\"/>\n",
        x0, y0, vw, vh);

    fprintf(f,
        "<image href=\"%s\" x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" preserveAspectRatio=\"none\"/>\n",
        image_href, x0, y0, vw, vh);

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"360\" height=\"28\" fill=\"#020617\" fill-opacity=\"0.55\"/>\n",
        x0, y0);
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#e5e7eb\" font-size=\"15\" font-family=\"monospace\">image: %s</text>\n",
        x0 + 12, y0 + 20, image_href);

    for (i = 0; i < (int)pkt->num_objs && i < HASEE_MAX_OBJS; i++) {
        const hasee_obj_t *o = &pkt->objs[i];
        int bx;
        int by;
        int bw;
        int bh;
        const char *color;

        if (pkt->img_w == 0 || pkt->img_h == 0)
            continue;

        bx = x0 + ((int)o->x1 * vw) / pkt->img_w;
        by = y0 + ((int)o->y1 * vh) / pkt->img_h;
        bw = (((int)o->x2 - (int)o->x1) * vw) / pkt->img_w;
        bh = (((int)o->y2 - (int)o->y1) * vh) / pkt->img_h;
        color = box_color(o->cls);

        if (bw < 1) bw = 1;
        if (bh < 1) bh = 1;

        if (state == STATE_SEVERED) {
            fprintf(f,
                "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
                "fill=\"none\" stroke=\"#94a3b8\" stroke-width=\"3\" "
                "stroke-dasharray=\"8 6\" opacity=\"0.75\"/>\n",
                bx, by, bw, bh);
            fprintf(f,
                "<rect x=\"%d\" y=\"%d\" width=\"170\" height=\"24\" fill=\"#334155\" opacity=\"0.85\"/>\n",
                bx, by - 27);
            fprintf(f,
                "<text x=\"%d\" y=\"%d\" fill=\"#e5e7eb\" font-size=\"14\" font-family=\"Arial\" font-weight=\"bold\">%s %.2f IGNORED</text>\n",
                bx + 6, by - 9, cls_name(o->cls), o->conf);
        } else {
            fprintf(f,
                "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" "
                "fill=\"none\" stroke=\"%s\" stroke-width=\"4\"/>\n",
                bx, by, bw, bh, color);
            fprintf(f,
                "<rect x=\"%d\" y=\"%d\" width=\"190\" height=\"26\" fill=\"%s\" opacity=\"0.90\"/>\n",
                bx, by - 28, color);
            fprintf(f,
                "<text x=\"%d\" y=\"%d\" fill=\"#111827\" font-size=\"16\" font-family=\"Arial\" font-weight=\"bold\">%s %.2f</text>\n",
                bx + 6, by - 9, cls_name(o->cls), o->conf);
        }
    }

    if (state == STATE_PROBATION) {
        fprintf(f,
            "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#78350f\" fill-opacity=\"0.25\"/>\n",
            x0, y0, vw, vh);
        fprintf(f,
            "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"70\" rx=\"12\" fill=\"#854d0e\" fill-opacity=\"0.94\"/>\n",
            x0 + 38, y0 + 300, vw - 76);
        fprintf(f,
            "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"27\" font-family=\"Arial\" font-weight=\"bold\">AI OUTPUT RESTRICTED</text>\n",
            x0 + 62, y0 + 333);
        fprintf(f,
            "<text x=\"%d\" y=\"%d\" fill=\"#fde68a\" font-size=\"18\" font-family=\"Arial\" font-weight=\"bold\">Fallback pre-armed / Risk persistence is increasing</text>\n",
            x0 + 62, y0 + 358);
    }

    if (state == STATE_SEVERED) {
        fprintf(f,
            "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"#020617\" fill-opacity=\"0.58\"/>\n",
            x0, y0, vw, vh);
        draw_rejected_stamp(f, x0 + vw - 140, y0 + 70);
        fprintf(f,
            "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"164\" rx=\"14\" "
            "fill=\"#991b1b\" fill-opacity=\"0.97\" stroke=\"#fecaca\" stroke-width=\"2\"/>\n",
            x0 + 40, y0 + 120, vw - 80);

        fprintf(f,
            "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"34\" "
            "font-family=\"Arial\" font-weight=\"bold\">%s</text>\n",
            x0 + 66, y0 + 170,
            cut_event ? "SAFETY GATE TRIGGERED" : "VM2 OUTPUT PATH CUT");

        fprintf(f,
            "<text x=\"%d\" y=\"%d\" fill=\"#bfdbfe\" font-size=\"26\" "
            "font-family=\"Arial\" font-weight=\"bold\">VM1 FALLBACK ONLY MODE</text>\n",
            x0 + 66, y0 + 212);

        fprintf(f,
            "<text x=\"%d\" y=\"%d\" fill=\"#fee2e2\" font-size=\"20\" "
            "font-family=\"Arial\" font-weight=\"bold\">VM2 AI result is received but ignored</text>\n",
            x0 + 66, y0 + 248);
    }

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" rx=\"14\" "
        "fill=\"#1e293b\" stroke=\"#94a3b8\" stroke-width=\"2\"/>\n",
        panel_x, panel_y, panel_w, panel_h);

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#f8fafc\" font-size=\"25\" "
        "font-family=\"Arial\" font-weight=\"bold\">Guardian Safety Panel</text>\n",
        panel_x + 26, panel_y + 42);

    fprintf(f,
        "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"46\" rx=\"9\" fill=\"%s\"/>\n",
        panel_x + 26, panel_y + 62, panel_w - 52, state_color(state));

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"21\" "
        "font-family=\"Arial\" font-weight=\"bold\" text-anchor=\"middle\">%s</text>\n",
        panel_x + panel_w / 2, panel_y + 92, safety_state_title(state));

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#cbd5e1\" font-size=\"16\" font-family=\"Arial\">Active Control Path</text>\n",
        panel_x + 30, panel_y + 135);
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"19\" font-family=\"Arial\" font-weight=\"bold\">%s</text>\n",
        panel_x + 30, panel_y + 160, active_path_text(state));

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#cbd5e1\" font-size=\"16\" font-family=\"Arial\">VM2 Output Gate</text>\n",
        panel_x + 30, panel_y + 195);
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"19\" font-family=\"Arial\" font-weight=\"bold\">%s</text>\n",
        panel_x + 30, panel_y + 220, vm2_gate_text(state));

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#cbd5e1\" font-size=\"16\" font-family=\"Arial\">Fallback Mode</text>\n",
        panel_x + 245, panel_y + 195);
    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#ffffff\" font-size=\"19\" font-family=\"Arial\" font-weight=\"bold\">%s</text>\n",
        panel_x + 245, panel_y + 220, fallback_text(state));

    draw_authority_bar(f, panel_x + 30, panel_y + 265, panel_w - 60,
                       "AI Control Authority", ai_authority_pct(state), "#22c55e");
    draw_authority_bar(f, panel_x + 30, panel_y + 325, panel_w - 60,
                       "Fallback Authority", fallback_authority_pct(state), "#38bdf8");

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#94a3b8\" font-size=\"13\" font-family=\"monospace\">seq=%u frame=%u objs=%u bad=%d good=%d</text>\n",
        panel_x + 30, panel_y + 380,
        pkt->seq, pkt->frame_id, pkt->num_objs, bad_streak, good_streak);

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#94a3b8\" font-size=\"13\" font-family=\"monospace\">uncertainty=%.2f latency=%ums health=%s</text>\n",
        panel_x + 30, panel_y + 400,
        pkt->uncertainty, pkt->latency_ms, health_name(pkt->health));

    fprintf(f,
        "<text x=\"%d\" y=\"%d\" fill=\"#cbd5e1\" font-size=\"13\" font-family=\"Arial\" font-weight=\"bold\">Risk: %s</text>\n",
        panel_x + 30, panel_y + 423, risk_reason);

    draw_runtime_flow(f, state);

    fprintf(f, "</svg>\n");
    fclose(f);
}

int main(void)
{
    int fd;
    int opt = 1;
    struct sockaddr_in addr;
    guardian_state_t state = STATE_NORMAL;
    int bad_streak = 0;
    int good_streak = 0;
    int fallback_only_latched = 0;
    uint32_t cut_seq = 0xffffffffu;

    mkdir(OUT_DIR, 0755);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[VM1 GuardianViz] socket");
        return 1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(GUARDIAN_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[VM1 GuardianViz] bind");
        close(fd);
        return 1;
    }

    printf("[VM1 GuardianViz] listening on UDP %d\n", GUARDIAN_PORT);
    printf("[VM1 GuardianViz] dashboard mode: Runtime Safety Gate / VM2 path cut / VM1 fallback only\n");
    printf("[VM1 GuardianViz] latest SVG: /demo/latest.svg\n");
    fflush(stdout);

    while (1) {
        hasee_frame_packet_t pkt;
        struct sockaddr_in src;
        socklen_t slen = sizeof(src);
        int n;
        int risky = 0;
        int cut_event = 0;
        char seq_svg_path[128];
        char risk_reason[160];

        n = recvfrom(fd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&src, &slen);
        if (n < 0) {
            perror("[VM1 GuardianViz] recvfrom");
            continue;
        }

        if (n != (int)sizeof(pkt) || pkt.magic != HASEE_MAGIC) {
            printf("[VM1 GuardianViz] invalid packet size=%d expected=%lu magic=0x%x\n",
                   n, (unsigned long)sizeof(pkt), pkt.magic);
            fflush(stdout);
            continue;
        }

        if (pkt.num_objs > HASEE_MAX_OBJS)
            pkt.num_objs = HASEE_MAX_OBJS;

        if (pkt.uncertainty >= 0.80f || pkt.latency_ms >= 40 ||
            pkt.health != HASEE_HEALTH_OK)
            risky = 1;

        if (risky) {
            bad_streak++;
            good_streak = 0;
        } else {
            good_streak++;
            if (good_streak >= 3)
                bad_streak = 0;
        }

        if (bad_streak >= 3)
            state = STATE_SEVERED;
        else if (bad_streak >= 1)
            state = STATE_PROBATION;
        else if (good_streak >= 3)
            state = STATE_NORMAL;

        /*
         * Demo latch:
         * Once VM2 is judged SEVERED, VM2 output path remains cut.
         * Even if VM2 keeps sending normal-looking packets later,
         * Guardian stays in VM1 fallback-only mode.
         */
        if (state == STATE_SEVERED && !fallback_only_latched) {
            fallback_only_latched = 1;
            cut_seq = pkt.seq;
            cut_event = 1;
        }

        if (fallback_only_latched) {
            state = STATE_SEVERED;
            if (bad_streak < 3)
                bad_streak = 3;
            if (pkt.seq == cut_seq)
                cut_event = 1;
        }

        snprintf(seq_svg_path, sizeof(seq_svg_path),
                 OUT_DIR "/result_seq_%04u.svg", pkt.seq);

        write_svg(seq_svg_path, &pkt, state, bad_streak, good_streak, cut_event);
        write_svg(OUT_DIR "/latest.svg", &pkt, state, bad_streak, good_streak, cut_event);

        build_risk_reason(&pkt, risk_reason, sizeof(risk_reason));

        printf("[VM1 GuardianViz] RX seq=%u frame_id=%u objs=%u uncertainty=%.2f latency=%u health=%s decision=%s active_path=\"%s\" vm2_gate=\"%s\" fallback=\"%s\" ai_authority=%d fallback_authority=%d result=\"%s\" risk=\"%s\" latest=/demo/latest.svg\n",
               pkt.seq,
               pkt.frame_id,
               pkt.num_objs,
               pkt.uncertainty,
               pkt.latency_ms,
               health_name(pkt.health),
               state_name(state),
               active_path_text(state),
               vm2_gate_text(state),
               fallback_text(state),
               ai_authority_pct(state),
               fallback_authority_pct(state),
               state == STATE_SEVERED ? "VM2_RESULT_IGNORED" : "VM2_RESULT_ACCEPTED",
               risk_reason);
        fflush(stdout);
    }

    close(fd);
    return 0;
}