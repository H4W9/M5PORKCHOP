#pragma once
// Drop-in replacement for the subset of Arduino `Preferences` (NVS) used by
// Porkchop, backed by SPIFFS instead of NVS. One file per namespace at
// "/<namespace>.prf", stored as simple "key=value\n" text (values are the raw
// 32-bit bit-pattern in unsigned decimal, so signed/bool round-trip exactly).
//
// Usage matches Preferences: begin(ns, readOnly) -> get*/put* -> end().
// Only the scalar types actually used are implemented.

#include <Arduino.h>
#include <SPIFFS.h>
#include <map>
#include <string>

class SpiffsPrefs {
public:
    bool begin(const char* name, bool readOnly = false) {
        _path = String("/") + name + ".prf";
        _readOnly = readOnly;
        _dirty = false;
        _kv.clear();
        // Idempotent: no-op if SPIFFS is already mounted (Config::init mounts it).
        SPIFFS.begin(false);
        _load();
        return true;
    }

    void end() {
        if (!_readOnly && _dirty) _save();
        _kv.clear();
    }

    bool     getBool  (const char* k, bool     d = false) { return _get(k, d ? 1u : 0u) != 0; }
    int8_t   getChar  (const char* k, int8_t   d = 0)     { return (int8_t)  _get(k, (uint32_t)(uint8_t)d); }
    uint8_t  getUChar (const char* k, uint8_t  d = 0)     { return (uint8_t) _get(k, d); }
    uint16_t getUShort(const char* k, uint16_t d = 0)     { return (uint16_t)_get(k, d); }
    uint32_t getUInt  (const char* k, uint32_t d = 0)     { return          _get(k, d); }
    uint32_t getULong (const char* k, uint32_t d = 0)     { return          _get(k, d); }

    size_t putBool  (const char* k, bool     v) { return _put(k, v ? 1u : 0u); }
    size_t putChar  (const char* k, int8_t   v) { return _put(k, (uint32_t)(uint8_t)v); }
    size_t putUChar (const char* k, uint8_t  v) { return _put(k, v); }
    size_t putUShort(const char* k, uint16_t v) { return _put(k, v); }
    size_t putUInt  (const char* k, uint32_t v) { return _put(k, v); }
    size_t putULong (const char* k, uint32_t v) { return _put(k, v); }

private:
    String _path;
    bool   _readOnly = false;
    bool   _dirty = false;
    std::map<std::string, uint32_t> _kv;

    uint32_t _get(const char* k, uint32_t def) {
        auto it = _kv.find(k);
        return it != _kv.end() ? it->second : def;
    }

    size_t _put(const char* k, uint32_t v) {
        _kv[k] = v;
        _dirty = true;
        return sizeof(uint32_t);
    }

    void _load() {
        File f = SPIFFS.open(_path, "r");
        if (!f) return;
        while (f.available()) {
            String line = f.readStringUntil('\n');
            int eq = line.indexOf('=');
            if (eq <= 0) continue;
            std::string key(line.c_str(), eq);
            uint32_t val = (uint32_t)strtoul(line.c_str() + eq + 1, nullptr, 10);
            _kv[key] = val;
        }
        f.close();
    }

    void _save() {
        File f = SPIFFS.open(_path, "w");
        if (!f) return;
        for (const auto& kv : _kv) {
            f.print(kv.first.c_str());
            f.print('=');
            f.print(kv.second);
            f.print('\n');
        }
        f.close();
    }
};
