#include "mood_loader.h"
#include "../../vendor/vjson/vjson.h"
#include "../util/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace config {

namespace {

float getNumber(const vjson::Value &obj, const std::string &key, float def = 0.0f) {
    if (!obj.has(key)) return def;
    return obj[key].asFloat(def);
}

bool getBool(const vjson::Value &obj, const std::string &key, bool def = false) {
    if (!obj.has(key)) return def;
    return obj[key].asBool(def);
}

std::vector<float> getFloatArray(const vjson::Value &obj, const std::string &key) {
    std::vector<float> out;
    if (!obj.has(key) || !obj[key].isArray()) return out;
    const auto &arr = obj[key].asArray();
    for (const auto& item : arr) {
        out.push_back(item.asFloat(0.0f));
    }
    return out;
}

std::vector<std::string> getStringArray(const vjson::Value &obj, const std::string &key) {
    std::vector<std::string> out;
    if (!obj.has(key) || !obj[key].isArray()) return out;
    const auto &arr = obj[key].asArray();
    for (const auto& item : arr) {
        if (item.isString()) {
            out.push_back(item.asString());
        }
    }
    return out;
}

brain::MoodRecipe parseMood(const vjson::Value &obj) {
    brain::MoodRecipe mood;
    
    mood.id = obj["id"].asString("");
    mood.displayName = obj["display_name"].asString("");
    mood.energy = std::clamp(getNumber(obj, "energy", 0.5f), 0.0f, 1.0f);
    mood.tension = std::clamp(getNumber(obj, "tension", 0.3f), 0.0f, 1.0f);
    mood.warmth = std::clamp(getNumber(obj, "warmth", 0.5f), 0.0f, 1.0f);
    mood.color = std::clamp(getNumber(obj, "color", 0.5f), 0.0f, 1.0f);
    mood.narrativeFrequency = std::clamp(getNumber(obj, "narrative_frequency", 0.05f), 0.0f, 1.0f);
    mood.densityCurve = getFloatArray(obj, "density_curve");
    mood.allowedTransitions = getStringArray(obj, "allowed_transitions");

    // stems
    if (obj.has("stems") && obj["stems"].isArray()) {
        const auto &arr = obj["stems"].asArray();
        for (const auto& stemVal : arr) {
            if (!stemVal.isObject()) continue;
            brain::StemConfig stem;
            stem.file = stemVal["file"].asString("");
            stem.role = stemVal["role"].asString("");
            stem.gainDb = stemVal["gain_db"].asFloat(0.0f);
            stem.loop = stemVal.has("loop") ? stemVal["loop"].asBool(true) : true;
            stem.probability = stemVal["probability"].asFloat(1.0f);
            mood.stems.push_back(stem);
        }
    }

    // synth
    if (obj.has("synth") && obj["synth"].isObject()) {
        const auto &sv = obj["synth"];
        mood.synth.presetFile = sv["preset"].asString("");
        mood.synth.seed = sv["seed"].asInt(0);
        mood.synth.patternDensity = sv["pattern_density"].asFloat(0.4f);
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

    auto parseResult = vjson::parse(data);
    if (!parseResult.has_value()) {
        util::logWarn("Failed to parse mood config JSON, using defaults");
        return brain::defaultMoodPack();
    }

    const vjson::Value& root = *parseResult;
    if (!root.isObject()) {
        util::logWarn("Mood config root is not an object, using defaults");
        return brain::defaultMoodPack();
    }

    brain::MoodPack pack;
    if (!root.has("moods") || !root["moods"].isArray()) {
        util::logWarn("Mood config missing 'moods' array, using defaults");
        return brain::defaultMoodPack();
    }
    
    const auto &arr = root["moods"].asArray();
    for (const auto& moodVal : arr) {
        if (!moodVal.isObject()) continue;
        auto mood = parseMood(moodVal);
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
