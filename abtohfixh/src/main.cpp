#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include   <vector>

using namespace std;

void framebuffer_size_callback(GLFWwindow* w, int x, int y) {
    glViewport(0, 0, x, y);
}

string load(const char* p) {
    ifstream f(p);
    stringstream s;
    s << f.rdbuf();
    return s.str();
}

struct Building {
    float x; float y; float w; float h;
};




int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* w = glfwCreateWindow(800, 800, "City Step", nullptr, nullptr);
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(w, framebuffer_size_callback);

    // ---------- SHADERS ----------
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    string vsrc = load("assets/vertex_core.glsl");
    const char* vc = vsrc.c_str();
    glShaderSource(vs, 1, &vc, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    string fsrc = load("assets/fragment_core.glsl");
    const char* fc = fsrc.c_str();
    glShaderSource(fs, 1, &fc, nullptr);
    glCompileShader(fs);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // ---------- BASE RECTANGLE ----------
    float base[] = {
        0,0,0,  1,0,0,  1,1,0,
        0,0,0,  1,1,0,  0,1,0
    };

    vector <Building> city;
    for (int i = 0; i < 8; i++) {
        city.push_back({
            i * 1.5f,
            1.f,
            1.f,
            3.f + i * 0.4f
            });
    }

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(base), base, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // ---------- PROJECTION ----------
    glm::mat4 proj = glm::ortho(0.f, 10.f, 0.f, 10.f);

    int mvpLoc = glGetUniformLocation(prog, "mvp");

    // ---------- LOOP ----------
    while (!glfwWindowShouldClose(w)) {
        glClearColor(0.15f, 0.15f, 0.2f, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(VAO);




        for (auto& b : city) {
            glm::mat4 m(1.f);
            m = glm::translate(m, glm::vec3(b.x, b.y, 0.f));
            m = glm::scale(m, glm::vec3(b.w, b.h, 1.f));

            glm::mat4 mvp = proj * m;
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }


        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
