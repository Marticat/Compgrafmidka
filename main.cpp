#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <cmath>
#include <random>

// Window settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 800;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Camera - теперь статическая
glm::vec3 cameraPos = glm::vec3(0.0f, 3.0f, 8.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, -0.3f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

// Character
glm::vec3 characterPos(0.0f, 0.5f, 0.0f);
float characterYaw = 0.0f;
float movementSpeed = 3.0f;
float runSpeed = 6.0f;
float crawlSpeed = 1.5f;

// Состояния персонажа
bool isJumping = false;
bool isLanding = false;
bool isRunning = false;
bool isCrawling = false;
bool isLeaningLeft = false;
bool isLeaningRight = false;

// Улучшенная физика прыжка
float jumpVelocity = 0.0f;
float characterHeight = 0.0f;
float jumpHoldTime = 0.0f;
const float gravity = -25.0f;
const float baseJumpForce = 8.0f;
const float maxJumpForce = 12.0f;
const float landingRecovery = 3.0f;

// Анимации прыжка
float jumpAnimationTime = 0.0f;
float jumpSquatBlend = 0.0f;    // Приседание перед прыжком
float jumpApexBlend = 0.0f;     // Апекс прыжка
float landingBlend = 0.0f;      // Приземление

// Анимации
float animationTime = 0.0f;
bool isMoving = false;
float movementBlend = 0.0f;
float runBlend = 0.0f;
float crawlBlend = 0.0f;
float leanBlend = 0.0f;

// Коллизия
struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;

    BoundingBox() : min(0.0f), max(0.0f) {}
    BoundingBox(glm::vec3 min, glm::vec3 max) : min(min), max(max) {}

    bool intersects(const BoundingBox& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
            (min.y <= other.max.y && max.y >= other.min.y) &&
            (min.z <= other.max.z && max.z >= other.min.z);
    }
};

// Model data with normals
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;

    Vertex() : position(0.0f), normal(0.0f) {}
    Vertex(glm::vec3 pos, glm::vec3 norm) : position(pos), normal(norm) {}
};

struct Mesh {
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 color = glm::vec3(0.7f, 0.6f, 0.8f);

    Mesh() = default;

    ~Mesh() {
        cleanup();
    }

    void setupMesh() {
        if (VAO != 0) return;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
            vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
            indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void cleanup() {
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (EBO != 0) glDeleteBuffers(1, &EBO);
    }
};

// Модель с коллизией
struct CollisionMesh {
    Mesh mesh;
    BoundingBox bounds;
    glm::vec3 position;
    std::string name;

    CollisionMesh() : position(0.0f), name("unknown") {}
    CollisionMesh(const Mesh& m, const BoundingBox& b, const glm::vec3& pos = glm::vec3(0.0f), const std::string& n = "unknown")
        : mesh(m), bounds(b), position(pos), name(n) {
    }

    BoundingBox getWorldBounds() const {
        return BoundingBox(bounds.min + position, bounds.max + position);
    }
};

// Shader class
class Shader {
public:
    unsigned int ID = 0;

    Shader() = default;

    Shader(const char* vertexPath, const char* fragmentPath) {
        load(vertexPath, fragmentPath);
    }

    bool load(const char* vertexPath, const char* fragmentPath) {
        std::string vertexCode, fragmentCode;
        std::ifstream vShaderFile, fShaderFile;

        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

        try {
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);

            std::stringstream vShaderStream, fShaderStream;
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();

            vShaderFile.close();
            fShaderFile.close();

            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        }
        catch (std::ifstream::failure& e) {
            std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
            return false;
        }

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        unsigned int vertex, fragment;

        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        if (!checkCompileErrors(vertex, "VERTEX")) return false;

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        if (!checkCompileErrors(fragment, "FRAGMENT")) {
            glDeleteShader(vertex);
            return false;
        }

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        if (!checkCompileErrors(ID, "PROGRAM")) {
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            ID = 0;
            return false;
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return true;
    }

    void use() {
        if (ID != 0) glUseProgram(ID);
    }

    void setMat4(const std::string& name, const glm::mat4& mat) const {
        if (ID != 0) {
            glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
        }
    }

    void setVec3(const std::string& name, const glm::vec3& value) const {
        if (ID != 0) {
            glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, &value[0]);
        }
    }

