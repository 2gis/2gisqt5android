// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_server_properties_manager.h"

#include "base/basictypes.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::StrictMock;

const char kTestHttpServerProperties[] = "TestHttpServerProperties";

class TestingHttpServerPropertiesManager : public HttpServerPropertiesManager {
 public:
  TestingHttpServerPropertiesManager(
      PrefService* pref_service,
      const char* pref_path,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
      : HttpServerPropertiesManager(pref_service, pref_path, io_task_runner) {
    InitializeOnNetworkThread();
  }

  virtual ~TestingHttpServerPropertiesManager() {}

  // Make these methods public for testing.
  using HttpServerPropertiesManager::ScheduleUpdateCacheOnPrefThread;
  using HttpServerPropertiesManager::ScheduleUpdatePrefsOnNetworkThread;

  // Post tasks without a delay during tests.
  virtual void StartPrefsUpdateTimerOnNetworkThread(
      base::TimeDelta delay) override {
    HttpServerPropertiesManager::StartPrefsUpdateTimerOnNetworkThread(
        base::TimeDelta());
  }

  void UpdateCacheFromPrefsOnUIConcrete() {
    HttpServerPropertiesManager::UpdateCacheFromPrefsOnPrefThread();
  }

  // Post tasks without a delay during tests.
  virtual void StartCacheUpdateTimerOnPrefThread(
      base::TimeDelta delay) override {
    HttpServerPropertiesManager::StartCacheUpdateTimerOnPrefThread(
        base::TimeDelta());
  }

  void UpdatePrefsFromCacheOnNetworkThreadConcrete(
      const base::Closure& callback) {
    HttpServerPropertiesManager::UpdatePrefsFromCacheOnNetworkThread(callback);
  }

  MOCK_METHOD0(UpdateCacheFromPrefsOnPrefThread, void());
  MOCK_METHOD1(UpdatePrefsFromCacheOnNetworkThread, void(const base::Closure&));
  MOCK_METHOD5(UpdateCacheFromPrefsOnNetworkThread,
               void(std::vector<std::string>* spdy_servers,
                    net::SpdySettingsMap* spdy_settings_map,
                    net::AlternateProtocolMap* alternate_protocol_map,
                    net::SupportsQuicMap* supports_quic_map,
                    bool detected_corrupted_prefs));
  MOCK_METHOD4(UpdatePrefsOnPref,
               void(base::ListValue* spdy_server_list,
                    net::SpdySettingsMap* spdy_settings_map,
                    net::AlternateProtocolMap* alternate_protocol_map,
                    net::SupportsQuicMap* supports_quic_map));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingHttpServerPropertiesManager);
};

class HttpServerPropertiesManagerTest : public testing::Test {
 protected:
  HttpServerPropertiesManagerTest() {}

  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(kTestHttpServerProperties);
    http_server_props_manager_.reset(
        new StrictMock<TestingHttpServerPropertiesManager>(
            &pref_service_,
            kTestHttpServerProperties,
            base::MessageLoop::current()->message_loop_proxy()));
    ExpectCacheUpdate();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    if (http_server_props_manager_.get())
      http_server_props_manager_->ShutdownOnPrefThread();
    base::RunLoop().RunUntilIdle();
    http_server_props_manager_.reset();
  }

  void ExpectCacheUpdate() {
    EXPECT_CALL(*http_server_props_manager_, UpdateCacheFromPrefsOnPrefThread())
        .WillOnce(Invoke(http_server_props_manager_.get(),
                         &TestingHttpServerPropertiesManager::
                             UpdateCacheFromPrefsOnUIConcrete));
  }

  void ExpectPrefsUpdate() {
    EXPECT_CALL(*http_server_props_manager_,
                UpdatePrefsFromCacheOnNetworkThread(_))
        .WillOnce(Invoke(http_server_props_manager_.get(),
                         &TestingHttpServerPropertiesManager::
                             UpdatePrefsFromCacheOnNetworkThreadConcrete));
  }

  void ExpectPrefsUpdateRepeatedly() {
    EXPECT_CALL(*http_server_props_manager_,
                UpdatePrefsFromCacheOnNetworkThread(_))
        .WillRepeatedly(
            Invoke(http_server_props_manager_.get(),
                   &TestingHttpServerPropertiesManager::
                       UpdatePrefsFromCacheOnNetworkThreadConcrete));
  }

  //base::RunLoop loop_;
  TestingPrefServiceSimple pref_service_;
  scoped_ptr<TestingHttpServerPropertiesManager> http_server_props_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManagerTest);
};

