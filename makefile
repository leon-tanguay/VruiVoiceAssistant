########################################################################
# Makefile for the Vrui Voice Assistant ("Hey Vera"), v2.
#
# This is a standalone Vrui add-on, modeled on the Kinect 3D Video
# Capture Project -- same pattern as v1's makefile. It builds two Vrui
# plug-ins -- a vislet (the assistant brain + on-screen orb) and two
# input tools (push-to-talk) -- against an *installed* Vrui, and `make
# install` drops them, plus the assistant's small runtime assets, into
# that existing Vrui installation. The Vrui source tree is never touched.
#
# Quick build:
#   make VRUI_MAKEDIR=/usr/local/share/Vrui-15.0/make -j$(nproc)
#   sudo make VRUI_MAKEDIR=/usr/local/share/Vrui-15.0/make install
#
# Changes from v1, and why (see ../vrui-assistant-v2/DEPENDENCIES.md):
#   - VRUI_MAKEDIR is validated instead of trusted (this box has both
#     Vrui-15.0 and Vrui-14.1 installed; pointing at the wrong one used
#     to fail silently or produce a stale build).
#   - getCommandMap() availability is detected, not assumed, and baked
#     into Config.h so VoiceAssistant.cpp can #if around it.
#   - -Wall -Wextra -Wreorder -Wshadow are on from the start.
#   - `check-deps` reports missing system libraries with the exact apt
#     command, instead of a bare linker error.
#   - Sources are discovered by wildcard (see the SOURCES comment below)
#     instead of hand-listed, so a new file just needs to exist.
########################################################################

# Directory containing the Vrui build system. The default below matches a
# standard Vrui installation; if Vrui was installed elsewhere, override it
# on the command line, e.g. make VRUI_MAKEDIR=/some/where/Vrui-15.0/make
VRUI_MAKEDIR = /usr/local/share/Vrui-15.0/make

########################################################################
# Preflight: fail loudly on a bad VRUI_MAKEDIR rather than building
# against whatever partial fragments happen to be there.
########################################################################

ifeq ($(wildcard $(VRUI_MAKEDIR)/Configuration.Vrui),)
  $(error VRUI_MAKEDIR=$(VRUI_MAKEDIR) does not look like a Vrui build \
    directory (no Configuration.Vrui there). Pass the correct one, e.g. \
    make VRUI_MAKEDIR=/usr/local/share/Vrui-15.0/make)
endif

########################################################################
# Everything below here should not have to be changed
########################################################################

PROJECT_NAME = VoiceAssistant
PROJECT_DISPLAYNAME = Vrui Voice Assistant

# Version of the created plug-ins and of the installed asset directory
# ($(VRUI_SHAREINSTALLDIR)/VoiceAssistant-<major>.<minor>):
PROJECT_MAJOR = 2
PROJECT_MINOR = 0
PROJECT_NUMERICVERSION = 2000

# Include definitions for the system environment and Vrui-provided packages:
include $(VRUI_MAKEDIR)/SystemDefinitions
include $(VRUI_MAKEDIR)/Packages.System
include $(VRUI_MAKEDIR)/Configuration.Vrui
include $(VRUI_MAKEDIR)/Packages.Vrui

# Package definition for this project (lets other projects depend on it):
include $(PROJECT_ROOT)/BuildRoot/Packages.VoiceAssistant

# Turn on warnings for every object this framework compiles. -Wreorder
# catches an initialiser list that doesn't match declaration order;
# -Wshadow catches the "s" constructor-parameter convention being broken.
# See STYLE_GUIDE.md section 6.
CFLAGS += -Wall -Wextra -Wreorder -Wshadow

# Detect whether this Vrui's Misc::CommandDispatcher exposes getCommandMap().
# It does on this box's Vrui-15.0 and does not on the also-installed
# Vrui-14.1; VoiceAssistant.cpp needs to know which, at compile time, rather
# than assume it (see DEPENDENCIES.md #1-2).
HAVE_GETCOMMANDMAP := $(shell grep -qs getCommandMap \
  $(VRUI_INCLUDEDIR)/Misc/CommandDispatcher.h 2>/dev/null && echo 1 || echo 0)

########################################################################
# Source layout and vendored third-party engines
########################################################################

# Directory holding this project's C++ sources:
PROJECT_SRCDIR = VoiceAssistant

