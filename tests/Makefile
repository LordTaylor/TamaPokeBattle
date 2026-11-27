CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -I../tests/stubs -I..

PET_TEST_OBJS = build/pet_tests.o build/pet.o build/species_chirp.o build/thumb_format.o build/audio_stub.o build/trainer.o
BATTLE_TEST_OBJS = build/battle_tests.o build/battle.o

.PHONY: test web-test clean

test: build/pet_tests build/battle_tests build/i18n_tests build/trainer_tests web-test
	./build/pet_tests
	./build/battle_tests
	./build/i18n_tests
	./build/trainer_tests

web-test:
	node web_serial_tests.mjs

build:
	mkdir -p build

build/pet_tests: $(PET_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PET_TEST_OBJS) -o $@

build/pet_tests.o: pet_tests.cpp ../pet.h ../trainer.h ../dex.h | build
	$(CXX) $(CXXFLAGS) -c pet_tests.cpp -o $@

build/pet.o: ../pet.cpp ../pet.h ../trainer.h ../dex.h ../audio.h | build
	$(CXX) $(CXXFLAGS) -c ../pet.cpp -o $@

build/species_chirp.o: ../species_chirp.cpp ../species_chirp.h ../dex.h | build
	$(CXX) $(CXXFLAGS) -c ../species_chirp.cpp -o $@

build/thumb_format.o: ../thumb_format.cpp ../thumb_format.h | build
	$(CXX) $(CXXFLAGS) -c ../thumb_format.cpp -o $@

build/audio_stub.o: audio_stub.cpp ../audio.h | build
	$(CXX) $(CXXFLAGS) -c audio_stub.cpp -o $@

