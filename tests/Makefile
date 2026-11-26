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
PVP_TEST_OBJS = build/pvp_tests.o build/pvp_packet.o build/pvp_peers.o build/pvp_reliable.o build/pvp_session.o build/pvp_session_battle.o
PVP_HEADERS = ../pvp_config.h ../pvp_packet.h ../pvp_session.h ../pvp_event.h ../pvp_peers.h ../pvp_reliable.h ../pvp_ring.h ../pvp_seq_window.h ../pvp_state.h ../pvp_out_packet.h ../pvp_peer_info.h ../battle.h

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
