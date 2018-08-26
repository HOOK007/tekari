#pragma once

#include <enoki/matrix.h>

#if defined(_WIN32)
#    if defined(APIENTRY)
#        undef APIENTRY
#    endif
#    define NOMINMAX  // Remove min/max macros when building on windows
#    include <windows.h>
#    undef NOMINMAX
#    undef near      // also cleanup some macros conflicting with variable names
#    undef far
#    pragma warning(disable : 4127) // warning C4127: conditional expression is constant
#    pragma warning(disable : 4244) // warning C4244: conversion from X to Y, possible loss of data
#    pragma warning(disable : 4251) // warning C4251: class X needs to have dll-interface to be used by clients of class Y
#    pragma warning(disable : 4714) // warning C4714: function X marked as __forceinline not inlined
#endif

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>
#include <functional>
#include <memory>
#include <chrono>
#include <iomanip>
#include <cstdarg>
#include <nanogui/glutil.h>
#include <array>
#include <tekari/matrix_xx.h>

// (re)define M_PI locally since it's not necessarily defined on some platforms
#undef M_PI
#define M_PI 3.141592653589793238463
#define TO_DEG(rad) ((rad) * 57.295779513)
#define TO_RAD(deg) ((deg) * 0.017453293)

#define ENABLE_PROFILING

#define TEKARI_NAMESPACE_BEGIN namespace tekari {
#define TEKARI_NAMESPACE_END }

#define GRAIN_SIZE 1024u

#if defined(EMSCRIPTEN)
    #define VERTEX_SHADER_STR(name) std::string((char*)webgl_##name##_vert, webgl_##name##_vert_size)
    #define FRAGMENT_SHADER_STR(name) std::string((char*)webgl_##name##_frag, webgl_##name##_frag_size)
    #define DATA_SAMPLES_PATH "datasets/"
#else
    #define DATA_SAMPLES_PATH "../resources/"
    #define VERTEX_SHADER_STR(name) std::string((char*)opengl_##name##_vert, opengl_##name##_vert_size)
    #define FRAGMENT_SHADER_STR(name) std::string((char*)opengl_##name##_frag, opengl_##name##_frag_size)
#endif

TEKARI_NAMESPACE_BEGIN

// ============== Extract useful types from namespaces ===========

// common types from std
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::vector;
using std::shared_ptr;
using std::pair;
using std::function;

// useful std functions
using std::make_pair;
using std::make_shared;
using std::to_string;

// usefull types from nanogui
using nanogui::Color;
using nanogui::Vector2f;
using nanogui::Vector3f;
using nanogui::Vector4f;
using nanogui::Vector2i;
using nanogui::Matrix4f;
using nanogui::Quaternion4f;

// custom types
using Matrix3f = enoki::Matrix<float, 3>;
using Matrix2Xf = vector<Vector2f>;
using Matrix4XXf = MatrixXX<Vector4f>;
using MatrixXXf = MatrixXX<float>;
using Matrix3Xi = MatrixXX<int>;
using VectorXu  = vector<uint32_t>;
using VectorXi8 = vector<int8_t>;
using VectorXf  = vector<float>;
using Mask = vector<bool>;

// ============= Log Utils =============

enum LogType
{
    Info, Warning, Error
};

#define Log(type, fmt, ...)                                                 \
    do {                                                                    \
        switch(type)                                                        \
        {                                                                   \
            case LogType::Info:                                             \
                fprintf(stdout, "\033[32;1m[Info:%s:%s:%d]\033[0m : ",      \
                    __FILE__, __func__, __LINE__);                          \
                fprintf(stdout, fmt, __VA_ARGS__);                          \
                break;                                                      \
            case LogType::Warning:                                          \
                fprintf(stderr, "\033[33;1m[Warning:%s:%s:%d]\033[0m : ",   \
                    __FILE__, __func__, __LINE__);                          \
                fprintf(stderr, fmt, __VA_ARGS__);                          \
                break;                                                      \
            case LogType::Error:                                            \
                fprintf(stderr, "\033[31;1m[Error:%s:%s:%d]\033[0m : ",     \
                    __FILE__, __func__, __LINE__);                          \
                fprintf(stderr, fmt, __VA_ARGS__);                          \
                break;                                                      \
        }                                                                   \
    } while(0)

// ============= 3D Utils =============

inline Vector4f project_on_screen(const Vector3f& point,
    const Vector2i& canvas_size,
    const Matrix4f& mvp)
{
    Vector4f projected_point( mvp * concat(point, 1.0f) );

    projected_point /= projected_point[3];
    projected_point[0] = (projected_point[0] + 1.0f) * 0.5f * canvas_size.x();
    projected_point[1] = canvas_size.y() - (projected_point[1] + 1.0f) * 0.5f * canvas_size.y();
    return projected_point;
}