    void setFloat(const std::string& name, float value) const {
        if (ID != 0) {
            glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
        }
    }

private:
    bool checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
                return false;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
                return false;
            }
        }
        return true;
    }
};

// Система коллизий
class CollisionSystem {
private:
    std::vector<CollisionMesh> obstacles;
    BoundingBox characterBounds;

public:
    CollisionSystem() {
        // Базовые границы персонажа (будет обновляться)
        characterBounds = BoundingBox(glm::vec3(-0.3f, 0.0f, -0.3f), glm::vec3(0.3f, 1.8f, 0.3f));
    }

    void addObstacle(const CollisionMesh& obstacle) {
        obstacles.push_back(obstacle);
    }

    void setCharacterBounds(const BoundingBox& bounds) {
        characterBounds = bounds;
    }

    BoundingBox getCharacterWorldBounds(const glm::vec3& position) const {
        return BoundingBox(characterBounds.min + position, characterBounds.max + position);
    }

    bool checkCollision(const glm::vec3& position) const {
        BoundingBox charBounds = getCharacterWorldBounds(position);

        for (const auto& obstacle : obstacles) {
            if (charBounds.intersects(obstacle.getWorldBounds())) {
                return true;
            }
        }
        return false;
    }

    glm::vec3 resolveCollision(const glm::vec3& oldPos, const glm::vec3& newPos) const {
        if (!checkCollision(newPos)) {
            return newPos;
        }

        // Пробуем двигаться только по X
        glm::vec3 testPos = glm::vec3(newPos.x, oldPos.y, oldPos.z);
        if (!checkCollision(testPos)) {
            return testPos;
        }

        // Пробуем двигаться только по Z
        testPos = glm::vec3(oldPos.x, oldPos.y, newPos.z);
        if (!checkCollision(testPos)) {
            return testPos;
        }

        // Если всё ещё коллизия, остаёмся на месте
        return oldPos;
    }