TEST_F(HttpServerPropertiesManagerTest,
       SingleUpdateForTwoSpdyServerPrefChanges) {
  ExpectCacheUpdate();

  // Set up the prefs for www.google.com:80 and mail.google.com:80 and then set
  // it twice. Only expect a single cache update.

  base::DictionaryValue* server_pref_dict = new base::DictionaryValue;

  // Set supports_spdy for www.google.com:80.
  server_pref_dict->SetBoolean("supports_spdy", true);

  // Set up alternate_protocol for www.google.com:80.
  base::DictionaryValue* alternate_protocol = new base::DictionaryValue;
  alternate_protocol->SetInteger("port", 443);
  alternate_protocol->SetString("protocol_str", "npn-spdy/3");
  server_pref_dict->SetWithoutPathExpansion("alternate_protocol",
                                            alternate_protocol);

  // Set up SupportsQuic for www.google.com:80.
  base::DictionaryValue* supports_quic = new base::DictionaryValue;
  supports_quic->SetBoolean("used_quic", true);
  supports_quic->SetString("address", "foo");
  server_pref_dict->SetWithoutPathExpansion("supports_quic", supports_quic);

  // Set the server preference for www.google.com:80.
  base::DictionaryValue* servers_dict = new base::DictionaryValue;
  servers_dict->SetWithoutPathExpansion("www.google.com:80", server_pref_dict);

  // Set the preference for mail.google.com server.
  base::DictionaryValue* server_pref_dict1 = new base::DictionaryValue;

  // Set supports_spdy for mail.google.com:80
  server_pref_dict1->SetBoolean("supports_spdy", true);

  // Set up alternate_protocol for mail.google.com:80
  base::DictionaryValue* alternate_protocol1 = new base::DictionaryValue;
  alternate_protocol1->SetInteger("port", 444);
  alternate_protocol1->SetString("protocol_str", "npn-spdy/3.1");

  server_pref_dict1->SetWithoutPathExpansion("alternate_protocol",
                                             alternate_protocol1);

  // Set up SupportsQuic for mail.google.com:80
  base::DictionaryValue* supports_quic1 = new base::DictionaryValue;
  supports_quic1->SetBoolean("used_quic", false);
  supports_quic1->SetString("address", "bar");
  server_pref_dict1->SetWithoutPathExpansion("supports_quic", supports_quic1);

  // Set the server preference for mail.google.com:80.
  servers_dict->SetWithoutPathExpansion("mail.google.com:80",
                                        server_pref_dict1);

  base::DictionaryValue* http_server_properties_dict =
      new base::DictionaryValue;
  HttpServerPropertiesManager::SetVersion(http_server_properties_dict, -1);
  http_server_properties_dict->SetWithoutPathExpansion("servers", servers_dict);

  // Set the same value for kHttpServerProperties multiple times.
  pref_service_.SetManagedPref(kTestHttpServerProperties,
                               http_server_properties_dict);
  base::DictionaryValue* http_server_properties_dict2 =
      http_server_properties_dict->DeepCopy();
  pref_service_.SetManagedPref(kTestHttpServerProperties,
                               http_server_properties_dict2);

  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  // Verify SupportsSpdy.
  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("www.google.com:80")));
  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("mail.google.com:80")));
  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(
      net::HostPortPair::FromString("foo.google.com:1337")));

  // Verify AlternateProtocol.
  ASSERT_TRUE(http_server_props_manager_->HasAlternateProtocol(
      net::HostPortPair::FromString("www.google.com:80")));
  ASSERT_TRUE(http_server_props_manager_->HasAlternateProtocol(
      net::HostPortPair::FromString("mail.google.com:80")));
  net::AlternateProtocolInfo port_alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(
          net::HostPortPair::FromString("www.google.com:80"));
  EXPECT_EQ(443, port_alternate_protocol.port);
  EXPECT_EQ(net::NPN_SPDY_3, port_alternate_protocol.protocol);
  port_alternate_protocol = http_server_props_manager_->GetAlternateProtocol(
      net::HostPortPair::FromString("mail.google.com:80"));
  EXPECT_EQ(444, port_alternate_protocol.port);
  EXPECT_EQ(net::NPN_SPDY_3_1, port_alternate_protocol.protocol);

  // Verify SupportsQuic.
  net::SupportsQuic supports_quic2 =
      http_server_props_manager_->GetSupportsQuic(
          net::HostPortPair::FromString("www.google.com:80"));
  EXPECT_TRUE(supports_quic2.used_quic);
  EXPECT_EQ("foo", supports_quic2.address);
  supports_quic2 = http_server_props_manager_->GetSupportsQuic(
      net::HostPortPair::FromString("mail.google.com:80"));
  EXPECT_FALSE(supports_quic2.used_quic);
  EXPECT_EQ("bar", supports_quic2.address);
}

