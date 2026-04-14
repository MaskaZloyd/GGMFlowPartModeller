.DEFAULT_GOAL := build

# Usage:
#   make                       -> conan install + configure + build with PRESET=debug
#   make build PRESET=release  -> conan install + configure + build a specific preset
#   make debug                 -> shorthand for PRESET=debug
#   make test PRESET=asan      -> run tests for a configured preset
#   make lint                  -> run clang-tidy using the active preset database
#   make format                -> format project sources in-place
#   make coverage-report       -> generate HTML coverage report
#   make conan-install         -> install Conan dependencies only

CMAKE          ?= cmake
CTEST          ?= ctest
CONAN          ?= conan
CLANG_TIDY     ?= clang-tidy
CLANG_FORMAT   ?= clang-format
LLVM_PROFDATA  ?= llvm-profdata
LLVM_COV       ?= llvm-cov

PRESET                ?= debug
COVERAGE_PRESET       ?= coverage
BUILD_DIR             ?= build/$(PRESET)
COVERAGE_BUILD_DIR    ?= build/$(COVERAGE_PRESET)
COMPILE_COMMANDS_LINK ?= compile_commands.json
LLVM_PROFILE_FILE     ?= $(COVERAGE_BUILD_DIR)/default.profraw
COVERAGE_OUTPUT_DIR   ?= build/coverage-report

# Map preset to Conan build_type
ifeq ($(filter $(PRESET),release),$(PRESET))
  CONAN_BUILD_TYPE := Release
else
  CONAN_BUILD_TYPE := Debug
endif

FORMAT_DIRS := src tests
LINT_DIRS   := src tests

FORMAT_FILES := $(shell find $(FORMAT_DIRS) -type f \( \
	-name '*.c' -o -name '*.cc' -o -name '*.cpp' -o \
	-name '*.h' -o -name '*.hh' -o -name '*.hpp' \
\) 2>/dev/null)

LINT_FILES := $(shell find $(LINT_DIRS) -type f \( \
	-name '*.cc' -o -name '*.cpp' \
\) 2>/dev/null)

.PHONY: all build configure conan-install rebuild compile-commands \
        debug release asan tsan ubsan coverage \
        test lint format format-check clean coverage-report help

all: build

conan-install:
	$(CONAN) install . --output-folder=$(BUILD_DIR) \
		-s build_type=$(CONAN_BUILD_TYPE) \
		-s compiler.cppstd=23 \
		-c tools.cmake.cmaketoolchain:user_presets="" \
		--build=missing
	@rm -f CMakeUserPresets.json

configure: conan-install
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		cache_source="$$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$(BUILD_DIR)/CMakeCache.txt")"; \
		if [ -n "$$cache_source" ] && [ "$$cache_source" != "$(CURDIR)" ]; then \
			echo "Removing stale CMake cache from $(BUILD_DIR) (was configured for $$cache_source)."; \
			rm -rf "$(BUILD_DIR)"; \
		fi; \
	fi
	$(CMAKE) --preset $(PRESET)

build: configure
	$(CMAKE) --build --preset $(PRESET)
	@if [ -f "$(BUILD_DIR)/compile_commands.json" ]; then \
		ln -sf "$(BUILD_DIR)/compile_commands.json" "$(COMPILE_COMMANDS_LINK)"; \
	fi

rebuild: clean build

compile-commands: configure
	@if [ -f "$(BUILD_DIR)/compile_commands.json" ]; then \
		ln -sf "$(BUILD_DIR)/compile_commands.json" "$(COMPILE_COMMANDS_LINK)"; \
	else \
		echo "compile_commands.json was not generated for preset '$(PRESET)'."; \
		exit 1; \
	fi

debug release asan tsan ubsan coverage:
	$(MAKE) build PRESET=$@

test: configure
	@mkdir -p "$(BUILD_DIR)"
	@if [ "$(PRESET)" = "$(COVERAGE_PRESET)" ]; then \
		LLVM_PROFILE_FILE="$(abspath $(LLVM_PROFILE_FILE))" \
			$(CTEST) --preset $(PRESET) --output-on-failure; \
	else \
		$(CTEST) --preset $(PRESET) --output-on-failure; \
	fi

