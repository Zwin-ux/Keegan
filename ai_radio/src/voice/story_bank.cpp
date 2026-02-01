#include "story_bank.h"
#include "../../vendor/vjson/vjson.h"
#include "../util/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace voice {

StoryBank::StoryBank() {
    std::random_device rd;
    rng_ = std::mt19937(rd());
}

bool StoryBank::loadFromFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    stories_.clear();

    std::ifstream f(path);
    if (!f.good()) {
        util::logWarn("StoryBank: Config not found: " + path);
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::string data = ss.str();

    auto result = vjson::parse(data);
    if (!result || !result->isArray()) {
        util::logError("StoryBank: Invalid JSON in " + path);
        return false;
    }

    const auto& arr = result->asArray();
    for (const auto& val : arr) {
        if (!val.isObject()) continue;

        auto s = std::make_shared<Story>();
        s->id = val["id"].asString();
        s->text = val["text"].asString();
        s->audioFile = val["audio_file"].asString();
        s->moodId = val["mood"].asString("any");
        
        if (!s->text.empty() && !s->audioFile.empty()) {
            if (s->player.load(s->audioFile)) {
                s->player.setLooping(false); 
                stories_.push_back(s);
            } else {
                util::logWarn("StoryBank: Failed to load audio for story " + s->id);
            }
        }
    }

    util::logInfo("StoryBank: Loaded " + std::to_string(stories_.size()) + " stories");
    return true;
}

std::shared_ptr<Story> StoryBank::pickStory(const std::string& currentMoodId, float currentTime, float globalCooldown) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<Story>> candidates;

    for (auto& s : stories_) {
        // Check mood
        if (s->moodId != "any" && s->moodId != currentMoodId) continue;
        
        // Check cooldown
        if (currentTime - s->lastPlayedTime < globalCooldown) continue;

        candidates.push_back(s);
    }

    if (candidates.empty()) return nullptr;

    // Pick random
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    return candidates[dist(rng_)];
}

void StoryBank::markPlayed(std::shared_ptr<Story> story, float currentTime) {
    if (story) {
        // No lock needed for atomic-like float write, but strictly speaking we should if we care about partial writes (unlikely for float on x64)
        // But the story object stays alive via shared_ptr.
        story->lastPlayedTime = currentTime;
    }
}

void StoryBank::addStory(std::shared_ptr<Story> story) {
    std::lock_guard<std::mutex> lock(mutex_);
    stories_.push_back(story);
    util::logInfo("StoryBank: Added new story: " + story->id);
}

size_t StoryBank::countForMood(const std::string& moodId) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& s : stories_) {
        if (s->moodId == "any" || s->moodId == moodId) {
            count++;
        }
    }
    return count;
}

} // namespace voice
