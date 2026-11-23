// ai_infer_tx.cpp
// VM2-side lightweight AI inference sender for H-A-SEE demo
// - Runs MobileNetV2-SSDLite VOC ncnn model on CPU
// - Reads /demo/frames/frame_XXXX.jpg
// - Sends the existing hasee_frame_packet_t UDP packet to VM1 Guardian
//
// Build target: aarch64/32 Linux guest binary
// Required headers: ncnn net.h, stb_image.h, hasee_frame_packet.h

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "net.h"
// #include "ncnn/src/net.h"

// Decode JPEG without OpenCV.
// Download from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include "stb_image.h"

#include "hasee_frame_packet.h"

#ifndef HASEE_NUM_FRAMES
#define HASEE_NUM_FRAMES 100
#endif

#define DEFAULT_FRAME_DIR      "/demo/frames"
#define DEFAULT_MODEL_PARAM    "/demo/model/detector.param"
#define DEFAULT_MODEL_BIN      "/demo/model/detector.bin"
#define DEFAULT_LABELS         "/demo/model/labels.txt"
#define DEFAULT_DST_IP         "10.10.10.1"
#define DEFAULT_DST_PORT       7777
#define DEFAULT_DELAY_MS       500
#define DETECTION_THRESHOLD    0.30f
#define MODEL_TARGET_SIZE      300

// MobileNetV2-SSDLite VOC model may contain a Silence layer.
// ncnn official example registers it as a no-op layer.
class Noop : public ncnn::Layer
{
};
DEFINE_LAYER_CREATOR(Noop)

struct Detection
{
    int voc_label;
    float prob;
    int x1;
    int y1;
    int x2;
    int y2;
};

struct Config
{
    std::string frame_dir = DEFAULT_FRAME_DIR;
    std::string model_param = DEFAULT_MODEL_PARAM;
    std::string model_bin = DEFAULT_MODEL_BIN;
    std::string labels = DEFAULT_LABELS;
    std::string dst_ip = DEFAULT_DST_IP;
    int dst_port = DEFAULT_DST_PORT;
    int delay_ms = DEFAULT_DELAY_MS;
    bool selftest = false;
    bool once = false;
    bool fault_injection = true;
    bool use_actual_latency = false;
    std::string selftest_image;
};

static uint64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

static void usage(const char* prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s [options]\n\n"
        "Options:\n"
        "  --frames DIR              frame directory, default /demo/frames\n"
        "  --param FILE              ncnn param, default /demo/model/detector.param\n"
        "  --bin FILE                ncnn bin, default /demo/model/detector.bin\n"
        "  --labels FILE             label file, default /demo/model/labels.txt\n"
        "  --dst IP                  VM1 Guardian IP, default 10.10.10.1\n"
        "  --port PORT               UDP port, default 7777\n"
        "  --delay-ms N              loop delay, default 500\n"
        "  --selftest IMAGE          run one image inference and exit\n"
        "  --once                    send one packet and exit\n"
        "  --no-fault-injection      disable forced OOD window\n"
        "  --use-actual-latency      send measured QEMU latency in packet\n"
        "  --help\n\n",
        prog);
}

static int parse_args(int argc, char** argv, Config& cfg)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            cfg.frame_dir = argv[++i];
        } else if (strcmp(argv[i], "--param") == 0 && i + 1 < argc) {
            cfg.model_param = argv[++i];
        } else if (strcmp(argv[i], "--bin") == 0 && i + 1 < argc) {
            cfg.model_bin = argv[++i];
        } else if (strcmp(argv[i], "--labels") == 0 && i + 1 < argc) {
            cfg.labels = argv[++i];
        } else if (strcmp(argv[i], "--dst") == 0 && i + 1 < argc) {
            cfg.dst_ip = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            cfg.dst_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--delay-ms") == 0 && i + 1 < argc) {
            cfg.delay_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--selftest") == 0 && i + 1 < argc) {
            cfg.selftest = true;
            cfg.selftest_image = argv[++i];
        } else if (strcmp(argv[i], "--once") == 0) {
            cfg.once = true;
        } else if (strcmp(argv[i], "--no-fault-injection") == 0) {
            cfg.fault_injection = false;
        } else if (strcmp(argv[i], "--use-actual-latency") == 0) {
            cfg.use_actual_latency = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 1;
        } else {
            fprintf(stderr, "[VM2 AI Infer] Unknown or incomplete option: %s\n", argv[i]);
            usage(argv[0]);
            return -1;
        }
    }

    if (cfg.dst_port <= 0 || cfg.dst_port > 65535) {
        fprintf(stderr, "[VM2 AI Infer] invalid port: %d\n", cfg.dst_port);
        return -1;
    }

    if (cfg.delay_ms < 0)
        cfg.delay_ms = 0;

    return 0;
}

