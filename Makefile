CXX ?= c++
BUILD_DIR := build
CPPFLAGS := -Isrc
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
THREAD_FLAGS := -pthread

APP := $(BUILD_DIR)/file-indexer
APP_SOURCES := src/main.cpp src/cli.cpp src/index.cpp src/queries.cpp src/reader.cpp src/tokenizer.cpp src/process_indexer.cpp src/persistent_index.cpp
CORE_SOURCES := src/index.cpp src/reader.cpp src/tokenizer.cpp
PROCESS_SOURCES := src/process_indexer.cpp $(CORE_SOURCES)
PERSISTENCE_SOURCES := src/persistent_index.cpp $(PROCESS_SOURCES)

.PHONY: all test clean

all: $(APP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(APP): $(APP_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $(APP_SOURCES) -o $@

$(BUILD_DIR)/bounded-queue-test: tests/bounded_queue_test.cpp src/bounded_queue.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $< -o $@

$(BUILD_DIR)/index-pipeline-test: tests/index_pipeline_test.cpp $(CORE_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $^ -o $@

$(BUILD_DIR)/process-indexer-test: tests/process_indexer_test.cpp $(PROCESS_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $^ -o $@

$(BUILD_DIR)/persistent-index-test: tests/persistent_index_test.cpp $(PERSISTENCE_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(THREAD_FLAGS) $^ -o $@

test: $(APP) $(BUILD_DIR)/bounded-queue-test $(BUILD_DIR)/index-pipeline-test $(BUILD_DIR)/process-indexer-test $(BUILD_DIR)/persistent-index-test
	$(BUILD_DIR)/bounded-queue-test
	$(BUILD_DIR)/index-pipeline-test
	$(BUILD_DIR)/process-indexer-test
	$(BUILD_DIR)/persistent-index-test

clean:
	rm -rf $(BUILD_DIR)
