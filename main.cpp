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
glm::vec3 characterPos(0.0f, 0.5f, 0.0f); // ПОВЫСИЛИ ПЕРСОНАЖА НАД ПОЛОМ
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

// Анимации
float animationTime = 0.0f;
bool isMoving = false;
float movementBlend = 0.0f;
float runBlend = 0.0f;
float crawlBlend = 0.0f;
float leanBlend = 0.0f;

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
        vertex.position.x *= 0.1f;
        vertex.position.y *= 0.6f;
        vertex.position.z *= 0.1f;
        vertex.position.x += (isLeft ? -0.25f : 0.25f);
        vertex.position.y += 0.3f;
    }
    return arm;
}

Mesh createLegMesh(bool isLeft = true, const glm::vec3& color = glm::vec3(0.3f, 0.3f, 0.3f)) {
    Mesh leg = createCubeMesh(color);
    for (auto& vertex : leg.vertices) {
        vertex.position.x *= 0.1f;
        vertex.position.y *= 0.6f;
        vertex.position.z *= 0.1f;
        vertex.position.x += (isLeft ? -0.12f : 0.12f);
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

// Input processing
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Проверяем движение персонажа
    bool wasMoving = isMoving;
    isMoving = false;

    // Ползание (C)
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

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        characterPos += characterForward * characterSpeed;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        characterPos -= characterForward * characterSpeed;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        characterYaw += 90.0f * deltaTime;
        isMoving = true;
    }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        characterYaw -= 90.0f * deltaTime;
        isMoving = true;
    }

    // Плавные переходы между состояниями
    float targetBlend = isMoving ? 1.0f : 0.0f;
    movementBlend = glm::mix(movementBlend, targetBlend, deltaTime * 5.0f);

    float targetRunBlend = isRunning ? 1.0f : 0.0f;
    runBlend = glm::mix(runBlend, targetRunBlend, deltaTime * 8.0f);

    float targetCrawlBlend = isCrawling ? 1.0f : 0.0f;
    crawlBlend = glm::mix(crawlBlend, targetCrawlBlend, deltaTime * 6.0f);

    // Наклоны
    float targetLean = 0.0f;
    if (isLeaningLeft) targetLean = 15.0f;
    else if (isLeaningRight) targetLean = -15.0f;
    leanBlend = glm::mix(leanBlend, targetLean, deltaTime * 6.0f);

    // Улучшенный прыжок (нельзя прыгать во время ползания)
    if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS && !isCrawling) {
        if (!isJumping && !isLanding) {
            isJumping = true;
            jumpHoldTime = 0.0f;
            jumpVelocity = baseJumpForce;
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

// Create a simple floor
Mesh createFloor() {
    Mesh floor;
    floor.color = glm::vec3(0.3f, 0.5f, 0.3f);

    float size = 10.0f;
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

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Character Animation", NULL, NULL);
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

    std::cout << "\nControls:" << std::endl;
    std::cout << "Arrow Keys - Move character" << std::endl;
    std::cout << "Z - Run" << std::endl;
    std::cout << "C - Crawl (по-пластунски)" << std::endl;
    std::cout << "Q/E - Lean body left/right" << std::endl;
    std::cout << "J - Jump (hold for higher jump)" << std::endl;
    std::cout << "ESC - Exit" << std::endl;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        animationTime += deltaTime;

        processInput(window);

        // Улучшенная физика прыжка с приземлением
        static float landingSquatCurrent = 0.0f;

        if (isJumping && !isCrawling) {
            characterHeight += jumpVelocity * deltaTime;
            jumpVelocity += gravity * deltaTime;

            if (characterHeight <= 0.0f) {
                characterHeight = 0.0f;
                isJumping = false;
                isLanding = true;
                landingSquatCurrent = 0.15f;
                jumpVelocity = 0.0f;
                jumpHoldTime = 0.0f;
            }
        }

        // Обработка приземления
        if (isLanding) {
            landingSquatCurrent -= landingRecovery * deltaTime;
            if (landingSquatCurrent <= 0.0f) {
                landingSquatCurrent = 0.0f;
                isLanding = false;
            }
        }

        // Микроанимации (только когда не ползаем)
        float breathing = isCrawling ? 0.0f : std::sin(animationTime * 3.0f) * 0.02f;
        float headBob = std::sin(animationTime * 8.0f) * 0.01f * movementBlend * (1.0f - crawlBlend);
        float idleArmSway = std::sin(animationTime * 2.0f) * 0.05f * (1.0f - movementBlend) * (1.0f - crawlBlend);

        // Rendering
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();

        // Camera
        glm::mat4 view = glm::lookAt(
            cameraPos,
            characterPos + glm::vec3(0.0f, 1.5f - crawlBlend * 0.8f, 0.0f), // Камера опускается при ползании
            cameraUp
        );
        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
            SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        shader.setMat4("view", view);
        shader.setMat4("projection", projection);
        shader.setVec3("viewPos", cameraPos);
        shader.setVec3("lightPos", glm::vec3(5.0f, 10.0f, 5.0f));
        shader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

        // Draw floor
        drawMesh(shader, floor, glm::mat4(1.0f));

        // Основные анимации с учетом всех состояний
        float walkCycleSpeed = isRunning ? 12.0f : 8.0f;
        float walkCycleAmplitude = isRunning ? 40.0f : 30.0f;
        float walkCycle = std::sin(animationTime * walkCycleSpeed) * walkCycleAmplitude * movementBlend * (1.0f - crawlBlend);

        float armSwingSpeed = isRunning ? 12.0f : 8.0f;
        float armSwingAmplitude = isRunning ? 35.0f : 25.0f;
        float armSwing = std::sin(animationTime * armSwingSpeed) * armSwingAmplitude * movementBlend * (1.0f - crawlBlend);

        // Анимация ползания по-пластунски
        float crawlCycle = std::sin(animationTime * 4.0f) * 25.0f * movementBlend * crawlBlend;

        // Base character transform с наклоном всего тела
        float currentHeight = characterHeight - landingSquatCurrent - crawlBlend * 0.8f;
        glm::mat4 characterBase = glm::translate(glm::mat4(1.0f),
            characterPos + glm::vec3(0.0f, currentHeight, 0.0f));
        characterBase = glm::rotate(characterBase, glm::radians(characterYaw), glm::vec3(0, 1, 0));

        // Наклон всего тела в стороны - ПРИМЕНЯЕТСЯ КО ВСЕМУ ТЕЛУ
        glm::mat4 characterBaseWithLean = characterBase;
        characterBaseWithLean = glm::rotate(characterBaseWithLean, glm::radians(leanBlend), glm::vec3(0, 0, 1));

        // Наклон туловища при движении (только при ходьбе/беге)
        float torsoLean = movementBlend * 5.0f * (1.0f - crawlBlend);

        // Torso с дыханием и наклоном (при ползании лежит на земле)
        glm::mat4 torsoTransform = characterBaseWithLean; // ИСПОЛЬЗУЕМ characterBaseWithLean
        torsoTransform = glm::translate(torsoTransform, glm::vec3(0.0f, 0.3f + breathing - crawlBlend * 0.6f, 0.0f));
        torsoTransform = glm::rotate(torsoTransform, glm::radians(torsoLean), glm::vec3(1, 0, 0)); // Наклон вперед при ходьбе
        drawMesh(shader, torso, torsoTransform);

        // Head с микродвижениями
        glm::mat4 headTransform = torsoTransform;
        headTransform = glm::translate(headTransform, glm::vec3(0.0f, 0.2f + headBob - crawlBlend * 0.1f, 0.0f));
        headTransform = glm::rotate(headTransform, glm::radians(15.0f * crawlBlend), glm::vec3(1, 0, 0)); // Голова приподнята при ползании
        drawMesh(shader, head, headTransform);

        // Arms - комбинированная анимация ходьбы, ползания и покоя
        // ИСПОЛЬЗУЕМ characterBaseWithLean ДЛЯ ВСЕХ ЧАСТЕЙ ТЕЛА
        glm::mat4 leftArmTransform = characterBaseWithLean;

        if (crawlBlend > 0.0f) {
            // Поза для ползания: руки вытянуты вперед
            leftArmTransform = glm::translate(leftArmTransform, glm::vec3(-0.15f, 0.1f, 0.2f));
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(-80.0f), glm::vec3(1, 0, 0)); // Руки вперед
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(crawlCycle), glm::vec3(1, 0, 0)); // Анимация ползания
        }
        else {
            // Нормальная поза - руки наклоняются вместе с телом
            leftArmTransform = glm::translate(leftArmTransform, glm::vec3(-0.05f, 0.35f, 0.0f));
            float leftArmRotation = armSwing + idleArmSway;
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(leftArmRotation), glm::vec3(1, 0, 0));
            leftArmTransform = glm::rotate(leftArmTransform, glm::radians(10.0f), glm::vec3(0, 0, 1));
        }
        drawMesh(shader, leftArm, leftArmTransform);

        glm::mat4 rightArmTransform = characterBaseWithLean;

        if (crawlBlend > 0.0f) {
            // Поза для ползания: руки вытянуты вперед
            rightArmTransform = glm::translate(rightArmTransform, glm::vec3(0.15f, 0.1f, 0.2f));
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(-80.0f), glm::vec3(1, 0, 0)); // Руки вперед
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(-crawlCycle), glm::vec3(1, 0, 0)); // Анимация ползания (противофаза)
        }
        else {
            // Нормальная поза - руки наклоняются вместе с телом
            rightArmTransform = glm::translate(rightArmTransform, glm::vec3(0.05f, 0.35f, 0.0f));
            float rightArmRotation = -armSwing - idleArmSway;
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(rightArmRotation), glm::vec3(1, 0, 0));
            rightArmTransform = glm::rotate(rightArmTransform, glm::radians(-10.0f), glm::vec3(0, 0, 1));
        }
        drawMesh(shader, rightArm, rightArmTransform);

        // Legs с анимацией ходьбы и ползания
        glm::mat4 leftLegTransform = characterBaseWithLean;

        if (crawlBlend > 0.0f) {
            // Поза для ползания: ноги сзади
            leftLegTransform = glm::translate(leftLegTransform, glm::vec3(-0.1f, 0.05f, -0.3f));
            leftLegTransform = glm::rotate(leftLegTransform, glm::radians(160.0f), glm::vec3(1, 0, 0)); // Ноги сзади
            leftLegTransform = glm::rotate(leftLegTransform, glm::radians(-crawlCycle * 0.5f), glm::vec3(1, 0, 0)); // Анимация ползания
        }
        else {
            // Нормальная поза - ноги наклоняются вместе с телом
            leftLegTransform = glm::translate(leftLegTransform, glm::vec3(-0.15f, -0.05f - landingSquatCurrent, 0.0f));
            leftLegTransform = glm::rotate(leftLegTransform, glm::radians(-walkCycle), glm::vec3(1, 0, 0));
        }
        drawMesh(shader, leftLeg, leftLegTransform);

        glm::mat4 rightLegTransform = characterBaseWithLean;

        if (crawlBlend > 0.0f) {
            // Поза для ползания: ноги сзади
            rightLegTransform = glm::translate(rightLegTransform, glm::vec3(0.1f, 0.05f, -0.3f));
            rightLegTransform = glm::rotate(rightLegTransform, glm::radians(160.0f), glm::vec3(1, 0, 0)); // Ноги сзади
            rightLegTransform = glm::rotate(rightLegTransform, glm::radians(crawlCycle * 0.5f), glm::vec3(1, 0, 0)); // Анимация ползания (противофаза)
        }
        else {
            // Нормальная поза - ноги наклоняются вместе с телом
            rightLegTransform = glm::translate(rightLegTransform, glm::vec3(0.15f, -0.05f - landingSquatCurrent, 0.0f));
            rightLegTransform = glm::rotate(rightLegTransform, glm::radians(walkCycle), glm::vec3(1, 0, 0));
        }
        drawMesh(shader, rightLeg, rightLegTransform);

        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
