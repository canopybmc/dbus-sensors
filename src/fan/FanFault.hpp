/*
// Copyright (c) 2026 9elements GmbH
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <memory>
#include <optional>
#include <string>

class FanFault : public std::enable_shared_from_this<FanFault>
{
  public:
    FanFault(boost::asio::io_context& io,
             std::shared_ptr<sdbusplus::asio::connection>& conn,
             const std::string& faultPath,
             const std::string& inventoryPath);

    void start();

  private:
    void poll();
    void notify(bool functional);

    boost::asio::steady_timer timer;
    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::string faultPath;
    std::string inventoryPath;
    std::optional<bool> lastFunctional;

    static constexpr unsigned int pollIntervalMs = 5000;
};
