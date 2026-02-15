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

#include "FanFault.hpp"

#include <boost/asio/error.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message.hpp>

#include <chrono>
#include <fstream>
#include <map>
#include <string>
#include <variant>

FanFault::FanFault(boost::asio::io_context& io,
                   std::shared_ptr<sdbusplus::asio::connection>& conn,
                   const std::string& faultPath,
                   const std::string& inventoryPath) :
    timer(io), conn(conn), faultPath(faultPath), inventoryPath(inventoryPath)
{}

void FanFault::start()
{
    poll();
}

void FanFault::poll()
{
    std::ifstream file(faultPath);
    if (!file.good())
    {
        lg2::error("Failed to read fan fault file '{PATH}'", "PATH",
                   faultPath);
    }
    else
    {
        int value = 0;
        file >> value;

        bool functional = (value == 0);
        if (lastFunctional != functional)
        {
            lastFunctional = functional;
            notify(functional);
        }
    }

    std::weak_ptr<FanFault> weakRef = weak_from_this();
    timer.expires_after(std::chrono::milliseconds(pollIntervalMs));
    timer.async_wait([weakRef](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }
        auto self = weakRef.lock();
        if (self)
        {
            self->poll();
        }
    });
}

void FanFault::notify(bool functional)
{
    using Property = std::string;
    using Value = std::variant<bool>;
    using PropertyMap = std::map<Property, Value>;

    using Interface = std::string;
    using InterfaceMap = std::map<Interface, PropertyMap>;

    using Object = sdbusplus::message::object_path;
    using ObjectMap = std::map<Object, InterfaceMap>;

    ObjectMap objectMap;
    InterfaceMap interfaceMap;
    PropertyMap propertyMap;

    propertyMap.emplace("Functional", functional);
    interfaceMap.emplace(
        "xyz.openbmc_project.State.Decorator.OperationalStatus",
        std::move(propertyMap));
    objectMap.emplace(inventoryPath, std::move(interfaceMap));

    // Derive fan name from inventory path for log messages
    std::string fanName = inventoryPath;
    auto pos = fanName.rfind('/');
    if (pos != std::string::npos)
    {
        fanName = fanName.substr(pos + 1);
    }

    // Create Redfish event log entry for the state change
    if (!functional)
    {
        lg2::error("{FAN} health status changed to Critical", "FAN", fanName,
                   "REDFISH_MESSAGE_ID",
                   std::string("ResourceEvent.1.0."
                               "ResourceStatusChangedCritical"),
                   "REDFISH_MESSAGE_ARGS", fanName + ",Critical");
    }
    else
    {
        lg2::info("{FAN} health status changed to OK", "FAN", fanName,
                  "REDFISH_MESSAGE_ID",
                  std::string("ResourceEvent.1.0.ResourceStatusChangedOK"),
                  "REDFISH_MESSAGE_ARGS", fanName + ",OK");
    }

    conn->async_method_call(
        [this, functional](const boost::system::error_code& ec) {
            if (ec)
            {
                lg2::error(
                    "Failed to update fan inventory functional state "
                    "for '{PATH}': '{ERROR}'",
                    "PATH", inventoryPath, "ERROR", ec.message());
            }
        },
        "xyz.openbmc_project.Inventory.Manager",
        "/xyz/openbmc_project/inventory",
        "xyz.openbmc_project.Inventory.Manager", "Notify",
        std::move(objectMap));
}