static const char* voc_label_name(int label)
{
    static const char* names[] = {
        "background", "aeroplane", "bicycle", "bird", "boat", "bottle",
        "bus", "car", "cat", "chair", "cow", "diningtable", "dog",
        "horse", "motorbike", "person", "pottedplant", "sheep", "sofa",
        "train", "tvmonitor"
    };

    if (label >= 0 && label < (int)(sizeof(names) / sizeof(names[0])))
        return names[label];
    return "unknown";
}

static int map_voc_to_hasee(int voc_label)
{
    // VOC label index:
    // 6=bus, 7=car, 14=motorbike, 15=person, 19=train
    // H-A-SEE dashboard currently supports car/pedestrian/cone.
    switch (voc_label) {
    case 6:   // bus
    case 7:   // car
    case 14:  // motorbike
    case 19:  // train
        return HASEE_CLS_CAR;
    case 15:  // person
        return HASEE_CLS_PEDESTRIAN;
    default:
        return 0; // ignore
    }
}

static int load_model(ncnn::Net& net, const Config& cfg)
{
    net.opt.use_vulkan_compute = false;
    net.opt.num_threads = 1;
    net.opt.use_packing_layout = true;

    net.register_custom_layer("Silence", Noop_layer_creator);

    if (net.load_param(cfg.model_param.c_str()) != 0) {
        fprintf(stderr, "[VM2 AI Infer] ERROR: load_param failed: %s\n", cfg.model_param.c_str());
        return -1;
    }

    if (net.load_model(cfg.model_bin.c_str()) != 0) {
        fprintf(stderr, "[VM2 AI Infer] ERROR: load_model failed: %s\n", cfg.model_bin.c_str());
        return -1;
    }

    fprintf(stdout, "[VM2 AI Infer] model loaded param=%s bin=%s\n",
            cfg.model_param.c_str(), cfg.model_bin.c_str());
    fflush(stdout);
    return 0;
}

static int detect_one_image(ncnn::Net& net,
                            const char* image_path,
                            std::vector<Detection>& detections,
                            int* img_w_out,
                            int* img_h_out,
                            uint32_t* actual_latency_ms_out)
{
    int img_w = 0;
    int img_h = 0;
    int img_c = 0;
    unsigned char* rgb = NULL;
    uint64_t t0;
    uint64_t t1;

    detections.clear();

    rgb = stbi_load(image_path, &img_w, &img_h, &img_c, 3);
    if (!rgb) {
        fprintf(stderr, "[VM2 AI Infer] ERROR: stbi_load failed: %s reason=%s\n",
                image_path, stbi_failure_reason());
        return -1;
    }

    if (img_w <= 0 || img_h <= 0) {
        fprintf(stderr, "[VM2 AI Infer] ERROR: invalid image size: %s %dx%d\n",
                image_path, img_w, img_h);
        stbi_image_free(rgb);
        return -1;
    }

    *img_w_out = img_w;
    *img_h_out = img_h;

    t0 = now_ms();

    // stb_image returns RGB. The official ncnn example feeds BGR.
    // PIXEL_RGB2BGR converts RGB input to BGR internally.
    ncnn::Mat in = ncnn::Mat::from_pixels_resize(
        rgb,
        ncnn::Mat::PIXEL_RGB2BGR,
        img_w,
        img_h,
        MODEL_TARGET_SIZE,
        MODEL_TARGET_SIZE);

    stbi_image_free(rgb);
    rgb = NULL;

    const float mean_vals[3] = {127.5f, 127.5f, 127.5f};
    const float norm_vals[3] = {1.0f / 127.5f, 1.0f / 127.5f, 1.0f / 127.5f};
    in.substract_mean_normalize(mean_vals, norm_vals);

    ncnn::Extractor ex = net.create_extractor();
    ex.set_light_mode(true);
    ex.input("data", in);

    ncnn::Mat out;
    if (ex.extract("detection_out", out) != 0) {
        fprintf(stderr, "[VM2 AI Infer] ERROR: extract detection_out failed\n");
        return -1;
    }

    t1 = now_ms();
    *actual_latency_ms_out = (uint32_t)(t1 - t0);

    for (int i = 0; i < out.h; i++) {
        const float* values = out.row(i);

        int voc_label = (int)values[0];
        float prob = values[1];

        if (prob < DETECTION_THRESHOLD)
            continue;

        int x1 = (int)(values[2] * img_w);
        int y1 = (int)(values[3] * img_h);
        int x2 = (int)(values[4] * img_w);
        int y2 = (int)(values[5] * img_h);

        x1 = clamp_int(x1, 0, img_w - 1);
        y1 = clamp_int(y1, 0, img_h - 1);
        x2 = clamp_int(x2, 0, img_w - 1);
        y2 = clamp_int(y2, 0, img_h - 1);

        if (x2 <= x1 || y2 <= y1)
            continue;

        Detection d;
        d.voc_label = voc_label;
        d.prob = prob;
        d.x1 = x1;
        d.y1 = y1;
        d.x2 = x2;
        d.y2 = y2;
        detections.push_back(d);
    }

    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) { return a.prob > b.prob; });

    return 0;
}

