#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_net.h>
#include <SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
//#include <SDL_gpu.h>
//#include <SFML/Network.hpp>
//#include <SFML/Graphics.hpp>
#include <algorithm>
#include <atomic>
#include <codecvt>
#include <functional>
#include <locale>
#include <mutex>
#include <thread>
#include <random>
#ifdef __ANDROID__
#include "vendor/PUGIXML/src/pugixml.hpp"
#include <android/log.h> //__android_log_print(ANDROID_LOG_VERBOSE, "Molly", "Example number log: %d", number);
#include <jni.h>
#else
#include <filesystem>
#include <pugixml.hpp>
#ifdef __EMSCRIPTEN__
namespace fs = std::__fs::filesystem;
#else
namespace fs = std::filesystem;
#endif
using namespace std::chrono_literals;
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include "main.h"
#endif

// NOTE: Remember to uncomment it on every release
//#define RELEASE

#if defined _MSC_VER && defined RELEASE
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

//240 x 240 (smart watch)
//240 x 320 (QVGA)
//360 x 640 (Galaxy S5)
//640 x 480 (480i - Smallest PC monitor)

#define PLAYER_SPEED 0.4
#define JUMP_HEIGHT 70
#define APPLE_SPEED 0.1

int windowWidth = 240;
int windowHeight = 320;
SDL_Point mousePos;
SDL_Point realMousePos;
bool keys[SDL_NUM_SCANCODES];
bool buttons[SDL_BUTTON_X2 + 1];
SDL_Window* window;
SDL_Renderer* renderer;
TTF_Font* robotoF;
bool running = true;

void logOutputCallback(void* userdata, int category, SDL_LogPriority priority, const char* message)
{
    std::cout << message << std::endl;
}

int random(int min, int max)
{
    return min + rand() % ((max + 1) - min);
}

int SDL_QueryTextureF(SDL_Texture* texture, Uint32* format, int* access, float* w, float* h)
{
    int wi, hi;
    int result = SDL_QueryTexture(texture, format, access, &wi, &hi);
    if (w) {
        *w = wi;
    }
    if (h) {
        *h = hi;
    }
    return result;
}

SDL_bool SDL_PointInFRect(const SDL_Point* p, const SDL_FRect* r)
{
    return ((p->x >= r->x) && (p->x < (r->x + r->w)) && (p->y >= r->y) && (p->y < (r->y + r->h))) ? SDL_TRUE : SDL_FALSE;
}

std::ostream& operator<<(std::ostream& os, SDL_FRect r)
{
    os << r.x << " " << r.y << " " << r.w << " " << r.h;
    return os;
}

std::ostream& operator<<(std::ostream& os, SDL_Rect r)
{
    SDL_FRect fR;
    fR.w = r.w;
    fR.h = r.h;
    fR.x = r.x;
    fR.y = r.y;
    os << fR;
    return os;
}

SDL_Texture* renderText(SDL_Texture* previousTexture, TTF_Font* font, SDL_Renderer* renderer, const std::string& text, SDL_Color color)
{
    if (previousTexture) {
        SDL_DestroyTexture(previousTexture);
    }
    SDL_Surface* surface;
    if (text.empty()) {
        surface = TTF_RenderUTF8_Blended(font, " ", color);
    }
    else {
        surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    }
    if (surface) {
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        return texture;
    }
    else {
        return 0;
    }
}

struct Text {
    std::string text;
    SDL_Surface* surface = 0;
    SDL_Texture* t = 0;
    SDL_FRect dstR{};
    bool autoAdjustW = false;
    bool autoAdjustH = false;
    float wMultiplier = 1;
    float hMultiplier = 1;

    ~Text()
    {
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
    }

    Text()
    {
    }

    Text(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
    }

    Text& operator=(const Text& rightText)
    {
        text = rightText.text;
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (rightText.surface) {
            surface = SDL_ConvertSurface(rightText.surface, rightText.surface->format, SDL_SWSURFACE);
        }
        if (rightText.t) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
        dstR = rightText.dstR;
        autoAdjustW = rightText.autoAdjustW;
        autoAdjustH = rightText.autoAdjustH;
        wMultiplier = rightText.wMultiplier;
        hMultiplier = rightText.hMultiplier;
        return *this;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, std::string text, SDL_Color c = { 255, 255, 255 })
    {
        this->text = text;
#if 1 // NOTE: renderText
        if (surface) {
            SDL_FreeSurface(surface);
        }
        if (t) {
            SDL_DestroyTexture(t);
        }
        if (text.empty()) {
            surface = TTF_RenderUTF8_Blended(font, " ", c);
        }
        else {
            surface = TTF_RenderUTF8_Blended(font, text.c_str(), c);
        }
        if (surface) {
            t = SDL_CreateTextureFromSurface(renderer, surface);
        }
#endif
        if (autoAdjustW) {
            SDL_QueryTextureF(t, 0, 0, &dstR.w, 0);
        }
        if (autoAdjustH) {
            SDL_QueryTextureF(t, 0, 0, 0, &dstR.h);
        }
        dstR.w *= wMultiplier;
        dstR.h *= hMultiplier;
    }