inline Vector3f get_3d_point(const Matrix2Xf& V2D, const MatrixXXf::Row& H, size_t index)
{
    return concat(V2D[index], H[index]);
}

template<typename Vector2>
inline Vector2 hemisphere_to_disk(const Vector2& i)
{
    float rad_v = TO_RAD(i[1]);
    return Vector2( i[0] * cos(rad_v) / 90.0f,
                    i[0] * sin(rad_v) / 90.0f );
}

template<typename Vector3, typename Vector2>
inline Vector3 hemisphere_to_vec3(const Vector2& i)
{

    float   rad_u = TO_RAD(i[0]),
            rad_v = TO_RAD(i[1]),
            sin_u = sin(rad_u);
    return Vector3(
            sin_u * sin(rad_v),
            sin_u * cos(rad_v),
            cos(rad_u)
        );
}

template<typename Vector2, typename Vector3>
inline Vector2 vec3_to_hemisphere(const Vector3& wi)
{
    return Vector2(
            TO_DEG(acos(wi[2])),
            TO_DEG(atan2(wi[1], wi[0]))
        );
}

template<typename Vector2, typename Vector3>
inline Vector2 vec3_to_disk(const Vector3& wi)
{
    float   norm_v = sqrt(wi[0]*wi[0] + wi[1]*wi[1] + wi[2]*wi[2]),
            acos_v = TO_DEG(acos(wi[2] / norm_v)),
            atan_v = atan2(wi[0], wi[1]);
    return Vector2( acos_v * cos(atan_v) / 90.0f,
                    acos_v * sin(atan_v) / 90.0f );
}

// ================= Timer Utils ================

template <typename TimeT = std::chrono::microseconds> class Timer {
public:
    Timer() {
        start = std::chrono::system_clock::now();
    }

    size_t value() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<TimeT>(now - start);
        return (size_t) duration.count();
    }

    size_t reset() {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<TimeT>(now - start);
        start = now;
        return (size_t) duration.count();
    }
private:
    std::chrono::system_clock::time_point start;
};

inline std::string time_string(double time, bool precise = false) {
    if (std::isnan(time) || std::isinf(time))
        return "inf";

    string suffix = "μs";
    if (time > 1000) {
        time /= 1000; suffix = "ms";
        if (time > 1000) {
            time /= 1000; suffix = "s";
            if (time > 60) {
                time /= 60; suffix = "m";
                if (time > 60) {
                    time /= 60; suffix = "h";
                    if (time > 12) {
                        time /= 12; suffix = "d";
                    }
                }
            }
        }
    }

    std::ostringstream os;
    os << std::setprecision(precise ? 4 : 1)
       << std::fixed << time << suffix;

    return os.str();
}

inline std::string mem_string(size_t size, bool precise = false) {
    double value = (double) size;
    const char *suffixes[] = {
        "B", "KiB", "MiB", "GiB", "TiB", "PiB"
    };
    int suffix = 0;
    while (suffix < 5 && value > 1024.0f) {
        value /= 1024.0f; ++suffix;
    }

    std::ostringstream os;
    os << std::setprecision(suffix == 0 ? 0 : (precise ? 4 : 1))
       << std::fixed << value << " " << suffixes[suffix];

    return os.str();
}

// ================= String Utils ================

// trim from start (in place)
inline void ltrim(string &s, function<int(int)> pred = [](int c) { return std::isspace(c); }) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [pred](int ch) {
        return !pred(ch);
    }));
}

// trim from end (in place)
inline void rtrim(string &s, function<int(int)> pred = [](int c) { return std::isspace(c); }) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [pred](int ch) {
        return !pred(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
inline void trim(string &s, function<int(int)> pred = [](int c) { return std::isspace(c); }) {
    ltrim(s, pred);
    rtrim(s, pred);
}

// trim from start (copying)
inline string ltrim_copy(string s, function<int(int)> pred = [](int c) { return std::isspace(c); }) {
    ltrim(s, pred);
    return s;
}

// trim from end (copying)
inline string rtrim_copy(string s, function<int(int)> pred = [](int c) { return std::isspace(c); }) {
    rtrim(s, pred);
    return s;
}

// trim from both ends (copying)
inline string trim_copy(string s, function<int(int)> pred = [](int c) { return std::isspace(c); }) {
    trim(s, pred);
    return s;
}

// ================= Math Utils ================

// (from instant-meshes)
inline float fast_acos(float x) {
    float negate = float(x < 0.0f);
    x = std::abs(x);
    float ret = -0.0187293f;
    ret *= x; ret = ret + 0.0742610f;
    ret *= x; ret = ret - 0.2121144f;
    ret *= x; ret = ret + 1.5707288f;
    ret = ret * std::sqrt(1.0f - x);
    ret = ret - 2.0f * negate * ret;
    return negate * (float) M_PI + ret;
}

TEKARI_NAMESPACE_END