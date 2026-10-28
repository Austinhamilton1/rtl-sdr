#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will automatically include system OpenGL headers

#include <algorithm>
#include <cmath>
#include <iostream>

#include "backends/am_radio.h"

int main(int argc, char **argv) {
    if(argc != 6) {
        std::cerr << "Inavalid usage: "
                  << "radio {center_frequency} {iq_sample_rate} "
                  << "{audio_sample_rate} {channel_cutoff} "
                  << "{audio_cutoff}\n";
        return 1;
    }

    RadioConfig config;
    config.center_frequency = std::stoul(argv[1]);
    config.iq_sample_rate = std::stoull(argv[2]);
    config.audio_sample_rate = std::stoull(argv[3]);
    config.channel_cutoff = std::stoul(argv[4]);
    config.audio_cutoff = std::stoul(argv[5]);
    config.audio_gain = 20.0f * std::log10(0.5f);

    AMReceiver am;
    am.initialize(config);
    am.set_station_frequency(config.center_frequency);

    // 1. Initialize GLFW
    if (!glfwInit()) return -1;

    // Decide GL+GLSL versions
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create a Linux window with GLFW
    GLFWwindow* window = glfwCreateWindow(400, 300, "AM Radio Interface (GLFW/GL3)", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // 2. Initialize Dear ImGui Core and Backends
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Start the radio
    am.run();

    // 3. Define your radio state variables
    float volume = 50.0f;                               // 0 to 100
    int frequency = config.center_frequency / 1000;     // AM Frequency in Hz
    
    // Equalizer variables (for later)
    float bass = 0.0f;
    float mid = 0.0f;
    float treble = 0.0f;

    // Main App Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 4. Draw your simple Radio UI
        // Center the ImGui panel perfectly in the window workspace
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        ImGui::SetNextWindowSize(ImVec2((float)display_w, (float)display_h));

        ImGui::Begin("AM Radio Control Panel", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
        ImGui::Text("AM Radio Controls");
        ImGui::Separator();

        // --- Volume Control ---
        ImGui::Text("Volume");
        if(ImGui::SliderFloat("##VolumeSlider", &volume, 0.0f, 100.0f, "%.0f%%")) {
            if(volume <= 0.0f) {
                am.set_audio_gain(-100.0f); // Effectively muted
            } else {
                const float linear_gain = volume / 100.0f;
                const float gain_db = 20.0f * std::log10(linear_gain);
                am.set_audio_gain(gain_db);
            }
        }
        
        ImGui::Separator();

        // --- Tuner Control ---
        ImGui::Text("AM Tuner (kHz)");
        bool frequency_changed = ImGui::SliderInt(
            "##TunerSlider", &frequency, 530, 1700, "%d kHz"
        );

        if (ImGui::Button("-1 kHz")) {
            const int previous = frequency;
            frequency = std::max(530, frequency - 1);
            frequency_changed |= frequency != previous;
        }

        ImGui::SameLine();

        if (ImGui::Button("+1 kHz")) {
            const int previous = frequency;
            frequency = std::min(1700, frequency + 1);
            frequency_changed |= frequency != previous;
        }

        if (frequency_changed) {
            am.set_station_frequency(
                static_cast<uint32_t>(frequency * 1000)
            );
        }

        // --- Future Equalizer Slot ---
        /*
        ImGui::Separator();
        ImGui::Text("Equalizer");
        ImGui::SliderFloat("Bass", &bass, -10.0f, 10.0f);
        ImGui::SliderFloat("Mid", &mid, -10.0f, 10.0f);
        ImGui::SliderFloat("Treble", &treble, -10.0f, 10.0f);
        */

        ImGui::End();

        // 5. Render everything to the screen via OpenGL
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    am.stop();

    // Cleanup backends
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}