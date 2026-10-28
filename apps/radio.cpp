#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will automatically include system OpenGL headers
#include <algorithm>

int main() {
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

    // 3. Define your radio state variables
    float volume = 50.0f;       // 0 to 100
    int frequency = 880;        // AM Frequency in kHz
    
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
        ImGui::SliderFloat("##VolumeSlider", &volume, 0.0f, 100.0f, "%.0f%%");
        
        ImGui::Separator();

        // --- Tuner Control ---
        ImGui::Text("AM Tuner (kHz)");
        ImGui::SliderInt("##TunerSlider", &frequency, 530, 1700, "%d kHz");
        
        // Quick-tune buttons
        if (ImGui::Button("-1 kHz")) { frequency = std::max(530, frequency - 1); }
        ImGui::SameLine();
        if (ImGui::Button("+1 kHz")) { frequency = std::min(1700, frequency + 1); }

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

    // Cleanup backends
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}