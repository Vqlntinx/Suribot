#include "voice_tracker.h"

#include <optional>
#include <stdexcept>
#include <utility>

namespace {

struct CompletedSession {
    dpp::snowflake user_id;
    dpp::snowflake channel_id;
    std::int64_t minutes;
    std::string reason;
};

std::int64_t elapsed_minutes(std::chrono::steady_clock::time_point started_at, std::chrono::steady_clock::time_point finished_at)
{
    return std::chrono::duration_cast<std::chrono::minutes>(finished_at - started_at).count();
}

std::string user_mention(dpp::snowflake user_id)
{
    return "<@" + std::to_string(user_id) + ">";
}

std::string channel_mention(dpp::snowflake channel_id)
{
    return "<#" + std::to_string(channel_id) + ">";
}

std::string filesystem_error_message(const std::filesystem::path& path, const std::error_code& error)
{
    return path.string() + ": " + error.message();
}

} // namespace

VoiceTracker::VoiceTracker(dpp::cluster& bot, dpp::snowflake voice_log_channel_id, std::filesystem::path data_path)
    : bot(bot), voiceLogChannelId(voice_log_channel_id), dataPath(std::move(data_path))
{
    loadUserData();
}

VoiceTracker::~VoiceTracker()
{
    {
        const auto now = Clock::now();
        std::lock_guard<std::mutex> lock(stateMutex);

        for (const auto& [user_id, active_session] : activeSessions) {
            userTotalMinutes[user_id] += elapsed_minutes(active_session.joined_at, now);
        }
        activeSessions.clear();
    }

    saveUserData();
}

void VoiceTracker::handle_voice_state_update(const dpp::voice_state_update_t& event)
{
    const dpp::snowflake user_id = event.state.user_id;
    const dpp::snowflake new_channel_id = event.state.channel_id;

    if (!user_id) {
        return;
    }

    std::optional<CompletedSession> completed_session;
    bool should_save = false;

    {
        const auto now = Clock::now();
        std::lock_guard<std::mutex> lock(stateMutex);

        auto active_session = activeSessions.find(user_id);
        if (new_channel_id) {
            if (active_session == activeSessions.end()) {
                activeSessions.emplace(user_id, VoiceSession{ new_channel_id, now });
                bot.log(dpp::ll_info, "User joined a voice channel: " + std::to_string(user_id));
                return;
            }

            if (active_session->second.channel_id == new_channel_id) {
                return;
            }

            completed_session = CompletedSession{
                user_id,
                active_session->second.channel_id,
                elapsed_minutes(active_session->second.joined_at, now),
                "채널 이동"
            };
            userTotalMinutes[user_id] += completed_session->minutes;
            active_session->second = VoiceSession{ new_channel_id, now };
            should_save = true;
        }
        else if (active_session != activeSessions.end()) {
            completed_session = CompletedSession{
                user_id,
                active_session->second.channel_id,
                elapsed_minutes(active_session->second.joined_at, now),
                "퇴장"
            };
            userTotalMinutes[user_id] += completed_session->minutes;
            activeSessions.erase(active_session);
            should_save = true;
        }
    }

    if (completed_session.has_value()) {
        logCompletedSession(
            completed_session->user_id,
            completed_session->channel_id,
            completed_session->minutes,
            completed_session->reason
        );
    }

    if (should_save) {
        saveUserData();
    }
}

void VoiceTracker::saveUserData()
{
    std::lock_guard<std::mutex> file_lock(fileMutex);
    nlohmann::json jsonData = nlohmann::json::object();

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (const auto& [user_id, total_minutes] : userTotalMinutes) {
            jsonData[std::to_string(user_id)] = total_minutes;
        }
    }

    std::error_code error;
    const auto parent_path = dataPath.parent_path();
    if (!parent_path.empty()) {
        std::filesystem::create_directories(parent_path, error);
        if (error) {
            bot.log(dpp::ll_error, "데이터 디렉터리 생성 실패: " + filesystem_error_message(parent_path, error));
            return;
        }
    }

    const auto temporary_path = dataPath.string() + ".tmp";
    std::ofstream file(temporary_path, std::ios::trunc);
    if (!file.is_open()) {
        bot.log(dpp::ll_error, "데이터 파일을 열 수 없습니다: " + std::filesystem::path(temporary_path).string());
        return;
    }

    file << jsonData.dump(4);
    file.close();

    if (!file) {
        bot.log(dpp::ll_error, "데이터 파일 저장 실패: " + std::filesystem::path(temporary_path).string());
        return;
    }

    std::filesystem::rename(temporary_path, dataPath, error);

    std::error_code exists_error;
    const bool target_exists = std::filesystem::exists(dataPath, exists_error);
    if (error && target_exists) {
        std::error_code remove_error;
        std::filesystem::remove(dataPath, remove_error);

        if (!remove_error) {
            error.clear();
            std::filesystem::rename(temporary_path, dataPath, error);
        }
    }

    if (error) {
        bot.log(dpp::ll_error, "데이터 파일 교체 실패: " + filesystem_error_message(dataPath, error));

        std::error_code remove_error;
        std::filesystem::remove(temporary_path, remove_error);
    }
}

