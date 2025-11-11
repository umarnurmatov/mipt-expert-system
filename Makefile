# PROGRAM CONFIG
BUILD_DIR    := build
SRC_DIR      := src
TEST_DIR     := test
INCLUDE_DIRS := include
LOG_DIR      := log
EXECUTABLE   := exyst.out

-include $(SRC_DIR)/sources.make
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SOURCES)))
DEPS := $(patsubst %.o,%.d,$(OBJS))

# LIBRARIES
LIBCUTILS_INCLUDE_PATH := ../cutils/include
LIBCUTILS              := -L../cutils/build/ -lcutils

# COMPILER CONFIG
CC := g++

CPPFLAGS_DEBUG := -D _DEBUG -ggdb3 -O0

CPPFLAGS_RELEASE := -O2 -march=native

CPPFLAGS_ASAN := -fcheck-new -fsized-deallocation -fstack-protector -fstrict-overflow -flto-odr-type-merging -fno-omit-frame-pointer -pie -fPIE -fsanitize=address,alignment,bool,bounds,enum,float-cast-overflow,float-divide-by-zero,integer-divide-by-zero,leak,nonnull-attribute,null,object-size,return,returns-nonnull-attribute,shift,signed-integer-overflow,undefined,unreachable,vla-bound,vptr

ifeq "$(TARGET)" "Release"
CPPFLAGS_TARGET := $(CPPFLAGS_RELEASE) $(CPPFLAGS_ASAN) -ggdb3
else
CPPFLAGS_TARGET := $(CPPFLAGS_DEBUG) $(CPPFLAGS_ASAN)
endif

CPPFLAGS_WARNINGS := -Wall -Wextra -Weffc++ -Waggressive-loop-optimizations -Wc++14-compat -Wmissing-declarations -Wcast-align -Wcast-qual -Wchar-subscripts -Wconditionally-supported -Wconversion -Wctor-dtor-privacy -Wempty-body -Wfloat-equal -Wformat-nonliteral -Wformat-security -Wformat-signedness -Wformat=2 -Winline -Wlogical-op -Wnon-virtual-dtor -Wopenmp-simd -Woverloaded-virtual -Wpacked -Wpointer-arith -Winit-self -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=2 -Wsuggest-attribute=noreturn -Wsuggest-final-methods -Wsuggest-final-types -Wsuggest-override -Wswitch-default -Wswitch-enum -Wsync-nand -Wundef -Wunreachable-code -Wunused -Wuseless-cast -Wvariadic-macros -Wno-literal-suffix -Wno-missing-field-initializers -Wno-narrowing -Wno-old-style-cast -Wno-varargs -Wstack-protector -Wlarger-than=8192 -Werror=vla -Wstack-usage=8192

CPPFLAGS_DEFINES = -DLOG_DIR='"log"' -DIMG_DIR='"img"'

CPPFLAGS := -MMD -MP -std=c++17 $(addprefix -I,$(INCLUDE_DIRS)) $(addprefix -I,$(LIBCUTILS_INCLUDE_PATH)) $(CPPFLAGS_WARNINGS) $(CPPFLAGS_DEFINES) $(CPPFLAGS_TARGET)

# PROGRAM
$(BUILD_DIR)/$(EXECUTABLE): $(OBJS)
	@echo -n Linking $@...
	@$(CC) $(CPPFLAGS) -o $@ $(OBJS) $(LIBCUTILS) 
	@echo done

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo Building $@...
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) -c -o $@ $< $(LIBCUTILS)

.PHONY: run
run: $(BUILD_DIR)/$(EXECUTABLE)
	./$< --log=log.html

# TESTS
-include $(TEST_DIR)/test_sources.make
TEST_EXECS := $(patsubst %.c,$(BUILD_DIR)/%.test,$(TEST_SOURCES))

test: prefix := "\#\#\#\#\#\# [TEST]"
test: $(TEST_EXECS)
	@$(foreach														 \
		exec,														 \
		$(TEST_EXECS),echo "$(prefix) Running $(notdir $(exec))..."; \
		hyperfine --export-json bench.json -w 2 -r 10 "taskset -c 3 $(exec) --log=$(patsubst %.test,%.html,$(notdir $(exec)))";   \
		echo -e "$(prefix) Finished $(notdir $(exec))\n";			 \
	)

$(BUILD_DIR)/%.test: $(BUILD_DIR)/%.test.o $(OBJS)
	@echo -n Building test $@...
	@$(CC) $(CPPFLAGS) -o $@ $^ $(LIBCUTILS)
	@echo done

$(BUILD_DIR)/%.test.o: $(TEST_DIR)/%.c
	@echo Building $@...
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CPPFLAGS) -c $< -o $@ $(LIBCUTILS)
	

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(LOG_DIR)

-include $(DEPS)