TEST_F(HttpServerPropertiesManagerTest, SupportsSpdy) {
  ExpectPrefsUpdate();

  // Post an update task to the network thread. SetSupportsSpdy calls
  // ScheduleUpdatePrefsOnNetworkThread.

  // Add mail.google.com:443 as a supporting spdy server.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, SetSpdySetting) {
  ExpectPrefsUpdate();

  // Add SpdySetting for mail.google.com:443.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  const net::SpdySettingsIds id1 = net::SETTINGS_UPLOAD_BANDWIDTH;
  const net::SpdySettingsFlags flags1 = net::SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  const net::SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  net::SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  net::SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(net::SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, ClearSpdySetting) {
  ExpectPrefsUpdateRepeatedly();

  // Add SpdySetting for mail.google.com:443.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  const net::SpdySettingsIds id1 = net::SETTINGS_UPLOAD_BANDWIDTH;
  const net::SpdySettingsFlags flags1 = net::SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  const net::SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  net::SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  net::SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(net::SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Clear SpdySetting for mail.google.com:443.
  http_server_props_manager_->ClearSpdySettings(spdy_server_mail);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  // Verify that there are no entries in the settings map for
  // mail.google.com:443.
  const net::SettingsMap& settings_map2_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(0U, settings_map2_ret.size());

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, ClearAllSpdySetting) {
  ExpectPrefsUpdateRepeatedly();

  // Add SpdySetting for mail.google.com:443.
  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  const net::SpdySettingsIds id1 = net::SETTINGS_UPLOAD_BANDWIDTH;
  const net::SpdySettingsFlags flags1 = net::SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  const net::SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  net::SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  net::SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(net::SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  // Clear All SpdySettings.
  http_server_props_manager_->ClearAllSpdySettings();

  // Run the task.
  base::RunLoop().RunUntilIdle();

  // Verify that there are no entries in the settings map.
  const net::SpdySettingsMap& spdy_settings_map2_ret =
      http_server_props_manager_->spdy_settings_map();
  ASSERT_EQ(0U, spdy_settings_map2_ret.size());

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, HasAlternateProtocol) {
  ExpectPrefsUpdate();

  net::HostPortPair spdy_server_mail("mail.google.com", 80);
  EXPECT_FALSE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  http_server_props_manager_->SetAlternateProtocol(
      spdy_server_mail, 443, net::NPN_SPDY_3, 1);

  // Run the task.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  ASSERT_TRUE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  net::AlternateProtocolInfo port_alternate_protocol =
      http_server_props_manager_->GetAlternateProtocol(spdy_server_mail);
  EXPECT_EQ(443, port_alternate_protocol.port);
  EXPECT_EQ(net::NPN_SPDY_3, port_alternate_protocol.protocol);
}

TEST_F(HttpServerPropertiesManagerTest, SupportsQuic) {
  ExpectPrefsUpdate();

  net::HostPortPair quic_server_mail("mail.google.com", 80);
  net::SupportsQuic supports_quic = http_server_props_manager_->GetSupportsQuic(
      quic_server_mail);
  EXPECT_FALSE(supports_quic.used_quic);
  EXPECT_EQ("", supports_quic.address);
  http_server_props_manager_->SetSupportsQuic(quic_server_mail, true, "foo");

  // Run the task.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  net::SupportsQuic supports_quic1 =
      http_server_props_manager_->GetSupportsQuic(quic_server_mail);
  EXPECT_TRUE(supports_quic1.used_quic);
  EXPECT_EQ("foo", supports_quic1.address);
}

TEST_F(HttpServerPropertiesManagerTest, Clear) {
  ExpectPrefsUpdate();

  net::HostPortPair spdy_server_mail("mail.google.com", 443);
  http_server_props_manager_->SetSupportsSpdy(spdy_server_mail, true);
  http_server_props_manager_->SetAlternateProtocol(
      spdy_server_mail, 443, net::NPN_SPDY_3, 1);
  http_server_props_manager_->SetSupportsQuic(spdy_server_mail, true, "foo");

  const net::SpdySettingsIds id1 = net::SETTINGS_UPLOAD_BANDWIDTH;
  const net::SpdySettingsFlags flags1 = net::SETTINGS_FLAG_PLEASE_PERSIST;
  const uint32 value1 = 31337;
  http_server_props_manager_->SetSpdySetting(
      spdy_server_mail, id1, flags1, value1);

  // Run the task.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  EXPECT_TRUE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  net::SupportsQuic supports_quic = http_server_props_manager_->GetSupportsQuic(
      spdy_server_mail);
  EXPECT_TRUE(supports_quic.used_quic);
  EXPECT_EQ("foo", supports_quic.address);

  // Check SPDY settings values.
  const net::SettingsMap& settings_map1_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  ASSERT_EQ(1U, settings_map1_ret.size());
  net::SettingsMap::const_iterator it1_ret = settings_map1_ret.find(id1);
  EXPECT_TRUE(it1_ret != settings_map1_ret.end());
  net::SettingsFlagsAndValue flags_and_value1_ret = it1_ret->second;
  EXPECT_EQ(net::SETTINGS_FLAG_PERSISTED, flags_and_value1_ret.first);
  EXPECT_EQ(value1, flags_and_value1_ret.second);

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());

  ExpectPrefsUpdate();

  // Clear http server data, time out if we do not get a completion callback.
  http_server_props_manager_->Clear(base::MessageLoop::QuitClosure());
  base::RunLoop().Run();

  EXPECT_FALSE(http_server_props_manager_->SupportsSpdy(spdy_server_mail));
  EXPECT_FALSE(
      http_server_props_manager_->HasAlternateProtocol(spdy_server_mail));
  net::SupportsQuic supports_quic1 =
      http_server_props_manager_->GetSupportsQuic(spdy_server_mail);
  EXPECT_FALSE(supports_quic1.used_quic);
  EXPECT_EQ("", supports_quic1.address);

  const net::SettingsMap& settings_map2_ret =
      http_server_props_manager_->GetSpdySettings(spdy_server_mail);
  EXPECT_EQ(0U, settings_map2_ret.size());

  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache0) {
  // Post an update task to the UI thread.
  http_server_props_manager_->ScheduleUpdateCacheOnPrefThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache1) {
  // Post an update task.
  http_server_props_manager_->ScheduleUpdateCacheOnPrefThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdateCache2) {
  http_server_props_manager_->UpdateCacheFromPrefsOnUIConcrete();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

//
// Tests for shutdown when updating prefs.
//
TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs0) {
  // Post an update task to the IO thread.
  http_server_props_manager_->ScheduleUpdatePrefsOnNetworkThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  http_server_props_manager_.reset();
  // Run the task after shutdown and deletion.
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs1) {
  ExpectPrefsUpdate();
  // Post an update task.
  http_server_props_manager_->ScheduleUpdatePrefsOnNetworkThread();
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(HttpServerPropertiesManagerTest, ShutdownWithPendingUpdatePrefs2) {
  // This posts a task to the UI thread.
  http_server_props_manager_->UpdatePrefsFromCacheOnNetworkThreadConcrete(
      base::Closure());
  // Shutdown comes before the task is executed.
  http_server_props_manager_->ShutdownOnPrefThread();
  // Run the task after shutdown, but before deletion.
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(http_server_props_manager_.get());
  http_server_props_manager_.reset();
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace net
