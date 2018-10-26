// Copyright 2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <gtest/gtest.h>

#include <vector>
#include <memory>
#include <string>

#ifdef _WIN32
#include <uxr/agent/transport/udp/UDPServerWindows.hpp>
#else
#include <uxr/agent/transport/udp/UDPServerLinux.hpp>
#endif  // _WIN32

#include <rosidl_typesupport_microxrcedds_shared/identifier.h>
#include <rosidl_typesupport_microxrcedds_shared/message_type_support.h>

#include "rmw/error_handling.h"
#include "rmw/node_security_options.h"
#include "rmw/rmw.h"
#include "rmw/validate_namespace.h"
#include "rmw/validate_node_name.h"

#include "./config.h"
#include "./test_utils.hpp"

class TestPublisher : public ::testing::Test
{
protected:
  static void SetUpTestCase()
  {
    #ifndef _WIN32
    freopen("/dev/null", "w", stderr);
    #endif
  }

  void SetUp()
  {
    rmw_ret_t ret = rmw_init();
    ASSERT_EQ(ret, RMW_RET_OK);

    server =
      std::unique_ptr<eprosima::uxr::Server>(new eprosima::uxr::UDPServer((uint16_t)atoi("8888")));
    server->run();
    // ASSERT_EQ(server->run(), true);

    rmw_node_security_options_t security_options;
    node = rmw_create_node("my_node", "/ns", 0, &security_options);
    ASSERT_NE((void *)node, (void *)NULL);
  }

  void TearDown()
  {
    // Stop agent
    server->stop();
  }

  rmw_node_t * node;
  std::unique_ptr<eprosima::uxr::Server> server;

  const char * topic_type = "topic_type";
  const char * topic_name = "topic_name";
  const char * package_name = "package_name";

  size_t id_gen = 0;
};

/*
   Testing subscription construction and destruction.
 */
TEST_F(TestPublisher, construction_and_destruction) {
  rosidl_message_type_support_t dummy_type_support;
  message_type_support_callbacks_t dummy_callbacks;
  ConfigureDummyTypeSupport(
    topic_type,
    package_name,
    &dummy_type_support,
    &dummy_callbacks);

  rmw_qos_profile_t dummy_qos_policies;
  ConfigureDefaultQOSPolices(&dummy_qos_policies);

  bool ignore_local_publications = true;

  rmw_publisher_t * pub = rmw_create_publisher(
    this->node,
    &dummy_type_support,
    topic_name,
    &dummy_qos_policies);
  ASSERT_NE((void *)pub, (void *)NULL);

  rmw_ret_t ret = rmw_destroy_publisher(this->node, pub);
  ASSERT_EQ(ret, RMW_RET_OK);
}


/*
   Testing node memory poll
 */
TEST_F(TestPublisher, memory_poll) {
  rosidl_message_type_support_t dummy_type_support;
  message_type_support_callbacks_t dummy_callbacks;
  ConfigureDummyTypeSupport(
    topic_type,
    package_name,
    &dummy_type_support,
    &dummy_callbacks);

  rmw_qos_profile_t dummy_qos_policies;
  ConfigureDefaultQOSPolices(&dummy_qos_policies);

  bool ignore_local_publications = true;

  std::vector<rmw_publisher_t *> publishers;
  rmw_ret_t ret;
  rmw_publisher_t * publisher;


  // Get all available nodes
  {
    for (size_t i = 0; i < MAX_PUBLISHERS_X_NODE; i++) {
      std::string aux_string(topic_type);
      aux_string.append(std::to_string(id_gen++));
      dummy_callbacks.message_name_ = aux_string.data();
      publisher = rmw_create_publisher(
        this->node,
        &dummy_type_support,
        std::string(topic_name).append(std::to_string(id_gen++)).data(),
        &dummy_qos_policies);
      ASSERT_NE((void *)publisher, (void *)NULL);
      publishers.push_back(publisher);
    }
  }


  // Try to get one
  {
    std::string aux_string(topic_type);
    aux_string.append(std::to_string(id_gen++));
    dummy_callbacks.message_name_ = aux_string.data();
    publisher = rmw_create_publisher(
      this->node,
      &dummy_type_support,
      std::string(topic_name).append(std::to_string(id_gen++)).data(),
      &dummy_qos_policies);
    ASSERT_EQ((void *)publisher, (void *)NULL);
    ASSERT_EQ(CheckErrorState(), true);

    // Relese one
    publisher = publishers.back();
    publishers.pop_back();
    ret = rmw_destroy_publisher(this->node, publisher);
    ASSERT_EQ(ret, RMW_RET_OK);
  }


  // Get one
  {
    std::string aux_string(topic_type);
    aux_string.append(std::to_string(id_gen++));
    dummy_callbacks.message_name_ = aux_string.data();
    publisher = rmw_create_publisher(
      this->node,
      &dummy_type_support,
      std::string(topic_name).append(std::to_string(id_gen++)).data(),
      &dummy_qos_policies);
    ASSERT_NE((void *)publisher, (void *)NULL);
    publishers.push_back(publisher);
  }


  // Release all
  {
    for (size_t i = 0; i < publishers.size(); i++) {
      publisher = publishers.at(i);
      ret = rmw_destroy_publisher(this->node, publisher);
      ASSERT_EQ(ret, RMW_RET_OK);
    }
    publishers.clear();
  }
}