    void setText(SDL_Renderer* renderer, TTF_Font* font, int value, SDL_Color c = { 255, 255, 255 })
    {
        setText(renderer, font, std::to_string(value), c);
    }

    void draw(SDL_Renderer* renderer)
    {
        if (t) {
            SDL_RenderCopyF(renderer, t, 0, &dstR);
        }
    }
};

int SDL_RenderDrawCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y + offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y + offsetx);
        status += SDL_RenderDrawPoint(renderer, x + offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x + offsety, y - offsetx);
        status += SDL_RenderDrawPoint(renderer, x - offsetx, y - offsety);
        status += SDL_RenderDrawPoint(renderer, x - offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

int SDL_RenderFillCircle(SDL_Renderer* renderer, int x, int y, int radius)
{
    int offsetx, offsety, d;
    int status;

    offsetx = 0;
    offsety = radius;
    d = radius - 1;
    status = 0;

    while (offsety >= offsetx) {

        status += SDL_RenderDrawLine(renderer, x - offsety, y + offsetx,
            x + offsety, y + offsetx);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y + offsety,
            x + offsetx, y + offsety);
        status += SDL_RenderDrawLine(renderer, x - offsetx, y - offsety,
            x + offsetx, y - offsety);
        status += SDL_RenderDrawLine(renderer, x - offsety, y - offsetx,
            x + offsety, y - offsetx);

        if (status < 0) {
            status = -1;
            break;
        }

        if (d >= 2 * offsetx) {
            d -= 2 * offsetx + 1;
            offsetx += 1;
        }
        else if (d < 2 * (radius - offsety)) {
            d += 2 * offsety - 1;
            offsety -= 1;
        }
        else {
            d += 2 * (offsety - offsetx - 1);
            offsety -= 1;
            offsetx += 1;
        }
    }

    return status;
}

SDL_bool SDL_FRectEmpty(const SDL_FRect* r)
{
    return ((!r) || (r->w <= 0) || (r->h <= 0)) ? SDL_TRUE : SDL_FALSE;
}

SDL_bool SDL_IntersectFRect(const SDL_FRect* A, const SDL_FRect* B, SDL_FRect* result)
{
    int Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    if (!result) {
        SDL_InvalidParamError("result");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        result->w = 0;
        result->h = 0;
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    result->x = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->w = Amax - Amin;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    result->y = Amin;
    if (Bmax < Amax)
        Amax = Bmax;
    result->h = Amax - Amin;

    return (SDL_FRectEmpty(result) == SDL_TRUE) ? SDL_FALSE : SDL_TRUE;
}

SDL_bool SDL_HasIntersectionF(const SDL_FRect* A, const SDL_FRect* B)
{
    int Amin, Amax, Bmin, Bmax;

    if (!A) {
        SDL_InvalidParamError("A");
        return SDL_FALSE;
    }

    if (!B) {
        SDL_InvalidParamError("B");
        return SDL_FALSE;
    }

    /* Special cases for empty rects */
    if (SDL_FRectEmpty(A) || SDL_FRectEmpty(B)) {
        return SDL_FALSE;
    }

    /* Horizontal intersection */
    Amin = A->x;
    Amax = Amin + A->w;
    Bmin = B->x;
    Bmax = Bmin + B->w;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    /* Vertical intersection */
    Amin = A->y;
    Amax = Amin + A->h;
    Bmin = B->y;
    Bmax = Bmin + B->h;
    if (Bmin > Amin)
        Amin = Bmin;
    if (Bmax < Amax)
        Amax = Bmax;
    if (Amax <= Amin)
        return SDL_FALSE;

    return SDL_TRUE;
}

int eventWatch(void* userdata, SDL_Event* event)
{
    // WARNING: Be very careful of what you do in the function, as it may run in a different thread
    if (event->type == SDL_APP_TERMINATING || event->type == SDL_APP_WILLENTERBACKGROUND) {
    }
    return 0;
}

struct Clock {
    Uint64 start = SDL_GetPerformanceCounter();

    float getElapsedTime()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        return secondsElapsed * 1000;
    }

    float restart()
    {
        Uint64 stop = SDL_GetPerformanceCounter();
        float secondsElapsed = (stop - start) / (float)SDL_GetPerformanceFrequency();
        start = SDL_GetPerformanceCounter();
        return secondsElapsed * 1000;
    }
};