build/battle_tests: $(BATTLE_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(BATTLE_TEST_OBJS) -o $@

build/battle_tests.o: battle_tests.cpp ../battle.h | build
	$(CXX) $(CXXFLAGS) -c battle_tests.cpp -o $@

build/battle.o: ../battle.cpp ../battle.h | build
	$(CXX) $(CXXFLAGS) -c ../battle.cpp -o $@

I18N_TEST_OBJS = build/i18n_tests.o build/i18n.o

build/i18n_tests: $(I18N_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(I18N_TEST_OBJS) -o $@

build/i18n_tests.o: i18n_tests.cpp ../i18n.h ../pet.h | build
	$(CXX) $(CXXFLAGS) -c i18n_tests.cpp -o $@

build/i18n.o: ../i18n.cpp ../i18n.h ../dex.h ../pet.h ../trainer.h | build
	$(CXX) $(CXXFLAGS) -c ../i18n.cpp -o $@

clean:
	rm -rf build

TRAINER_TEST_OBJS = build/trainer_tests.o build/trainer.o

build/trainer_tests: $(TRAINER_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(TRAINER_TEST_OBJS) -o $@

build/trainer_tests.o: trainer_tests.cpp ../trainer.h | build
	$(CXX) $(CXXFLAGS) -c trainer_tests.cpp -o $@

build/trainer.o: ../trainer.cpp ../trainer.h | build
	$(CXX) $(CXXFLAGS) -c ../trainer.cpp -o $@

# --- PvP protocol tests ---
PVP_TEST_OBJS = build/pvp_tests.o build/pvp_packet.o build/pvp_peers.o build/pvp_reliable.o build/pvp_session.o build/pvp_session_handshake.o build/pvp_session_battle.o
PVP_HEADERS = ../pvp_config.h ../pvp_packet.h ../pvp_session.h ../pvp_event.h ../pvp_peers.h ../pvp_reliable.h ../pvp_ring.h ../pvp_seq_window.h ../pvp_state.h ../pvp_out_packet.h ../pvp_peer_info.h ../pvp_time.h ../battle.h

pvp-test: build/pvp_tests
	./build/pvp_tests

build/pvp_tests: $(PVP_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PVP_TEST_OBJS) -o $@

build/pvp_tests.o: pvp_tests.cpp pvp_test_harness.h $(PVP_HEADERS) | build
	$(CXX) $(CXXFLAGS) -c pvp_tests.cpp -o $@

build/pvp_%.o: ../pvp_%.cpp $(PVP_HEADERS) | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: pvp-test
test: pvp-test

# PvP battle engine tests: own object dir (no clash with generic build/pvp_%.o rules),
# built with UBSan so out-of-range reads and signed overflow fail the run.
PVP_BATTLE_SANITIZE ?= -fsanitize=undefined -fno-sanitize-recover=undefined
PVP_BATTLE_TEST_OBJS = build/pvpbat/pvp_battle_tests.o build/pvpbat/pvp_battle.o build/pvpbat/battle.o

.PHONY: pvp-battle-test

pvp-battle-test: build/pvp_battle_engine_tests
	./build/pvp_battle_engine_tests

build/pvpbat:
	mkdir -p build/pvpbat

build/pvp_battle_engine_tests: $(PVP_BATTLE_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PVP_BATTLE_SANITIZE) $(PVP_BATTLE_TEST_OBJS) -o $@

build/pvpbat/pvp_battle_tests.o: pvp_battle_tests.cpp ../pvp_battle.h ../pvp_config.h ../trainer.h ../battle_core.h ../battle.h | build/pvpbat
	$(CXX) $(CXXFLAGS) $(PVP_BATTLE_SANITIZE) -c pvp_battle_tests.cpp -o $@

build/pvpbat/pvp_battle.o: ../pvp_battle.cpp ../pvp_battle.h ../pvp_config.h ../battle_core.h ../battle.h | build/pvpbat
	$(CXX) $(CXXFLAGS) $(PVP_BATTLE_SANITIZE) -c ../pvp_battle.cpp -o $@

build/pvpbat/battle.o: ../battle.cpp ../battle_core.h ../battle.h | build/pvpbat
	$(CXX) $(CXXFLAGS) $(PVP_BATTLE_SANITIZE) -c ../battle.cpp -o $@

test: pvp-battle-test
build/battle.o: ../battle_core.h

# PvP radio-callback boundary filter (pure header + serializer for real packets)
PVP_IN_PACKET_OBJS = build/pvpin/pvp_in_packet_tests.o build/pvpin/pvp_packet.o

.PHONY: pvp-in-packet-test

pvp-in-packet-test: build/pvp_in_packet_tests
	./build/pvp_in_packet_tests

build/pvpin:
	mkdir -p build/pvpin

build/pvp_in_packet_tests: $(PVP_IN_PACKET_OBJS)
	$(CXX) $(CXXFLAGS) $(PVP_IN_PACKET_OBJS) -o $@

build/pvpin/pvp_in_packet_tests.o: pvp_in_packet_tests.cpp pvp_test_harness.h ../pvp_in_packet.h $(PVP_HEADERS) | build/pvpin
	$(CXX) $(CXXFLAGS) -c pvp_in_packet_tests.cpp -o $@

build/pvpin/pvp_packet.o: ../pvp_packet.cpp $(PVP_HEADERS) | build/pvpin
	$(CXX) $(CXXFLAGS) -c ../pvp_packet.cpp -o $@

test: pvp-in-packet-test

# --- PvP T4.1 core: validator, match, badges, pump, app (host tests, no radio, no UI) ---
PVP_CORE_HEADERS = ../pvp_in_packet.h ../pvp_link.h ../pvp_profile.h ../pvp_ledger.h ../pvp_validator.h ../pvp_match.h ../pvp_pump.h ../pvp_app.h ../pvp_ui_model.h ../pvp_badges.h ../pvp_versions.h ../pvp_battle.h ../battle_core.h $(PVP_HEADERS) ../trainer.h ../dex.h

# extra prerequisites only; the recipe still comes from the build/pvp_%.o pattern rule
build/pvp_validator.o build/pvp_match.o build/pvp_pump.o build/pvp_app.o build/pvp_app_events.o build/pvp_badges.o build/pvp_battle.o: $(PVP_CORE_HEADERS)

# pet.cpp now uses the badge ring; its tests also cover the validator <-> Pet::calcStat contract
PET_TEST_OBJS += build/pvp_badges.o build/pvp_validator.o build/pvp_battle.o build/pvp_packet.o build/battle.o
build/pet_tests: build/pvp_badges.o build/pvp_validator.o build/pvp_battle.o build/pvp_packet.o build/battle.o
build/pet_tests.o build/pet.o: ../pvp_badges.h ../pvp_validator.h

PVP_VALIDATOR_TEST_OBJS = build/pvp_validator_tests.o build/pvp_validator.o build/pvp_battle.o build/pvp_packet.o build/battle.o
PVP_MATCH_TEST_OBJS = build/pvp_match_tests.o build/pvp_match.o build/pvp_battle.o build/pvp_packet.o build/battle.o
PVP_BADGES_TEST_OBJS = build/pvp_badges_tests.o build/pvp_badges.o
PVP_APP_TEST_OBJS = build/pvp_app_tests.o build/pvp_app.o build/pvp_app_events.o build/pvp_pump.o build/pvp_match.o build/pvp_validator.o build/pvp_badges.o build/pvp_battle.o build/pvp_packet.o build/pvp_peers.o build/pvp_reliable.o build/pvp_session.o build/pvp_session_handshake.o build/pvp_session_battle.o build/battle.o

.PHONY: pvp-core-test pvp-validator-test pvp-match-test pvp-badges-test pvp-app-test

pvp-core-test: pvp-validator-test pvp-match-test pvp-badges-test pvp-app-test

pvp-validator-test: build/pvp_validator_tests
	./build/pvp_validator_tests

pvp-match-test: build/pvp_match_tests
	./build/pvp_match_tests

pvp-badges-test: build/pvp_badges_tests
	./build/pvp_badges_tests

pvp-app-test: build/pvp_app_tests
	./build/pvp_app_tests

build/pvp_validator_tests: $(PVP_VALIDATOR_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PVP_VALIDATOR_TEST_OBJS) -o $@

build/pvp_match_tests: $(PVP_MATCH_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PVP_MATCH_TEST_OBJS) -o $@

build/pvp_badges_tests: $(PVP_BADGES_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PVP_BADGES_TEST_OBJS) -o $@

build/pvp_app_tests: $(PVP_APP_TEST_OBJS)
	$(CXX) $(CXXFLAGS) $(PVP_APP_TEST_OBJS) -o $@

build/pvp_validator_tests.o: pvp_validator_tests.cpp pvp_test_harness.h $(PVP_CORE_HEADERS) | build
	$(CXX) $(CXXFLAGS) -c pvp_validator_tests.cpp -o $@

build/pvp_match_tests.o: pvp_match_tests.cpp pvp_test_harness.h $(PVP_CORE_HEADERS) | build
	$(CXX) $(CXXFLAGS) -c pvp_match_tests.cpp -o $@

build/pvp_badges_tests.o: pvp_badges_tests.cpp pvp_test_harness.h $(PVP_CORE_HEADERS) | build
	$(CXX) $(CXXFLAGS) -c pvp_badges_tests.cpp -o $@

build/pvp_app_tests.o: pvp_app_tests.cpp pvp_app_fakes.h pvp_test_harness.h $(PVP_CORE_HEADERS) | build
	$(CXX) $(CXXFLAGS) -c pvp_app_tests.cpp -o $@

test: pvp-core-test

# T4.1 review fixes: headers split out of pvp_pump.h / pvp_validator.h / pvp_profile.h
build/pvp_validator.o build/pvp_match.o build/pvp_pump.o build/pvp_app.o build/pvp_app_events.o build/pvp_badges.o build/pvp_validator_tests.o build/pvp_match_tests.o build/pvp_badges_tests.o build/pvp_app_tests.o: ../pvp_event_sink.h ../pvp_stat_range.h ../pvp_deny_reason.h