    void drawObstacles(Shader& shader) const {
        for (const auto& obstacle : obstacles) {
            glm::mat4 transform = glm::translate(glm::mat4(1.0f), obstacle.position);

            // Специальные трансформации для разных типов препятствий
            if (obstacle.name.find("arch") != std::string::npos) {
                // Арка - добавляем немного высоты
                transform = glm::translate(transform, glm::vec3(0.0f, 1.0f, 0.0f));
            }
            else if (obstacle.name.find("stairs") != std::string::npos) {
                // Лестница
                transform = glm::translate(transform, glm::vec3(0.0f, -0.5f, 0.0f));
            }

            shader.setMat4("model", transform);
            shader.setVec3("objectColor", obstacle.mesh.color);
            glBindVertexArray(obstacle.mesh.VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(obstacle.mesh.indices.size()), GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
    }
};

// Create fallback cube mesh
Mesh createCubeMesh(const glm::vec3& color = glm::vec3(0.7f, 0.6f, 0.8f)) {
    Mesh mesh;
    mesh.color = color;

    mesh.vertices = {
        {glm::vec3(-0.5f, -0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},
        {glm::vec3(0.5f, -0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},
        {glm::vec3(0.5f,  0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},
        {glm::vec3(-0.5f,  0.5f,  0.5f), glm::vec3(0.0f, 0.0f, 1.0f)},
        {glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f)},
        {glm::vec3(0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f)},
        {glm::vec3(0.5f,  0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f)},
        {glm::vec3(-0.5f,  0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f)}
    };

    mesh.indices = {
        0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
        0, 3, 7, 7, 4, 0, 1, 2, 6, 6, 5, 1,
        3, 2, 6, 6, 7, 3, 0, 1, 5, 5, 4, 0
    };

    mesh.setupMesh();
    return mesh;
}

// Create specific body part meshes
Mesh createTorsoMesh() {
    Mesh torso = createCubeMesh(glm::vec3(0.8f, 0.2f, 0.2f));
    for (auto& vertex : torso.vertices) {
        vertex.position.x *= 0.4f;
        vertex.position.y *= 0.8f;
        vertex.position.z *= 0.2f;
        vertex.position.y += 0.4f;
    }
    return torso;
}

Mesh createHeadMesh() {
    Mesh head = createCubeMesh(glm::vec3(0.9f, 0.8f, 0.7f));
    for (auto& vertex : head.vertices) {
        vertex.position.x *= 0.25f;
        vertex.position.y *= 0.25f;
        vertex.position.z *= 0.2f;
        vertex.position.y += 1.2f;
    }
    return head;
}

Mesh createArmMesh(bool isLeft = true, const glm::vec3& color = glm::vec3(0.2f, 0.4f, 0.8f)) {
    Mesh arm = createCubeMesh(color);
    for (auto& vertex : arm.vertices) {
        vertex.position.x *= 0.08f;
        vertex.position.y *= 0.6f;
        vertex.position.z *= 0.1f;
        vertex.position.y -= 0.3f;
    }
    return arm;
}

Mesh createLegMesh(bool isLeft = true, const glm::vec3& color = glm::vec3(0.3f, 0.3f, 0.3f)) {
    Mesh leg = createCubeMesh(color);
    for (auto& vertex : leg.vertices) {
        vertex.position.x *= 0.1f;
        vertex.position.y *= 0.6f;
        vertex.position.z *= 0.1f;
        vertex.position.y -= 0.3f;
    }
    return leg;
}

// Load OBJ with Assimp or create fallback
bool loadOBJ(const std::string& path, Mesh& mesh, const glm::vec3& color = glm::vec3(0.7f, 0.6f, 0.8f)) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "Creating fallback geometry for: " << path << std::endl;

        if (path.find("torso") != std::string::npos) mesh = createTorsoMesh();
        else if (path.find("head") != std::string::npos) mesh = createHeadMesh();
        else if (path.find("left_arm") != std::string::npos) mesh = createArmMesh(true, color);
        else if (path.find("right_arm") != std::string::npos) mesh = createArmMesh(false, color);
        else if (path.find("left_leg") != std::string::npos) mesh = createLegMesh(true, color);
        else if (path.find("right_leg") != std::string::npos) mesh = createLegMesh(false, color);
        else mesh = createCubeMesh(color);

        return true;
    }

    if (scene->mNumMeshes == 0) return false;

    aiMesh* ai_mesh = scene->mMeshes[0];
    mesh.vertices.clear();
    mesh.indices.clear();

    for (unsigned int i = 0; i < ai_mesh->mNumVertices; i++) {
        Vertex vertex;
        vertex.position = glm::vec3(ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z);

        if (ai_mesh->HasNormals()) {
            vertex.normal = glm::vec3(ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z);
        }
        else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        mesh.vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < ai_mesh->mNumFaces; i++) {
        aiFace face = ai_mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            mesh.indices.push_back(face.mIndices[j]);
        }
    }

    mesh.color = color;
    mesh.setupMesh();
    return true;
}

// Draw mesh function
void drawMesh(Shader& shader, const Mesh& mesh, const glm::mat4& transform) {
    shader.setMat4("model", transform);
    shader.setVec3("objectColor", mesh.color);
    glBindVertexArray(mesh.VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// Create a simple floor
Mesh createFloor() {
    Mesh floor;
    floor.color = glm::vec3(0.3f, 0.5f, 0.3f);

    float size = 15.0f; // Увеличили пол
    floor.vertices = {
        {glm::vec3(-size, 0.0f, -size), glm::vec3(0.0f, 1.0f, 0.0f)},
        {glm::vec3(size, 0.0f, -size), glm::vec3(0.0f, 1.0f, 0.0f)},
        {glm::vec3(-size, 0.0f, size), glm::vec3(0.0f, 1.0f, 0.0f)},
        {glm::vec3(size, 0.0f, size), glm::vec3(0.0f, 1.0f, 0.0f)}
    };

    floor.indices = { 0, 1, 2, 2, 1, 3 };
    floor.setupMesh();
    return floor;
}

// Create obstacle meshes
Mesh createWallMesh(const glm::vec3& size = glm::vec3(1.0f, 2.0f, 0.2f),
    const glm::vec3& color = glm::vec3(0.5f, 0.3f, 0.1f)) {
    Mesh wall = createCubeMesh(color);
    for (auto& vertex : wall.vertices) {
        vertex.position.x *= size.x;
        vertex.position.y *= size.y;
        vertex.position.z *= size.z;
    }
    return wall;
}

Mesh createBoxMesh(const glm::vec3& size = glm::vec3(1.0f, 1.0f, 1.0f),
    const glm::vec3& color = glm::vec3(0.8f, 0.6f, 0.2f)) {
    Mesh box = createCubeMesh(color);
    for (auto& vertex : box.vertices) {
        vertex.position.x *= size.x;
        vertex.position.y *= size.y;
        vertex.position.z *= size.z;
    }
    return box;
}

// Input processing
void processInput(GLFWwindow* window, CollisionSystem& collisionSystem) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Сохраняем старую позицию для разрешения коллизий
    glm::vec3 oldCharacterPos = characterPos;

    // Проверяем движение персонажа
    bool wasMoving = isMoving;
    isMoving = false;

    // Ползание (C) - теперь просто наклон вперед
    bool wasCrawling = isCrawling;
    isCrawling = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;

    // Бег (Z)
    bool wasRunning = isRunning;
    isRunning = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;

    // Наклоны (Q/E)
    isLeaningLeft = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    isLeaningRight = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;

    // Character movement (arrow keys)
    glm::vec3 characterForward(std::sin(glm::radians(characterYaw)), 0, std::cos(glm::radians(characterYaw)));
    glm::vec3 characterRight(std::cos(glm::radians(characterYaw)), 0, -std::sin(glm::radians(characterYaw)));

    // Выбор скорости в зависимости от состояния
    float currentSpeed = isCrawling ? crawlSpeed : (isRunning ? runSpeed : movementSpeed);
    float characterSpeed = currentSpeed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        characterPos += characterForward * characterSpeed;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        characterPos -= characterForward * characterSpeed;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        characterYaw += 90.0f * deltaTime;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        characterYaw -= 90.0f * deltaTime;
        isMoving = true;
    }

    // Проверка коллизий и разрешение
    characterPos = collisionSystem.resolveCollision(oldCharacterPos, characterPos);

    // Плавные переходы между состояниями
    float targetBlend = isMoving ? 1.0f : 0.0f;
    movementBlend = glm::mix(movementBlend, targetBlend, deltaTime * 5.0f);

    float targetRunBlend = isRunning ? 1.0f : 0.0f;
    runBlend = glm::mix(runBlend, targetRunBlend, deltaTime * 8.0f);

    // Плавный переход в режим ползания (наклон вперед)
    float targetCrawlBlend = isCrawling ? 1.0f : 0.0f;
    crawlBlend = glm::mix(crawlBlend, targetCrawlBlend, deltaTime * 6.0f);

    // Наклоны
    float targetLean = 0.0f;
    if (isLeaningLeft) targetLean = 15.0f;
    else if (isLeaningRight) targetLean = -15.0f;
    leanBlend = glm::mix(leanBlend, targetLean, deltaTime * 6.0f);

    // Улучшенный прыжок (нельзя прыгать во время ползания)
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !isCrawling) {
        if (!isJumping && !isLanding) {
            isJumping = true;
            jumpHoldTime = 0.0f;
            jumpVelocity = baseJumpForce;
            jumpAnimationTime = 0.0f;
        }
        else if (isJumping && jumpVelocity > 0.0f) {
            jumpHoldTime += deltaTime;
            float jumpPower = glm::min(baseJumpForce + jumpHoldTime * 15.0f, maxJumpForce);
            jumpVelocity = jumpPower;
        }
    }
    else if (isJumping && jumpHoldTime == 0.0f) {
        jumpVelocity = baseJumpForce;
    }
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Character Animation by Adilzhan Kurmet and Adilet Malikov", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader shader;
    if (!shader.load("shaders/vertex.glsl", "shaders/fragment.glsl")) {
        std::cerr << "Failed to load shaders\n";
        return -1;
    }

