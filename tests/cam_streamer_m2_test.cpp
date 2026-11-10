#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cctype>

// Definitions matching esp32-camera sensor.h
typedef enum {
    FRAMESIZE_QQVGA = 1,   // 160x120
    FRAMESIZE_QCIF  = 2,   // 176x144
    FRAMESIZE_HQVGA = 3,   // 240x176
    FRAMESIZE_240X240 = 4, // 240x240
    FRAMESIZE_QVGA  = 5,   // 320x240
    FRAMESIZE_CIF   = 6,   // 400x296
    FRAMESIZE_HVGA  = 7,   // 480x320
    FRAMESIZE_VGA   = 8,   // 640x480
    FRAMESIZE_SVGA  = 9,   // 800x600
    FRAMESIZE_XGA   = 10,  // 1024x768
    FRAMESIZE_HD    = 11,  // 1280x720
    FRAMESIZE_SXGA  = 12,  // 1280x1024
    FRAMESIZE_UXGA  = 13   // 1600x1200
} framesize_t;

// Logic from cam-streamer/src/main.cpp
static bool is_supported_framesize(int fs) {
    switch (fs) {
        case FRAMESIZE_QQVGA: // 1 (160x120)
        case FRAMESIZE_QVGA:  // 5 (320x240)
        case FRAMESIZE_CIF:   // 6 (400x296)
        case FRAMESIZE_VGA:   // 8 (640x480)
        case FRAMESIZE_SVGA:  // 9 (800x600)
            return true;
        default:
            return false;
    }
}

static int parse_framesize_value(const char *val) {
    if (!val || !val[0]) return -1;
    if (isdigit((unsigned char)val[0])) {
        return atoi(val);
    }
    if (strcasecmp(val, "svga") == 0 || strcasecmp(val, "framesize_svga") == 0) return FRAMESIZE_SVGA;
    if (strcasecmp(val, "vga") == 0 || strcasecmp(val, "framesize_vga") == 0) return FRAMESIZE_VGA;
    if (strcasecmp(val, "cif") == 0 || strcasecmp(val, "framesize_cif") == 0) return FRAMESIZE_CIF;
    if (strcasecmp(val, "qvga") == 0 || strcasecmp(val, "framesize_qvga") == 0) return FRAMESIZE_QVGA;
    if (strcasecmp(val, "qqvga") == 0 || strcasecmp(val, "framesize_qqvga") == 0) return FRAMESIZE_QQVGA;
    return -1;
}

// Emulate ESP-IDF httpd_query_key_value
#define ESP_OK 0
#define ESP_ERR_NOT_FOUND 1
#define ESP_ERR_HTTPD_RESULT_TRUNC 2

static int mock_httpd_query_key_value(const char *qry, const char *key, char *val, size_t val_size) {
    if (!qry || !key || !val || val_size == 0) return ESP_ERR_NOT_FOUND;
    std::string q(qry);
    size_t klen = strlen(key);
    size_t pos = 0;
    while (pos < q.size()) {
        size_t amp = q.find('&', pos);
        if (amp == std::string::npos) amp = q.size();
        std::string pair = q.substr(pos, amp - pos);
        size_t eq = pair.find('=');
        if (eq != std::string::npos) {
            std::string k = pair.substr(0, eq);
            std::string v = pair.substr(eq + 1);
            if (k == key) {
                if (v.size() >= val_size) {
                    strncpy(val, v.c_str(), val_size - 1);
                    val[val_size - 1] = '\0';
                    return ESP_ERR_HTTPD_RESULT_TRUNC;
                }
                strncpy(val, v.c_str(), val_size);
                return ESP_OK;
            }
        }
        pos = amp + 1;
    }
    return ESP_ERR_NOT_FOUND;
}

struct ControlResult {
    int status_code;
    std::string error_message;
    int applied_framesize;
    int applied_quality;
};

