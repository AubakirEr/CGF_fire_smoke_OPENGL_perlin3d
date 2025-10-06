// main.cpp — Animated Fire & Smoke with 3D Perlin Noise (OpenGL + GLFW + GLAD)
// g++ main.cpp glad.c -lglfw -ldl -std=c++17 -O2   (Linux/Mac)
// cl /std:c++17 main.cpp glad.obj glfw3.lib opengl32.lib gdi32.lib user32.lib (Windows)

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <algorithm>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ---------- tiny helpers ----------
template<typename T>
static T clamp01(T v) { return v < T(0) ? T(0) : (v > T(1) ? T(1) : v); } // avoids std::clamp hassle

static void check(bool ok, const char* msg) {
    if (!ok) { fprintf(stderr, "[FATAL] %s\n", msg); std::exit(EXIT_FAILURE); }
}

static GLuint makeShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    const GLchar* s = (const GLchar*)src;       // явный cast
    glShaderSource(sh, 1, &s, nullptr);
    glCompileShader(sh);

    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        if (len < 1) len = 1;
        std::vector<GLchar> buf(len);
        glGetShaderInfoLog(sh, len, nullptr, buf.data());   // GLchar*
        fprintf(stderr, "Shader compile error:\n%s\n", buf.data());
        std::exit(EXIT_FAILURE);
    }
    return sh;
}

static GLuint makeProgram(const char* vsSrc, const char* fsSrc) {
    GLuint p = glCreateProgram();
    GLuint v = makeShader(GL_VERTEX_SHADER, vsSrc);
    GLuint f = makeShader(GL_FRAGMENT_SHADER, fsSrc);
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);

    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        if (len < 1) len = 1;
        std::vector<GLchar> buf(len);
        glGetProgramInfoLog(p, len, nullptr, buf.data());   // GLchar*
        fprintf(stderr, "Program link error:\n%s\n", buf.data());
        std::exit(EXIT_FAILURE);
    }
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// ---------- 3D Perlin noise (CPU) ----------
static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static float grad(int hash, float x, float y, float z) {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

struct Perlin3D {
    std::array<int, 512> p;
    Perlin3D(unsigned seed = 1337) {
        std::vector<int> perm(256);
        for (int i = 0; i < 256; ++i) perm[i] = i;
        // simple LCG shuffle
        unsigned s = seed;
        for (int i = 255; i > 0; --i) {
            s = s * 1664525u + 1013904223u;
            int j = s % (i + 1);
            std::swap(perm[i], perm[j]);
        }
        for (int i = 0; i < 512; ++i) p[i] = perm[i & 255];
    }
    float noise(float x, float y, float z) const {
        int X = (int)floorf(x) & 255, Y = (int)floorf(y) & 255, Z = (int)floorf(z) & 255;
        x -= floorf(x); y -= floorf(y); z -= floorf(z);
        float u = fade(x), v = fade(y), w = fade(z);
        int A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z;
        int B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;

        float res = lerp(
            lerp(lerp(grad(p[AA], x, y, z),
                grad(p[BA], x - 1, y, z), u),
                lerp(grad(p[AB], x, y - 1, z),
                    grad(p[BB], x - 1, y - 1, z), u), v),
            lerp(lerp(grad(p[AA + 1], x, y, z - 1),
                grad(p[BA + 1], x - 1, y, z - 1), u),
                lerp(grad(p[AB + 1], x, y - 1, z - 1),
                    grad(p[BB + 1], x - 1, y - 1, z - 1), u), v),
            w);
        // bring to [0,1]
        return 0.5f * (res + 1.0f);
    }
};

static GLuint make3DNoiseTex(int N, int octaves, float lacunarity, float gain, unsigned seed) {
    Perlin3D per(seed);
    std::vector<unsigned char> vox(N * N * N);
    float invN = 1.0f / float(N);
    for (int z = 0; z < N; ++z) {
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                float fx = x * invN, fy = y * invN, fz = z * invN;
                // fBm
                float f = 0.0f, amp = 1.0f, freq = 1.0f;
                for (int o = 0; o < octaves; ++o) {
                    f += amp * per.noise(fx * freq * 8.0f, fy * freq * 8.0f, fz * freq * 8.0f);
                    freq *= lacunarity; amp *= gain;
                }
                f = clamp01(f / 1.5f); // normalize a bit
                vox[(z * N + y) * N + x] = (unsigned char)std::round(f * 255.0f);
            }
        }
    }
    GLuint tex; glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, N, N, N, 0, GL_RED, GL_UNSIGNED_BYTE, vox.data());
    glBindTexture(GL_TEXTURE_3D, 0);
    return tex;
}

