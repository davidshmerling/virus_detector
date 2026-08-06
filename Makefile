CXX = g++

CXXFLAGS_COMMON = -Wall -Wextra -std=c++23 -I. -Iexternal
LDFLAGS_COMMON = -pthread -lsqlite3

# Default: portable debug-friendly build.
MODE ?= release

ifeq ($(MODE),bench)
# Local benchmark only: -march=native is not portable across CPUs.
CXXFLAGS = $(CXXFLAGS_COMMON) -O3 -DNDEBUG -march=native -flto
LDFLAGS = $(LDFLAGS_COMMON) -flto
BUILD_DIR = build-bench
else
CXXFLAGS = $(CXXFLAGS_COMMON) -O2 -g
LDFLAGS = $(LDFLAGS_COMMON)
BUILD_DIR = build
endif

OBJ_DIR   = $(BUILD_DIR)/obj
DEP_DIR   = $(BUILD_DIR)/dep
BIN_DIR   = $(BUILD_DIR)/bin

TARGET = $(BIN_DIR)/av_scanner

# Flat object names under obj/ so they do not clash with
# the Scanner/ source folder on case-insensitive disks.
OBJECTS = \
	main \
	Application \
	CommandParser \
	ConsolePrinter \
	Logger \
	PerformanceProfiler \
	ScannerCore \
	FileProcessor \
	QuarantineManager \
	QuarantineRepository \
	ExcludeManager \
	CacheManager \
	SqliteCacheRepository \
	JsonCheckpointRepository \
	ProgressTracker \
	ThreadPool \
	SignatureManager \
	FileEnumerator \
	FileScanner \
	AhoCorasick

OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(OBJECTS)))
DEPS = $(addprefix $(DEP_DIR)/,$(addsuffix .d,$(OBJECTS)))

all: $(TARGET)

bench:
	$(MAKE) MODE=bench all

$(OBJ_DIR) $(DEP_DIR) $(BIN_DIR):
	mkdir -p $@

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

define COMPILE
$(OBJ_DIR)/$(1).o: $(2) | $(OBJ_DIR) $(DEP_DIR)
	$$(CXX) $$(CXXFLAGS) -MMD -MP -MF $(DEP_DIR)/$(1).d -c $(2) -o $$@
endef

$(eval $(call COMPILE,main,main.cpp))
$(eval $(call COMPILE,Application,Application/Application.cpp))
$(eval $(call COMPILE,CommandParser,CLI/CommandParser.cpp))
$(eval $(call COMPILE,ConsolePrinter,CLI/ConsolePrinter.cpp))
$(eval $(call COMPILE,Logger,Logger/Logger.cpp))
$(eval $(call COMPILE,PerformanceProfiler,Performance/PerformanceProfiler.cpp))
$(eval $(call COMPILE,ScannerCore,Scanner/Scanner.cpp))
$(eval $(call COMPILE,FileProcessor,Scanner/FileProcessor.cpp))
$(eval $(call COMPILE,QuarantineManager,Quarantine/QuarantineManager.cpp))
$(eval $(call COMPILE,QuarantineRepository,Quarantine/QuarantineRepository.cpp))
$(eval $(call COMPILE,ExcludeManager,Exclude/ExcludeManager.cpp))
$(eval $(call COMPILE,CacheManager,Cache/CacheManager.cpp))
$(eval $(call COMPILE,SqliteCacheRepository,Cache/SqliteCacheRepository.cpp))
$(eval $(call COMPILE,JsonCheckpointRepository,Resume/JsonCheckpointRepository.cpp))
$(eval $(call COMPILE,ProgressTracker,Resume/ProgressTracker.cpp))
$(eval $(call COMPILE,ThreadPool,ThreadPool/ThreadPool.cpp))
$(eval $(call COMPILE,SignatureManager,Scanner/SignatureManager/SignatureManager.cpp))
$(eval $(call COMPILE,FileEnumerator,Scanner/FileEnumerator/FileEnumerator.cpp))
$(eval $(call COMPILE,FileScanner,Scanner/FileScanner/FileScanner.cpp))
$(eval $(call COMPILE,AhoCorasick,Scanner/Automaton/AhoCorasick.cpp))

-include $(DEPS)

clean:
	rm -rf build build-bench

.PHONY: all bench clean
