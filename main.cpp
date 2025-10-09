#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <vector>

//настройки
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

//глобальные переменные
float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 position(0.0f, 0.0f, 0.0f);
float yaw = 0.0f; 
float speed = 2.0f;

// Прыжок
bool isJumping = false;
float verticalSpeed = 0.0f;

// Камера
glm::vec3 camFront(0.0f, 0.0f, -1.0f);
glm::vec3 camUp(0.0f, 1.0f, 0.0f);
float pitch = 0.0f;

// Мышь
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// для клавиш (поменять вф в будущем)
void processInput(GLFWwindow* window) {
    glm::vec3 forward(sin(glm::radians(yaw)), 0.0f, cos(glm::radians(yaw)));
    glm::vec3 right(cos(glm::radians(yaw)), 0.0f, -sin(glm::radians(yaw)));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += speed * deltaTime * forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= speed * deltaTime * forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        yaw += 90.0f * deltaTime; // вращаем тело влево
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        yaw -= 90.0f * deltaTime; // вращаем тело вправо

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !isJumping) {
        isJumping = true;
        verticalSpeed = 5.0f;
    }
}

// mouse callback 
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // Y инвертирована
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(pitch)) * sin(glm::radians(yaw));
    front.y = sin(glm::radians(pitch));
    front.z = cos(glm::radians(pitch)) * cos(glm::radians(yaw));
    camFront = glm::normalize(front);
}

// OBJ loader (хз где находит, пробовать 2 пути)
struct Vertex { glm::vec3 pos; };
std::vector<Vertex> vertices;
std::vector<unsigned int> indices;

bool loadModel(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
    if (!scene) { std::cerr << importer.GetErrorString() << std::endl; return false; }

    vertices.clear();
    indices.clear();

    aiMesh* mesh = scene->mMeshes[0];
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        vertices.push_back({ glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z) });
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace f = mesh->mFaces[i];
        for (unsigned int j = 0; j < f.mNumIndices; ++j)
            indices.push_back(f.mIndices[j]);
    }

    return true;
}

//  main
int main() {
    // GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Biped Prototype", NULL, NULL);
    if (!window) { std::cerr << "Failed to create GLFW window\n"; return -1; }
    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD\n"; return -1; }

    glEnable(GL_DEPTH_TEST);

    // Load model
    if (!loadModel("C:/Users/Ad/Desktop/midterm/models/biped.obj")) {
        std::cerr << "Failed to load model." << std::endl;
        return -1;
    }

    // VAO/VBO
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    //  shader (vertex + fragment)
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * view * model * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        void main() { FragColor = vec4(0.6, 0.2, 0.8, 1.0); }
    )";

    auto compileShader = [](unsigned int type, const char* src) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) { char info[512]; glGetShaderInfoLog(shader, 512, NULL, info); std::cerr << info << std::endl; }
        return shader;
        };

    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Camera (потом поменять)

    glm::vec3 camPos(0.0f, 3.0f, 7.0f);        // камера чуть выше и сзади
    glm::vec3 camTarget(0.0f, 1.0f, 0.0f);    // центр сцены
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);


    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // Прыжок
        if (isJumping) {
            verticalSpeed -= 9.8f * deltaTime;
            position.y += verticalSpeed * deltaTime;
            if (position.y <= 0.0f) { position.y = 0.0f; isJumping = false; verticalSpeed = 0.0f; }
        }

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::rotate(model, glm::radians(yaw), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, &model[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