// ---------- fullscreen-ish quad (vertical billboard) ----------
static GLuint makeUnitQuadVAO() {
    float vboData[] = {
        // pos.x pos.y   uv.x uv.y
        -0.5f, 0.0f,     0.0f, 0.0f,
         0.5f, 0.0f,     1.0f, 0.0f,
         0.5f, 1.0f,     1.0f, 1.0f,
        -0.5f, 1.0f,     0.0f, 1.0f
    };
    unsigned idx[] = { 0,1,2, 0,2,3 };
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vboData), vboData, GL_STATIC_DRAW);
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    return vao;
}

// ---------- shaders ----------
static const char* VERT = R"(#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 vUV;
uniform float uAspect;
uniform float uHeight;  // billboard height in NDC (0..2 roughly)
uniform float uWidth;   // billboard width
uniform vec2  uOffset;  // NDC offset (bottom center at y=-1)
void main(){
    vec2 pos = aPos;
    pos.x *= uWidth;
    pos.y *= uHeight;
    pos.x /= uAspect;
    pos += uOffset;             // move near the bottom
    // convert quad with bottom at offset.y to clip space
    vec2 ndc = vec2(pos.x, -1.0 + pos.y*2.0); // bottom anchored
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
}
)";

static const char* FRAG_FIRE = R"(#version 330 core
out vec4 FragColor;
in vec2 vUV;
uniform sampler3D uNoise;
uniform float uTime, uScale, uSpeed, uSoftEdge, uIntensity;

vec3 fireColor(float t){
    t = clamp(t, 0.0, 1.0);
    return (t < 0.45)
        ? mix(vec3(0.08,0.00,0.00), vec3(1.00,0.32,0.04), t/0.45)
        : mix(vec3(1.00,0.32,0.04), vec3(1.00,0.92,0.45), (t-0.45)/0.55);
}

void main(){
    vec2 uv = vUV;

    // --- конус, сужающийся кверху ---
    float halfW_bottom = 0.46;
    float halfW_top    = 0.02;
    float halfW = mix(halfW_bottom, halfW_top, pow(uv.y, 1.15));

    float edge = uSoftEdge * 0.55;
    float dx = abs(uv.x - 0.5);

    // БЕЗ центральной чёрной линии:
    float maskTri = 1.0 - smoothstep(halfW - edge, halfW, dx);  

    // --- округлая макушка (полудиск чуть выше 1.0) ---
    float capAspect = 0.55;
    float capR      = 0.22;
    float capY      = 1.04;
    vec2  capP      = vec2((uv.x - 0.5)/capAspect, uv.y - capY);
    float capD      = length(capP);
    float maskCap   = smoothstep(capR - edge, capR, capD);

    // итоговая форма
    float mask = min(maskTri, maskCap);

    // анимация шума
    float wobble = sin(uv.y*12.0 + uTime*7.0)*0.01;
    float z = uTime * uSpeed;
    vec3 p = vec3((uv.x + wobble) * uScale, uv.y * uScale, z);
    float n = texture(uNoise, p).r;

    float baseBoost = smoothstep(0.0, 0.28, 1.0 - uv.y);
    float t = clamp(n*1.18 + baseBoost*0.32, 0.0, 1.0);

    vec3 col    = fireColor(t) * (uIntensity * (0.45 + 0.75*t));
    float alpha = mask * (0.28 + 0.72*t);

    FragColor = vec4(col * alpha, alpha);
}
)";



static const char* FRAG_SMOKE = R"(#version 330 core
out vec4 FragColor;
in vec2 vUV;
uniform sampler3D uNoise;
uniform float uTime;
uniform float uScale;
uniform float uSpeed;
uniform float uSoftEdge;
uniform float uOpacity;

void main(){
    vec2 uv = vUV;

    // лёгкая волна (оставляем как было)
    float wave = sin(uv.y * 8.0 + uTime * 0.8) * 0.1 +
                 sin(uv.y * 3.5 + uTime * 0.4) * 0.05;

    float dx = abs(uv.x - 0.5 - wave);
    float halfW = 0.35;
    float edge  = uSoftEdge * 0.8;

    // ✔ Правильная маска (без инвертированных краёв) — никакой чёрной линии по центру
    float mask = 1.0 - smoothstep(halfW - edge, halfW, dx);

    // движение шума
    float z = uTime * uSpeed;
    vec3 p = vec3(uv.x * uScale + wave, uv.y * uScale, z);
    float n = texture(uNoise, p).r;

    // ослабление и осветление кверху
    float fadeUp = smoothstep(0.0, 1.0, uv.y);
    float density = clamp(n * 1.2 - 0.25 + (1.0 - fadeUp) * 0.15, 0.0, 1.0);

    float a = mask * density * uOpacity;
    vec3  c = mix(vec3(0.2), vec3(0.55), fadeUp) * density;

    FragColor = vec4(c, a);
}
)";