SDL_Texture* getT(SDL_Renderer* renderer, std::string name)
{
    static std::unordered_map<std::string, SDL_Texture*> map;
    auto it = map.find(name);
    if (it == map.end()) {
        SDL_Texture* t = IMG_LoadTexture(renderer, ("res/" + name).c_str());
        map.insert({ name, t });
        return t;
    }
    else {
        return it->second;
    }
}

struct Entity {
    SDL_FRect r{};
    int dx = 0;
    bool jumping = false;
    float jumpedDistance = 0;
    bool hasTeeths = false;
};

struct Tile {
    SDL_FRect dstR{};
    SDL_Rect srcR{};
    std::string source;
};

enum class State {
    Menu,
    Gameplay,
    Credits,
};

SDL_Texture* charactersT;
SDL_Texture* sheetT;
SDL_Texture* appleT;
SDL_Texture* leftArrowT;
SDL_Texture* rightArrowT;
SDL_Texture* characterT;
SDL_Texture* character2T;
SDL_Texture* character3T;
SDL_Texture* backArrowT;
SDL_Texture* teethT;
SDL_Texture* muteT;
SDL_Texture* unmuteT;
Entity player;
std::vector<Entity> bots;
Clock globalClock;
std::vector<SDL_FRect> applesRects;
State state = State::Menu;
std::vector<Tile> tiles;
Text botsCountText;
Text botsCountValueText;
SDL_FRect leftArrowBtnR;
SDL_FRect rightArrowBtnR;
Text playText;
Text playerPointsText;
Text enemyPointsText;
Text enemy2PointsText;
Text enemy3PointsText;
SDL_FRect playerPointR;
SDL_FRect enemyPointR;
SDL_FRect enemy2PointR;
SDL_FRect enemy3PointR;
SDL_FRect teethR;
int mapWidth;
int mapHeight;
Mix_Music* music;
Mix_Chunk* jumpS;
Mix_Chunk* pickupS;
Mix_Chunk* selectS;
Text creditsText;
Text authorText;
Text creditText;
Text credit2Text;
Text credit3Text;
SDL_FRect backArrowBtnR;
SDL_FRect soundR;
bool soundsMuted = false;

void loadMap(std::string filename)
{
    pugi::xml_document doc;
    doc.load_file(("res/" + filename).c_str());
    pugi::xml_node mapNode = doc.child("map");
    mapWidth = mapNode.attribute("width").as_int() * mapNode.attribute("tilewidth").as_int();
    mapHeight = mapNode.attribute("height").as_int() * mapNode.attribute("tileheight").as_int();
    auto layersIt = mapNode.children("layer");
    for (pugi::xml_node& layer : layersIt) {
        std::vector<int> data;
        {
            std::string csv = layer.child("data").text().as_string();
            std::stringstream ss(csv);
            std::string value;
            while (std::getline(ss, value, ',')) {
                data.push_back(std::stoi(value));
            }
        }
        for (int y = 0; y < mapNode.attribute("height").as_int(); ++y) {
            for (int x = 0; x < mapNode.attribute("width").as_int(); ++x) {
                Tile t;
                t.dstR.w = mapNode.attribute("tilewidth").as_int();
                t.dstR.h = mapNode.attribute("tileheight").as_int();
                t.dstR.x = x * t.dstR.w;
                t.dstR.y = y * t.dstR.h;
                int h = mapNode.attribute("height").as_int();
                int id = data[y * mapNode.attribute("width").as_int() + x];
                if (id != 0) {
                    pugi::xml_node tilesetNode;
                    auto tilesetsIt = mapNode.children("tileset");
                    for (pugi::xml_node& tileset : tilesetsIt) {
                        int firstgid = tileset.attribute("firstgid").as_int();
                        int tileCount = tileset.attribute("tilecount").as_int();
                        int lastGid = firstgid + tileCount - 1;
                        if (id >= firstgid && id <= lastGid) {
                            tilesetNode = tileset;
                            break;
                        }
                    }
                    t.srcR.w = tilesetNode.attribute("tilewidth").as_int();
                    t.srcR.h = tilesetNode.attribute("tileheight").as_int();
                    t.srcR.x = (id - tilesetNode.attribute("firstgid").as_int()) % tilesetNode.attribute("columns").as_int() * t.srcR.w;
                    t.srcR.y = (id - tilesetNode.attribute("firstgid").as_int()) / tilesetNode.attribute("columns").as_int() * t.srcR.h;
                    t.source = tilesetNode.child("image").attribute("source").as_string();
                    tiles.push_back(t);
                }
            }
        }
    }
}