# Vendored native engines living in the source tree (gitignored, fetched
# by scripts/download_models.sh). libvosk is linked into the vislet; the
# standalone piper binary is exec'd at runtime.
VOSK_BUILDDIR  = $(PROJECT_ROOT)/$(PROJECT_SRCDIR)/third_party/vosk
PIPER_BUILDDIR = $(PROJECT_ROOT)/$(PROJECT_SRCDIR)/third_party/piper

########################################################################
# Installation directories (under the existing Vrui installation)
########################################################################

# Small runtime assets (catalog, earcons, vendored libvosk + piper) live
# beside Vrui's other resources, in a versioned sub-directory. The earcon
# WAVs are for OpenAL's sound()/AL_POSITION path (see DEPENDENCIES.md
# #11 -- sound should come from the orb's 3D location, which needs
# OpenAL enabled in this box's Vrui build; the WAVs and the path that
# plays them are a real, required feature, not dead code).
SHAREINSTALLDIR  = $(VRUI_SHAREINSTALLDIR)/$(PROJECT_FULLNAME)
SOUNDSINSTALLDIR = $(SHAREINSTALLDIR)/sounds
VOSKINSTALLDIR   = $(SHAREINSTALLDIR)/lib
PIPERINSTALLDIR  = $(SHAREINSTALLDIR)/piper

# Plug-ins go into Vrui's standard plug-in directories so Vrui finds them:
VISLETINSTALLDIR = $(VRUI_PLUGININSTALLDIR)/$(VRVISLETSDIREXT)
TOOLINSTALLDIR   = $(VRUI_PLUGININSTALLDIR)/$(VRTOOLSDIREXT)
MAKEINSTALLDIR   = $(VRUI_MAKEINSTALLDIR)

########################################################################
# Final targets
########################################################################

# Local build-output locations for the two plug-ins (mirrors the install
# sub-directory names under Vrui's local plug-in output root):
PROJECT_VISLETDIR = $(PROJECT_ROOT)/$(PLUGINDIR)/$(VRVISLETSDIREXT)
PROJECT_TOOLDIR   = $(PROJECT_ROOT)/$(PLUGINDIR)/$(VRTOOLSDIREXT)

VISLET     := $(PROJECT_VISLETDIR)/libVoiceAssistant.$(PLUGINFILEEXT)
TOOL       := $(PROJECT_TOOLDIR)/libVoiceAssistantTool.$(PLUGINFILEEXT)
NUMBERTOOL := $(PROJECT_TOOLDIR)/libVoiceAssistantNumberTool.$(PLUGINFILEEXT)

ALL = $(VISLET) $(TOOL) $(NUMBERTOOL)

.PHONY: all
all: $(ALL)

########################################################################
# Configuration: generate VoiceAssistant/Config.h from Config.h.template,
# baking in the install-time asset directory and the getCommandMap probe
# above. Config.h is a real file target (not a stamp), so it is
# regenerated whenever it is missing or the template changes; the plug-in
# objects depend on it (declared after the BasicMakefile include). The
# diff-guard avoids needless recompiles when the baked value is unchanged.
########################################################################

CONFIGFILE = $(PROJECT_SRCDIR)/Config.h

$(CONFIGFILE): $(PROJECT_SRCDIR)/Config.h.template
	@echo "Configuring $(PROJECT_DISPLAYNAME): asset dir $(SHAREINSTALLDIR), getCommandMap=$(HAVE_GETCOMMANDMAP)"
	@cp $< $@.temp
	@$(call CONFIG_SETSTRINGVAR,$@.temp,VOICEASSISTANT_CONFIG_SHAREDIR,$(SHAREINSTALLDIR))
	@sed -i -e 's/\#define VOICEASSISTANT_CONFIG_HAVE_GETCOMMANDMAP .*/\#define VOICEASSISTANT_CONFIG_HAVE_GETCOMMANDMAP $(HAVE_GETCOMMANDMAP)/' $@.temp
	@if ! diff -qN $@.temp $@ >/dev/null 2>&1 ; then mv -f $@.temp $@ ; else rm -f $@.temp ; fi

.PHONY: config
config: $(CONFIGFILE)

########################################################################
# check-deps: report missing system libraries with the exact fix, instead
# of a bare linker error partway through a long build.
########################################################################

.PHONY: check-deps
check-deps:
	@echo "Checking system dependencies..."
	@ok=1; \
	for pc in libcurl espeak-ng libpulse libpulse-simple; do \
	  pkg-config --exists $$pc 2>/dev/null || ldconfig -p | grep -q "lib$${pc#lib}" || { \
	    echo "  missing: $$pc"; ok=0; }; \
	done; \
	if [ $$ok -eq 0 ]; then \
	  echo "Fix with:"; \
	  echo "  sudo apt install libcurl4-openssl-dev libespeak-ng-dev libpulse-dev unzip"; \
	  exit 1; \
	else \
	  echo "All system dependencies present."; \
	fi

