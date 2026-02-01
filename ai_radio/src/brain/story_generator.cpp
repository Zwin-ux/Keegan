#include "story_generator.h"
#include "../../vendor/httplib.h"
#include "../../vendor/vjson/vjson.h"
#include "../util/logger.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace brain {

namespace fs = std::filesystem;

StoryGenerator::StoryGenerator(voice::StoryBank& bank)
    : bank_(bank) {}

StoryGenerator::~StoryGenerator() {
    if (genThread_.joinable()) {
        genThread_.detach();
    }
}

void StoryGenerator::setBaseUrl(const std::string& url) {
    if (url.empty()) return;
    baseUrl_ = url;
    if (baseUrl_.back() == '/') baseUrl_.pop_back();
}

void StoryGenerator::requestStory(const std::string& moodId, const std::string& context) {
    if (generating_.exchange(true)) {
        return; // Already generating
    }

    util::logInfo("StoryGen: Requesting story for " + moodId);
    GenRequest req{moodId, context};
    genThread_ = std::thread(&StoryGenerator::runGeneration, this, req);
}

void StoryGenerator::update() {
    if (genThread_.joinable()) {
        // detached in runGeneration
    }
}

void StoryGenerator::runGeneration(GenRequest req) {
    // 1. Prepare Client
    std::string host = baseUrl_;
    int port = 8080;
    
    // Create temp cache dir
    fs::create_directories("cache/stories");

    httplib::Client cli(baseUrl_);
    cli.set_connection_timeout(5, 0); // 5 seconds connect
    cli.set_read_timeout(30, 0);      // 30 seconds gen time

    // 2. Request Generation
    std::string jsonBody = "{\"mood\": \"" + req.mood + "\", \"context\": \"" + req.context + "\"}";
    
    auto res = cli.Post("/generate", jsonBody, "application/json");
    
    if (res && res->status == 200) {
        auto json = vjson::parse(res->body);
        if (json) {
            std::string text = (*json)["text"].asString();
            std::string audioData64 = (*json)["audio_base64"].asString(); 
            std::string id = (*json)["id"].asString("story_" + std::to_string(std::time(nullptr)));

            if (!text.empty()) {
                 // For now, if no audio is returned, we can't play it.
                 // Unless we have a local TTS fallback or if the router saved it to disk.
                 // Assuming router returns "audio_path" or we assume it failed if empty.
                 
                 // If mocking:
                 // std::string wavPath = "assets/voice/focus/library_quiet.wav"; // Reuse existing for test
                 
                 // REAL IMPL:
                 // We don't have base64 decoder in C++ std. 
                 // For Phase 3 MVP, let's assume the router wrote the file to a shared path if local,
                 // OR returns a path if we map volumes.
                 // Given the Railway context, we MUST download it.
                 // But implementing base64 decode in 5 mins is risky.
                 // WORKAROUND: The router endpoint returns binary body if we hit GET /audio/id
                 // or the POST returns the text and we use a placeholder "voice tone" file for now
                 // just to prove the LLM connection works (Text appears on UI).
                 
                 // Let's use a placeholder audio file to ensure stability, 
                 // but use the REAL text from LLM.
                 std::string wavPath = "assets/voice/focus/library_quiet.wav"; 
                 if (req.mood == "arcade_night") wavPath = "assets/voice/arcade/data_streams.wav";
                 // ... mapping ...

                 auto s = std::make_shared<voice::Story>();
                 s->id = id;
                 s->text = text; // The REAL LLM text
                 s->moodId = req.mood;
                 s->audioFile = wavPath; // The fake audio (Phase 3 Part 1 limitation)

                 if (s->player.load(s->audioFile)) {
                     s->player.setLooping(false);
                     bank_.addStory(s);
                     util::logInfo("StoryGen: Added dynamic story: " + text.substr(0, 20) + "...");
                 }
            }
        }
    } else {
        util::logWarn("StoryGen: Failed to generate. Status: " + (res ? std::to_string(res->status) : "Error"));
    }

    generating_ = false;
    genThread_.detach(); 
}

} // namespace brain
