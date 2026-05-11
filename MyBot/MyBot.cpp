#include "MyBot.h"
#include "voice_tracker.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace {

constexpr const char* BOT_TOKEN_ENV = "SURIBOT_TOKEN";
constexpr const char* VOICE_LOG_CHANNEL_ENV = "SURIBOT_VOICE_LOG_CHANNEL_ID";
constexpr const char* DATA_PATH_ENV = "SURIBOT_DATA_PATH";

struct BotConfig {
    std::string bot_token;
    dpp::snowflake voice_log_channel_id;
    std::filesystem::path data_path;
};

std::optional<std::string> read_environment_variable(const char* name)
{
#ifdef _WIN32
    char* value = nullptr;
    size_t value_size = 0;
    if (_dupenv_s(&value, &value_size, name) != 0 || value == nullptr) {
        return std::nullopt;
    }

    std::string result(value);
    std::free(value);
#else
    const char* value = std::getenv(name);
    if (value == nullptr) {
        return std::nullopt;
    }

    std::string result(value);
#endif

    if (result.empty()) {
        return std::nullopt;
    }

    return result;
}

std::string read_required_environment_variable(const char* name)
{
    auto value = read_environment_variable(name);
    if (!value.has_value()) {
        throw std::runtime_error(std::string(name) + " 환경변수를 설정해야 합니다.");
    }

    return *value;
}

dpp::snowflake parse_snowflake(const std::string& value, const char* environment_variable_name)
{
    try {
        size_t parsed_length = 0;
        const auto parsed_value = std::stoull(value, &parsed_length);
        if (parsed_length != value.size() || parsed_value == 0) {
            throw std::invalid_argument("invalid snowflake");
        }

        return dpp::snowflake(parsed_value);
    }
    catch (const std::exception&) {
        throw std::runtime_error(std::string(environment_variable_name) + " 값은 Discord snowflake 숫자여야 합니다.");
    }
}

std::filesystem::path make_absolute_path(const std::filesystem::path& path)
{
    std::error_code error;
    auto absolute_path = std::filesystem::absolute(path, error);
    if (error) {
        return path;
    }

    return absolute_path;
}

std::filesystem::path get_default_data_path(int argc, char* argv[])
{
    std::filesystem::path base_path = std::filesystem::current_path();

    if (argc > 0 && argv[0] != nullptr && argv[0][0] != '\0') {
        std::error_code error;
        auto executable_path = std::filesystem::absolute(argv[0], error);
        if (!error && executable_path.has_parent_path()) {
            base_path = executable_path.parent_path();
        }
    }

    return base_path / "user_data.json";
}

BotConfig load_config(int argc, char* argv[])
{
    BotConfig config;
    config.bot_token = read_required_environment_variable(BOT_TOKEN_ENV);
    config.voice_log_channel_id = parse_snowflake(
        read_required_environment_variable(VOICE_LOG_CHANNEL_ENV),
        VOICE_LOG_CHANNEL_ENV
    );

    auto configured_data_path = read_environment_variable(DATA_PATH_ENV);
    config.data_path = make_absolute_path(
        configured_data_path.has_value()
            ? std::filesystem::path(*configured_data_path)
            : get_default_data_path(argc, argv)
    );

    return config;
}

} // namespace

int main(int argc, char* argv[])
{
    try {
        const BotConfig config = load_config(argc, argv);

        dpp::cluster bot(config.bot_token);

        bot.on_log(dpp::utility::cout_logger());
        bot.log(dpp::ll_info, "User data path: " + config.data_path.string());

        VoiceTracker voice_tracker(bot, config.voice_log_channel_id, config.data_path);

        bot.on_voice_state_update([&voice_tracker](const dpp::voice_state_update_t& event) {
            voice_tracker.handle_voice_state_update(event);
            });

        bot.on_slashcommand([&voice_tracker](const dpp::slashcommand_t& event) {
            std::string command_name = event.command.get_command_name();

            if (command_name == "순위") {
                voice_tracker.handleTopUsersCommand(event);
            }
            else if (command_name == "업타임") {
                auto param = event.get_parameter("유저");
                if (std::holds_alternative<dpp::snowflake>(param)) {
                    dpp::snowflake user_id = std::get<dpp::snowflake>(param);
                    voice_tracker.getUserUptime(event, user_id);
                }
                else {
                    event.reply("유효한 유저를 선택해 주세요.");
                }
            }
            });

        bot.on_ready([&bot](const dpp::ready_t& event) {
            if (dpp::run_once<struct register_bot_commands>()) {
                bot.global_command_create(
                    dpp::slashcommand("순위", "누적 시간 상위 5명 사용자 목록을 표시합니다.", bot.me.id)
                );

                dpp::slashcommand uptime_cmd("업타임", "특정 사용자의 음성 채널 업타임을 조회합니다.", bot.me.id);
                uptime_cmd.add_option(
                    dpp::command_option(dpp::co_user, "유저", "업타임을 확인할 사용자", true)
                );
                bot.global_command_create(uptime_cmd);

                bot.log(dpp::ll_info, "명령어 등록 완료");
            }
            });

        bot.start(dpp::st_wait);
    }
    catch (const std::exception& exception) {
        std::cerr << "봇 시작 실패: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
