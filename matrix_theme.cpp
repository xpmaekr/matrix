#include <ncurses.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <cctype>
#include <csignal>
#include <cstring>
#include <cmath>
#include <map>

struct RGB { int r = 0, g = 0, b = 0; };

static volatile sig_atomic_t reload_flag = 0;
static void on_usr1(int) { reload_flag = 1; }

static std::string env_str(const char* key) {
    const char* v = getenv(key);
    return v ? v : "";
}

static bool read_file(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n\"'");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n\"'");
    return s.substr(a, b - a + 1);
}

static bool parse_hex(const std::string& value, RGB& out) {
    std::string v = trim(value);
    size_t hash = v.find('#');
    if (hash == std::string::npos || v.size() < hash + 7) return false;
    std::string h = v.substr(hash + 1, 6);
    for (char c : h) if (!isxdigit((unsigned char)c)) return false;
    auto hex2 = [](const std::string& two) {
        int v = 0;
        for (char c : two) {
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return -1;
            v = v * 16 + d;
        }
        return v;
    };
    out.r = hex2(h.substr(0, 2));
    out.g = hex2(h.substr(2, 2));
    out.b = hex2(h.substr(4, 2));
    return true;
}

// Все файлы, куда Omarchy складывает палитру (colors.toml из темы/обоев).
static std::vector<std::string> theme_files() {
    std::vector<std::string> files;
    std::string home = env_str("HOME");
    std::string state = env_str("XDG_STATE_HOME");
    if (state.empty()) state = home + "/.local/state";

    std::string custom = env_str("OMARCHY_MATRIX_COLORS");
    if (!custom.empty()) files.push_back(custom);

    files.push_back(state + "/omarchy/current/theme/colors.toml");
    files.push_back(home + "/.config/omarchy/current/theme/colors.toml");

    // Тема по имени — для старых версий Omarchy без current/theme.
    std::string name;
    if (read_file(state + "/omarchy/current/theme.name", name)) {
        name = trim(name);
        if (!name.empty()) {
            files.push_back(home + "/.config/omarchy/themes/" + name + "/colors.toml");
            files.push_back("/usr/share/omarchy/themes/" + name + "/colors.toml");
        }
    }
    return files;
}

static bool load_theme(std::map<std::string, RGB>& colors, bool& light) {
    std::string text;
    for (const auto& path : theme_files()) {
        if (!read_file(path, text)) continue;

        std::istringstream in(text);
        std::string line;
        bool found = false;
        while (std::getline(in, line)) {
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string key = trim(line.substr(0, eq));
            std::string value = trim(line.substr(eq + 1));

            if (key == "mode") {
                light = (value.find("light") != std::string::npos);
                continue;
            }
            if (key.empty() || key[0] == '#') continue;

            RGB rgb;
            if (parse_hex(value, rgb)) {
                colors[key] = rgb;
                found = true;
            }
        }
        if (found) return true;
    }
    return false;
}

static RGB pick(const std::map<std::string, RGB>& colors,
                std::initializer_list<const char*> keys,
                RGB fallback) {
    for (const char* key : keys) {
        auto it = colors.find(key);
        if (it != colors.end()) return it->second;
    }
    return fallback;
}

// Ближайший цвет из палитры xterm-256.
static int to_256(RGB c) {
    static const int levels[6] = {0, 95, 135, 175, 215, 255};
    auto nearest_level = [](int v) {
        int best = 0;
        for (int i = 1; i < 6; i++)
            if (std::abs(v - levels[i]) < std::abs(v - levels[best])) best = i;
        return best;
    };
    int ri = nearest_level(c.r), gi = nearest_level(c.g), bi = nearest_level(c.b);
    int cube = 16 + 36 * ri + 6 * gi + bi;
    int cr = levels[ri], cg = levels[gi], cb = levels[bi];
    long cube_dist = (long)(c.r - cr) * (c.r - cr) + (long)(c.g - cg) * (c.g - cg) + (long)(c.b - cb) * (c.b - cb);

    int avg = (c.r + c.g + c.b) / 3;
    int gi2 = std::min(23, std::max(0, (avg - 8 + 5) / 10));
    int gv = 8 + 10 * gi2;
    long gray_dist = (long)(c.r - gv) * (c.r - gv) * 3;

    return cube_dist <= gray_dist ? cube : 232 + gi2;
}

static int to_basic(RGB c) {
    int ri = c.r > 127, gi = c.g > 127, bi = c.b > 127;
    int idx = ri | (gi << 1) | (bi << 2);
    static const int map[8] = {
        COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
        COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE
    };
    return map[idx];
}

static int terminal_color(RGB c) {
    if (COLORS >= 256) return to_256(c);
    return to_basic(c);
}

static int color_main, color_flash, color_rain, color_mutation;