static void make_packet_from_detections(hasee_frame_packet_t* pkt,
                                        uint32_t seq,
                                        uint32_t frame_id,
                                        int img_w,
                                        int img_h,
                                        const std::vector<Detection>& detections,
                                        uint32_t actual_latency_ms,
                                        const Config& cfg)
{
    memset(pkt, 0, sizeof(*pkt));

    pkt->magic = HASEE_MAGIC;
    pkt->seq = seq;
    pkt->frame_id = frame_id;
    pkt->img_w = (uint32_t)img_w;
    pkt->img_h = (uint32_t)img_h;

    float max_conf = 0.0f;
    uint32_t out_count = 0;

    for (size_t i = 0; i < detections.size() && out_count < HASEE_MAX_OBJS; i++) {
        const Detection& d = detections[i];
        int hasee_cls = map_voc_to_hasee(d.voc_label);

        if (hasee_cls == 0)
            continue;

        hasee_obj_t* o = &pkt->objs[out_count];
        o->x1 = (uint16_t)clamp_int(d.x1, 0, 65535);
        o->y1 = (uint16_t)clamp_int(d.y1, 0, 65535);
        o->x2 = (uint16_t)clamp_int(d.x2, 0, 65535);
        o->y2 = (uint16_t)clamp_int(d.y2, 0, 65535);
        o->cls = (uint8_t)hasee_cls;
        o->conf = d.prob;

        if (d.prob > max_conf)
            max_conf = d.prob;

        out_count++;
    }

    pkt->num_objs = out_count;

    /*
    * Demo policy:
    * No VOC detection does not immediately mean AI health fault.
    * The VOC model is not trained for nuScenes-specific classes/conditions,
    * so zero detection should not trigger OOD by itself in this PoC.
    */
    if (out_count == 0)
        pkt->uncertainty = 0.45f;
    else
        pkt->uncertainty = 1.0f - max_conf;

    // QEMU CPU inference latency is not target-board latency.
    // Default mode sends normalized latency to preserve the existing demo story.
    // Use --use-actual-latency if you want Guardian to receive measured QEMU latency.
    if (cfg.use_actual_latency) {
        pkt->latency_ms = actual_latency_ms;
    } else {
        if (actual_latency_ms > 35)
            pkt->latency_ms = 23;
        else
            pkt->latency_ms = actual_latency_ms;
    }

    pkt->health = HASEE_HEALTH_OK;
    if (pkt->uncertainty >= 0.80f || pkt->latency_ms >= 40)
        pkt->health = HASEE_HEALTH_OOD;

    // Deterministic OOD injection window for H-A-SEE safety-gate demo.
    // This keeps NORMAL -> PROBATION -> SEVERED behavior reproducible.
    if (cfg.fault_injection) {
        uint32_t phase = seq % HASEE_NUM_FRAMES;
        if (phase >= 10 && phase <= 18) {
            pkt->uncertainty = 0.91f;
            pkt->latency_ms = 44;
            pkt->health = HASEE_HEALTH_OOD;
        }
    }
}

static int open_udp_socket(struct sockaddr_in* dst, const Config& cfg)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[VM2 AI Infer] socket");
        return -1;
    }

    memset(dst, 0, sizeof(*dst));
    dst->sin_family = AF_INET;
    dst->sin_port = htons((uint16_t)cfg.dst_port);

    if (inet_pton(AF_INET, cfg.dst_ip.c_str(), &dst->sin_addr) != 1) {
        fprintf(stderr, "[VM2 AI Infer] invalid dst ip: %s\n", cfg.dst_ip.c_str());
        close(fd);
        return -1;
    }

    return fd;
}

