# ALB Language Interpreter v0.1.1 - macOS Makefile
# Requires: g++ (Xcode Command Line Tools) with C++11 support
#
# Usage:
#   make            - Build ALB interpreter
#   make app        - Build ALB.app bundle (with ALB.ico icon)
#   make dmg        - Build ALB.dmg installer (includes ALB.app + ALB_IDE.app)
#   make clean      - Remove build artifacts
#   make install    - Install to /usr/local/bin (optional, needs sudo)

CXX      ?= g++
# macOS: clang++ needs SDK path for C++ headers
SDK_PATH  = $(shell xcrun --show-sdk-path 2>/dev/null)
CXXFLAGS ?= -std=c++11 -O2 $(if $(SDK_PATH),-isysroot $(SDK_PATH) -isystem $(SDK_PATH)/usr/include/c++/v1,)
LDFLAGS  ?= -ldl -lc++

BIN      = ALB
SRC      = main.cpp
APP      = $(BIN).app
DMG      = $(BIN)-Installer.dmg
ICON_SRC = ALB.ico
ICON_DST = $(APP)/Contents/Resources/$(BIN).icns
CREATE_DMG = create-dmg/create-dmg

.PHONY: all clean check install app dmg

all: check $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC) $(LDFLAGS)

app: $(BIN)
	@echo "==> Building $(APP)..."
	@rm -rf $(APP)
	@mkdir -p $(APP)/Contents/MacOS
	@mkdir -p $(APP)/Contents/Resources
	@cp $(BIN) $(APP)/Contents/Resources/$(BIN)
	@chmod 755 $(APP)/Contents/Resources/$(BIN)
	@python3 -c "from PIL import Image; img=Image.open('$(ICON_SRC)'); img.save('$(ICON_DST)', format='ICNS')"
	@cp ALB_Launcher.sh $(APP)/Contents/MacOS/ALB_Launcher
	@sed -i '' 's|/ALB"|/$(BIN)"|' $(APP)/Contents/MacOS/ALB_Launcher; chmod 755 $(APP)/Contents/MacOS/ALB_Launcher
	@printf '<?xml version="1.0" encoding="UTF-8"?>\n\
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n\
<plist version="1.0">\n\
<dict>\n\
  <key>CFBundleName</key><string>ALB</string>\n\
  <key>CFBundleDisplayName</key><string>ALB Interpreter</string>\n\
  <key>CFBundleIdentifier</key><string>com.alb.interpreter</string>\n\
  <key>CFBundleVersion</key><string>0.1.1</string>\n\
  <key>CFBundleShortVersionString</key><string>0.1.1</string>\n\
  <key>CFBundleExecutable</key><string>ALB_Launcher</string>\n\
  <key>CFBundleIconFile</key><string>ALB</string>\n\
  <key>CFBundlePackageType</key><string>APPL</string>\n\
  <key>LSMinimumSystemVersion</key><string>10.13</string>\n\
  <key>NSHighResolutionCapable</key><true/>\n\
</dict>\n\
</plist>' > $(APP)/Contents/Info.plist
	@codesign --force --deep --sign - $(APP)
	@echo "==> Done! $(APP) created."
	@ls -lh $(ICON_DST)

dmg: app
	@echo "==> Building $(DMG)..."
	@rm -f $(DMG)
	@rm -rf dmg_staging
	@mkdir -p dmg_staging
	@cp -R $(APP) dmg_staging/
	@test -d IDE/dist/ALB_IDE.app && cp -R IDE/dist/ALB_IDE.app dmg_staging/ || echo "(IDE app not found, skipping)"
	@$(CREATE_DMG) \
		--volname "ALB v0.1.1" \
		--window-size 600 400 \
		--icon-size 100 \
		--icon "$(APP)" 150 200 \
		--app-drop-link 450 200 \
		--overwrite \
		$(DMG) \
		dmg_staging
	@rm -rf dmg_staging
	@echo "==> Done! $(DMG) created."
	@ls -lh $(DMG)

clean:
	rm -f $(BIN)
	rm -rf $(APP)
	rm -f $(DMG)
	rm -rf dmg_staging

check:
	@which $(CXX) > /dev/null 2>&1 || (echo "ERROR: g++ not found." && \
		echo "Please install Xcode Command Line Tools:" && \
		echo "  xcode-select --install" && \
		echo "Or install via Homebrew:" && \
		echo "  brew install gcc" && \
		exit 1)

install: $(BIN)
	install -m 755 $(BIN) /usr/local/bin/$(BIN)
	@echo "Installed to /usr/local/bin/$(BIN)"