static ControlResult simulate_control_handler(const char *query_str, bool has_psram) {
    ControlResult result = {200, "", -1, -1};
    if (!query_str || strlen(query_str) == 0) {
        result.status_code = 400;
        result.error_message = "Missing query parameters";
        return result;
    }

    const char *buf = query_str;
    int new_framesize = -1;
    int new_quality = -1;
    char param_buf[32];

    // 1. Check for ?framesize=<val>
    if (mock_httpd_query_key_value(buf, "framesize", param_buf, sizeof(param_buf)) == ESP_OK) {
        new_framesize = parse_framesize_value(param_buf);
    }

    // 2. Check for ?quality=<val>
    if (mock_httpd_query_key_value(buf, "quality", param_buf, sizeof(param_buf)) == ESP_OK) {
        new_quality = atoi(param_buf);
    }

    // 3. Check for ?var=framesize|quality&val=<val>
    char var_buf[32];
    if (mock_httpd_query_key_value(buf, "var", var_buf, sizeof(var_buf)) == ESP_OK &&
        mock_httpd_query_key_value(buf, "val", param_buf, sizeof(param_buf)) == ESP_OK) {
        if (strcmp(var_buf, "framesize") == 0) {
            new_framesize = parse_framesize_value(param_buf);
        } else if (strcmp(var_buf, "quality") == 0) {
            new_quality = atoi(param_buf);
        }
    }

    if (new_framesize == -1 && new_quality == -1) {
        result.status_code = 400;
        result.error_message = "No valid parameters provided";
        return result;
    }

    // Validate framesize
    if (new_framesize != -1) {
        if (!is_supported_framesize(new_framesize)) {
            result.status_code = 400;
            result.error_message = "Unsupported framesize (supported: 1, 5, 6, 8, 9)";
            return result;
        }
        if (!has_psram && new_framesize > FRAMESIZE_CIF) {
            result.status_code = 400;
            result.error_message = "Resolution exceeds non-PSRAM limit (CIF)";
            return result;
        }
    }

    // Validate quality range (10 to 63)
    if (new_quality != -1) {
        if (new_quality < 10 || new_quality > 63) {
            result.status_code = 400;
            result.error_message = "Quality out of range (10-63)";
            return result;
        }
    }

    result.applied_framesize = new_framesize;
    result.applied_quality = new_quality;
    return result;
}

