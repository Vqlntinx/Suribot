#include "voice_tracker.h"
#include <fstream>
#include <DPP/nlohmann/json.hpp>  // JSON 라이브러리 필요

const dpp::snowflake VOICE_LOG_CHANNEL_ID = 1303203144377040917;

VoiceTracker::VoiceTracker(dpp::cluster& bot) : bot(bot)
{
	loadUserData();
}

VoiceTracker::~VoiceTracker()
{
    saveUserData();
}
void VoiceTracker::handle_voice_state_update(const dpp::voice_state_update_t& event) {
    dpp::snowflake user_id = event.state.user_id;

    if (event.state.channel_id && userJoinTimes.find(user_id) == userJoinTimes.end()) {
        userJoinTimes[user_id] = std::chrono::system_clock::now();
        bot.log(dpp::ll_info, "User joined the voice channel.");
    }

    else if (!event.state.channel_id && userJoinTimes.find(user_id) != userJoinTimes.end()) {
        auto joinTime = userJoinTimes[user_id];
        auto leaveTime = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::minutes>(leaveTime - joinTime);

        userTotalTimes[user_id] += duration.count();

        std::string message = "<@" + std::to_string(user_id) + "> spent " +
            std::to_string(duration.count()) + " minutes in the <#" +
            std::to_string(event.state.channel_id) + "> channel.";
        bot.message_create(dpp::message(VOICE_LOG_CHANNEL_ID, message));

        userJoinTimes.erase(user_id);
        bot.log(dpp::ll_info, "User left the voice channel.");
    }
}

void VoiceTracker::saveUserData() {
    nlohmann::json jsonData;
    for (const auto& [user_id, totalTime] : userTotalTimes) {
        jsonData[std::to_string(user_id)] = totalTime;
    }
    std::ofstream file("user_data.json");
    file << jsonData.dump(4);
}

void VoiceTracker::loadUserData() {
    std::ifstream file("user_data.json");
    if (file.is_open()) {
        nlohmann::json jsonData;
        file >> jsonData;
        for (auto& [key, value] : jsonData.items()) {
            userTotalTimes[std::stoull(key)] = value;
        }
    }
}