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
#include "material.h"

using namespace std::chrono_literals;

color ray_color(const ray& r, const hittable& world, int depth) {
    hit_record rec;
    // If we've exceeded the ray bounce limit, no more light is gathered.
    if (depth <= 0)
        return color(0, 0, 0);

    if (world.hit(r, epsilon, infinity, rec)) {
        ray scattered;
        color attenuation;
        if (rec.mat_ptr->scatter(r, rec, attenuation, scattered))
            return attenuation * ray_color(scattered, world, depth - 1);
        return color(0, 0, 0);
    }
    vec3 unit_direction = unit_vector(r.direction());
    auto t = 0.5 * (unit_direction.y() + 1.0);
    return (1.0 - t) * color(1.0, 1.0, 1.0) + t * color(0.5, 0.7, 1.0);
}

void do_render(const hittable_list& world, const camera& cam, int image_width, int image_height, unsigned char* image_data, int samples_per_pixel, int max_depth, bool& finish)
{
    std::thread render([&world, &cam, image_width, image_height, image_data, samples_per_pixel, max_depth, &finish]() {
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
    /* Initialize the library */
    if (!glfwInit())
        return 1;

    /* Create a windowed mode window and its OpenGL context */
    GLFWwindow* window = glfwCreateWindow(1280, 1080, "Dear ImGui GLFW+OpenGL4 example", NULL, NULL);
    if (!window)
        return 1;

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    if (!gladLoadGL())
        return 1;

    int image_size[2] = { 800, 600 };
    std::vector<unsigned char> image(image_size[0] * image_size[1] * 3);

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
    int render_samples = 128;
    int render_depth = 64;
    int look_from[3] = { 13, 2, 3 };
    int look_to[3] = { 0, 0, 0 };
    int view_up[3] = { 0, 1, 0 };
    int view_fov = 20;
    float cam_aperture = 0.1;
    float cam_focus_dist = 10.0;

    hittable_list world;
    world.add(make_shared<sphere>(point3(0, -1000, 0), 1000, make_shared<lambertian>(color(0.5, 0.5, 0.5))));
    for (int a = -11; a < 11; ++a) {
        for (int b = -11; b < 11; ++b) {
            auto choose_mat = random_double();
            point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());
            if ((center - vec3(4, 0.2, 0)).length() > 0.9) {
                if (choose_mat < 0.8) {
                    // diffuse
                    auto albedo = color::random() * color::random();
                    world.add(make_shared<sphere>(center, 0.2, make_shared<lambertian>(albedo)));
                } else if (choose_mat < 0.95) {
                    // metal
                    auto albedo = color::random(.5, 1);
                    auto fuzz = random_double(0, .5);
                    world.add(make_shared<sphere>(center, 0.2, make_shared<metal>(albedo, fuzz)));
                } else {
                    // glass
                    world.add(make_shared<sphere>(center, 0.2, make_shared<dielectric>(1.5)));
                }
            }
        }
    }
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0, make_shared<dielectric>(1.5)));
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0, make_shared<lambertian>(color(.4, .2, .1))));
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0, make_shared<metal>(color(.7, .6, .5), 0.0)));

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        /* Poll for and process events */
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Ray Tracing In One Weekend");
        if (ImGui::Button("Render")) {
            render_finish = false;
            render_start = std::chrono::system_clock::now();
            do_render(world, camera(
                point3(look_from[0], look_from[1], look_from[2]),
                point3(look_to[0], look_to[1], look_to[2]),
                point3(view_up[0], view_up[1], view_up[2]),
                view_fov, double(image_size[0]) / image_size[1], 0.1, 10.0),
                image_size[0], image_size[1], image.data(), render_samples, render_depth, render_finish);
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
        ImGui::DragInt("samples", &render_samples, 1, 0, INT_MAX);
        ImGui::DragInt("depth", &render_depth, 1, 0, INT_MAX);
        if (ImGui::SliderInt("#threads", &render_threads, 1, omp_get_num_procs())) {
            omp_set_num_threads(render_threads);
        }
        ImGui::DragInt("fov", &view_fov, 1, 0, 360);
        ImGui::DragFloat("aperture", &cam_aperture);
        ImGui::DragFloat("focut dist", &cam_focus_dist);
        ImGui::DragInt3("look from", look_from);
        ImGui::DragInt3("look at", look_to);
        ImGui::DragInt3("view up", view_up);
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
