# dawn-mre-android
1. Clone dawn.
2. Compile with `cmake -B build -G Ninja -Wno-dev -DCMAKE_TOOLCHAIN_FILE=~/bin/android-ndk-r27/build/cmake/android.toolchain.cmake  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DDAWN_ENABLE_VULKAN=ON -DDAWN_ENABLE_OPENGLES=ON -DCMAKE_BUILD_TYPE=Release`
3. `cmake --build build --target mre`
4. Run it with `./mre bug.wgsl`

Alternatively, get the apk from the most recent Github Action.
