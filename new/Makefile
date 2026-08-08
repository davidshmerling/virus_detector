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
	SqliteCacheManager \
	CacheWriter \
	CacheManager \
	CommandParser \
	ConsolePrinter \
	ExcludeManager \
	SignatureLoader \
	AutomatonBuilder \
	AutomatonScanner \
	FileProcessor \
	QuarantineFileOperations \
	QuarantineRepository \
	QuarantineManager \
	ScanManager

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
$(eval $(call COMPILE,FileTreeWalker,Scan/FileTreeWalker/FileTreeWalker.cpp))
$(eval $(call COMPILE,ThreadPool,ThreadPool/ThreadPool.cpp))
$(eval $(call COMPILE,ResumeManager,Resume/ResumeManager.cpp))
$(eval $(call COMPILE,SqliteCacheManager,Cache/SqliteCacheManager.cpp))
$(eval $(call COMPILE,CacheWriter,Cache/CacheWriter.cpp))
$(eval $(call COMPILE,CacheManager,Cache/CacheManager.cpp))
$(eval $(call COMPILE,CommandParser,CLI/CommandParser.cpp))
$(eval $(call COMPILE,ConsolePrinter,Console/ConsolePrinter.cpp))
$(eval $(call COMPILE,ExcludeManager,Exclude/ExcludeManager.cpp))
$(eval $(call COMPILE,SignatureLoader,Signature/SignatureLoader.cpp))
$(eval $(call COMPILE,AutomatonBuilder,Scan/AutomatonBuilder/AutomatonBuilder.cpp))
$(eval $(call COMPILE,AutomatonScanner,Scan/AutomatonScanner/AutomatonScanner.cpp))
$(eval $(call COMPILE,FileProcessor,Scan/FileProcessor/FileProcessor.cpp))
$(eval $(call COMPILE,QuarantineFileOperations,Quarantine/QuarantineFileOperations.cpp))
$(eval $(call COMPILE,QuarantineRepository,Quarantine/QuarantineRepository.cpp))
$(eval $(call COMPILE,QuarantineManager,Quarantine/QuarantineManager.cpp))
$(eval $(call COMPILE,ScanManager,Scan/ScanManager/ScanManager.cpp))

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean


