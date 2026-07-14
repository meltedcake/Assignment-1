#pragma once
#include "Platform.h"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>

#ifdef PLATFORM_ANDROID
#include <android/asset_manager.h>
#endif

class ShaderHelper {
public:
    static GLuint compileShader(GLenum type, const char* src)
    {
        GLuint shader = glCreateShader(type);
        if (!shader) { LOGE("glCreateShader failed (type=0x%x)", type); return 0; }

        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
            std::string log(static_cast<size_t>(len), '\0');
            glGetShaderInfoLog(shader, len, nullptr, &log[0]);
            LOGE("shader compile error:\n%s", log.c_str());
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static GLuint linkProgram(GLuint vert, GLuint frag)
    {
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vert);
        glAttachShader(prog, frag);
        glLinkProgram(prog);

        GLint ok = GL_FALSE;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
            std::string log(static_cast<size_t>(len), '\0');
            glGetProgramInfoLog(prog, len, nullptr, &log[0]);
            LOGE("program link error:\n%s", log.c_str());
            glDeleteProgram(prog);
            prog = 0;
        }

        glDetachShader(prog, vert);
        glDetachShader(prog, frag);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return prog;
    }

    // method 1: stringify - build directly from inline source strings.
    static GLuint buildProgram(const char* vertSrc, const char* fragSrc)
    {
        GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
        if (!vs) return 0;
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!fs) { glDeleteShader(vs); return 0; }
        return linkProgram(vs, fs);
    }

#ifdef PLATFORM_ANDROID
    // method 2 (android): file-based - load shader source from apk assets.
    static std::string loadAsset(AAssetManager* mgr, const char* path)
    {
        if (!mgr) { LOGE("loadAsset: null AAssetManager"); return std::string(); }

        AAsset* asset = AAssetManager_open(mgr, path, AASSET_MODE_BUFFER);
        if (!asset) { LOGE("loadAsset: could not open %s", path); return std::string(); }

        off_t len = AAsset_getLength(asset);
        std::string data(static_cast<size_t>(len), '\0');
        AAsset_read(asset, &data[0], static_cast<size_t>(len));
        AAsset_close(asset);
        return data;
    }

    static GLuint buildProgramFromAssets(AAssetManager* mgr, const char* vertPath, const char* fragPath)
    {
        std::string vertSrc = loadAsset(mgr, vertPath);
        std::string fragSrc = loadAsset(mgr, fragPath);
        if (vertSrc.empty() || fragSrc.empty()) {
            LOGE("buildProgramFromAssets: failed to load %s / %s", vertPath, fragPath);
            return 0;
        }
        return buildProgram(vertSrc.c_str(), fragSrc.c_str());
    }
#else
    // method 2 (desktop/web): file-based - load shader source from disk.
    // tries assets/shader/<f>, then <f>, then shader/<f>, then assets/<f>.
    static std::string loadFile(const char* filename)
    {
        static const char* kSearchPaths[] = {
                "assets/shader/%s",
                "%s",
                "shader/%s",
                "assets/%s"
        };

        for (const char* pattern : kSearchPaths) {
            char path[512];
            snprintf(path, sizeof(path), pattern, filename);
            std::ifstream file(path);
            if (file.is_open()) {
                std::stringstream ss;
                ss << file.rdbuf();
                return ss.str();
            }
        }
        LOGE("loadFile: could not find %s in any search path", filename);
        return std::string();
    }

    static GLuint buildProgramFromFile(const char* vertFile, const char* fragFile)
    {
        std::string vertSrc = loadFile(vertFile);
        std::string fragSrc = loadFile(fragFile);
        if (vertSrc.empty() || fragSrc.empty()) {
            LOGE("buildProgramFromFile: failed to load %s / %s", vertFile, fragFile);
            return 0;
        }
        return buildProgram(vertSrc.c_str(), fragSrc.c_str());
    }
#endif
};