########################################################################
# Cleanup hooks (invoked by BasicMakefile's clean / squeakyclean)
########################################################################

.PHONY: extraclean
extraclean:
	-rm -f $(CONFIGFILE)
	-rm -f $(ALL)

# Include the basic makefile (compile/link pattern rules, clean, etc.):
include $(VRUI_MAKEDIR)/BasicMakefile

########################################################################
# Build rules for the plug-ins
########################################################################

# C++ sources making up the self-contained vislet "brain". Discovered by
# wildcard rather than hand-listed: drop a new .cpp into VoiceAssistant/
# and it is picked up automatically, EXCEPT the two tool sources, which
# are their own separate plug-in targets below and must not be linked
# into the vislet as well.
VOICEASSISTANT_SOURCES := $(filter-out \
  $(PROJECT_SRCDIR)/VoiceAssistantTool.cpp $(PROJECT_SRCDIR)/VoiceAssistantNumberTool.cpp, \
  $(wildcard $(PROJECT_SRCDIR)/*.cpp))

# Make all plug-in objects depend on the generated configuration (Config.h):
$(call PLUGINOBJNAMES,$(VOICEASSISTANT_SOURCES) $(PROJECT_SRCDIR)/VoiceAssistantTool.cpp $(PROJECT_SRCDIR)/VoiceAssistantNumberTool.cpp): $(CONFIGFILE)

# Vendored libvosk: link the in-tree copy, and record rpaths to BOTH the
# build-tree copy (so it runs uninstalled, e.g. with -dir) and the installed
# copy (so it runs after `make install`). The remaining libs are system ones.
VOICEASSISTANT_EXTRALIBS = -L$(VOSK_BUILDDIR) -Wl,-rpath,$(VOSK_BUILDDIR) -Wl,-rpath,$(VOSKINSTALLDIR) \
                           -lvosk -lcurl -lespeak-ng -lpulse-simple -lpulse

# Keep the PIC object files from being deleted as pattern-rule intermediates:
.SECONDARY: $(call PLUGINOBJNAMES,$(VOICEASSISTANT_SOURCES) $(PROJECT_SRCDIR)/VoiceAssistantTool.cpp $(PROJECT_SRCDIR)/VoiceAssistantNumberTool.cpp)

#
# The vislet (assistant brain + on-screen orb). Sound (mic capture), IO and
# Threads are always needed. OpenAL is linked when this Vrui build has it
# (SYSTEM_HAVE_OPENAL, from Configuration.Vrui) -- it's required for the
# orb's spatial audio (DEPENDENCIES.md #11), not an optional extra; on a
# Vrui build without it, the #if SYSTEM_HAVE_OPENAL guards in the source
# degrade gracefully rather than failing to link.
# Target-specific PACKAGES/CFLAGS propagate to every PIC object compile.
#
$(VISLET): PACKAGES = MYVRUI MYGLGEOMETRY MYGLSUPPORT MYGLWRAPPERS MYGEOMETRY MYMATH MYMISC MYIO MYTHREADS MYSOUND MYALSUPPORT GL
ifneq ($(SYSTEM_HAVE_OPENAL),0)
  $(VISLET): PACKAGES += OPENAL
endif
$(VISLET): CFLAGS += -I$(PROJECT_SRCDIR)/third_party/vosk
$(VISLET): $(call PLUGINOBJNAMES,$(VOICEASSISTANT_SOURCES))
	@mkdir -p $(PROJECT_VISLETDIR)
	@echo Linking $@...
	@$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $^ $(PLUGINLFLAGS) $(VOICEASSISTANT_EXTRALIBS)

#
# The push-to-talk tool (command mode).
#
$(TOOL): PACKAGES = MYVRUI MYGLGEOMETRY MYGLSUPPORT MYGLWRAPPERS MYGEOMETRY MYMATH MYMISC GL
$(TOOL): $(call PLUGINOBJNAMES,$(PROJECT_SRCDIR)/VoiceAssistantTool.cpp)
	@mkdir -p $(PROJECT_TOOLDIR)
	@echo Linking $@...
	@$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $^ $(PLUGINLFLAGS)

#
# The push-to-talk tool (number-dictation mode).
#
$(NUMBERTOOL): PACKAGES = MYVRUI MYGLGEOMETRY MYGLSUPPORT MYGLWRAPPERS MYGEOMETRY MYMATH MYMISC GL
$(NUMBERTOOL): $(call PLUGINOBJNAMES,$(PROJECT_SRCDIR)/VoiceAssistantNumberTool.cpp)
	@mkdir -p $(PROJECT_TOOLDIR)
	@echo Linking $@...
	@$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $^ $(PLUGINLFLAGS)

########################################################################
# Convenience: compile a single object without linking, so you can check
# one file for errors/warnings while it's still the only one written.
# Usage: make VRUI_MAKEDIR=... obj FILE=WakeDetector
########################################################################

.PHONY: obj
obj:
	@test -n "$(FILE)" || { echo "Usage: make obj FILE=<name, no .cpp>"; exit 1; }
	@$(MAKE) --no-print-directory VRUI_MAKEDIR=$(VRUI_MAKEDIR) \
	  $(call PLUGINOBJNAMES,$(PROJECT_SRCDIR)/$(FILE).cpp)

########################################################################
# Installation
########################################################################

# INSTALLPREFIX is an optional staging prefix for packaging (like DESTDIR). It is
# prepended to the install *destinations* only; the real (un-prefixed) directories
# above are what get baked into the vislet's rpath and into Config.h, so those stay
# correct regardless of staging. The *INSTALLDIR vars are absolute, so no separator
# is needed: empty INSTALLPREFIX leaves them as-is, a non-empty one stages them.
INSTALLPREFIX ?=

.PHONY: install
install: $(ALL)
	@echo Installing $(PROJECT_DISPLAYNAME) into the Vrui installation...
# Install the plug-ins into Vrui's plug-in directories:
	@echo Installing plug-ins...
	@install -d $(INSTALLPREFIX)$(VISLETINSTALLDIR)
	@install $(VISLET) $(INSTALLPREFIX)$(VISLETINSTALLDIR)
	@install -d $(INSTALLPREFIX)$(TOOLINSTALLDIR)
	@install $(TOOL) $(INSTALLPREFIX)$(TOOLINSTALLDIR)
	@install $(NUMBERTOOL) $(INSTALLPREFIX)$(TOOLINSTALLDIR)
# Install the command catalog and earcon WAVs:
	@echo Installing runtime assets...
	@install -d $(INSTALLPREFIX)$(SHAREINSTALLDIR)
	@install -m u=rw,go=r $(PROJECT_SRCDIR)/VoiceAssistant-catalog.json $(INSTALLPREFIX)$(SHAREINSTALLDIR)
	@install -d $(INSTALLPREFIX)$(SOUNDSINSTALLDIR)
	@install -m u=rw,go=r $(PROJECT_SRCDIR)/sounds/* $(INSTALLPREFIX)$(SOUNDSINSTALLDIR)
# Install the vendored libvosk shared library (the vislet rpaths to here):
	@echo Installing vendored libvosk...
	@install -d $(INSTALLPREFIX)$(VOSKINSTALLDIR)
	@install -m u=rwx,go=rx $(VOSK_BUILDDIR)/libvosk.so $(INSTALLPREFIX)$(VOSKINSTALLDIR)
# Install the vendored standalone piper (binary + sibling libs + espeak-ng-data);
# cp -a preserves the symlinks and the binary's $ORIGIN-relative siblings:
	@echo Installing vendored piper...
	@install -d $(INSTALLPREFIX)$(PIPERINSTALLDIR)
	@cp -a $(PIPER_BUILDDIR)/. $(INSTALLPREFIX)$(PIPERINSTALLDIR)/
	@rm -f $(INSTALLPREFIX)$(PIPERINSTALLDIR)/.gitignore $(INSTALLPREFIX)$(PIPERINSTALLDIR)/fetch-piper.sh
	@echo Done. Run scripts/download_models.sh to fetch the STT/TTS models.

.PHONY: uninstall
uninstall:
	@echo Uninstalling $(PROJECT_DISPLAYNAME)...
	@rm -f $(INSTALLPREFIX)$(VISLETINSTALLDIR)/libVoiceAssistant.$(PLUGINFILEEXT)
	@rm -f $(INSTALLPREFIX)$(TOOLINSTALLDIR)/libVoiceAssistantTool.$(PLUGINFILEEXT)
	@rm -f $(INSTALLPREFIX)$(TOOLINSTALLDIR)/libVoiceAssistantNumberTool.$(PLUGINFILEEXT)
	@rm -rf $(INSTALLPREFIX)$(SHAREINSTALLDIR)