SDL_FRect generateEnemyR()
{
    SDL_FRect r;
    r.w = 32;
    r.h = 32;
    r.x = random(0, windowWidth - r.w);
    r.y = windowHeight - r.h;
    return r;
}

void randomizeTeethPosition(SDL_FRect& teethR, std::vector<Tile> tiles)
{
    int minY = tiles.front().dstR.y;
    int minIndex = 0;
    for (int i = 1; i < tiles.size(); ++i) {
        if (minY > tiles[i].dstR.y) {
            minY = tiles[i].dstR.y;
            minIndex = i;
        }
    }
    teethR.x = random(tiles[minIndex].dstR.x, tiles[minIndex].dstR.x + tiles[minIndex].dstR.w - teethR.w);
    teethR.y = minY - teethR.h;
}

void restartLevel(Entity& player, std::vector<SDL_FRect>& applesRects, std::vector<Entity>& bots, SDL_FRect& teethR)
{
    player.r.x = windowWidth / 2 - player.r.w / 2;
    player.r.y = windowHeight - player.r.h;
    applesRects.clear();
    randomizeTeethPosition(teethR, tiles);
    for (int i = 0; i < bots.size(); ++i) {
        bots[i].r = generateEnemyR();
    }
    player.hasTeeths = false;
}

void muteMusicAndSounds()
{
    Mix_VolumeMusic(0);
    Mix_Volume(-1, 0);
}

void unmuteMusicAndSounds()
{
    Mix_VolumeMusic(128);
    Mix_Volume(-1, 128);
}

