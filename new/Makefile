CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++23 -O2 -g -I. -Iexternal
LDFLAGS = -pthread -lsqlite3

BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
DEP_DIR   = $(BUILD_DIR)/dep
BIN_DIR   = $(BUILD_DIR)/bin

TARGET = $(BIN_DIR)/av_scanner

OBJECTS = \
	main \
	SegfaultHandler \
	Logger \
	FileTreeWalker \
	ThreadPool \
	ResumeManager \
	SqliteCache \
	CacheWriter \
	CacheManager

OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(OBJECTS)))
DEPS = $(addprefix $(DEP_DIR)/,$(addsuffix .d,$(OBJECTS)))

all: $(TARGET)

$(OBJ_DIR) $(DEP_DIR) $(BIN_DIR):
	mkdir -p $@

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

define COMPILE
$(OBJ_DIR)/$(1).o: $(2) | $(OBJ_DIR) $(DEP_DIR)
	$$(CXX) $$(CXXFLAGS) -MMD -MP -MF $(DEP_DIR)/$(1).d -c $(2) -o $$@
endef

$(eval $(call COMPILE,main,main.cpp))
$(eval $(call COMPILE,SegfaultHandler,CrashHandler/SegfaultHandler.cpp))
$(eval $(call COMPILE,Logger,Logger/Logger.cpp))
$(eval $(call COMPILE,FileTreeWalker,FileTreeWalker/FileTreeWalker.cpp))
$(eval $(call COMPILE,ThreadPool,ThreadPool/ThreadPool.cpp))
$(eval $(call COMPILE,ResumeManager,Resume/ResumeManager.cpp))
$(eval $(call COMPILE,SqliteCache,Cache/SqliteCache.cpp))
$(eval $(call COMPILE,CacheWriter,Cache/CacheWriter.cpp))
$(eval $(call COMPILE,CacheManager,Cache/CacheManager.cpp))

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
