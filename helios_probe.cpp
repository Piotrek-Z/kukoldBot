//służy do podstawowego testu połączenia i kilku żeczy


#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>

#include "rokae/robot.h"

#ifndef HELIOS_DOF
#define HELIOS_DOF 7
#endif

#if HELIOS_DOF == 7
using Robot = rokae::xMateErProRobot;
#else
using Robot = rokae::xMateRobot;
#endif

using namespace std::chrono_literals;

static const char* powerStr(rokae::PowerState s) {
  switch (s) {
    case rokae::PowerState::on:    return "on";
    case rokae::PowerState::off:   return "off";
    case rokae::PowerState::estop: return "estop";
    case rokae::PowerState::gstop: return "gstop (drzwi bezpieczenstwa)";
    default:                       return "unknown";
  }
}

static const char* modeStr(rokae::OperateMode m) {
  switch (m) {
    case rokae::OperateMode::manual:    return "manual";
    case rokae::OperateMode::automatic: return "automatic";
    default:                            return "unknown";
  }
}

class HeliosProbe : public rclcpp::Node {
public:
  HeliosProbe() : Node("helios_probe") {
    const auto ip = declare_parameter<std::string>("robot_ip", "192.168.0.160");
    const auto local_ip = declare_parameter<std::string>("local_ip", "");
    pub_ = create_publisher<sensor_msgs::msg::JointState>("helios/joint_states", 10);

    std::error_code ec;
    RCLCPP_INFO(get_logger(), "Lacze z robotem %s (local_ip: '%s') ...",
                ip.c_str(), local_ip.c_str());

    robot_ = std::make_unique<Robot>(ip, local_ip);
    robot_->connectToRobot(ec);
    if (ec) throw std::runtime_error("connectToRobot: " + ec.message());

    auto info = robot_->robotInfo(ec);
    if (ec) throw std::runtime_error("robotInfo: " + ec.message());
    RCLCPP_INFO(get_logger(), "model: %s | osie: %d | kontroler: %s | id: %s",
                info.type.c_str(), info.joint_num, info.version.c_str(), info.id.c_str());

    auto power = robot_->powerState(ec);
    if (!ec) RCLCPP_INFO(get_logger(), "zasilanie: %s", powerStr(power));

    auto mode = robot_->operateMode(ec);
    if (!ec) RCLCPP_INFO(get_logger(), "tryb pracy: %s", modeStr(mode));

    auto op = robot_->operationState(ec);
    if (!ec) RCLCPP_INFO(get_logger(), "stan pracy (enum): %d", static_cast<int>(op));

    if (info.joint_num != HELIOS_DOF) {
      RCLCPP_ERROR(get_logger(),
        "Kontroler zglasza %d osi, a wezel zbudowano na %d. Odczyt stawow wylaczony.",
        info.joint_num, HELIOS_DOF);
      return;   // bez timera, tylko informacje powyzej
    }

    timer_ = create_wall_timer(100ms, [this]() { tick(); });   // 10 Hz
  }

  ~HeliosProbe() override {
    std::error_code ec;
    if (robot_) robot_->disconnectFromRobot(ec);
  }

private:
  void tick() {
    std::error_code ec;
    auto q = robot_->jointPos(ec);   // rad
    if (ec) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000,
                           "jointPos: %s", ec.message().c_str());
      return;
    }
    sensor_msgs::msg::JointState msg;
    msg.header.stamp = now();
    for (size_t i = 0; i < q.size(); ++i) {
      msg.name.push_back("joint" + std::to_string(i + 1));
    }
    msg.position.assign(q.begin(), q.end());
    pub_->publish(msg);
  }

  std::unique_ptr<Robot> robot_;
  rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr pub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char** argv) {
  rclcpp::init(argc, argv);
  int rc = 0;
  try {
    rclcpp::spin(std::make_shared<HeliosProbe>());
  } catch (const std::exception& e) {
    RCLCPP_FATAL(rclcpp::get_logger("helios_probe"), "%s", e.what());
    rc = 1;
  }
  rclcpp::shutdown();
  return rc;
}