void mainLoop()
{
    float deltaTime = globalClock.restart();
    SDL_Event event;
    if (state == State::Menu) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
                if (SDL_PointInFRect(&mousePos, &playText.dstR)) {
                    state = State::Gameplay;
                    restartLevel(player, applesRects, bots, teethR);
                    bots.clear();
                    playerPointsText.setText(renderer, robotoF, "0");
                    enemyPointsText.setText(renderer, robotoF, "0");
                    enemy2PointsText.setText(renderer, robotoF, "0");
                    enemy3PointsText.setText(renderer, robotoF, "0");
                    if (botsCountValueText.text == "1") {
                        bots.push_back(Entity());
                        bots.back().r = generateEnemyR();
                    }
                    else if (botsCountValueText.text == "2") {
                        bots.push_back(Entity());
                        bots.back().r = generateEnemyR();
                        bots.push_back(Entity());
                        bots.back().r = generateEnemyR();
                    }
                    else if (botsCountValueText.text == "3") {
                        bots.push_back(Entity());
                        bots.back().r = generateEnemyR();
                        bots.push_back(Entity());
                        bots.back().r = generateEnemyR();
                        bots.push_back(Entity());
                        bots.back().r = generateEnemyR();
                    }
                    Mix_PlayChannel(-1, selectS, 0);
                }
                if (SDL_PointInFRect(&mousePos, &creditsText.dstR)) {
                    state = State::Credits;
                }
                if (SDL_PointInFRect(&mousePos, &soundR)) {
                    soundsMuted = !soundsMuted;
                    if (soundsMuted) {
                        muteMusicAndSounds();
                    }
                    else {
                        unmuteMusicAndSounds();
                    }
                }
                if (SDL_PointInFRect(&mousePos, &leftArrowBtnR)) {
                    if (botsCountValueText.text == "2") {
                        botsCountValueText.setText(renderer, robotoF, "1", {});
                        Mix_PlayChannel(-1, selectS, 0);
                    }
                    else if (botsCountValueText.text == "3") {
                        botsCountValueText.setText(renderer, robotoF, "2", {});
                        Mix_PlayChannel(-1, selectS, 0);
                    }
                }
                if (SDL_PointInFRect(&mousePos, &rightArrowBtnR)) {
                    if (botsCountValueText.text == "1") {
                        botsCountValueText.setText(renderer, robotoF, "2", {});
                        Mix_PlayChannel(-1, selectS, 0);
                    }
                    else if (botsCountValueText.text == "2") {
                        botsCountValueText.setText(renderer, robotoF, "3", {});
                        Mix_PlayChannel(-1, selectS, 0);
                    }
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        botsCountText.draw(renderer);
        botsCountValueText.draw(renderer);
        SDL_RenderCopyF(renderer, leftArrowT, 0, &leftArrowBtnR);
        SDL_RenderCopyF(renderer, rightArrowT, 0, &rightArrowBtnR);
        playText.draw(renderer);
        creditsText.draw(renderer);
        if (soundsMuted) {
            SDL_RenderCopyF(renderer, muteT, 0, &soundR);
        }
        else {
            SDL_RenderCopyF(renderer, unmuteT, 0, &soundR);
        }
        SDL_RenderPresent(renderer);
    }
    else if (state == State::Gameplay) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    state = State::Menu;
                }
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        player.dx = 0;
        if (keys[SDL_SCANCODE_A]) {
            player.dx = -1;
        }
        else if (keys[SDL_SCANCODE_D]) {
            player.dx = 1;
        }
        if (keys[SDL_SCANCODE_W]) {
            if (player.r.y == windowHeight - player.r.h) {
                player.jumping = true;
                Mix_PlayChannel(-1, jumpS, 0);
            }
            for (int i = 0; i < tiles.size(); ++i) {
                if (player.r.x + player.r.w >= tiles[i].dstR.x
                    && player.r.x <= tiles[i].dstR.x + tiles[i].dstR.w
                    && player.r.y + player.r.h == tiles[i].dstR.y) {
                    player.jumping = true;
                    Mix_PlayChannel(-1, jumpS, 0);
                    break;
                }
            }
        }
        if (player.jumping) {
            player.r.y -= PLAYER_SPEED * deltaTime;
            player.jumpedDistance += PLAYER_SPEED * deltaTime;
            if (player.jumpedDistance > JUMP_HEIGHT) {
                player.jumping = false;
                player.jumpedDistance = 0;
            }
        }
        else {
            player.r.y += PLAYER_SPEED * deltaTime;
            for (int i = 0; i < tiles.size(); ++i) {
                if (player.r.x + player.r.w >= tiles[i].dstR.x
                    && player.r.x <= tiles[i].dstR.x + tiles[i].dstR.w
                    && player.r.y + player.r.h >= tiles[i].dstR.y
                    && player.r.y < tiles[i].dstR.y + tiles[i].dstR.h) {
                    player.r.y = tiles[i].dstR.y - player.r.h;
                    break;
                }
            }
        }
        player.r.x += player.dx * deltaTime;
        player.r.x = std::clamp(player.r.x, 0.f, windowWidth - player.r.w);
        if (player.r.y + player.r.h > windowHeight) {
            player.r.y = windowHeight - player.r.h;
        }
        for (int i = 0; i < applesRects.size(); ++i) {
            applesRects[i].y += APPLE_SPEED * deltaTime;
            if (applesRects[i].y > windowHeight) {
                applesRects.erase(applesRects.begin() + i--);
            }
        }
        for (int i = 0; i < bots.size(); ++i) {
            bots[i].dx = random(-1, 1);
            bots[i].r.x += bots[i].dx;

            if (bots[i].r.y == windowHeight - bots[i].r.h) {
                bots[i].jumping = true;
            }
            for (int j = 0; j < tiles.size(); ++j) {
                if (bots[i].r.x + bots[i].r.w >= tiles[j].dstR.x
                    && bots[i].r.x <= tiles[j].dstR.x + tiles[j].dstR.w
                    && bots[i].r.y + bots[i].r.h == tiles[j].dstR.y) {
                    bots[i].jumping = true;
                    break;
                }
            }

            if (bots[i].jumping) {
                bots[i].r.y -= PLAYER_SPEED * deltaTime;
                bots[i].jumpedDistance += PLAYER_SPEED * deltaTime;
                if (bots[i].jumpedDistance > JUMP_HEIGHT) {
                    bots[i].jumping = false;
                    bots[i].jumpedDistance = 0;
                }
            }
            else {
                bots[i].r.y += PLAYER_SPEED * deltaTime;
                for (int j = 0; j < tiles.size(); ++j) {
                    if (bots[i].r.x + bots[i].r.w >= tiles[j].dstR.x
                        && bots[i].r.x <= tiles[j].dstR.x + tiles[j].dstR.w
                        && bots[i].r.y + bots[i].r.h >= tiles[j].dstR.y
                        && bots[i].r.y < tiles[j].dstR.y + tiles[j].dstR.h) {
                        bots[i].r.y = tiles[j].dstR.y - bots[i].r.h;
                        break;
                    }
                }
            }
        }
        for (int i = 0; i < bots.size(); ++i) {
            if (bots[i].r.y + bots[i].r.h > windowHeight) {
                bots[i].r.y = windowHeight - bots[i].r.h;
            }
            bots[i].r.x = std::clamp(bots[i].r.x, 0.f, windowWidth - bots[i].r.w);
        }
        {
            std::vector<int> numbers{ 0, 1 };
            if (bots.size() > 1) {
                numbers.push_back(2);
            }
            if (bots.size() > 2) {
                numbers.push_back(3);
            }
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(numbers.begin(), numbers.end(), g);
            for (int i = 0; i < numbers.size(); ++i) {
                if (numbers[i] == 0) {
                    if (SDL_HasIntersectionF(&player.r, &teethR) && !player.hasTeeths) {
                        player.hasTeeths = true;
                        Mix_PlayChannel(-1, pickupS, 0);
                        for (int i = 0; i < 5; ++i) {
                            applesRects.push_back(SDL_FRect());
                            applesRects.back().w = 32;
                            applesRects.back().h = 32;
                            applesRects.back().x = random(0, windowWidth - applesRects.back().w);
                            applesRects.back().y = -applesRects.back().h;
                            for (int j = 0; j < i; ++j) {
                                int count = 0;
                                while (SDL_HasIntersectionF(&applesRects[i], &applesRects[j]) && count < 10000) {
                                    applesRects[i].x = random(0, windowWidth - applesRects.back().w);
                                    ++count;
                                }
                            }
                        }
                        break;
                    }
                }
                else if (numbers[i] == 1) {
                    if (SDL_HasIntersectionF(&bots[0].r, &teethR) && !player.hasTeeths) {
                        enemyPointsText.setText(renderer, robotoF, std::stoi(enemyPointsText.text) + 5);
                        restartLevel(player, applesRects, bots, teethR);
                        break;
                    }
                }
                else if (numbers[i] == 2) {
                    if (SDL_HasIntersectionF(&bots[1].r, &teethR) && !player.hasTeeths) {
                        enemy2PointsText.setText(renderer, robotoF, std::stoi(enemy2PointsText.text) + 5);
                        restartLevel(player, applesRects, bots, teethR);
                        break;
                    }
                }
                else if (numbers[i] == 3) {
                    if (SDL_HasIntersectionF(&bots[2].r, &teethR) && !player.hasTeeths) {
                        enemy3PointsText.setText(renderer, robotoF, std::stoi(enemy3PointsText.text) + 5);
                        restartLevel(player, applesRects, bots, teethR);
                        break;
                    }
                }
            }
        }
        for (int i = 0; i < applesRects.size(); ++i) {
            if (SDL_HasIntersectionF(&player.r, &applesRects[i])) {
                playerPointsText.setText(renderer, robotoF, std::stoi(playerPointsText.text) + 1);
                applesRects.erase(applesRects.begin() + i--);
            }
        }
        if (player.hasTeeths && applesRects.size() == 0) {
            restartLevel(player, applesRects, bots, teethR);
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        for (Tile& t : tiles) {
            SDL_RenderCopyF(renderer, getT(renderer, t.source), &t.srcR, &t.dstR);
        }
        SDL_Rect characterR;
        characterR.w = 23;
        characterR.h = 22;
        characterR.x = 0;
        characterR.y = 42;
        SDL_RenderCopyF(renderer, charactersT, &characterR, &playerPointR);
        SDL_RenderCopyF(renderer, characterT, 0, &enemyPointR);
        if (!player.hasTeeths) {
            SDL_RenderCopyF(renderer, teethT, 0, &teethR);
        }
        if (bots.size() > 1) {
            SDL_RenderCopyF(renderer, character2T, 0, &enemy2PointR);
            enemy2PointsText.draw(renderer);
        }
        if (bots.size() > 2) {
            SDL_RenderCopyF(renderer, character3T, 0, &enemy3PointR);
            enemy3PointsText.draw(renderer);
        }
        playerPointsText.draw(renderer);
        enemyPointsText.draw(renderer);
        for (int i = 0; i < applesRects.size(); ++i) {
            SDL_RenderCopyF(renderer, appleT, 0, &applesRects[i]);
        }
        for (int i = 0; i < bots.size(); ++i) {
            if (i == 0) {
                SDL_RenderCopyF(renderer, characterT, 0, &bots[i].r);
            }
            if (i == 1) {
                SDL_RenderCopyF(renderer, character2T, 0, &bots[i].r);
            }
            if (i == 2) {
                SDL_RenderCopyF(renderer, character3T, 0, &bots[i].r);
            }
        }
        SDL_RenderCopyF(renderer, charactersT, &characterR, &player.r);
        SDL_RenderPresent(renderer);
    }
    else if (state == State::Credits) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                SDL_RenderSetScale(renderer, event.window.data1 / (float)windowWidth, event.window.data2 / (float)windowHeight);
            }
            if (event.type == SDL_KEYDOWN) {
                keys[event.key.keysym.scancode] = true;
            }
            if (event.type == SDL_KEYUP) {
                keys[event.key.keysym.scancode] = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                buttons[event.button.button] = true;
                if (SDL_PointInFRect(&mousePos, &backArrowBtnR)) {
                    state = State::Menu;
                }
            }
            if (event.type == SDL_MOUSEBUTTONUP) {
                buttons[event.button.button] = false;
            }
            if (event.type == SDL_MOUSEMOTION) {
                float scaleX, scaleY;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                mousePos.x = event.motion.x / scaleX;
                mousePos.y = event.motion.y / scaleY;
                realMousePos.x = event.motion.x;
                realMousePos.y = event.motion.y;
            }
        }
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
        SDL_RenderClear(renderer);
        authorText.draw(renderer);
        creditText.draw(renderer);
        credit2Text.draw(renderer);
        credit3Text.draw(renderer);
        SDL_RenderCopyF(renderer, backArrowT, 0, &backArrowBtnR);
        SDL_RenderPresent(renderer);
    }
}

