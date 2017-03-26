# C Project Makefile

#========= Debug mode setup ========#
# debug.h macros.
DEBUG := -DDEBUG -DVERBOSE -UTRACE
NDEBUG := -UDEBUG -DVERBOSE -UTRACE

#===== Compiler / linker setup =====#
# gcc with MinGW setup.
CC := gcc
CFLAGS := -g -O3 -Wall -Wpedantic -Wextra -std=gnu99
DFLAGS := -MP -MMD
LFLAGS := -s -lm
INCLUDE := 
LIBRARY := 
IMPORTANT :=

#======== Source code setup ========#
# Directory for all project files and
# the main.c file.
INCLUDE_DIR := ./src
SRC_DIR := ./src

# Source files
# CFILES excluses MAIN
CFILES := $(wildcard $(SRC_DIR)/*.c)
HFILES := $(wildcard $(INCLUDE_DIR)/*.h)
IMPORTANT += $(SRC_DIR)

# Important files
MAKEFILE := Makefile
IMPORTANT += $(MAKEFILE) README.md

#=========== Build setup ===========#
# Directory for built files.
BUILD_DIR := build
OFILES := $(CFILES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DFILES := $(OFILES:%.o=%.d)

# Tar file of the entire project
TAR := ./project.tar.gz

#========= Source archive ==========#
# Archive name
ARCHIVE := $(BUILD_DIR)/liball.a

# Linking
INCLUDE += -I$(INCLUDE_DIR)
LIBRARY += -L$(BUILD_DIR) -lall

#=========== OpenGL Setup ==========#
ifeq ($(shell uname),Linux)
	CFLAGS += -UWINDOWS -DGLEW_STATIC
	LIBRARY += -lGLEW -lglut -lGL -lGLU -UWINDOWS
else
	CFLAGS += -DWINDOWS -DGLEW_STATIC
	LIBRARY += -lglew32 -lglut32win -lopengl32 -lglu32
endif

#========== libwes64 Setup =========#
LIB_DIR := lib
LIBWES64_DIR := $(LIB_DIR)/libwes64
INCLUDE += -I$(LIBWES64_DIR)/include
LIBRARY += -L$(LIBWES64_DIR)/bin -lwes64

#============ Main files ===========#
# Standalone text executable sources
# to link with the rest of the code.
MAIN_DIR := main
MCFILES := $(wildcard $(MAIN_DIR)/*.c)
MOFILES := $(MCFILES:$(MAIN_DIR)/%.c=$(BUILD_DIR)/$(MAIN_DIR)/%.o)
MDFILES := $(MOFILES:%.o=%.d)
IMPORTANT += $(MCFILES)

ALL_EXECUTABLES := $(MCFILES:$(MAIN_DIR)/%.c=%.exe)
TESTS := $(filter test_%.exe,$(ALL_EXECUTABLES))
EXECUTABLES := $(filter-out test_%.exe,$(ALL_EXECUTABLES))

#========== Documentation ==========#
# Doxygen documentation setup
DOC_DIR := docs
DOXYFILE := Doxyfile
DOXFILES := $(wildcard doxygen/*)
IMPORTANT += $(DOXYFILE) $(DOXFILES)

#============== Rules ==============#
# Default: just make executables
.PHONY: default
default: $(BUILD_DIR) libwes64 $(ARCHIVE) $(EXECUTABLES)

# Make just the tests
.PHONY: tests
tests: $(BUILD_DIR) $(ARCHIVE) $(TESTS)

# Default - make the executable
.PHONY: all
all: default tests

# Make libwes64
.PHONY: libwes64
libwes64:
	$(MAKE) -C $(LIBWES64_DIR) default

# Put all the .o files in the build directory
$(BUILD_DIR):
	-mkdir $@
	-mkdir $@/$(MAIN_DIR)

# Compile the source files
.SECONDARY: $(DFILES)
.SECONDARY: $(OFILES)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(MAKEFILE)
	$(CC) $(CFLAGS) $(DFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@
	
# Compile the main method executables
.SECONDARY: $(MDFILES)
.SECONDARY: $(MOFILES)
$(BUILD_DIR)/$(MAIN_DIR)/%.o: $(MAIN_DIR)/%.c $(MAKEFILE)
	$(CC) $(CFLAGS) $(DFLAGS) $(DEBUG) $(INCLUDE) -c $< -o $@

# Automatic dependency files
-include $(DFILES)
-include $(MDFILES)

# Documentation
.PHONY: documentation
documentation: $(DOC_DIR)
$(DOC_DIR): $(DOXFILES) $(CFILES) $(HFILES) $(MCFILES)
	doxygen Doxyfile

# Make archive of the code
.PHONY: archive
archive: $(ARCHIVE)
$(ARCHIVE): $(OFILES)
	ar -rcs $@ $^

# Make executable for each driver
%.exe: $(BUILD_DIR)/$(MAIN_DIR)/%.o $(ARCHIVE) 
	$(CC) -o $@ $< $(LIBRARY) $(LFLAGS)

#============== Clean ==============#
# Clean up build files and executable
.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR) $(EXECUTABLES) $(TESTS) $(ARCHIVE)
	
#============= Archive =============#
# Package all the files into a tar.
.PHONY: tar
tar: $(TAR)
$(TAR): $(IMPORTANT)
	tar -czvf $@ $^

#===================================#
