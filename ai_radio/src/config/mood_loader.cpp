#include "mood_loader.h"
#include "../../vendor/vjson/vjson.h"
#include "../util/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace config {

namespace {

float getNumber(const vjson::Object &obj, const std::string &key, float def = 0.0f) {
    auto val = obj.Find(key.c_str());
    if (!val) return def;
    double d;
    if (val->GetDouble(d)) return static_cast<float>(d);
    return def;
}

bool getBool(const vjson::Object &obj, const std::string &key, bool def = false) {
    auto val = obj.Find(key.c_str());
    if (!val) return def;
    bool b;
    if (val->GetBool(b)) return b;
    double d;
    if (val->GetDouble(d)) return d > 0.5;
    return def;
}

std::vector<float> getFloatArray(const vjson::Object &obj, const std::string &key) {
    std::vector<float> out;
    auto arrVal = obj.Find(key.c_str());
    if (!arrVal || !arrVal->IsArray()) return out;
    const auto &arr = arrVal->AsArray();
    for (size_t i = 0; i < arr.Count(); ++i) {
        double d;
        if (arr[i].GetDouble(d)) out.push_back(static_cast<float>(d));
    }
    return out;
}

std::vector<std::string> getStringArray(const vjson::Object &obj, const std::string &key) {
    std::vector<std::string> out;
    auto arrVal = obj.Find(key.c_str());
    if (!arrVal || !arrVal->IsArray()) return out;
    const auto &arr = arrVal->AsArray();
    for (size_t i = 0; i < arr.Count(); ++i) {
        const char *s;
        if (arr[i].GetString(s)) out.push_back(std::string(s));
    }
    return out;
}

brain::MoodRecipe parseMood(const vjson::Object &obj) {
    brain::MoodRecipe mood;
    const char *s;
    if (obj.Find("id") && obj.Find("id")->GetString(s)) mood.id = s;
    if (obj.Find("display_name") && obj.Find("display_name")->GetString(s)) mood.displayName = s;
    mood.energy = std::clamp(getNumber(obj, "energy", 0.5f), 0.0f, 1.0f);
    mood.tension = std::clamp(getNumber(obj, "tension", 0.3f), 0.0f, 1.0f);
    mood.warmth = std::clamp(getNumber(obj, "warmth", 0.5f), 0.0f, 1.0f);
    mood.color = std::clamp(getNumber(obj, "color", 0.5f), 0.0f, 1.0f);
    mood.narrativeFrequency = std::clamp(getNumber(obj, "narrative_frequency", 0.05f), 0.0f, 1.0f);
    mood.densityCurve = getFloatArray(obj, "density_curve");
    mood.allowedTransitions = getStringArray(obj, "allowed_transitions");

    // stems
    auto stemsVal = obj.Find("stems");
    if (stemsVal && stemsVal->IsArray()) {
        const auto &arr = stemsVal->AsArray();
        for (size_t i = 0; i < arr.Count(); ++i) {
            if (!arr[i].IsObject()) continue;
            const auto &stObj = arr[i].AsObject();
            brain::StemConfig stem;
            if (stObj.Find("file") && stObj.Find("file")->GetString(s)) stem.file = s;
            if (stObj.Find("role") && stObj.Find("role")->GetString(s)) stem.role = s;
            stem.gainDb = getNumber(stObj, "gain_db", 0.0f);
            stem.loop = getBool(stObj, "loop", true);
            stem.probability = getNumber(stObj, "probability", 1.0f);
            mood.stems.push_back(stem);
        }
    }

    // synth
    auto synthVal = obj.Find("synth");
    if (synthVal && synthVal->IsObject()) {
        const auto &sv = synthVal->AsObject();
        if (sv.Find("preset") && sv.Find("preset")->GetString(s)) mood.synth.presetFile = s;
        mood.synth.seed = static_cast<int>(getNumber(sv, "seed", 0));
        mood.synth.patternDensity = getNumber(sv, "pattern_density", 0.4f);
    }

    return mood;
}

} // namespace

brain::MoodPack MoodLoader::loadFromFile(const std::string &path, bool &ok) {
    ok = false;
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) {
        util::logWarn("Mood config not found: " + path + " (using defaults)");
        return brain::defaultMoodPack();
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string data = ss.str();

    vjson::Object root;
    if (!root.ParseJSON(data.c_str(), data.c_str() + data.size())) {
        return brain::defaultMoodPack();
    }

    brain::MoodPack pack;
    auto moodsVal = root.Find("moods");
    if (!moodsVal || !moodsVal->IsArray()) {
        util::logWarn("Mood config missing 'moods' array, using defaults");
        return brain::defaultMoodPack();
    }
    const auto &arr = moodsVal->AsArray();
    for (size_t i = 0; i < arr.Count(); ++i) {
        if (!arr[i].IsObject()) continue;
        auto mood = parseMood(arr[i].AsObject());
        if (mood.id.empty() || mood.displayName.empty()) {
            util::logWarn("Skipping mood entry missing id or display_name");
            continue;
        }
        pack.moods.push_back(std::move(mood));
    }
    if (pack.moods.empty()) {
        util::logWarn("No valid moods found, using defaults");
        return brain::defaultMoodPack();
    }
    ok = true;
    return pack;
}

} // namespace config
