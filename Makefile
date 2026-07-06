CXX = g++
UNAME := $(shell uname)

# ── Platform detection ────────────────────────────────────────────────────────

ifeq ($(UNAME), Darwin)
    PLATFORM = macos
else
    PLATFORM = linux
endif

# ── Compiler flags ────────────────────────────────────────────────────────────

COMMON_OPT = -O3 

ifeq ($(PLATFORM), macos)
    CXXFLAGS = -std=c++20 $(COMMON_OPT) -I/opt/homebrew/include \
               -Iwhisper.cpp -Iwhisper.cpp/include -Iwhisper.cpp/ggml/include \
               $(shell pkg-config --cflags opencv4)
    LDFLAGS  = -L/opt/homebrew/lib -Lwhisper.cpp/build/src \
               $(shell pkg-config --libs opencv4) \
               -framework ApplicationServices -framework Cocoa \
               -framework AVFoundation -framework CoreMedia \
               -Wl,-rpath,$(PWD)/whisper.cpp/build/src
    LIBS     = -lsfml-graphics -lsfml-window -lsfml-system \
               -lportaudio -lcurl -lobjc -lwhisper
else
    # Linux — SFML 3 is built from source into /usr/local by make setup.
    # pkg-config is NOT used for SFML because apt's libsfml-dev is SFML 2
    # and would cause linker errors with SFML 3 API calls.
    CXXFLAGS = -std=c++20 $(COMMON_OPT) \
               -I/usr/local/include \
               -Iwhisper.cpp -Iwhisper.cpp/include -Iwhisper.cpp/ggml/include \
               $(shell pkg-config --cflags opencv4) \
               $(shell pkg-config --cflags portaudio-2.0) \
               -Wno-deprecated-enum-enum-conversion
    LDFLAGS  = -L/usr/local/lib \
               -Lwhisper.cpp/build/src \
               $(shell pkg-config --libs opencv4) \
               -Wl,-rpath,$(PWD)/whisper.cpp/build/src \
               -Wl,-rpath,/usr/local/lib
    LIBS     = -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio \
               $(shell pkg-config --libs portaudio-2.0) \
               -lcurl -lwhisper -lpthread -lX11 -lXtst
endif

# ── Sources ───────────────────────────────────────────────────────────────────

TARGET = mainAssistant
SRCS = Landing.cpp action.cpp APIAction.cpp BrowserAction.cpp config.cpp \
		CameraThread.cpp MainAssistant.cpp MouseOutput.cpp \
       Settings.cpp SystemAction.cpp SystemCommandAction.cpp TimerAction.cpp \
       TimeAction.cpp ConversationLogger.cpp UI.cpp VisionInput.cpp \
       WeatherAction.cpp VoiceCommandListener.cpp UIHelpers.cpp UIWidgets.cpp \
       AudioManager.cpp History.cpp QuickReference.cpp

# ── Rules ─────────────────────────────────────────────────────────────────────

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET) $(TEST_TARGET) *.o settings.ini conversation.json

# ── Setup ─────────────────────────────────────────────────────────────────────

ifeq ($(PLATFORM), macos)
setup:
	brew install opencv sfml portaudio curl pkg-config brightness cmake
	/usr/bin/python3 -m venv venv
	./venv/bin/pip install mediapipe==0.10.9 opencv-python matplotlib
	@if [ ! -f "whisper.cpp/CMakeLists.txt" ]; then \
		rm -rf whisper.cpp; \
		git clone https://github.com/ggerganov/whisper.cpp; \
	fi
	cmake -S whisper.cpp -B whisper.cpp/build -DWHISPER_METAL=ON
	cmake --build whisper.cpp/build --config Release
	@if [ ! -f "ggml-medium.en.bin" ]; then \
		curl -L -o ggml-medium.en.bin \
		https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin; \
	fi
else ifeq ($(PLATFORM), linux)
setup:
	sudo apt-get update
	sudo apt-get install -y \
		libopencv-dev portaudio19-dev libcurl4-openssl-dev pkg-config \
		cmake git python3-venv python3-pip libfreetype6-dev libxrandr-dev \
		libxcursor-dev libxi-dev libxtst-dev libx11-dev libudev-dev \
		libogg-dev libflac-dev libvorbis-dev libgl1-mesa-dev libegl1-mesa-dev \
		espeak espeak-ng libespeak-ng-dev speech-dispatcher \
		xdotool xclip xsel pulseaudio-utils brightnessctl scrot gnome-screenshot
	python3 -m venv venv
	./venv/bin/pip install mediapipe==0.10.13 opencv-python matplotlib
	@# ── Build SFML 3 ──────────────────────────────────────────────────────
	@if [ ! -f "/usr/local/lib/libsfml-graphics.so" ]; then \
		echo "Building SFML 3 from source..."; \
		rm -rf SFML; \
		git clone --branch 3.0.0 https://github.com/SFML/SFML.git; \
		cmake -S SFML -B SFML/build -DCMAKE_BUILD_TYPE=Release; \
		cmake --build SFML/build -- -j$$(nproc); \
		sudo cmake --install SFML/build; \
		sudo ldconfig; \
		rm -rf SFML; \
	fi
	@# ── Build whisper.cpp (FORCE CLEAN TO FIX PATH ERRORS) ────────────────
	@if [ ! -d "whisper.cpp" ]; then \
		git clone https://github.com/ggerganov/whisper.cpp; \
	fi
	@# Remove the build folder to clear the Windows CMakeCache.txt
	rm -rf whisper.cpp/build
	cmake -S whisper.cpp -B whisper.cpp/build -DWHISPER_CUBLAS=OFF
	cmake --build whisper.cpp/build --config Release -- -j$$(nproc)
	@# ── Download whisper model ────────────────────────────────────────────
	@if [ ! -f "ggml-medium.en.bin" ]; then \
		curl -L -o ggml-medium.en.bin \
		https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium.en.bin; \
	fi

endif

