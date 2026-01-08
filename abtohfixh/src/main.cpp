#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

void framebuffer_size_callback(GLFWwindow* w, int x, int y) {
    glViewport(0, 0, x, y);
}

// ---------- CITY DATA ----------

enum CellType { Empty, Road, Block };

struct Cell {
    CellType t;
};

struct City {
    int r, c;
    vector<Cell> g;

    Cell& at(int y, int x) {
        return g[y * c + x];
    }
};

// ---------- SHADER LOADER ----------

string load(const char* p) {
    ifstream f(p);
    stringstream s;
    s << f.rdbuf();
    return s.str();
}

// ---------- MAIN ----------

int main() {
    // ---------- INIT GLFW ----------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* w = glfwCreateWindow(800, 800, "Procedural City Grid", nullptr, nullptr);
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

    // ---------- BASE QUAD ----------
    float quad[] = {
        0,0,0,  1,0,0,  1,1,0,
        0,0,0,  1,1,0,  0,1,0
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // ---------- CITY GENERATION ----------
    City city;
    city.r = 20;
    city.c = 20;
    city.g.resize(20 * 20);

    for (auto& c : city.g)
        c.t = Empty;

    // Roads
    for (int y = 0; y < city.r; y++) {
        for (int x = 0; x < city.c; x++) {
            if (y % 5 == 0 || x % 5 == 0)
                city.at(y, x).t = Road;
        }
    }

    // Blocks
    for (int y = 0; y < city.r; y++) {
        for (int x = 0; x < city.c; x++) {
            if (city.at(y, x).t == Empty)
                city.at(y, x).t = Block;
        }
    }

    // ---------- CAMERA ----------
    glm::mat4 proj = glm::ortho(0.f, 20.f, 0.f, 20.f);
    int mvpLoc = glGetUniformLocation(prog, "mvp");
    int colorLoc = glGetUniformLocation(prog, "color");

    // ---------- LOOP ----------
    while (!glfwWindowShouldClose(w)) {
        glClearColor(0.1f, 0.1f, 0.15f, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(VAO);

        for (int y = 0; y < city.r; y++) {
            for (int x = 0; x < city.c; x++) {
                Cell& c = city.at(y, x);

                glm::vec3 col;
                if (c.t == Road) col = { 0.2f, 0.2f, 0.2f };
                else col = { 0.6f, 0.6f, 0.6f };

                glUniform3fv(colorLoc, 1, &col.x);

                glm::mat4 m(1.f);
                m = glm::translate(m, glm::vec3(x, y, 0));
                glm::mat4 mvp = proj * m;

                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        glfwSwapBuffers(w);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