int main(int argc, char* argv[])
{
    std::srand(std::time(0));
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
    SDL_LogSetOutputFunction(logOutputCallback, 0);
    SDL_Init(SDL_INIT_EVERYTHING);
    TTF_Init();
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    SDL_GetMouseState(&mousePos.x, &mousePos.y);
    window = SDL_CreateWindow("Molly", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    robotoF = TTF_OpenFont("res/roboto.ttf", 72);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_RenderSetScale(renderer, w / (float)windowWidth, h / (float)windowHeight);
    SDL_AddEventWatch(eventWatch, 0);
    charactersT = IMG_LoadTexture(renderer, "res/characters.png");
    sheetT = IMG_LoadTexture(renderer, "res/sheet.png");
    appleT = IMG_LoadTexture(renderer, "res/apple.png");
    leftArrowT = IMG_LoadTexture(renderer, "res/leftArrow.png");
    rightArrowT = IMG_LoadTexture(renderer, "res/rightArrow.png");
    characterT = IMG_LoadTexture(renderer, "res/character.png");
    character2T = IMG_LoadTexture(renderer, "res/character2.png");
    character3T = IMG_LoadTexture(renderer, "res/character3.png");
    teethT = IMG_LoadTexture(renderer, "res/teeth.png");
    backArrowT = IMG_LoadTexture(renderer, "res/backArrow.png");
    muteT = IMG_LoadTexture(renderer, "res/mute.png");
    unmuteT = IMG_LoadTexture(renderer, "res/unmute.png");
    music = Mix_LoadMUS("res/music.wav");
    jumpS = Mix_LoadWAV("res/jump.wav");
    pickupS = Mix_LoadWAV("res/pickup.wav");
    selectS = Mix_LoadWAV("res/select.wav");
    Mix_PlayMusic(music, -1);
    player.r.w = 32;
    player.r.h = 32;
    player.r.x = windowWidth / 2 - player.r.w / 2;
    player.r.y = windowHeight - player.r.h;
    loadMap("map.tmx");
    botsCountText.setText(renderer, robotoF, "Bots count: ", {});
    botsCountText.dstR.w = 100;
    botsCountText.dstR.h = 30;
    botsCountText.dstR.x = 20;
    botsCountText.dstR.y = 40;
    botsCountValueText.autoAdjustW = true;
    botsCountValueText.wMultiplier = 0.4;
    botsCountValueText.setText(renderer, robotoF, "1", {});
    botsCountValueText.dstR.h = 30;
    botsCountValueText.dstR.x = botsCountText.dstR.x + botsCountText.dstR.w;
    botsCountValueText.dstR.y = botsCountText.dstR.y + botsCountText.dstR.h / 2 - botsCountValueText.dstR.h / 2;
    leftArrowBtnR.w = 32;
    leftArrowBtnR.h = 32;
    leftArrowBtnR.x = botsCountValueText.dstR.x + botsCountValueText.dstR.w / 2 - leftArrowBtnR.w - 10;
    leftArrowBtnR.y = botsCountValueText.dstR.y + botsCountValueText.dstR.h + 5;
    rightArrowBtnR = leftArrowBtnR;
    rightArrowBtnR.x = botsCountValueText.dstR.x + botsCountValueText.dstR.w / 2 + 10;
    playText.setText(renderer, robotoF, "Play", {});
    playText.dstR.w = 100;
    playText.dstR.h = 50;
    playText.dstR.x = leftArrowBtnR.x + leftArrowBtnR.w - playText.dstR.w / 2;
    playText.dstR.y = leftArrowBtnR.y + 100;
    creditsText = playText;
    creditsText.setText(renderer, robotoF, "Credits", {});
    creditsText.dstR.w = 50;
    creditsText.dstR.h = 20;
    creditsText.dstR.x = playText.dstR.x + playText.dstR.w / 2 - creditsText.dstR.w / 2;
    creditsText.dstR.y = playText.dstR.y + playText.dstR.h + 5;
    playerPointR.w = 32;
    playerPointR.h = 32;
    playerPointR.x = 0;
    playerPointR.y = 0;
    enemyPointR.w = 32;
    enemyPointR.h = 32;
    enemyPointR.x = windowWidth / 4;
    enemyPointR.y = 0;
    enemy2PointR.w = 32;
    enemy2PointR.h = 32;
    enemy2PointR.x = windowWidth / 4 * 2;
    enemy2PointR.y = 0;
    enemy3PointR.w = 32;
    enemy3PointR.h = 32;
    enemy3PointR.x = windowWidth / 4 * 3;
    enemy3PointR.y = 0;
    playerPointsText.autoAdjustW = true;
    playerPointsText.wMultiplier = 0.4;
    playerPointsText.setText(renderer, robotoF, "0");
    playerPointsText.dstR.h = 40;
    playerPointsText.dstR.x = playerPointR.x + playerPointR.w;
    playerPointsText.dstR.y = 0;
    enemyPointsText = playerPointsText;
    enemyPointsText.dstR.x = enemyPointR.x + enemyPointR.w;
    enemy2PointsText = playerPointsText;
    enemy2PointsText.dstR.x = enemy2PointR.x + enemy2PointR.w;
    enemy3PointsText = playerPointsText;
    enemy3PointsText.dstR.x = enemy3PointR.x + enemy3PointR.w;
    teethR.w = 32;
    teethR.h = 32;
    randomizeTeethPosition(teethR, tiles);
    authorText.setText(renderer, robotoF, "Author: Huberti", {});
    authorText.dstR.w = 100;
    authorText.dstR.h = 20;
    authorText.dstR.x = windowWidth / 2 - authorText.dstR.w / 2;
    authorText.dstR.y = 0;
    creditText.setText(renderer, robotoF, "Smashicons", {});
    creditText.dstR.w = 100;
    creditText.dstR.h = 30;
    creditText.dstR.x = windowWidth / 2 - creditText.dstR.w / 2;
    creditText.dstR.y = authorText.dstR.y + authorText.dstR.h;
    credit2Text.setText(renderer, robotoF, "Becris", {});
    credit2Text.dstR.w = 100;
    credit2Text.dstR.h = 30;
    credit2Text.dstR.x = windowWidth / 2 - credit2Text.dstR.w / 2;
    credit2Text.dstR.y = creditText.dstR.y + creditText.dstR.h;
    credit3Text.setText(renderer, robotoF, "Pixel perfect", {});
    credit3Text.dstR.w = 100;
    credit3Text.dstR.h = 30;
    credit3Text.dstR.x = windowWidth / 2 - credit3Text.dstR.w / 2;
    credit3Text.dstR.y = credit2Text.dstR.y + credit2Text.dstR.h;
    backArrowBtnR.w = 32;
    backArrowBtnR.h = 32;
    backArrowBtnR.x = 0;
    backArrowBtnR.y = 0;
    soundR.w = 32;
    soundR.h = 32;
    soundR.x = 0;
    soundR.y = 0;
    globalClock.restart();
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, 1);
#else
    while (running) {
        mainLoop();
    }
#endif
    // TODO: On mobile remember to use eventWatch function (it doesn't reach this code when terminating)
    return 0;
}