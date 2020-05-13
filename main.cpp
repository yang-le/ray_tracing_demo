#include <iostream>
#include <thread>
#include <chrono>

#include <glad/glad.h> 
// Include glfw3.h after our OpenGL definitions
#include <GLFW/glfw3.h>
#include <omp.h>

#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "rtweekend.h"
#include "hittable_list.h"
#include "sphere.h"
#include "color.h"
#include "camera.h"

using namespace std::chrono_literals;

color ray_color(const ray& r, const hittable& world, int depth) {
    hit_record rec;
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0, 0, 0);

    if (world.hit(r, epsilon, infinity, rec)) {
        point3 target = rec.p + vec3::random_in_hemisphere(rec.normal);
        return 0.5 * ray_color(ray(rec.p, target - rec.p), world, depth - 1);
    }
    vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

void do_render(int image_width, int image_height, unsigned char* image_data, bool& finish)
{
    std::thread render([image_width, image_height, image_data, &finish]() {
        const int samples_per_pixel = 128;
        const int max_depth = 64;

        hittable_list world;
        world.add(make_shared<sphere>(point3(0, 0, -1), 0.5));
        world.add(make_shared<sphere>(point3(0, -100.5, -1), 100));
        camera cam;

        #pragma omp parallel for
        for (int j = 0; j < image_height; ++j) {
            //std::cerr << "\rScanlines remaining: " << j << ' ' << std::flush;
            for (int i = 0; i < image_width; ++i) {
                color pixel_color(0, 0, 0);
                for (int s = 0; s < samples_per_pixel; ++s) {
                    auto u = (i + random_double()) / (image_width - 1);
                    auto v = (image_height - 1 - j + random_double()) / (image_height - 1);
                    ray r = cam.get_ray(u, v);
                    pixel_color += ray_color(r, world, max_depth);
                }
                write_color(&image_data[3 * (j * image_width + i)], pixel_color, samples_per_pixel);
            }
            if (finish) break;
        }
        finish = true;
    });
    render.detach();
}

int main(void)
{
    auto aspect_ratio = 16.0 / 9.0;
    int image_size[2] = { 1024, image_size[0] / aspect_ratio };
    std::vector<unsigned char> image(image_size[0] * image_size[1] * 3);

    /* Initialize the library */
    if (!glfwInit())
        return 1;

    /* Create a windowed mode window and its OpenGL context */
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL4 example", NULL, NULL);
    if (!window)
        return 1;

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    if (!gladLoadGL())
        return 1;

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload pixels into texture
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_size[0], image_size[1], 0, GL_RGB, GL_UNSIGNED_BYTE, image.data());
    glBindTexture(GL_TEXTURE_2D, NULL);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    bool show_demo_window = false;
    bool render_finish = true;
    auto render_start = std::chrono::system_clock::now();
    int render_time = 0;
    int render_threads = omp_get_max_threads();

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        /* Poll for and process events */
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("OpenGL Texture Text");
        if (ImGui::Button("Render")) {
            render_finish = false;
            render_start = std::chrono::system_clock::now();
            do_render(image_size[0], image_size[1], image.data(), render_finish);
        }
        ImGui::SameLine();
        ImGui::Text("time = %ds", render_time);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            render_finish = true;
            memset(image.data(), 0, image.size());
            glBindTexture(GL_TEXTURE_2D, image_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_size[0], image_size[1], GL_RGB, GL_UNSIGNED_BYTE, image.data());
            glBindTexture(GL_TEXTURE_2D, NULL);
            render_time = 0;
        }
        if (ImGui::SliderInt("#threads", &render_threads, 1, omp_get_num_procs())) {
            omp_set_num_threads(render_threads);
        }
        if (ImGui::DragInt2("size", image_size, 1, 1, INT_MAX)) {
            image.resize(image_size[0] * image_size[1] * 3);
            glBindTexture(GL_TEXTURE_2D, image_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image_size[0], image_size[1], 0, GL_RGB, GL_UNSIGNED_BYTE, image.data());
            glBindTexture(GL_TEXTURE_2D, NULL);
        }
        ImGui::Image((ImTextureID)image_texture, ImVec2(image_size[0], image_size[1]));
        ImGui::Checkbox("Demo Window", &show_demo_window);
        ImGui::End();

        if (show_demo_window)
            ImGui::ShowDemoWindow();

        // Rendering
        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!render_finish) {
            glBindTexture(GL_TEXTURE_2D, image_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, image_size[0], image_size[1], GL_RGB, GL_UNSIGNED_BYTE, image.data());
            glBindTexture(GL_TEXTURE_2D, NULL);
            render_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - render_start).count();
        }

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        std::this_thread::sleep_for(10ms);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}
