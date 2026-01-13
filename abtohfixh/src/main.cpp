#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <cstdint>



using namespace std;

// ---------------- CALLBACK ----------------
void framebuffer_size_callback(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

struct RNG {
    std::mt19937 g;
    RNG(uint32_t s) : g(s) {}
};
// ---------------- CITY DATA ----------------
enum CellType {
    CELL_EMPTY,
    CELL_ROAD,
    CELL_BLOCK
};

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

// ---------------- BLOCK + BUILDING ----------------
struct BlockBounds {
    int x0, y0, x1, y1;
};

enum class BlockType {
    Dense,
    Sparse,
    Park
};


struct Building {
    float x, y, w, h;
};


struct Block {
    BlockBounds b;
    BlockType type;
    vector<Building> buildings;
};




// ---------------- SHADER LOADER ----------------
string load(const char* p) {
    ifstream f(p);
    stringstream s;
    s << f.rdbuf();
    return s.str();
}

// ---------------- FIND ALL BLOCKS ----------------
vector<BlockBounds> findAllBlocks(const City& c) {
    vector<BlockBounds> blocks;
    vector<bool> visited(c.r * c.c, false);

    for (int y = 0; y < c.r; y++) {
        for (int x = 0; x < c.c; x++) {
            int idx = y * c.c + x;
            if (visited[idx] || c.g[idx].t != CELL_BLOCK) continue;

            int x1 = x, y1 = y;
            while (x1 < c.c && c.g[y * c.c + x1].t == CELL_BLOCK) x1++;
            while (y1 < c.r && c.g[y1 * c.c + x].t == CELL_BLOCK) y1++;

            for (int yy = y; yy < y1; yy++)
                for (int xx = x; xx < x1; xx++)
                    visited[yy * c.c + xx] = true;

            blocks.push_back({ x, y, x1, y1 });
        }
    }
    return blocks;
}

//------Random----
BlockType chooseBlockType(RNG& rng) {
    std::uniform_int_distribution<int> d(0, 99);
    int r = d(rng.g);

    if (r < 30) return BlockType::Dense;
    if (r < 90) return BlockType::Sparse;
    return BlockType::Park;
}

uint32_t hashSeed(uint32_t seed, uint32_t i) {
    seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

float rf(RNG& rng, float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(rng.g);
}

bool overlaps(const Building& a, const Building& b) {
    return !(
        a.x + a.w < b.x ||
        b.x + b.w < a.x ||
        a.y + a.h < b.y ||
        b.y + b.h < a.y
        );
}


// ---------------- BUILDING GENERATION ----------------
vector<Building> genDense(const BlockBounds& b, RNG& rng) {
    vector<Building> out;

    int attempts = 0;
    int target = (b.x1 - b.x0) * (b.y1 - b.y0) / 2;

    while ((int)out.size() < target && attempts < target * 10) {
        attempts++;

        float s = rf(rng, 0.45f, 0.7f);
        Building nb{
            rf(rng, b.x0 + 1.0f, b.x1 - 1.0f - s),
            rf(rng, b.y0 + 1.0f, b.y1 - 1.0f - s),
            s, s
        };

        bool ok = true;
        for (auto& o : out)
            if (overlaps(nb, o)) { ok = false; break; }

        if (ok) out.push_back(nb);
    }

    return out;
}


vector<Building> genSparse(const BlockBounds& b, RNG& rng) {
    vector<Building> out;

    int target = 3 + (int)rf(rng, 0, 3);
    int attempts = 0;

    while ((int)out.size() < target && attempts < 50) {
        attempts++;

        float s = rf(rng, 0.7f, 1.2f);
        Building nb{
            rf(rng, b.x0 + 1.5f, b.x1 - 1.5f - s),
            rf(rng, b.y0 + 1.5f, b.y1 - 1.5f - s),
            s, s
        };

        bool ok = true;
        for (auto& o : out)
            if (overlaps(nb, o)) { ok = false; break; }

        if (ok) out.push_back(nb);
    }

    return out;
}



vector<Building> genPark(const BlockBounds&, RNG& rng) {
    return {};
}


// ---------------- MAIN ----------------
int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    GLFWwindow* w = glfwCreateWindow(900, 900, "City Step", nullptr, nullptr);
    glfwMakeContextCurrent(w);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(w, framebuffer_size_callback);

    glEnable(GL_DEPTH_TEST);

    // ---------------- SHADERS ----------------
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

    // ---------------- CENTERED QUAD ----------------
    float quad[] = {
        -0.5f,-0.5f,0,  0.5f,-0.5f,0,  0.5f,0.5f,0,
        -0.5f,-0.5f,0,  0.5f,0.5f,0, -0.5f,0.5f,0
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glEnableVertexAttribArray(0);

    // ---------------- CITY ----------------
    City city;
    city.r = 32;
    city.c = 32;
    city.g.resize(city.r * city.c);

    uint32_t seed = 1337;
    RNG rng(seed);

    for (auto& c : city.g) c.t = CELL_EMPTY;

    for (int y = 0; y < city.r; y++)
        for (int x = 0; x < city.c; x++)
            if (y % 8 == 0 || x % 8 == 0)
                city.at(y, x).t = CELL_ROAD;

    for (int y = 0; y < city.r; y++)
        for (int x = 0; x < city.c; x++)
            if (city.at(y, x).t == CELL_EMPTY)
                city.at(y, x).t = CELL_BLOCK;

    // ---------------- CAMERA ----------------
    float margin = 1.0f;

    glm::mat4 proj = glm::ortho(
        -margin,
        city.c + margin,
        -margin,
        city.r + margin
    );

    int mvpLoc = glGetUniformLocation(prog, "mvp");
    int colorLoc = glGetUniformLocation(prog, "color");

    auto rawBlocks = findAllBlocks(city);
    vector<Block> blocks;

    for (int i = 0; i < rawBlocks.size(); i++) {
        blocks.push_back({ rawBlocks[i], chooseBlockType(rng), {} });
    }

    for (size_t i = 0; i < blocks.size(); i++) {
        uint32_t bseed = hashSeed(seed, static_cast<uint32_t>(i));
        RNG brng(bseed);

        switch (blocks[i].type) {
        case BlockType::Dense:
            blocks[i].buildings = genDense(blocks[i].b, brng);
            break;

        case BlockType::Sparse:
            blocks[i].buildings = genSparse(blocks[i].b, brng);
            break;

        case BlockType::Park:
            blocks[i].buildings.clear();
            break;
        }
    }
    for (auto& blk : blocks) {
        if (blk.type == BlockType::Dense)  std::cout << "D ";
        if (blk.type == BlockType::Sparse) std::cout << "S ";
        if (blk.type == BlockType::Park)   std::cout << "P ";
    }
    std::cout << std::endl;



    // ---------------- LOOP ----------------
    while (!glfwWindowShouldClose(w)) {
        glClearColor(0.12f, 0.12f, 0.18f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(VAO);

        // ---- DRAW ROADS ONLY ----
        for (int y = 0; y < city.r; y++) {
            for (int x = 0; x < city.c; x++) {
                if (city.at(y, x).t != CELL_ROAD) continue;

                glm::vec3 col(0.15f, 0.15f, 0.15f);
                glUniform3fv(colorLoc, 1, &col.x);

                glm::mat4 m = glm::translate(
                    glm::mat4(1),
                    glm::vec3(x + 0.5f, y + 0.5f, 0)
                );

                glm::mat4 mvp = proj * m;
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
        }

        // ---- DRAW BUILDINGS ----



        for (auto& blk : blocks) {


            if (blk.type == BlockType::Park) {
                glm::vec3 bcol = { 0.1f, 0.4f, 0.1f };
                glm::mat4 m = glm::translate(
                    glm::mat4(1),
                    glm::vec3(
                        (blk.b.x0 + blk.b.x1) * 0.5f,
                        (blk.b.y0 + blk.b.y1) * 0.5f,
                        -0.01f
                    )
                );

                m = glm::scale(
                    m,
                    glm::vec3(
                        blk.b.x1 - blk.b.x0 - 1,
                        blk.b.y1 - blk.b.y0 - 1,
                        1
                    )
                );

                glUniform3fv(colorLoc, 1, &bcol.x);
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(proj * m));
                glDrawArrays(GL_TRIANGLES, 0, 6);
                continue;
            }
            glm::vec3 bcol;
            if (blk.type == BlockType::Dense)
                bcol = { 0.2f, 0.5f, 0.9f };
            else if (blk.type == BlockType::Sparse)
                bcol = { 1.0f, 0.2f, 0.2f };

            auto& buildings = blk.buildings;


            glUniform3fv(colorLoc, 1, &bcol.x);

            for (auto& b : buildings) {
                glm::mat4 m = glm::translate(
                    glm::mat4(1),
                    glm::vec3(b.x + b.w * 0.5f, b.y + b.h * 0.5f, 0)
                );
                m = glm::scale(m, glm::vec3(b.w, b.h, 1));

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
