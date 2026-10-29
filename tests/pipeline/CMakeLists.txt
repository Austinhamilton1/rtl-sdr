add_executable(pipeline_test
    test_pipeline.cpp
)

target_link_libraries(pipeline_test
PRIVATE 
    audio
    backends
    dsp
    radio_sdr
)