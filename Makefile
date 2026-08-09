CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++23 -O2 -g -I. -Iexternal -Iexternal/SQLiteCpp/include
LDFLAGS = -pthread -lsqlite3

BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
DEP_DIR   = $(BUILD_DIR)/dep

# The runnable binary lives at the repo root, outside build/.
TARGET = av

OBJECTS = \
	main \
	Application \
	SegfaultHandler \
	Logger \
	FileTreeWalker \
	ResumePathFilter \
	FileInfo \
	ThreadPool \
	ResumeManager \
	SqliteCacheManager \
	CacheWriter \
	CacheManager \
	CommandParser \
	ConsolePrinter \
	ExcludeSet \
	PathFilter \
	ScanRootGuard \
	SignatureLoader \
	AutomatonBuilder \
	AutomatonScanner \
	FileProcessor \
	QuarantineFileOperations \
	QuarantineRepository \
	QuarantineManager \
	PerformanceMonitor \
	ScanPipeline \
	SQLiteCpp_Backup \
	SQLiteCpp_Column \
	SQLiteCpp_Database \
	SQLiteCpp_Exception \
	SQLiteCpp_Savepoint \
	SQLiteCpp_Statement \
	SQLiteCpp_Transaction

OBJS = $(addprefix $(OBJ_DIR)/,$(addsuffix .o,$(OBJECTS)))
DEPS = $(addprefix $(DEP_DIR)/,$(addsuffix .d,$(OBJECTS)))

all: $(TARGET)

$(OBJ_DIR) $(DEP_DIR):
	mkdir -p $@

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

define COMPILE
$(OBJ_DIR)/$(1).o: $(2) | $(OBJ_DIR) $(DEP_DIR)
	$$(CXX) $$(CXXFLAGS) -MMD -MP -MF $(DEP_DIR)/$(1).d -c $(2) -o $$@
endef

$(eval $(call COMPILE,main,main.cpp))
$(eval $(call COMPILE,Application,Application/Application.cpp))
$(eval $(call COMPILE,SegfaultHandler,CrashHandler/SegfaultHandler.cpp))
$(eval $(call COMPILE,Logger,Logger/Logger.cpp))
$(eval $(call COMPILE,FileTreeWalker,Scan/Traversal/FileTreeWalker.cpp))
$(eval $(call COMPILE,ResumePathFilter,Resume/ResumePathFilter.cpp))
$(eval $(call COMPILE,FileInfo,Scan/Traversal/FileInfo.cpp))
$(eval $(call COMPILE,ThreadPool,ThreadPool/ThreadPool.cpp))
$(eval $(call COMPILE,ResumeManager,Resume/ResumeManager.cpp))
$(eval $(call COMPILE,SqliteCacheManager,Cache/SqliteCacheManager.cpp))
$(eval $(call COMPILE,CacheWriter,Cache/CacheWriter.cpp))
$(eval $(call COMPILE,CacheManager,Cache/CacheManager.cpp))
$(eval $(call COMPILE,CommandParser,CLI/CommandParser.cpp))
$(eval $(call COMPILE,ConsolePrinter,Console/ConsolePrinter.cpp))
$(eval $(call COMPILE,ExcludeSet,Exclude/ExcludeSet.cpp))
$(eval $(call COMPILE,PathFilter,Exclude/PathFilter.cpp))
$(eval $(call COMPILE,ScanRootGuard,Exclude/ScanRootGuard.cpp))
$(eval $(call COMPILE,SignatureLoader,Signature/SignatureLoader.cpp))
$(eval $(call COMPILE,AutomatonBuilder,Scan/Automaton/AutomatonBuilder.cpp))
$(eval $(call COMPILE,AutomatonScanner,Scan/Automaton/AutomatonScanner.cpp))
$(eval $(call COMPILE,FileProcessor,Scan/FileProcessor.cpp))
$(eval $(call COMPILE,QuarantineFileOperations,Quarantine/QuarantineFileOperations.cpp))
$(eval $(call COMPILE,QuarantineRepository,Quarantine/QuarantineRepository.cpp))
$(eval $(call COMPILE,QuarantineManager,Quarantine/QuarantineManager.cpp))
$(eval $(call COMPILE,PerformanceMonitor,Performance/PerformanceMonitor.cpp))
$(eval $(call COMPILE,ScanPipeline,Scan/Pipeline/ScanPipeline.cpp))
$(eval $(call COMPILE,SQLiteCpp_Backup,external/SQLiteCpp/src/Backup.cpp))
$(eval $(call COMPILE,SQLiteCpp_Column,external/SQLiteCpp/src/Column.cpp))
$(eval $(call COMPILE,SQLiteCpp_Database,external/SQLiteCpp/src/Database.cpp))
$(eval $(call COMPILE,SQLiteCpp_Exception,external/SQLiteCpp/src/Exception.cpp))
$(eval $(call COMPILE,SQLiteCpp_Savepoint,external/SQLiteCpp/src/Savepoint.cpp))
$(eval $(call COMPILE,SQLiteCpp_Statement,external/SQLiteCpp/src/Statement.cpp))
$(eval $(call COMPILE,SQLiteCpp_Transaction,external/SQLiteCpp/src/Transaction.cpp))

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) dist

# Build a distributable Debian package (.deb) into dist/.
# Override the version with: make deb VERSION=1.2.3
VERSION ?= 0.1.0
deb:
	./packaging/build-deb.sh $(VERSION)

.PHONY: all clean deb
