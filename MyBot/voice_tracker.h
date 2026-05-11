#pragma once
#ifndef VOICE_TRACKER_H
#define VOICE_TRACKER_H
#include <vector>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <dpp/dpp.h>
#include <unordered_map>
#include <chrono>
#include <fstream>
#include <mutex>
#include <dpp/nlohmann/json.hpp>
#include <string>

class VoiceTracker {
public:
    VoiceTracker(dpp::cluster& bot, dpp::snowflake voice_log_channel_id, std::filesystem::path data_path);
    ~VoiceTracker();
    void handle_voice_state_update(const dpp::voice_state_update_t& event);
    void saveUserData();
    void loadUserData();
    void handleTopUsersCommand(const dpp::slashcommand_t& event);
    void getUserUptime(const dpp::slashcommand_t& event, dpp::snowflake user_id);
    void clearUserData();

private:
    using Clock = std::chrono::steady_clock;

    struct VoiceSession {
        dpp::snowflake channel_id;
        Clock::time_point joined_at;
    };

    std::unordered_map<dpp::snowflake, std::int64_t> getTotalTimesSnapshot() const;
    void logCompletedSession(dpp::snowflake user_id, dpp::snowflake channel_id, std::int64_t minutes, const std::string& reason);

    dpp::cluster& bot;
    dpp::snowflake voiceLogChannelId;
    std::filesystem::path dataPath;
    mutable std::mutex stateMutex;
    mutable std::mutex fileMutex;
    std::unordered_map<dpp::snowflake, VoiceSession> activeSessions;
    std::unordered_map<dpp::snowflake, std::int64_t> userTotalMinutes;
};

#endif // VOICE_TRACKER_H