static void print_detections(const std::vector<Detection>& detections)
{
    for (size_t i = 0; i < detections.size(); i++) {
        const Detection& d = detections[i];
        int hasee_cls = map_voc_to_hasee(d.voc_label);
        fprintf(stdout,
                "[VM2 AI Infer]   det[%zu] voc=%d(%s) prob=%.3f box=(%d,%d,%d,%d) hasee_cls=%d%s\n",
                i,
                d.voc_label,
                voc_label_name(d.voc_label),
                d.prob,
                d.x1, d.y1, d.x2, d.y2,
                hasee_cls,
                hasee_cls == 0 ? " ignored" : "");
    }
}

int main(int argc, char** argv)
{
    Config cfg;
    int parse_ret = parse_args(argc, argv, cfg);
    if (parse_ret > 0)
        return 0;
    if (parse_ret < 0)
        return 1;

    fprintf(stdout, "[VM2 AI Infer] starting\n");
    fprintf(stdout, "[VM2 AI Infer] frames=%s\n", cfg.frame_dir.c_str());
    fprintf(stdout, "[VM2 AI Infer] model_param=%s\n", cfg.model_param.c_str());
    fprintf(stdout, "[VM2 AI Infer] model_bin=%s\n", cfg.model_bin.c_str());
    fprintf(stdout, "[VM2 AI Infer] dst=%s:%d\n", cfg.dst_ip.c_str(), cfg.dst_port);
    fprintf(stdout, "[VM2 AI Infer] fault_injection=%s use_actual_latency=%s\n",
            cfg.fault_injection ? "on" : "off",
            cfg.use_actual_latency ? "on" : "off");
    fflush(stdout);

    ncnn::Net net;
    if (load_model(net, cfg) != 0)
        return 1;

    if (cfg.selftest) {
        std::vector<Detection> detections;
        int img_w = 0;
        int img_h = 0;
        uint32_t actual_latency_ms = 0;

        if (detect_one_image(net, cfg.selftest_image.c_str(), detections,
                             &img_w, &img_h, &actual_latency_ms) != 0) {
            return 1;
        }

        fprintf(stdout,
                "[VM2 AI Infer] selftest image=%s size=%dx%d actual_latency=%ums raw_detections=%zu\n",
                cfg.selftest_image.c_str(), img_w, img_h, actual_latency_ms, detections.size());
        print_detections(detections);
        fflush(stdout);
        return 0;
    }

    struct sockaddr_in dst;
    int udp_fd = open_udp_socket(&dst, cfg);
    if (udp_fd < 0)
        return 1;

    uint32_t seq = 0;

    while (1) {
        char image_path[256];
        uint32_t frame_id = seq % HASEE_NUM_FRAMES;
        std::vector<Detection> detections;
        int img_w = 0;
        int img_h = 0;
        uint32_t actual_latency_ms = 0;
        hasee_frame_packet_t pkt;

        snprintf(image_path, sizeof(image_path), "%s/frame_%04u.jpg",
                 cfg.frame_dir.c_str(), frame_id);

        if (detect_one_image(net, image_path, detections, &img_w, &img_h,
                             &actual_latency_ms) != 0) {
            fprintf(stderr, "[VM2 AI Infer] inference failed for %s\n", image_path);
            usleep(500000);
            continue;
        }

        make_packet_from_detections(&pkt, seq, frame_id, img_w, img_h,
                                    detections, actual_latency_ms, cfg);

        ssize_t sent = sendto(udp_fd, &pkt, sizeof(pkt), 0,
                              (struct sockaddr*)&dst, sizeof(dst));
        if (sent != (ssize_t)sizeof(pkt)) {
            fprintf(stderr,
                    "[VM2 AI Infer] sendto failed sent=%ld expected=%lu errno=%d\n",
                    (long)sent, (unsigned long)sizeof(pkt), errno);
        }

        fprintf(stdout,
                "[VM2 AI Infer] TX seq=%u frame=%u img=%dx%d raw_det=%zu hasee_objs=%u actual_latency=%ums packet_latency=%u uncertainty=%.2f health=%s dst=%s:%d\n",
                pkt.seq,
                pkt.frame_id,
                img_w,
                img_h,
                detections.size(),
                pkt.num_objs,
                actual_latency_ms,
                pkt.latency_ms,
                pkt.uncertainty,
                pkt.health == HASEE_HEALTH_OK ? "OK" : "OOD",
                cfg.dst_ip.c_str(),
                cfg.dst_port);
        fflush(stdout);

        seq++;

        if (cfg.once)
            break;

        if (cfg.delay_ms > 0)
            usleep((useconds_t)cfg.delay_ms * 1000U);
    }

    close(udp_fd);
    return 0;
}