static void apply_theme() {
    std::map<std::string, RGB> colors;
    bool light = false;
    bool themed = load_theme(colors, light);

    // Фолбэк — классический зелёный Matrix.
    RGB main = pick(colors, {"green", "bright_green"}, RGB{0, 255, 70});
    RGB flash = pick(colors, {"bright_green", "foreground", "bright_foreground", "accent"}, RGB{220, 255, 220});
    RGB rain = pick(colors, {"accent", "blue", "cyan", "bright_blue"}, RGB{0, 140, 60});
    RGB mutation = pick(colors, {"magenta", "bright_magenta", "cyan", "accent"}, flash);

    if (light) {
        // На светлом фоне белое мерцание не видно — берём тёмные цвета.
        flash = pick(colors, {"dark_foreground", "foreground", "accent", "green"}, RGB{0, 90, 40});
        rain = pick(colors, {"muted", "dark_foreground", "blue"}, RGB{90, 110, 130});
        main = pick(colors, {"dark_foreground", "green", "foreground"}, RGB{20, 90, 40});
    }

    color_main = terminal_color(main);
    color_flash = terminal_color(flash);
    color_rain = terminal_color(rain);
    color_mutation = terminal_color(mutation);

    init_pair(1, color_main, -1);
    init_pair(2, color_flash, -1);
    init_pair(3, color_rain, -1);
    init_pair(4, color_mutation, -1);

    (void)themed;
}

int main(int argc, char** argv) {
    srand(time(NULL));

    if (argc > 1 && std::strcmp(argv[1], "--colors") == 0) {
        std::map<std::string, RGB> colors;
        bool light = false;
        bool themed = load_theme(colors, light);
        RGB main = pick(colors, {"green", "bright_green"}, RGB{0, 255, 70});
        printf("theme: %s (%s)\n", themed ? "omarchy" : "fallback", light ? "light" : "dark");
        printf("main:   #%02x%02x%02x\n", main.r, main.g, main.b);
        return 0;
    }

    signal(SIGUSR1, on_usr1);

    initscr();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    start_color();
    use_default_colors();
    apply_theme();

    // Твой рисунок
    std::vector<std::string> art = {
        "        010010",
        "0101101. o1   000          10    ..( 010010",
        "0100100   1           0101 010600 0101010",
        "  01001100             1001001001 0101110",
        "    10001                 .  o01010111010",
        "    100                   00100 30 0100101",
        "     00                   000101 0  0  10 1",
        "           0181           01010      1 0'",
        "           00100          01010        ^  01",
        "            0101          0001 1          10001",
        "            010           000          010111",
        "            01                        10 0",
        "            1                          0"
    };

    int max_len = 0;
    for (const auto& s : art) max_len = std::max((int)s.length(), max_len);

    int ch;
    while ((ch = getch()) != 'q') {
        if (reload_flag) {
            reload_flag = 0;
            apply_theme();
        }

        erase();

        // Получаем размеры ЭКРАНА КАЖДЫЙ КАДР (чтобы картинка всегда была по центру при изменении окна)
        int max_y, max_x;
        getmaxyx(stdscr, max_y, max_x);

        int start_y = (max_y - art.size()) / 2;
        int start_x = (max_x - max_len) / 2;

        // Легкий фоновый дождь (цвет акцента темы)
        for (int i = 0; i < max_x; i += 3) {
            if (rand() % 10 == 0) {
                attron(COLOR_PAIR(3) | A_DIM);
                mvaddch(rand() % max_y, i, rand() % 2 + '0');
                attroff(COLOR_PAIR(3) | A_DIM);
            }
        }

        // Отрисовка "кота" с изменением символов
        for (size_t i = 0; i < art.size(); i++) {
            for (size_t j = 0; j < art[i].length(); j++) {
                char c = art[i][j];
                if (c != ' ') {
                    bool mutated = false;
                    // Мутация символов (0 в 1 и наоборот)
                    if ((c == '0' || c == '1') && (rand() % 15 == 0)) {
                        c = (c == '0') ? '1' : '0';
                        mutated = true;
                    }

                    // Мерцание
                    if (mutated) {
                        attron(COLOR_PAIR(4) | A_BOLD);
                    } else if (rand() % 20 == 0) {
                        attron(COLOR_PAIR(2) | A_BOLD);
                    } else {
                        attron(COLOR_PAIR(1) | A_BOLD);
                    }

                    // Защита от вылета за границы, если терминал слишком маленький
                    int draw_y = start_y + i;
                    int draw_x = start_x + j;
                    if (draw_y >= 0 && draw_y < max_y && draw_x >= 0 && draw_x < max_x) {
                        mvaddch(draw_y, draw_x, c);
                    }

                    attroff(COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3) | COLOR_PAIR(4) | A_BOLD);
                }
            }
        }
        refresh();
        napms(75); // Скорость анимации
    }
    endwin();
    return 0;
}
