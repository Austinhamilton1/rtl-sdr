add_executable(audio_test
    test_audio.cpp
)

target_link_libraries(audio_test PRIVATE audio)