    // Load character parts
    Mesh torso, head, leftArm, rightArm, leftLeg, rightLeg;

    loadOBJ("models/torso.obj", torso, glm::vec3(0.8f, 0.2f, 0.2f));
    loadOBJ("models/head.obj", head, glm::vec3(0.9f, 0.8f, 0.7f));
    loadOBJ("models/left_arm.obj", leftArm, glm::vec3(0.2f, 0.4f, 0.8f));
    loadOBJ("models/right_arm.obj", rightArm, glm::vec3(0.2f, 0.4f, 0.8f));
    loadOBJ("models/left_leg.obj", leftLeg, glm::vec3(0.3f, 0.3f, 0.3f));
    loadOBJ("models/right_leg.obj", rightLeg, glm::vec3(0.3f, 0.3f, 0.3f));

    Mesh floor = createFloor();

    // Создаём систему коллизий и добавляем ПРОСТЫЕ препятствия
    CollisionSystem collisionSystem;

    // Всего 4 простых препятствия
    Mesh wallMesh = createWallMesh(glm::vec3(8.0f, 2.0f, 0.3f), glm::vec3(0.5f, 0.3f, 0.1f));
    BoundingBox wallBounds(glm::vec3(-4.0f, 0.0f, -0.15f), glm::vec3(4.0f, 2.0f, 0.15f));
    collisionSystem.addObstacle(CollisionMesh(wallMesh, wallBounds, glm::vec3(0.0f, 1.0f, 5.0f)));