void VoiceTracker::loadUserData()
{
    std::lock_guard<std::mutex> file_lock(fileMutex);
    std::ifstream file(dataPath);
    if (!file.is_open()) {
        return;
    }

    try {
        nlohmann::json jsonData;
        file >> jsonData;

        if (!jsonData.is_object()) {
            bot.log(dpp::ll_warning, "데이터 파일 형식이 올바르지 않습니다: " + dataPath.string());
            return;
        }

        std::unordered_map<dpp::snowflake, std::int64_t> loaded_totals;
        for (const auto& [key, value] : jsonData.items()) {
            if (!value.is_number_integer() && !value.is_number_unsigned()) {
                bot.log(dpp::ll_warning, "잘못된 업타임 값은 건너뜁니다: " + key);
                continue;
            }

            uint64_t user_id_value = 0;
            try {
                size_t parsed_length = 0;
                user_id_value = std::stoull(key, &parsed_length);
                if (parsed_length != key.size() || user_id_value == 0) {
                    throw std::invalid_argument("invalid user id");
                }
            }
            catch (const std::exception&) {
                bot.log(dpp::ll_warning, "잘못된 사용자 ID는 건너뜁니다: " + key);
                continue;
            }

            std::int64_t total_minutes = 0;
            try {
                total_minutes = value.get<std::int64_t>();
            }
            catch (const std::exception&) {
                bot.log(dpp::ll_warning, "범위를 벗어난 업타임 값은 건너뜁니다: " + key);
                continue;
            }

            if (total_minutes < 0) {
                bot.log(dpp::ll_warning, "음수 업타임 값은 건너뜁니다: " + key);
                continue;
            }

            loaded_totals[dpp::snowflake(user_id_value)] = total_minutes;
        }

        std::lock_guard<std::mutex> lock(stateMutex);
        userTotalMinutes = std::move(loaded_totals);
    }
    catch (const std::exception& exception) {
        bot.log(dpp::ll_error, "데이터 파일 로드 실패: " + std::string(exception.what()));
    }
}

void VoiceTracker::handleTopUsersCommand(const dpp::slashcommand_t& event)
{
    auto totals = getTotalTimesSnapshot();
    std::vector<std::pair<dpp::snowflake, std::int64_t>> userTimes(totals.begin(), totals.end());
    std::sort(userTimes.begin(), userTimes.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
        });

    if (userTimes.empty()) {
        event.reply("아직 업타임 기록이 없습니다.");
        return;
    }

    std::string message = "누적 업타임 상위 5명:\n";
    int rank = 1;
    for (const auto& [user_id, total_minutes] : userTimes) {
        message += "Rank " + std::to_string(rank) + ": " + user_mention(user_id) + " - " +
            std::to_string(total_minutes) + " 분\n";
        if (++rank > 5) {
            break;
        }
    }

    event.reply(message);
}

void VoiceTracker::getUserUptime(const dpp::slashcommand_t& event, dpp::snowflake user_id)
{
    if (user_id == 0) {
        event.reply("올바른 사용자 ID가 아닙니다. 다시 시도해주세요.");
        return;
    }

    const auto totals = getTotalTimesSnapshot();
    auto total_time = totals.find(user_id);
    if (total_time != totals.end()) {
        std::string message = user_mention(user_id) + "님의 총 업타임: " + std::to_string(total_time->second) + " 분";
        event.reply(message);
    }
    else {
        event.reply(user_mention(user_id) + "님의 업타임 기록이 없습니다.");
    }
}

void VoiceTracker::clearUserData()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        userTotalMinutes.clear();
        activeSessions.clear();
    }

    saveUserData();
}

std::unordered_map<dpp::snowflake, std::int64_t> VoiceTracker::getTotalTimesSnapshot() const
{
    const auto now = Clock::now();
    std::lock_guard<std::mutex> lock(stateMutex);

    auto totals = userTotalMinutes;
    for (const auto& [user_id, active_session] : activeSessions) {
        totals[user_id] += elapsed_minutes(active_session.joined_at, now);
    }

    return totals;
}

void VoiceTracker::logCompletedSession(dpp::snowflake user_id, dpp::snowflake channel_id, std::int64_t minutes, const std::string& reason)
{
    std::string message = "유저: " + user_mention(user_id) +
        "   채널: " + channel_mention(channel_id) +
        "   업타임: " + std::to_string(minutes) + "분" +
        "   사유: " + reason;

    dpp::cluster* bot_ptr = &bot;
    bot.message_create(dpp::message(voiceLogChannelId, message), [bot_ptr](const dpp::confirmation_callback_t& callback) {
        if (callback.is_error()) {
            bot_ptr->log(dpp::ll_error, "음성 로그 메시지 전송 실패: " + callback.get_error().message);
        }
        });
}