// ---------- main ----------
int main() {
    check(glfwInit() != 0, "GLFW init failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(900, 1200, "Animated Fire & Smoke (Perlin 3D)", nullptr, nullptr);
    check(win != nullptr, "Window creation failed");
    glfwMakeContextCurrent(win);
    check(gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) != 0, "GLAD init failed");
    glfwSwapInterval(1);

    // resources
    GLuint tex3d = make3DNoiseTex(96, /*octaves*/5, /*lacunarity*/2.01f, /*gain*/0.52f, /*seed*/42);
    GLuint vao = makeUnitQuadVAO();
    GLuint progFire = makeProgram(VERT, FRAG_FIRE);
    GLuint progSmoke = makeProgram(VERT, FRAG_SMOKE);

    // state
    glEnable(GL_BLEND);

    float fireScale = 3.2f;
    float smokeScale = 2.2f;
    float fireSpeed = 0.75f;
    float smokeSpeed = 0.18f;
    float fireHeight = 0.55f;   // NDC-ish height
    float smokeHeight = 1.8f;
    float fireWidth = 0.52f;
    float smokeWidth = 0.9f;

    float fireIntensity = 2.0f;
    float smokeOpacity = 0.55f;

    auto t0 = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, 1);

        // quick controls
        if (glfwGetKey(win, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)  smokeScale = std::max(0.5f, smokeScale - 0.01f);
        if (glfwGetKey(win, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) smokeScale = std::min(6.0f, smokeScale + 0.01f);
        if (glfwGetKey(win, GLFW_KEY_MINUS) == GLFW_PRESS)         fireScale = std::max(0.8f, fireScale - 0.01f);
        if (glfwGetKey(win, GLFW_KEY_EQUAL) == GLFW_PRESS)         fireScale = std::min(6.0f, fireScale + 0.01f);
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS)             smokeSpeed = std::min(0.8f, smokeSpeed + 0.001f);
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS)             smokeSpeed = std::max(0.02f, smokeSpeed - 0.001f);
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS)             fireSpeed = std::min(2.0f, fireSpeed + 0.005f);
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS)             fireSpeed = std::max(0.05f, fireSpeed - 0.005f);
        if (glfwGetKey(win, GLFW_KEY_1) == GLFW_PRESS)             fireHeight = std::max(0.3f, fireHeight - 0.005f);
        if (glfwGetKey(win, GLFW_KEY_2) == GLFW_PRESS)             fireHeight = std::min(0.9f, fireHeight + 0.005f);

        int w, h; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        auto t1 = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float>(t1 - t0).count();

        float aspect = (h == 0) ? 1.0f : float(w) / float(h);

        glBindVertexArray(vao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, tex3d);

        // --- FIRE (draw first, additive) ---
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive glow
        glUseProgram(progFire);
        glUniform1i(glGetUniformLocation(progFire, "uNoise"), 0);
        glUniform1f(glGetUniformLocation(progFire, "uTime"), time);
        glUniform1f(glGetUniformLocation(progFire, "uScale"), fireScale);
        glUniform1f(glGetUniformLocation(progFire, "uSpeed"), fireSpeed);
        glUniform1f(glGetUniformLocation(progFire, "uSoftEdge"), 0.25f);
        glUniform1f(glGetUniformLocation(progFire, "uIntensity"), fireIntensity);
        glUniform1f(glGetUniformLocation(progFire, "uAspect"), aspect);
        glUniform1f(glGetUniformLocation(progFire, "uHeight"), fireHeight);
        glUniform1f(glGetUniformLocation(progFire, "uWidth"), fireWidth);
        glUniform2f(glGetUniformLocation(progFire, "uOffset"), 0.0f, 0.05f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // --- SMOKE (alpha blended over) ---
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(progSmoke);
        glUniform1i(glGetUniformLocation(progSmoke, "uNoise"), 0);
        glUniform1f(glGetUniformLocation(progSmoke, "uTime"), time);
        glUniform1f(glGetUniformLocation(progSmoke, "uScale"), smokeScale);
        glUniform1f(glGetUniformLocation(progSmoke, "uSpeed"), smokeSpeed);
        glUniform1f(glGetUniformLocation(progSmoke, "uSoftEdge"), 0.35f);
        glUniform1f(glGetUniformLocation(progSmoke, "uOpacity"), smokeOpacity);
        glUniform1f(glGetUniformLocation(progSmoke, "uAspect"), aspect);
        glUniform1f(glGetUniformLocation(progSmoke, "uHeight"), smokeHeight);
        glUniform1f(glGetUniformLocation(progSmoke, "uWidth"), smokeWidth);
        glUniform2f(glGetUniformLocation(progSmoke, "uOffset"), 0.0f, 0.05f);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(win);
    }

    glDeleteProgram(progFire);
    glDeleteProgram(progSmoke);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex3d);

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