    Mesh boxMesh = createBoxMesh(glm::vec3(1.5f, 1.0f, 1.5f), glm::vec3(0.8f, 0.6f, 0.2f));
    BoundingBox boxBounds(glm::vec3(-0.75f, 0.0f, -0.75f), glm::vec3(0.75f, 1.0f, 0.75f));
    collisionSystem.addObstacle(CollisionMesh(boxMesh, boxBounds, glm::vec3(3.0f, 0.5f, -3.0f)));

    Mesh boxMesh2 = createBoxMesh(glm::vec3(1.0f, 0.8f, 1.0f), glm::vec3(0.4f, 0.2f, 0.8f));
    BoundingBox boxBounds2(glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 0.8f, 0.5f));
    collisionSystem.addObstacle(CollisionMesh(boxMesh2, boxBounds2, glm::vec3(-2.0f, 0.4f, 2.0f)));

    std::cout << "\n3D CHARACTER" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "WASD - Move character" << std::endl;
    std::cout << "Z - Run" << std::endl;
    std::cout << "C - Lean body to front" << std::endl;
    std::cout << "Q/E - Lean body left/right" << std::endl;
    std::cout << "Space - Jump (hold for higher jump)" << std::endl;
    std::cout << "ESC - Exit" << std::endl;


    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        animationTime += deltaTime;

        processInput(window, collisionSystem);

        // Улучшенная физика прыжка с приземлением
        static float landingSquatCurrent = 0.0f;

        if (isJumping && !isCrawling) {
            jumpAnimationTime += deltaTime;
            characterHeight += jumpVelocity * deltaTime;
            jumpVelocity += gravity * deltaTime;

            // Анимации прыжка
            if (jumpVelocity > 0.0f) {
                // Фаза взлёта
                jumpSquatBlend = glm::max(0.0f, 1.0f - jumpAnimationTime * 8.0f);
                jumpApexBlend = glm::min(jumpAnimationTime * 4.0f, 1.0f);
            }
            else {
                // Фаза падения
                jumpSquatBlend = 0.0f;
                jumpApexBlend = glm::max(0.0f, 1.0f - (-jumpVelocity) * 0.5f);
            }

            if (characterHeight <= 0.0f) {
                characterHeight = 0.0f;
                isJumping = false;
                isLanding = true;
                landingSquatCurrent = 0.15f;
                landingBlend = 1.0f;
                jumpVelocity = 0.0f;
                jumpHoldTime = 0.0f;
                jumpAnimationTime = 0.0f;
            }
        }

        // Обработка приземления
        if (isLanding) {
            landingSquatCurrent -= landingRecovery * deltaTime;
            landingBlend = glm::max(0.0f, landingBlend - deltaTime * 4.0f);

            if (landingSquatCurrent <= 0.0f) {
                landingSquatCurrent = 0.0f;
                isLanding = false;
                landingBlend = 0.0f;
            }
        }

        // Сброс анимаций прыжка когда не прыгаем
        if (!isJumping && !isLanding) {
            jumpSquatBlend = 0.0f;
            jumpApexBlend = 0.0f;
        }

        // Микроанимации (работают всегда, включая ползание)
        float breathing = std::sin(animationTime * 3.0f) * 0.02f;
        float headBob = std::sin(animationTime * 8.0f) * 0.01f * movementBlend * (1.0f - crawlBlend);
        float idleArmSway = std::sin(animationTime * 2.0f) * 0.05f * (1.0f - movementBlend) * (1.0f - crawlBlend);

        // Rendering
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // Camera
        glm::mat4 view = glm::lookAt(
            cameraPos,
            characterPos + glm::vec3(0.0f, 1.5f, 0.0f), // Камера не опускается при ползании
            cameraUp
        );
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        shader.setMat4("view", view);
        shader.setMat4("projection", projection);
        shader.setVec3("viewPos", cameraPos);
        shader.setVec3("lightPos", glm::vec3(5.0f, 10.0f, 5.0f));
        shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

        // Draw floor
        drawMesh(shader, floor, glm::mat4(1.0f));

        // Draw obstacles
        collisionSystem.drawObstacles(shader);

        // Основные анимации с учетом всех состояний
        float walkCycleSpeed = isRunning ? 12.0f : 8.0f;
        float walkCycleAmplitude = isRunning ? 40.0f : 30.0f;
        float walkCycle = std::sin(animationTime * walkCycleSpeed) * walkCycleAmplitude * movementBlend * (1.0f - crawlBlend);

        float armSwingSpeed = isRunning ? 12.0f : 8.0f;
        float armSwingAmplitude = isRunning ? 35.0f : 25.0f;
        float armSwing = std::sin(animationTime * armSwingSpeed) * armSwingAmplitude * movementBlend * (1.0f - crawlBlend);

        // Base character transform с наклоном всего тела
        float currentHeight = characterHeight - landingSquatCurrent;
        glm::mat4 characterBase = glm::translate(glm::mat4(1.0f),
            characterPos + glm::vec3(0.0f, currentHeight, 0.0f));
        characterBase = glm::rotate(characterBase, glm::radians(characterYaw), glm::vec3(0, 1, 0));

        // Наклон всего тела в стороны
        glm::mat4 characterBaseWithLean = characterBase;
        characterBaseWithLean = glm::rotate(characterBaseWithLean, glm::radians(leanBlend), glm::vec3(0, 0, 1));

        // ПРОСТОЙ НАКЛОН ВПЕРЕД ПРИ ПОЛЗАНИИ
        float forwardLean = crawlBlend * 45.0f; // Наклон вперед на 45 градусов

        // Наклон туловища при движении (только при ходьбе/беге)
        float torsoLean = movementBlend * 5.0f * (1.0f - crawlBlend);

        // Torso с дыханием и наклоном + анимации прыжка
        glm::mat4 torsoTransform = characterBaseWithLean;

        // Приседание перед прыжком
        float squatOffset = jumpSquatBlend * 0.1f;
        // Поджатие ног в апексе прыжка
        float apexTuck = jumpApexBlend * 0.2f;
        // Приземление
        float landingSquat = landingBlend * 0.15f;

        torsoTransform = glm::translate(torsoTransform, glm::vec3(0.0f, 0.3f + breathing - squatOffset - landingSquat, 0.0f));
        // Наклон вперед при ползании
        torsoTransform = glm::rotate(torsoTransform, glm::radians(torsoLean + forwardLean), glm::vec3(1, 0, 0));
        drawMesh(shader, torso, torsoTransform);

        // Head с микродвижениями + анимации прыжка - УЛУЧШЕННЫЙ НАКЛОН
        glm::mat4 headTransform = torsoTransform;
        float headJumpTilt = jumpApexBlend * 10.0f; // Наклон головы в прыжке
        // УВЕЛИЧЕННАЯ компенсация наклона головы при ползании, чтобы смотреть вперед
        float headCompensation = -forwardLean * -0.4f; // УВЕЛИЧЕНО с 0.7f до 0.9f
        headTransform = glm::translate(headTransform, glm::vec3(0.0f, 0.2f + headBob + apexTuck, 0.0f));
        headTransform = glm::rotate(headTransform, glm::radians(headJumpTilt + headCompensation), glm::vec3(1, 0, 0));
        drawMesh(shader, head, headTransform);

        // Arms с анимациями прыжка - УЛУЧШЕННЫЙ НАКЛОН
        glm::mat4 leftArmTransform = characterBaseWithLean;

        if (isJumping) {
            // Анимация рук в прыжке - взмах при отталкивании
            float armJumpSwing = jumpSquatBlend * 60.0f - jumpApexBlend * 30.0f;
            leftArmTransform = glm::translate(leftArmTransform, glm::vec3(-0.25f, 0.7f - squatOffset, 0.0f));
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(armJumpSwing), glm::vec3(1, 0, 0));
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(10.0f), glm::vec3(0, 0, 1));
        }
        else {
            leftArmTransform = glm::translate(leftArmTransform, glm::vec3(-0.25f, 0.7f, 0.0f));
            float leftArmRotation = armSwing + idleArmSway;
            // УВЕЛИЧЕННЫЙ наклон рук вперед при ползании
            leftArmRotation += forwardLean * 1.2f; // УВЕЛИЧЕНО с 0.8f до 1.2f
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(leftArmRotation), glm::vec3(1, 0, 0));
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(10.0f), glm::vec3(0, 0, 1));
        }
        drawMesh(shader, leftArm, leftArmTransform);

        glm::mat4 rightArmTransform = characterBaseWithLean;

        if (isJumping) {
            // Анимация рук в прыжке - взмах при отталкивании
            float armJumpSwing = jumpSquatBlend * 60.0f - jumpApexBlend * 30.0f;
            rightArmTransform = glm::translate(rightArmTransform, glm::vec3(0.25f, 0.7f - squatOffset, 0.0f));
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(armJumpSwing), glm::vec3(1, 0, 0));
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(-10.0f), glm::vec3(0, 0, 1));
        }
        else {
            rightArmTransform = glm::translate(rightArmTransform, glm::vec3(0.25f, 0.7f, 0.0f));
            float rightArmRotation = -armSwing - idleArmSway;
            // УВЕЛИЧЕННЫЙ наклон рук вперед при ползании
            rightArmRotation += forwardLean * 1.2f; // УВЕЛИЧЕНО с 0.8f до 1.2f
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(rightArmRotation), glm::vec3(1, 0, 0));
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(-10.0f), glm::vec3(0, 0, 1));
        }
        drawMesh(shader, rightArm, rightArmTransform);

        // Legs с анимациями прыжка
        glm::mat4 leftLegTransform = characterBaseWithLean;

        if (isJumping) {
            // Анимация ног в прыжке - приседание + поджатие
            float legSquat = jumpSquatBlend * 45.0f;
            float legTuck = jumpApexBlend * 60.0f;
            leftLegTransform = glm::translate(leftLegTransform, glm::vec3(-0.12f, 0.1f - squatOffset, 0.0f));
            leftLegTransform = glm::rotate(leftLegTransform, glm::radians(-legSquat - legTuck), glm::vec3(1, 0, 0));
            leftLegTransform = glm::translate(leftLegTransform, glm::vec3(0.0f, -0.3f, 0.0f));
        }
        else {
            leftLegTransform = glm::translate(leftLegTransform, glm::vec3(-0.12f, 0.1f, 0.0f));
            float leftLegRotation = -walkCycle;
            // Небольшой наклон ног вперед при ползании
            leftLegRotation += forwardLean * 0.3f;
            leftLegTransform = glm::rotate(leftLegTransform, glm::radians(leftLegRotation), glm::vec3(1, 0, 0));
            leftLegTransform = glm::translate(leftLegTransform, glm::vec3(0.0f, -0.3f, 0.0f));
        }
        drawMesh(shader, leftLeg, leftLegTransform);

        glm::mat4 rightLegTransform = characterBaseWithLean;

        if (isJumping) {
            // Анимация ног в прыжке - приседание + поджатие
            float legSquat = jumpSquatBlend * 45.0f;
            float legTuck = jumpApexBlend * 60.0f;
            rightLegTransform = glm::translate(rightLegTransform, glm::vec3(0.12f, 0.1f - squatOffset, 0.0f));
            rightLegTransform = glm::rotate(rightLegTransform, glm::radians(-legSquat - legTuck), glm::vec3(1, 0, 0));
            rightLegTransform = glm::translate(rightLegTransform, glm::vec3(0.0f, -0.3f, 0.0f));
        }
        else {
            rightLegTransform = glm::translate(rightLegTransform, glm::vec3(0.12f, 0.1f, 0.0f));
            float rightLegRotation = walkCycle;
            // Небольшой наклон ног вперед при ползании
            rightLegRotation += forwardLean * 0.3f;
            rightLegTransform = glm::rotate(rightLegTransform, glm::radians(rightLegRotation), glm::vec3(1, 0, 0));
            rightLegTransform = glm::translate(rightLegTransform, glm::vec3(0.0f, -0.3f, 0.0f));
        }
        drawMesh(shader, rightLeg, rightLegTransform);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