lint: $(BUILD_DIR)/compile_commands.json
	@if [ -z "$(strip $(LINT_FILES))" ]; then \
		echo "No C++ source files found to lint."; \
		exit 0; \
	fi
	$(CLANG_TIDY) -p $(BUILD_DIR) $(LINT_FILES)

$(BUILD_DIR)/compile_commands.json: conan-install
	$(CMAKE) --preset $(PRESET)

format:
	@if [ -z "$(strip $(FORMAT_FILES))" ]; then \
		echo "No source files found to format."; \
		exit 0; \
	fi
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check:
	@if [ -z "$(strip $(FORMAT_FILES))" ]; then \
		echo "No source files found to check."; \
		exit 0; \
	fi
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

coverage-report:
	@profraw="$(LLVM_PROFILE_FILE)"; \
	if [ ! -f "$$profraw" ]; then \
		echo "Coverage profile '$$profraw' was not found."; \
		echo "Run 'make test PRESET=$(COVERAGE_PRESET)' or execute an instrumented binary first."; \
		echo "Example: LLVM_PROFILE_FILE=$$profraw build/$(COVERAGE_PRESET)/your-binary"; \
		exit 1; \
	fi; \
	bin="$(COVERAGE_BIN)"; \
	if [ -z "$$bin" ]; then \
		set -- $$(find "$(COVERAGE_BUILD_DIR)" -type f -perm -111 \
			! -path '*/CMakeFiles/*' 2>/dev/null); \
		case "$$#" in \
			0) \
				echo "No coverage executable found under '$(COVERAGE_BUILD_DIR)'."; \
				echo "Pass COVERAGE_BIN=/path/to/executable."; \
				exit 1 ;; \
			1) \
				bin="$$1" ;; \
			*) \
				echo "Multiple executables found under '$(COVERAGE_BUILD_DIR)'."; \
				echo "Pass COVERAGE_BIN=/path/to/executable. Candidates:"; \
				printf '  %s\n' "$$@"; \
				exit 1 ;; \
		esac; \
	fi; \
	echo "Merging profiles..."; \
	$(LLVM_PROFDATA) merge -sparse "$$profraw" -o "$(COVERAGE_BUILD_DIR)/default.profdata"; \
	echo "Generating HTML report -> $(COVERAGE_OUTPUT_DIR)/"; \
	$(LLVM_COV) show "$$bin" \
		-instr-profile="$(COVERAGE_BUILD_DIR)/default.profdata" \
		-format=html \
		-output-dir="$(COVERAGE_OUTPUT_DIR)"; \
	echo "Report: $(COVERAGE_OUTPUT_DIR)/index.html"

clean:
	rm -rf build "$(COMPILE_COMMANDS_LINK)"

help:
	@echo "Targets:"
	@echo "  build             Conan install + configure + build (PRESET=<name>, default: debug)"
	@echo "  configure         Conan install + configure only for PRESET=<name>"
	@echo "  conan-install     Install Conan dependencies for PRESET=<name>"
	@echo "  rebuild           Clean and build again"
	@echo "  compile-commands  Refresh root compile_commands.json symlink"
	@echo "  debug             Shorthand for 'make build PRESET=debug'"
	@echo "  release           Shorthand for 'make build PRESET=release'"
	@echo "  asan              Shorthand for 'make build PRESET=asan'"
	@echo "  tsan              Shorthand for 'make build PRESET=tsan'"
	@echo "  ubsan             Shorthand for 'make build PRESET=ubsan'"
	@echo "  coverage          Shorthand for 'make build PRESET=coverage'"
	@echo "  test              Run CTest for PRESET=<name>"
	@echo "  lint              Run clang-tidy for PRESET=<name>"
	@echo "  format            Auto-format sources with clang-format"
	@echo "  format-check      Check formatting without modifying files"
	@echo "  coverage-report   Generate HTML coverage report"
	@echo "                    Use COVERAGE_BIN=/path/to/executable when needed"
	@echo "  clean             Remove build artifacts and compile_commands.json link"