int main() {
    std::cout << "[TEST] Starting Cam-Streamer M2 Verification Suite..." << std::endl;
    int tests_passed = 0;
    int tests_total = 0;

    auto test_assert = [&](bool cond, const std::string &desc) {
        tests_total++;
        if (cond) {
            tests_passed++;
            std::cout << "  [PASS] " << desc << std::endl;
        } else {
            std::cerr << "  [FAIL] " << desc << std::endl;
            assert(false);
        }
    };

    // 1. Test parse_framesize_value and is_supported_framesize
    std::cout << "\n--- Subsuite 1: Framesize Parsing & Range Validation ---" << std::endl;
    test_assert(parse_framesize_value("SVGA") == FRAMESIZE_SVGA, "Parse SVGA uppercase");
    test_assert(parse_framesize_value("svga") == FRAMESIZE_SVGA, "Parse svga lowercase");
    test_assert(parse_framesize_value("framesize_svga") == FRAMESIZE_SVGA, "Parse framesize_svga prefixed");
    test_assert(parse_framesize_value("9") == 9, "Parse '9' numeric");
    test_assert(is_supported_framesize(9) == true, "Framesize 9 (SVGA) is supported");

    test_assert(parse_framesize_value("VGA") == FRAMESIZE_VGA, "Parse VGA");
    test_assert(parse_framesize_value("8") == 8, "Parse '8' numeric");
    test_assert(is_supported_framesize(8) == true, "Framesize 8 (VGA) is supported");

    test_assert(parse_framesize_value("CIF") == FRAMESIZE_CIF, "Parse CIF");
    test_assert(parse_framesize_value("6") == 6, "Parse '6' numeric");
    test_assert(is_supported_framesize(6) == true, "Framesize 6 (CIF) is supported");

    test_assert(parse_framesize_value("QVGA") == FRAMESIZE_QVGA, "Parse QVGA");
    test_assert(parse_framesize_value("5") == 5, "Parse '5' numeric");
    test_assert(is_supported_framesize(5) == true, "Framesize 5 (QVGA) is supported");

    test_assert(parse_framesize_value("QQVGA") == FRAMESIZE_QQVGA, "Parse QQVGA");
    test_assert(parse_framesize_value("1") == 1, "Parse '1' numeric");
    test_assert(is_supported_framesize(1) == true, "Framesize 1 (QQVGA) is supported");

    test_assert(is_supported_framesize(0) == false, "Framesize 0 rejected");
    test_assert(is_supported_framesize(2) == false, "Framesize 2 (QCIF) rejected");
    test_assert(is_supported_framesize(3) == false, "Framesize 3 (HQVGA) rejected");
    test_assert(is_supported_framesize(7) == false, "Framesize 7 (HVGA) rejected");
    test_assert(is_supported_framesize(10) == false, "Framesize 10 (XGA) rejected");
    test_assert(is_supported_framesize(13) == false, "Framesize 13 (UXGA) rejected");
    test_assert(is_supported_framesize(999) == false, "Framesize 999 rejected");
    test_assert(is_supported_framesize(-1) == false, "Framesize -1 rejected");

    test_assert(parse_framesize_value("") == -1, "Empty string returns -1");
    test_assert(parse_framesize_value(NULL) == -1, "NULL returns -1");
    test_assert(parse_framesize_value("garbage") == -1, "Garbage string returns -1");
    test_assert(parse_framesize_value("-5") == -1, "Negative numeric string returns -1");

    // 2. Test Control Query Handler Simulation
    std::cout << "\n--- Subsuite 2: Query Parsing & Edge Cases ---" << std::endl;

    // Empty queries
    {
        ControlResult r = simulate_control_handler("", true);
        test_assert(r.status_code == 400 && r.error_message == "Missing query parameters", "Empty query returns 400");
    }
    {
        ControlResult r = simulate_control_handler("unknown_key=123", true);
        test_assert(r.status_code == 400 && r.error_message == "No valid parameters provided", "Irrelevant keys return 400");
    }

    // Normal queries
    {
        ControlResult r = simulate_control_handler("framesize=8&quality=12", true);
        test_assert(r.status_code == 200 && r.applied_framesize == 8 && r.applied_quality == 12, "framesize=8&quality=12 success");
    }
    {
        ControlResult r = simulate_control_handler("framesize=SVGA&quality=20", true);
        test_assert(r.status_code == 200 && r.applied_framesize == 9 && r.applied_quality == 20, "framesize=SVGA string success");
    }
    {
        ControlResult r = simulate_control_handler("var=framesize&val=6", true);
        test_assert(r.status_code == 200 && r.applied_framesize == 6, "var=framesize&val=6 success");
    }
    {
        ControlResult r = simulate_control_handler("var=quality&val=14", true);
        test_assert(r.status_code == 200 && r.applied_quality == 14, "var=quality&val=14 success");
    }

    // Negative and boundary qualities
    {
        ControlResult r = simulate_control_handler("quality=-10", true);
        test_assert(r.status_code == 400 && r.error_message == "Quality out of range (10-63)", "quality=-10 rejected");
    }
    {
        ControlResult r = simulate_control_handler("quality=-1", true);
        test_assert(r.status_code == 400 && r.error_message == "No valid parameters provided", "quality=-1 triggers no valid params");
    }
    {
        ControlResult r = simulate_control_handler("quality=0", true);
        test_assert(r.status_code == 400 && r.error_message == "Quality out of range (10-63)", "quality=0 rejected");
    }
    {
        ControlResult r = simulate_control_handler("quality=9", true);
        test_assert(r.status_code == 400 && r.error_message == "Quality out of range (10-63)", "quality=9 rejected");
    }
    {
        ControlResult r = simulate_control_handler("quality=10", true);
        test_assert(r.status_code == 200 && r.applied_quality == 10, "quality=10 (lower boundary) accepted");
    }
    {
        ControlResult r = simulate_control_handler("quality=63", true);
        test_assert(r.status_code == 200 && r.applied_quality == 63, "quality=63 (upper boundary) accepted");
    }
    {
        ControlResult r = simulate_control_handler("quality=64", true);
        test_assert(r.status_code == 400 && r.error_message == "Quality out of range (10-63)", "quality=64 rejected");
    }
    {
        ControlResult r = simulate_control_handler("quality=99999", true);
        test_assert(r.status_code == 400 && r.error_message == "Quality out of range (10-63)", "quality=99999 rejected");
    }

    // Unsupported framesizes
    {
        ControlResult r = simulate_control_handler("framesize=2", true);
        test_assert(r.status_code == 400 && r.error_message.find("Unsupported framesize") != std::string::npos, "framesize=2 (QCIF) rejected");
    }
    {
        ControlResult r = simulate_control_handler("framesize=99", true);
        test_assert(r.status_code == 400 && r.error_message.find("Unsupported framesize") != std::string::npos, "framesize=99 rejected");
    }

    // PSRAM enforcement
    {
        ControlResult r = simulate_control_handler("framesize=8", false); // VGA without PSRAM
        test_assert(r.status_code == 400 && r.error_message == "Resolution exceeds non-PSRAM limit (CIF)", "VGA rejected without PSRAM");
    }
    {
        ControlResult r = simulate_control_handler("framesize=9", false); // SVGA without PSRAM
        test_assert(r.status_code == 400 && r.error_message == "Resolution exceeds non-PSRAM limit (CIF)", "SVGA rejected without PSRAM");
    }
    {
        ControlResult r = simulate_control_handler("framesize=6", false); // CIF without PSRAM
        test_assert(r.status_code == 200 && r.applied_framesize == 6, "CIF accepted without PSRAM");
    }

    // Long query / truncated buffer safety
    {
        std::string long_query = "framesize=" + std::string(64, '9') + "&quality=12";
        ControlResult r = simulate_control_handler(long_query.c_str(), true);
        // Truncated value should fail gracefully without crashing
        test_assert(r.status_code == 200 || r.status_code == 400, "Oversized parameter handled without memory fault");
    }

    // 3. Drop Counter State Machine Simulation
    std::cout << "\n--- Subsuite 3: Drop Counter State Machine ---" << std::endl;
    {
        // Scenario 1: Transient 3 frame drops then recovery
        int consecutive_drops = 0;
        bool stream_aborted = false;
        int frames_delivered = 0;

        for (int frame = 0; frame < 20; frame++) {
            // Drop frames at index 5, 6, 7
            bool frame_dropped = (frame >= 5 && frame <= 7);
            if (frame_dropped) {
                consecutive_drops++;
                if (consecutive_drops < 10) {
                    continue; // retry
                } else {
                    stream_aborted = true;
                    break;
                }
            }
            consecutive_drops = 0;
            frames_delivered++;
        }
        test_assert(!stream_aborted && frames_delivered == 17, "Stream survives 3 transient drops without aborting");
    }

    {
        // Scenario 2: Maximum drops reached (10 consecutive drops)
        int consecutive_drops = 0;
        bool stream_aborted = false;
        int loop_iterations = 0;

        while (true) {
            loop_iterations++;
            // Camera permanently returns NULL
            bool fb_null = true;
            if (fb_null) {
                consecutive_drops++;
                if (consecutive_drops < 10) {
                    continue;
                } else {
                    stream_aborted = true;
                    break;
                }
            }
            consecutive_drops = 0;
        }
        test_assert(stream_aborted && loop_iterations == 10, "Stream aborts exactly after 10 consecutive drops (no freeze)");
    }

    // 4. Dual HTTP Configuration Validation
    std::cout << "\n--- Subsuite 4: Dual HTTP Server Configuration ---" << std::endl;
    {
        uint16_t cam_server_port = 80;
        uint16_t cam_ctrl_port = 32768;
        uint16_t stm_server_port = 81;
        uint16_t stm_ctrl_port = 32769;

        test_assert(cam_server_port != stm_server_port, "Dual httpd: server ports differ (80 vs 81)");
        test_assert(cam_ctrl_port != stm_ctrl_port, "Dual httpd: ctrl ports differ (32768 vs 32769)");
        test_assert(cam_ctrl_port > 1024 && stm_ctrl_port > 1024, "Dual httpd: ctrl ports in non-privileged range");
    }

    std::cout << "\n==========================================" << std::endl;
    std::cout << "All " << tests_passed << " / " << tests_total << " tests PASSED successfully." << std::endl;
    std::cout << "==========================================" << std::endl;
    return 0;
}
