#pragma once

namespace Topic {
  constexpr auto ANNOUNCE = "quiz/announce";
  constexpr auto JOIN = "quiz/join";
  constexpr auto ASSIGN = "quiz/assign/";
  constexpr auto STATE = "quiz/state";
  constexpr auto BUZZ = "quiz/buzz";
  constexpr auto QUEUE = "quiz/queue";
  constexpr auto CMD = "quiz/cmd";
  constexpr auto PING = "quiz/ping";
}

enum class Phase : uint8_t { BOOT=0, LOBBY, READY, OPEN, ANSWER, RESET };
enum class ClientState : uint8_t { DISCONNECTED=0, CONNECTING, ASSIGNED, IDLE, LOCKED_AFTER_BUZZ, ACTIVE_TURN, CELEBRATE, WRONG_FLASH };
enum class ButtonPress : uint8_t { NONE=0, SHORT, LONG, VERY_LONG };
enum class AnimationType : uint8_t { SOLID=0, PULSE, SPIN, FLASH, RAINBOW, OFF };

namespace Command {
  constexpr auto LIGHT_WHITE = "LIGHT_WHITE";
  constexpr auto ANIM_ACTIVE = "ANIM_ACTIVE";
  constexpr auto IDLE_COLOR = "IDLE_COLOR";
  constexpr auto CELEBRATE = "CELEBRATE";
  constexpr auto WRONG_FLASH = "WRONG_FLASH";
  constexpr auto RESET = "RESET";
  constexpr auto TEST_MODE_ON = "TEST_MODE_ON";
  constexpr auto TEST_MODE_OFF = "TEST_MODE_OFF";
}

namespace JsonKey {
  constexpr auto ID="id"; constexpr auto VERSION="version"; constexpr auto TIMESTAMP="t"; constexpr auto BATTERY_MV="batteryMv";
  constexpr auto MAX_CLIENTS="maxClients"; constexpr auto LOCKED="locked"; constexpr auto CAPABILITY="cap"; constexpr auto FIRMWARE="fw";
  constexpr auto COLOR="color"; constexpr auto SLOT="slot"; constexpr auto PHASE="phase"; constexpr auto ORDER="order"; constexpr auto ACTIVE="active";
  constexpr auto CMD="cmd"; constexpr auto TARGET="target";
}

constexpr auto PROTOCOL_VERSION="1.0";

inline const char* phaseToString(Phase phase){
  switch(phase){case Phase::BOOT:return "BOOT";case Phase::LOBBY:return "LOBBY";case Phase::READY:return "READY";case Phase::OPEN:return "OPEN";case Phase::ANSWER:return "ANSWER";case Phase::RESET:return "RESET";default:return "UNKNOWN";}
}
inline Phase stringToPhase(const char* str){
  if(strcmp(str,"BOOT")==0)return Phase::BOOT;if(strcmp(str,"LOBBY")==0)return Phase::LOBBY;if(strcmp(str,"READY")==0)return Phase::READY;if(strcmp(str,"OPEN")==0)return Phase::OPEN;if(strcmp(str,"ANSWER")==0)return Phase::ANSWER;if(strcmp(str,"RESET")==0)return Phase::RESET;return Phase::BOOT;
}
